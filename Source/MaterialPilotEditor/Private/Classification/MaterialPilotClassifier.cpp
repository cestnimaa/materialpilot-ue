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
        TEXT("orm"), TEXT("arm"), TEXT("rma"), TEXT("mra"), TEXT("occlusionroughnessmetallic")
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

    auto SetResult = [&Analysis](EMaterialPilotSemantic Semantic, int32 Confidence, const FString& Route, const FString& Reason)
    {
        if (Confidence > Analysis.Confidence)
        {
            Analysis.Semantic = Semantic;
            Analysis.Confidence = Confidence;
            Analysis.ChannelRoute = Route;
            Analysis.Reasons.Reset();
            Analysis.Reasons.Add(Reason);
        }
    };

    if (MaterialPilotClassifier::LooksLikePackedToken(Normalized, TEXT("occlusionroughnessmetallic")))
    {
        SetResult(EMaterialPilotSemantic::PackedORM, 99, TEXT("R=AO, G=Roughness, B=Metallic"), TEXT("Substance Unreal packed profile name matched."));
    }
    if (MaterialPilotClassifier::LooksLikePackedToken(Normalized, TEXT("orm")) || MaterialPilotClassifier::LooksLikePackedToken(Normalized, TEXT("arm")))
    {
        SetResult(EMaterialPilotSemantic::PackedORM, 97, TEXT("R=AO, G=Roughness, B=Metallic"), TEXT("ORM/ARM packed token matched."));
    }
    if (MaterialPilotClassifier::LooksLikePackedToken(Normalized, TEXT("rma")))
    {
        SetResult(EMaterialPilotSemantic::PackedRMA, 96, TEXT("R=Roughness, G=Metallic, B=AO"), TEXT("RMA packed token matched."));
    }
    if (MaterialPilotClassifier::LooksLikePackedToken(Normalized, TEXT("mra")))
    {
        SetResult(EMaterialPilotSemantic::PackedMRA, 96, TEXT("R=Metallic, G=Roughness, B=AO"), TEXT("MRA packed token matched."));
    }

    if (HasToken(Tokens, { TEXT("basecolor"), TEXT("albedo"), TEXT("diffuse"), TEXT("base"), TEXT("color"), TEXT("col"), TEXT("bc") }))
    {
        SetResult(EMaterialPilotSemantic::BaseColor, 92, TEXT("RGB -> Base Color"), TEXT("Base color token matched."));
    }
    if (HasToken(Tokens, { TEXT("normal"), TEXT("nrm"), TEXT("norm"), TEXT("nor") }) || HasTokenContaining(Tokens, TEXT("normal")))
    {
        SetResult(EMaterialPilotSemantic::Normal, 93, TEXT("RGB -> Normal"), TEXT("Normal token matched."));
    }
    if (HasToken(Tokens, { TEXT("roughness"), TEXT("rough"), TEXT("rgh") }))
    {
        SetResult(EMaterialPilotSemantic::Roughness, 91, TEXT("R -> Roughness"), TEXT("Roughness token matched."));
    }
    if (HasToken(Tokens, { TEXT("metallic"), TEXT("metalness"), TEXT("metal"), TEXT("mtl") }))
    {
        SetResult(EMaterialPilotSemantic::Metallic, 91, TEXT("R -> Metallic"), TEXT("Metallic token matched."));
    }
    if (HasToken(Tokens, { TEXT("ao"), TEXT("ambientocclusion"), TEXT("occlusion") }))
    {
        SetResult(EMaterialPilotSemantic::AmbientOcclusion, 90, TEXT("R -> Ambient Occlusion"), TEXT("Ambient occlusion token matched."));
    }
    if (HasToken(Tokens, { TEXT("opacity"), TEXT("opacitymask"), TEXT("alpha"), TEXT("mask") }))
    {
        SetResult(EMaterialPilotSemantic::Opacity, 86, TEXT("R/A -> Opacity Mask"), TEXT("Opacity or mask token matched."));
    }
    if (HasToken(Tokens, { TEXT("emissive"), TEXT("emission"), TEXT("emit") }))
    {
        SetResult(EMaterialPilotSemantic::Emissive, 88, TEXT("RGB -> Emissive Color"), TEXT("Emissive token matched."));
    }
    if (HasToken(Tokens, { TEXT("specular"), TEXT("spec") }))
    {
        SetResult(EMaterialPilotSemantic::Specular, 84, TEXT("R -> Specular"), TEXT("Specular token matched."));
    }
    if (HasToken(Tokens, { TEXT("gloss"), TEXT("glossiness") }))
    {
        SetResult(EMaterialPilotSemantic::Gloss, 88, TEXT("OneMinus(R) -> Roughness"), TEXT("Glossiness token matched; will invert to roughness."));
    }
    if (HasToken(Tokens, { TEXT("smoothness"), TEXT("smooth") }))
    {
        SetResult(EMaterialPilotSemantic::Gloss, 86, TEXT("OneMinus(R/A) -> Roughness"), TEXT("Smoothness token matched; will invert to roughness."));
    }

    if (Analysis.Semantic == EMaterialPilotSemantic::Normal)
    {
        if (HasToken(Tokens, { TEXT("normalgl"), TEXT("opengl"), TEXT("gl") }) || Normalized.Contains(TEXT("normal_gl")))
        {
            Analysis.NormalConvention = EMaterialPilotNormalConvention::OpenGL;
            Analysis.Reasons.Add(TEXT("OpenGL normal token matched; green channel should be flipped."));
        }
        else if (HasToken(Tokens, { TEXT("normaldx"), TEXT("directx"), TEXT("dx") }) || Normalized.Contains(TEXT("normal_dx")))
        {
            Analysis.NormalConvention = EMaterialPilotNormalConvention::DirectX;
            Analysis.Reasons.Add(TEXT("DirectX normal token matched."));
        }
    }

    if (Analysis.Semantic == EMaterialPilotSemantic::Unknown)
    {
        Analysis.ChannelRoute = TEXT("Unmapped");
        Analysis.Confidence = 0;
        Analysis.Reasons.Add(TEXT("No recognized semantic token."));
    }

    Analysis.bReviewRequired = Analysis.Confidence < 85 || Analysis.Semantic == EMaterialPilotSemantic::Unknown;
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
    int32 ConfidenceTotal = 0;
    int32 ConfidenceCount = 0;

    bool bHasPackedORM = false;
    bool bHasPackedRMA = false;
    bool bHasPackedMRA = false;
    bool bHasBaseColor = false;
    bool bHasNormal = false;
    bool bHasNormalGL = false;
    bool bHasOpacity = false;
    bool bHasEmissive = false;
    bool bHasGloss = false;

    TMap<EMaterialPilotSemantic, int32> SemanticCounts;

    for (const FMaterialPilotTextureAnalysis& Texture : Analysis.Textures)
    {
        if (Texture.Semantic != EMaterialPilotSemantic::Unknown)
        {
            ConfidenceTotal += Texture.Confidence;
            ConfidenceCount++;
        }

        SemanticCounts.FindOrAdd(Texture.Semantic)++;

        bHasPackedORM |= Texture.Semantic == EMaterialPilotSemantic::PackedORM;
        bHasPackedRMA |= Texture.Semantic == EMaterialPilotSemantic::PackedRMA;
        bHasPackedMRA |= Texture.Semantic == EMaterialPilotSemantic::PackedMRA;
        bHasBaseColor |= Texture.Semantic == EMaterialPilotSemantic::BaseColor;
        bHasNormal |= Texture.Semantic == EMaterialPilotSemantic::Normal;
        bHasNormalGL |= Texture.Semantic == EMaterialPilotSemantic::Normal && Texture.NormalConvention == EMaterialPilotNormalConvention::OpenGL;
        bHasOpacity |= Texture.Semantic == EMaterialPilotSemantic::Opacity;
        bHasEmissive |= Texture.Semantic == EMaterialPilotSemantic::Emissive;
        bHasGloss |= Texture.Semantic == EMaterialPilotSemantic::Gloss;
    }

    Analysis.AggregateConfidence = ConfidenceCount > 0 ? FMath::RoundToInt(static_cast<float>(ConfidenceTotal) / static_cast<float>(ConfidenceCount)) : 0;

    for (const TPair<EMaterialPilotSemantic, int32>& Pair : SemanticCounts)
    {
        if (Pair.Key != EMaterialPilotSemantic::Unknown && Pair.Value > 1)
        {
            Analysis.Warnings.Add(FString::Printf(TEXT("Duplicate %s maps detected; the first one will be used."), *MaterialPilotSemanticToString(Pair.Key)));
        }
    }

    if (!bHasBaseColor)
    {
        Analysis.Warnings.Add(TEXT("No base color map detected. The generated material will use Unreal defaults for base color."));
    }

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
