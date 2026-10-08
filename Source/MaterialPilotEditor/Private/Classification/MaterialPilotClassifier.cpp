#include "Classification/MaterialPilotClassifier.h"

#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Misc/Char.h"
#include "TextureResource.h"

namespace MaterialPilotClassifier
{
    static const TArray<FString> SemanticSuffixes =
    {
        TEXT("basecolor"), TEXT("base"), TEXT("albedo"), TEXT("diffuse"), TEXT("color"), TEXT("col"), TEXT("bc"),
        TEXT("normal"), TEXT("normaldx"), TEXT("normalgl"), TEXT("nrm"), TEXT("norm"), TEXT("nor"),
        TEXT("roughness"), TEXT("rough"), TEXT("rgh"),
        TEXT("metallic"), TEXT("metalness"), TEXT("metal"), TEXT("mtl"),
        TEXT("ao"), TEXT("ambientocclusion"), TEXT("occlusion"),
        TEXT("opacity"), TEXT("opacitymask"), TEXT("alpha"), TEXT("mask"),
        TEXT("emissive"), TEXT("emission"), TEXT("emit"),
        TEXT("specular"), TEXT("spec"),
        TEXT("gloss"), TEXT("glossiness"), TEXT("smoothness"), TEXT("smooth"),
        TEXT("height"), TEXT("displacement"), TEXT("disp"), TEXT("bump"), TEXT("depth"),
        TEXT("orm"), TEXT("arm"), TEXT("rma"), TEXT("mra"), TEXT("occlusionroughnessmetallic")
    };

    struct FTextureSignal
    {
        bool bAvailable = false;
        bool bMostlyGray = false;
        bool bHasColorChroma = false;
        bool bLooksNormal = false;
        bool bMostlyBinary = false;
        bool bMostlyDark = false;
    };

    struct FSemanticCandidate
    {
        EMaterialPilotSemantic Semantic = EMaterialPilotSemantic::Unknown;
        int32 Score = 0;
        FString Route;
        EMaterialPilotNormalConvention NormalConvention = EMaterialPilotNormalConvention::ProjectDefault;
        TArray<FString> Reasons;
    };

    static FString NormalizeName(const FString& Input)
    {
        FString Result;
        Result.Reserve(Input.Len());

        for (TCHAR Char : Input)
        {
            if (FChar::IsAlnum(Char))
            {
                Result.AppendChar(FChar::ToLower(Char));
            }
            else
            {
                Result.AppendChar(TEXT('_'));
            }
        }

        while (Result.Contains(TEXT("__")))
        {
            Result.ReplaceInline(TEXT("__"), TEXT("_"));
        }

        Result.TrimStartAndEndInline();
        return Result;
    }

    static bool LooksLikePackedToken(const FString& NormalizedName, const FString& Token)
    {
        return NormalizedName == Token ||
            NormalizedName.EndsWith(TEXT("_") + Token) ||
            NormalizedName.Contains(TEXT("_") + Token + TEXT("_"));
    }

    static bool HasAnyToken(const TArray<FString>& Tokens, const TArray<FString>& Candidates)
    {
        for (const FString& Candidate : Candidates)
        {
            if (Tokens.Contains(Candidate))
            {
                return true;
            }
        }
        return false;
    }

    static FString RouteForSemantic(EMaterialPilotSemantic Semantic)
    {
        switch (Semantic)
        {
        case EMaterialPilotSemantic::BaseColor:
            return TEXT("RGB -> Base Color");
        case EMaterialPilotSemantic::Normal:
            return TEXT("RGB -> Normal");
        case EMaterialPilotSemantic::Roughness:
            return TEXT("R -> Roughness");
        case EMaterialPilotSemantic::Metallic:
            return TEXT("R -> Metallic");
        case EMaterialPilotSemantic::AmbientOcclusion:
            return TEXT("R -> Ambient Occlusion");
        case EMaterialPilotSemantic::Opacity:
            return TEXT("R/A -> Opacity Mask");
        case EMaterialPilotSemantic::Emissive:
            return TEXT("RGB -> Emissive Color");
        case EMaterialPilotSemantic::Specular:
            return TEXT("R -> Specular");
        case EMaterialPilotSemantic::Gloss:
            return TEXT("OneMinus(R) -> Roughness");
        case EMaterialPilotSemantic::Height:
            return TEXT("Optional height/displacement map");
        case EMaterialPilotSemantic::PackedORM:
            return TEXT("R=AO, G=Roughness, B=Metallic");
        case EMaterialPilotSemantic::PackedRMA:
            return TEXT("R=Roughness, G=Metallic, B=AO");
        case EMaterialPilotSemantic::PackedMRA:
            return TEXT("R=Metallic, G=Roughness, B=AO");
        case EMaterialPilotSemantic::PackedMetallicSmoothness:
            return TEXT("R=Metallic, A=Smoothness");
        default:
            return TEXT("Unmapped");
        }
    }

    static int32 SemanticPriority(EMaterialPilotSemantic Semantic)
    {
        switch (Semantic)
        {
        case EMaterialPilotSemantic::Normal:
            return 100;
        case EMaterialPilotSemantic::PackedORM:
        case EMaterialPilotSemantic::PackedRMA:
        case EMaterialPilotSemantic::PackedMRA:
        case EMaterialPilotSemantic::PackedMetallicSmoothness:
            return 95;
        case EMaterialPilotSemantic::BaseColor:
            return 90;
        case EMaterialPilotSemantic::Roughness:
            return 85;
        case EMaterialPilotSemantic::Metallic:
            return 84;
        case EMaterialPilotSemantic::AmbientOcclusion:
            return 83;
        case EMaterialPilotSemantic::Height:
            return 40;
        default:
            return 50;
        }
    }

    static void AddEvidence(TArray<FSemanticCandidate>& Candidates, EMaterialPilotSemantic Semantic, int32 Score, const FString& Reason, EMaterialPilotNormalConvention NormalConvention = EMaterialPilotNormalConvention::ProjectDefault)
    {
        for (FSemanticCandidate& Candidate : Candidates)
        {
            if (Candidate.Semantic == Semantic)
            {
                Candidate.Score += Score;
                Candidate.Reasons.AddUnique(Reason);
                if (NormalConvention != EMaterialPilotNormalConvention::ProjectDefault)
                {
                    Candidate.NormalConvention = NormalConvention;
                }
                return;
            }
        }

        FSemanticCandidate NewCandidate;
        NewCandidate.Semantic = Semantic;
        NewCandidate.Score = Score;
        NewCandidate.Route = RouteForSemantic(Semantic);
        NewCandidate.NormalConvention = NormalConvention;
        NewCandidate.Reasons.Add(Reason);
        Candidates.Add(NewCandidate);
    }

    static FTextureSignal AnalyzeTexturePixels(UTexture2D* Texture)
    {
        FTextureSignal Signal;
        if (!Texture || !Texture->Source.IsValid())
        {
            return Signal;
        }

        const int32 SizeX = Texture->Source.GetSizeX();
        const int32 SizeY = Texture->Source.GetSizeY();
        const ETextureSourceFormat Format = Texture->Source.GetFormat();
        const bool bIsGray8 = Format == TSF_G8;
        const bool bIsBGRA8 = Format == TSF_BGRA8 || Format == TSF_RGBA8_DEPRECATED;
        if (SizeX <= 0 || SizeY <= 0 || (!bIsGray8 && !bIsBGRA8))
        {
            return Signal;
        }

        TArray64<uint8> MipData;
        if (!Texture->Source.GetMipData(MipData, 0))
        {
            return Signal;
        }

        const int64 PixelCount = static_cast<int64>(SizeX) * static_cast<int64>(SizeY);
        const int64 BytesPerPixel = bIsGray8 ? 1 : 4;
        if (PixelCount <= 0 || MipData.Num() < PixelCount * BytesPerPixel)
        {
            return Signal;
        }

        const int64 Stride = FMath::Max<int64>(1, PixelCount / 4096);
        double SumR = 0.0;
        double SumG = 0.0;
        double SumB = 0.0;
        double SumDelta = 0.0;
        int32 BinarySamples = 0;
        int32 SampleCount = 0;

        for (int64 PixelIndex = 0; PixelIndex < PixelCount; PixelIndex += Stride)
        {
            const int64 Offset = PixelIndex * BytesPerPixel;
            float R = 0.0f;
            float G = 0.0f;
            float B = 0.0f;

            if (bIsGray8)
            {
                R = G = B = static_cast<float>(MipData[Offset]) / 255.0f;
            }
            else
            {
                B = static_cast<float>(MipData[Offset]) / 255.0f;
                G = static_cast<float>(MipData[Offset + 1]) / 255.0f;
                R = static_cast<float>(MipData[Offset + 2]) / 255.0f;
            }

            SumR += R;
            SumG += G;
            SumB += B;
            SumDelta += FMath::Abs(R - G) + FMath::Abs(G - B) + FMath::Abs(R - B);

            const float Luma = (R + G + B) / 3.0f;
            if (Luma < 0.06f || Luma > 0.94f)
            {
                BinarySamples++;
            }
            SampleCount++;
        }

        if (SampleCount <= 0)
        {
            return Signal;
        }

        const float AvgR = static_cast<float>(SumR / SampleCount);
        const float AvgG = static_cast<float>(SumG / SampleCount);
        const float AvgB = static_cast<float>(SumB / SampleCount);
        const float AvgDelta = static_cast<float>(SumDelta / SampleCount);
        const float AvgLuma = (AvgR + AvgG + AvgB) / 3.0f;
        const float BinaryRatio = static_cast<float>(BinarySamples) / static_cast<float>(SampleCount);

        Signal.bAvailable = true;
        Signal.bMostlyGray = bIsGray8 || AvgDelta < 0.08f;
        Signal.bHasColorChroma = !Signal.bMostlyGray && AvgDelta > 0.16f;
        Signal.bLooksNormal = !bIsGray8 && AvgB > AvgR + 0.12f && AvgB > AvgG + 0.08f && AvgB > 0.55f && AvgR > 0.18f && AvgG > 0.18f;
        Signal.bMostlyBinary = BinaryRatio > 0.65f;
        Signal.bMostlyDark = AvgLuma < 0.18f;
        return Signal;
    }

    static int32 RecipeSelectionScore(const FMaterialPilotTextureAnalysis& Texture)
    {
        int32 Score = Texture.Confidence;
        if (Texture.Semantic == EMaterialPilotSemantic::Normal)
        {
            if (Texture.NormalConvention == EMaterialPilotNormalConvention::DirectX)
            {
                Score += 12;
            }
            else if (Texture.NormalConvention == EMaterialPilotNormalConvention::OpenGL)
            {
                Score -= 8;
            }
        }
        return Score;
    }

    static int32 FindBestIndexForSemantic(const FMaterialPilotSetAnalysis& Analysis, EMaterialPilotSemantic Semantic)
    {
        int32 BestIndex = INDEX_NONE;
        int32 BestScore = MIN_int32;

        for (int32 Index = 0; Index < Analysis.Textures.Num(); ++Index)
        {
            const FMaterialPilotTextureAnalysis& Texture = Analysis.Textures[Index];
            if (Texture.Semantic != Semantic)
            {
                continue;
            }

            const int32 Score = RecipeSelectionScore(Texture);
            if (BestIndex == INDEX_NONE || Score > BestScore)
            {
                BestIndex = Index;
                BestScore = Score;
            }
        }

        return BestIndex;
    }

    static void MarkSelected(FMaterialPilotSetAnalysis& Analysis, int32 Index)
    {
        if (!Analysis.Textures.IsValidIndex(Index))
        {
            return;
        }

        FMaterialPilotTextureAnalysis& Texture = Analysis.Textures[Index];
        Texture.bUsedByRecipe = true;
        Texture.Resolution = TEXT("Used");
        Texture.bReviewRequired = Texture.Confidence < 60;
    }

    static int32 CountSemantic(const FMaterialPilotSetAnalysis& Analysis, EMaterialPilotSemantic Semantic)
    {
        int32 Count = 0;
        for (const FMaterialPilotTextureAnalysis& Texture : Analysis.Textures)
        {
            if (Texture.Semantic == Semantic)
            {
                Count++;
            }
        }
        return Count;
    }
}

FMaterialPilotSetAnalysis FMaterialPilotClassifier::AnalyzeTextures(const TArray<UTexture2D*>& Textures)
{
    FMaterialPilotSetAnalysis Analysis;
    Analysis.SetName = TEXT("MaterialPilotSet");

    for (UTexture2D* Texture : Textures)
    {
        if (!IsValid(Texture))
        {
            continue;
        }

        Analysis.Textures.Add(AnalyzeTexture(Texture));
    }

    if (Analysis.Textures.Num() > 0)
    {
        TMap<FString, int32> StemCounts;
        for (const FMaterialPilotTextureAnalysis& TextureAnalysis : Analysis.Textures)
        {
            StemCounts.FindOrAdd(TextureAnalysis.Stem)++;
        }

        FString BestStem;
        int32 BestCount = 0;
        for (const TPair<FString, int32>& Pair : StemCounts)
        {
            if (Pair.Value > BestCount)
            {
                BestStem = Pair.Key;
                BestCount = Pair.Value;
            }
        }

        Analysis.SetName = MaterialPilotMakeObjectSafeName(BestStem.IsEmpty() ? Analysis.Textures[0].Stem : BestStem);
    }

    FinalizeRecipe(Analysis);
    return Analysis;
}

FMaterialPilotTextureAnalysis FMaterialPilotClassifier::AnalyzeTexture(UTexture2D* Texture)
{
    FMaterialPilotTextureAnalysis Analysis;
    Analysis.Texture = Texture;
    Analysis.AssetName = Texture ? Texture->GetName() : TEXT("Texture");

    const TArray<FString> Tokens = Tokenize(Analysis.AssetName);
    const FString Normalized = FString::Join(Tokens, TEXT("_"));
    Analysis.Stem = BuildStem(Analysis.AssetName, Tokens);

    TArray<MaterialPilotClassifier::FSemanticCandidate> Candidates;
    auto Add = [&Candidates](EMaterialPilotSemantic Semantic, int32 Score, const FString& Reason, EMaterialPilotNormalConvention NormalConvention = EMaterialPilotNormalConvention::ProjectDefault)
    {
        MaterialPilotClassifier::AddEvidence(Candidates, Semantic, Score, Reason, NormalConvention);
    };

    if (MaterialPilotClassifier::LooksLikePackedToken(Normalized, TEXT("occlusionroughnessmetallic")))
    {
        Add(EMaterialPilotSemantic::PackedORM, 120, TEXT("Substance Unreal packed profile name matched."));
    }
    if (MaterialPilotClassifier::LooksLikePackedToken(Normalized, TEXT("orm")) || MaterialPilotClassifier::LooksLikePackedToken(Normalized, TEXT("arm")))
    {
        Add(EMaterialPilotSemantic::PackedORM, 116, TEXT("ORM/ARM packed token matched."));
    }
    if (MaterialPilotClassifier::LooksLikePackedToken(Normalized, TEXT("rma")))
    {
        Add(EMaterialPilotSemantic::PackedRMA, 114, TEXT("RMA packed token matched."));
    }
    if (MaterialPilotClassifier::LooksLikePackedToken(Normalized, TEXT("mra")))
    {
        Add(EMaterialPilotSemantic::PackedMRA, 114, TEXT("MRA packed token matched."));
    }

    if (HasToken(Tokens, { TEXT("basecolor"), TEXT("albedo"), TEXT("diffuse"), TEXT("bc") }))
    {
        Add(EMaterialPilotSemantic::BaseColor, 95, TEXT("Strong base color token matched."));
    }
    if (HasToken(Tokens, { TEXT("color"), TEXT("col") }))
    {
        Add(EMaterialPilotSemantic::BaseColor, 76, TEXT("Color token matched."));
    }
    if (HasToken(Tokens, { TEXT("base") }))
    {
        Add(EMaterialPilotSemantic::BaseColor, 12, TEXT("Weak shared base token matched."));
    }

    if (HasToken(Tokens, { TEXT("normaldx"), TEXT("directx"), TEXT("dx") }) || Normalized.Contains(TEXT("normal_dx")))
    {
        Add(EMaterialPilotSemantic::Normal, 104, TEXT("DirectX normal token matched."), EMaterialPilotNormalConvention::DirectX);
    }
    if (HasToken(Tokens, { TEXT("normalgl"), TEXT("opengl") }) || Normalized.Contains(TEXT("normal_gl")) || Normalized.Contains(TEXT("normal_opengl")))
    {
        Add(EMaterialPilotSemantic::Normal, 100, TEXT("OpenGL normal token matched; green channel will be flipped if selected."), EMaterialPilotNormalConvention::OpenGL);
    }
    if (HasToken(Tokens, { TEXT("normal"), TEXT("nrm"), TEXT("norm"), TEXT("nor") }) || HasTokenContaining(Tokens, TEXT("normal")))
    {
        Add(EMaterialPilotSemantic::Normal, 94, TEXT("Normal token matched."), EMaterialPilotNormalConvention::DirectX);
    }
    if (HasToken(Tokens, { TEXT("roughness"), TEXT("rough"), TEXT("rgh") }))
    {
        Add(EMaterialPilotSemantic::Roughness, 96, TEXT("Roughness token matched."));
    }
    if (HasToken(Tokens, { TEXT("metallic"), TEXT("metalness"), TEXT("metal"), TEXT("mtl") }))
    {
        Add(EMaterialPilotSemantic::Metallic, 98, TEXT("Metallic/metalness token matched."));
    }
    if (HasToken(Tokens, { TEXT("ao"), TEXT("ambientocclusion"), TEXT("occlusion") }))
    {
        Add(EMaterialPilotSemantic::AmbientOcclusion, 94, TEXT("Ambient occlusion token matched."));
    }
    if (HasToken(Tokens, { TEXT("opacity"), TEXT("opacitymask"), TEXT("alpha"), TEXT("mask") }))
    {
        Add(EMaterialPilotSemantic::Opacity, 88, TEXT("Opacity or mask token matched."));
    }
    if (HasToken(Tokens, { TEXT("emissive"), TEXT("emission"), TEXT("emit") }))
    {
        Add(EMaterialPilotSemantic::Emissive, 90, TEXT("Emissive token matched."));
    }
    if (HasToken(Tokens, { TEXT("specular"), TEXT("spec") }))
    {
        Add(EMaterialPilotSemantic::Specular, 78, TEXT("Specular token matched."));
    }
    if (HasToken(Tokens, { TEXT("gloss"), TEXT("glossiness") }))
    {
        Add(EMaterialPilotSemantic::Gloss, 92, TEXT("Glossiness token matched; will invert to roughness."));
    }
    if (HasToken(Tokens, { TEXT("smoothness"), TEXT("smooth") }))
    {
        Add(EMaterialPilotSemantic::Gloss, 88, TEXT("Smoothness token matched; will invert to roughness."));
    }
    if (HasToken(Tokens, { TEXT("height"), TEXT("displacement"), TEXT("disp"), TEXT("bump"), TEXT("depth") }))
    {
        Add(EMaterialPilotSemantic::Height, 90, TEXT("Height/displacement token matched."));
    }

    if (Tokens.Num() > 0)
    {
        const FString& LastToken = Tokens.Last();
        if (LastToken == TEXT("roughness") || LastToken == TEXT("rough") || LastToken == TEXT("rgh"))
        {
            Add(EMaterialPilotSemantic::Roughness, 16, TEXT("Final token favors roughness."));
        }
        else if (LastToken == TEXT("metallic") || LastToken == TEXT("metalness") || LastToken == TEXT("metal"))
        {
            Add(EMaterialPilotSemantic::Metallic, 16, TEXT("Final token favors metallic."));
        }
        else if (LastToken == TEXT("ao") || LastToken == TEXT("occlusion"))
        {
            Add(EMaterialPilotSemantic::AmbientOcclusion, 16, TEXT("Final token favors ambient occlusion."));
        }
        else if (LastToken == TEXT("normal") || LastToken == TEXT("nrm"))
        {
            Add(EMaterialPilotSemantic::Normal, 12, TEXT("Final token favors normal."), EMaterialPilotNormalConvention::DirectX);
        }
        else if (LastToken == TEXT("height") || LastToken == TEXT("displacement") || LastToken == TEXT("bump") || LastToken == TEXT("depth"))
        {
            Add(EMaterialPilotSemantic::Height, 12, TEXT("Final token favors height."));
        }
    }

    const MaterialPilotClassifier::FTextureSignal Signal = MaterialPilotClassifier::AnalyzeTexturePixels(Texture);
    if (Signal.bAvailable)
    {
        if (Signal.bLooksNormal)
        {
            Add(EMaterialPilotSemantic::Normal, 30, TEXT("Texture data has a blue-dominant normal-map pattern."), EMaterialPilotNormalConvention::DirectX);
        }
        if (Signal.bHasColorChroma)
        {
            Add(EMaterialPilotSemantic::BaseColor, 18, TEXT("Texture data has visible color variation."));
        }
        if (Signal.bMostlyGray)
        {
            Add(EMaterialPilotSemantic::Roughness, 7, TEXT("Texture data is grayscale, suitable for surface data maps."));
            Add(EMaterialPilotSemantic::Metallic, 7, TEXT("Texture data is grayscale, suitable for surface data maps."));
            Add(EMaterialPilotSemantic::AmbientOcclusion, 7, TEXT("Texture data is grayscale, suitable for surface data maps."));
            Add(EMaterialPilotSemantic::Height, 7, TEXT("Texture data is grayscale, suitable for height/data maps."));
        }
        if (Signal.bMostlyBinary)
        {
            Add(EMaterialPilotSemantic::Opacity, 10, TEXT("Texture data is mostly black/white, suitable for masks."));
            Add(EMaterialPilotSemantic::Metallic, 6, TEXT("Texture data is mostly black/white, suitable for metallic masks."));
        }
        if (Signal.bMostlyDark && MaterialPilotClassifier::HasAnyToken(Tokens, { TEXT("metallic"), TEXT("metalness"), TEXT("metal") }))
        {
            Add(EMaterialPilotSemantic::Metallic, 8, TEXT("Texture data is dark grayscale, common for mostly non-metal materials."));
        }
    }

    if (Texture)
    {
        if (Texture->CompressionSettings == TC_Normalmap)
        {
            Add(EMaterialPilotSemantic::Normal, 28, TEXT("Texture import settings are already Normalmap."), EMaterialPilotNormalConvention::DirectX);
        }
        else if (!Texture->SRGB)
        {
            Add(EMaterialPilotSemantic::Roughness, 4, TEXT("Texture import setting is non-sRGB, common for data maps."));
            Add(EMaterialPilotSemantic::Metallic, 4, TEXT("Texture import setting is non-sRGB, common for data maps."));
            Add(EMaterialPilotSemantic::AmbientOcclusion, 4, TEXT("Texture import setting is non-sRGB, common for data maps."));
        }
        else
        {
            Add(EMaterialPilotSemantic::BaseColor, 5, TEXT("Texture import setting is sRGB, common for color maps."));
        }
    }

    const MaterialPilotClassifier::FSemanticCandidate* BestCandidate = nullptr;
    for (const MaterialPilotClassifier::FSemanticCandidate& Candidate : Candidates)
    {
        if (!BestCandidate ||
            Candidate.Score > BestCandidate->Score ||
            (Candidate.Score == BestCandidate->Score && MaterialPilotClassifier::SemanticPriority(Candidate.Semantic) > MaterialPilotClassifier::SemanticPriority(BestCandidate->Semantic)))
        {
            BestCandidate = &Candidate;
        }
    }

    if (BestCandidate && BestCandidate->Score >= 35)
    {
        Analysis.Semantic = BestCandidate->Semantic;
        Analysis.Confidence = FMath::Clamp(BestCandidate->Score, 0, 99);
        Analysis.ChannelRoute = BestCandidate->Route;
        Analysis.NormalConvention = BestCandidate->NormalConvention;
        Analysis.Reasons = BestCandidate->Reasons;

        if (Analysis.Semantic == EMaterialPilotSemantic::Normal && Analysis.NormalConvention == EMaterialPilotNormalConvention::ProjectDefault)
        {
            Analysis.NormalConvention = EMaterialPilotNormalConvention::DirectX;
        }
    }
    else
    {
        Analysis.Semantic = EMaterialPilotSemantic::Unknown;
        Analysis.ChannelRoute = TEXT("Unmapped");
        Analysis.Confidence = 0;
        Analysis.Reasons.Add(TEXT("No recognized semantic token or reliable texture-data signature."));
    }

    Analysis.bReviewRequired = Analysis.Semantic != EMaterialPilotSemantic::Unknown && Analysis.Confidence < 60;
    Analysis.Resolution = Analysis.Semantic == EMaterialPilotSemantic::Unknown ? TEXT("Ignored") : TEXT("Pending set resolver");
    return Analysis;
}

TArray<FString> FMaterialPilotClassifier::Tokenize(const FString& Name)
{
    TArray<FString> Tokens;
    MaterialPilotClassifier::NormalizeName(Name).ParseIntoArray(Tokens, TEXT("_"), true);
    return Tokens;
}

FString FMaterialPilotClassifier::BuildStem(const FString& AssetName, const TArray<FString>& Tokens)
{
    TArray<FString> StemTokens;
    for (const FString& Token : Tokens)
    {
        if (!MaterialPilotClassifier::SemanticSuffixes.Contains(Token))
        {
            StemTokens.Add(Token);
        }
    }

    if (StemTokens.Num() == 0 && Tokens.Num() > 0)
    {
        StemTokens.Add(Tokens[0]);
    }

    return StemTokens.Num() > 0 ? FString::Join(StemTokens, TEXT("_")) : AssetName;
}

bool FMaterialPilotClassifier::HasToken(const TArray<FString>& Tokens, const TArray<FString>& Candidates)
{
    for (const FString& Candidate : Candidates)
    {
        if (Tokens.Contains(Candidate))
        {
            return true;
        }
    }
    return false;
}

bool FMaterialPilotClassifier::HasTokenContaining(const TArray<FString>& Tokens, const FString& Needle)
{
    for (const FString& Token : Tokens)
    {
        if (Token.Contains(Needle))
        {
            return true;
        }
    }
    return false;
}

void FMaterialPilotClassifier::FinalizeRecipe(FMaterialPilotSetAnalysis& Analysis)
{
    Analysis.Warnings.Reset();

    for (FMaterialPilotTextureAnalysis& Texture : Analysis.Textures)
    {
        Texture.bUsedByRecipe = false;
        Texture.bReviewRequired = false;
        if (Texture.Semantic == EMaterialPilotSemantic::Unknown)
        {
            Texture.Resolution = TEXT("Ignored");
        }
        else if (Texture.Semantic == EMaterialPilotSemantic::Height)
        {
            Texture.Resolution = TEXT("Detected optional");
        }
        else
        {
            Texture.Resolution = TEXT("Alternative ignored");
        }
    }

    const int32 BestPackedORM = MaterialPilotClassifier::FindBestIndexForSemantic(Analysis, EMaterialPilotSemantic::PackedORM);
    const int32 BestPackedRMA = MaterialPilotClassifier::FindBestIndexForSemantic(Analysis, EMaterialPilotSemantic::PackedRMA);
    const int32 BestPackedMRA = MaterialPilotClassifier::FindBestIndexForSemantic(Analysis, EMaterialPilotSemantic::PackedMRA);
    const int32 BestPackedMetallicSmoothness = MaterialPilotClassifier::FindBestIndexForSemantic(Analysis, EMaterialPilotSemantic::PackedMetallicSmoothness);

    int32 BestPacked = INDEX_NONE;
    const int32 PackedCandidates[] = { BestPackedORM, BestPackedRMA, BestPackedMRA, BestPackedMetallicSmoothness };
    for (int32 CandidateIndex : PackedCandidates)
    {
        if (CandidateIndex != INDEX_NONE &&
            (BestPacked == INDEX_NONE || MaterialPilotClassifier::RecipeSelectionScore(Analysis.Textures[CandidateIndex]) > MaterialPilotClassifier::RecipeSelectionScore(Analysis.Textures[BestPacked])))
        {
            BestPacked = CandidateIndex;
        }
    }

    if (BestPacked != INDEX_NONE)
    {
        MaterialPilotClassifier::MarkSelected(Analysis, BestPacked);
    }

    const EMaterialPilotSemantic AlwaysSelectRoles[] =
    {
        EMaterialPilotSemantic::BaseColor,
        EMaterialPilotSemantic::Normal,
        EMaterialPilotSemantic::Opacity,
        EMaterialPilotSemantic::Emissive,
        EMaterialPilotSemantic::Specular
    };

    for (EMaterialPilotSemantic Role : AlwaysSelectRoles)
    {
        MaterialPilotClassifier::MarkSelected(Analysis, MaterialPilotClassifier::FindBestIndexForSemantic(Analysis, Role));
    }

    if (BestPacked == INDEX_NONE)
    {
        MaterialPilotClassifier::MarkSelected(Analysis, MaterialPilotClassifier::FindBestIndexForSemantic(Analysis, EMaterialPilotSemantic::Metallic));
        MaterialPilotClassifier::MarkSelected(Analysis, MaterialPilotClassifier::FindBestIndexForSemantic(Analysis, EMaterialPilotSemantic::AmbientOcclusion));

        const int32 BestRoughness = MaterialPilotClassifier::FindBestIndexForSemantic(Analysis, EMaterialPilotSemantic::Roughness);
        if (BestRoughness != INDEX_NONE)
        {
            MaterialPilotClassifier::MarkSelected(Analysis, BestRoughness);
        }
        else
        {
            MaterialPilotClassifier::MarkSelected(Analysis, MaterialPilotClassifier::FindBestIndexForSemantic(Analysis, EMaterialPilotSemantic::Gloss));
        }
    }

    int32 ConfidenceTotal = 0;
    int32 ConfidenceCount = 0;
    int32 UnknownCount = 0;
    int32 HeightCount = 0;
    bool bIgnoredOpenGLNormal = false;
    bool bSelectedDirectXNormal = false;

    for (FMaterialPilotTextureAnalysis& Texture : Analysis.Textures)
    {
        if (Texture.Semantic == EMaterialPilotSemantic::Unknown)
        {
            UnknownCount++;
            Texture.Resolution = TEXT("Ignored");
            Texture.bReviewRequired = false;
            continue;
        }

        if (Texture.Semantic == EMaterialPilotSemantic::Height)
        {
            HeightCount++;
            Texture.Resolution = TEXT("Detected optional");
            Texture.bReviewRequired = false;
        }

        if (!Texture.bUsedByRecipe && Texture.Semantic != EMaterialPilotSemantic::Height)
        {
            Texture.Resolution = TEXT("Alternative ignored");
            Texture.bReviewRequired = false;
        }

        if (Texture.bUsedByRecipe || Texture.Semantic == EMaterialPilotSemantic::Height)
        {
            ConfidenceTotal += Texture.Confidence;
            ConfidenceCount++;
        }

        bSelectedDirectXNormal |= Texture.bUsedByRecipe &&
            Texture.Semantic == EMaterialPilotSemantic::Normal &&
            Texture.NormalConvention != EMaterialPilotNormalConvention::OpenGL;
        bIgnoredOpenGLNormal |= !Texture.bUsedByRecipe &&
            Texture.Semantic == EMaterialPilotSemantic::Normal &&
            Texture.NormalConvention == EMaterialPilotNormalConvention::OpenGL;
    }

    Analysis.AggregateConfidence = ConfidenceCount > 0 ? FMath::RoundToInt(static_cast<float>(ConfidenceTotal) / static_cast<float>(ConfidenceCount)) : 0;

    const EMaterialPilotSemantic DuplicateRoles[] =
    {
        EMaterialPilotSemantic::BaseColor,
        EMaterialPilotSemantic::Normal,
        EMaterialPilotSemantic::Roughness,
        EMaterialPilotSemantic::Metallic,
        EMaterialPilotSemantic::AmbientOcclusion,
        EMaterialPilotSemantic::Opacity,
        EMaterialPilotSemantic::Emissive,
        EMaterialPilotSemantic::Specular,
        EMaterialPilotSemantic::Gloss
    };

    for (EMaterialPilotSemantic Role : DuplicateRoles)
    {
        const int32 Count = MaterialPilotClassifier::CountSemantic(Analysis, Role);
        if (Count > 1)
        {
            if (const FMaterialPilotTextureAnalysis* Selected = Analysis.FindSemantic(Role))
            {
                Analysis.Warnings.Add(FString::Printf(TEXT("Multiple %s candidates found; selected %s and ignored the other variants."), *MaterialPilotSemanticToString(Role), *Selected->AssetName));
            }
        }
    }

    if (BestPacked != INDEX_NONE)
    {
        Analysis.Warnings.Add(TEXT("Packed surface map selected; separate roughness/metallic/AO alternatives will be ignored for this parent material."));
    }

    if (bSelectedDirectXNormal && bIgnoredOpenGLNormal)
    {
        Analysis.Warnings.Add(TEXT("Both DirectX and OpenGL normal variants were found; selected the Unreal-friendly DirectX/default normal and ignored the OpenGL variant."));
    }

    if (HeightCount > 0)
    {
        Analysis.Warnings.Add(TEXT("Height/displacement map detected as optional. The current generated material keeps building and leaves it unconnected."));
    }

    if (UnknownCount > 0)
    {
        Analysis.Warnings.Add(FString::Printf(TEXT("Ignored %d unrecognized optional texture(s); this does not block material creation."), UnknownCount));
    }

    if (!Analysis.FindSemantic(EMaterialPilotSemantic::BaseColor))
    {
        Analysis.Warnings.Add(TEXT("No base color map selected. The generated material will use Unreal's default base color."));
    }

    const bool bHasNormal = Analysis.FindSemantic(EMaterialPilotSemantic::Normal) != nullptr;
    const bool bHasNormalGL = bHasNormal && Analysis.FindSemantic(EMaterialPilotSemantic::Normal)->NormalConvention == EMaterialPilotNormalConvention::OpenGL;
    const bool bHasOpacity = Analysis.FindSemantic(EMaterialPilotSemantic::Opacity) != nullptr;
    const bool bHasEmissive = Analysis.FindSemantic(EMaterialPilotSemantic::Emissive) != nullptr;
    const bool bHasPackedORM = Analysis.FindSemantic(EMaterialPilotSemantic::PackedORM) != nullptr;
    const bool bHasPackedRMA = Analysis.FindSemantic(EMaterialPilotSemantic::PackedRMA) != nullptr;
    const bool bHasPackedMRA = Analysis.FindSemantic(EMaterialPilotSemantic::PackedMRA) != nullptr;
    const bool bHasGloss = Analysis.FindSemantic(EMaterialPilotSemantic::Gloss) != nullptr;

    FString Packing = TEXT("SEPARATE");
    if (bHasPackedORM)
    {
        Packing = TEXT("PACKED_ORM");
    }
    else if (bHasPackedRMA)
    {
        Packing = TEXT("PACKED_RMA");
    }
    else if (bHasPackedMRA)
    {
        Packing = TEXT("PACKED_MRA");
    }
    else if (bHasGloss)
    {
        Packing = TEXT("GLOSS_TO_ROUGHNESS");
    }

    Analysis.RecipeSignature = FString::Printf(
        TEXT("PBR_MR_%s_%s_%s_%s"),
        bHasOpacity ? TEXT("MASKED") : TEXT("OPAQUE"),
        *Packing,
        bHasNormal ? (bHasNormalGL ? TEXT("NORMAL_GL") : TEXT("NORMAL_DX")) : TEXT("NO_NORMAL"),
        bHasEmissive ? TEXT("EMISSIVE") : TEXT("NO_EMISSIVE"));
}
