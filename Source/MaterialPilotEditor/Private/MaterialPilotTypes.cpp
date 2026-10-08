#include "MaterialPilotTypes.h"

#include "Components/PrimitiveComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Misc/Char.h"
#include "UObject/Object.h"

bool FMaterialPilotTarget::IsValid() const
{
    switch (Kind)
    {
    case EMaterialPilotTargetKind::Component:
        return Component.IsValid();
    case EMaterialPilotTargetKind::StaticMeshAsset:
        return StaticMesh.IsValid();
    case EMaterialPilotTargetKind::SkeletalMeshAsset:
        return SkeletalMesh.IsValid();
    default:
        return false;
    }
}

FString FMaterialPilotTextureAnalysis::ToReportLine() const
{
    FString ReasonText = FString::Join(Reasons, TEXT("; "));
    if (ReasonText.IsEmpty())
    {
        ReasonText = TEXT("No strong evidence.");
    }

    FString ResolutionText = Resolution;
    if (ResolutionText.IsEmpty())
    {
        ResolutionText = bUsedByRecipe ? TEXT("Used") : (bReviewRequired ? TEXT("Review suggested") : TEXT("Ignored"));
    }

    return FString::Printf(
        TEXT("%s -> %s | %s | %d%% | %s | %s"),
        *AssetName,
        *MaterialPilotSemanticToString(Semantic),
        *ChannelRoute,
        Confidence,
        *ResolutionText,
        *ReasonText);
}

bool FMaterialPilotSetAnalysis::HasRequiredInputs() const
{
    const bool bHasBaseColor = FindSemantic(EMaterialPilotSemantic::BaseColor) != nullptr;
    const bool bHasNormal = FindSemantic(EMaterialPilotSemantic::Normal) != nullptr;
    const bool bHasPacked = FindSemantic(EMaterialPilotSemantic::PackedORM) != nullptr ||
        FindSemantic(EMaterialPilotSemantic::PackedRMA) != nullptr ||
        FindSemantic(EMaterialPilotSemantic::PackedMRA) != nullptr ||
        FindSemantic(EMaterialPilotSemantic::PackedMetallicSmoothness) != nullptr;
    const bool bHasSeparateSurface = FindSemantic(EMaterialPilotSemantic::Roughness) != nullptr ||
        FindSemantic(EMaterialPilotSemantic::Metallic) != nullptr ||
        FindSemantic(EMaterialPilotSemantic::AmbientOcclusion) != nullptr;

    return bHasBaseColor || bHasNormal || bHasPacked || bHasSeparateSurface;
}

bool FMaterialPilotSetAnalysis::HasBlockingAmbiguity(int32 RequiredConfidence) const
{
    (void)RequiredConfidence;
    return !HasRequiredInputs();
}

const FMaterialPilotTextureAnalysis* FMaterialPilotSetAnalysis::FindSemantic(EMaterialPilotSemantic Semantic) const
{
    return Textures.FindByPredicate([Semantic](const FMaterialPilotTextureAnalysis& Texture)
    {
        return Texture.Semantic == Semantic && Texture.bUsedByRecipe;
    });
}

FString FMaterialPilotSetAnalysis::BuildReport() const
{
    FString Report;
    Report += FString::Printf(TEXT("Texture Set: %s\n"), *SetName);
    Report += FString::Printf(TEXT("Recipe Signature: %s\n"), *RecipeSignature);
    Report += FString::Printf(TEXT("Aggregate Confidence: %d%%\n\n"), AggregateConfidence);

    for (const FString& Warning : Warnings)
    {
        Report += FString::Printf(TEXT("WARNING: %s\n"), *Warning);
    }

    if (Warnings.Num() > 0)
    {
        Report += TEXT("\n");
    }

    for (const FMaterialPilotTextureAnalysis& Texture : Textures)
    {
        Report += Texture.ToReportLine();
        Report += TEXT("\n");
    }

    return Report;
}

FString MaterialPilotSemanticToString(EMaterialPilotSemantic Semantic)
{
    switch (Semantic)
    {
    case EMaterialPilotSemantic::BaseColor:
        return TEXT("Base Color");
    case EMaterialPilotSemantic::Normal:
        return TEXT("Normal");
    case EMaterialPilotSemantic::Roughness:
        return TEXT("Roughness");
    case EMaterialPilotSemantic::Metallic:
        return TEXT("Metallic");
    case EMaterialPilotSemantic::AmbientOcclusion:
        return TEXT("Ambient Occlusion");
    case EMaterialPilotSemantic::Opacity:
        return TEXT("Opacity / Mask");
    case EMaterialPilotSemantic::Emissive:
        return TEXT("Emissive");
    case EMaterialPilotSemantic::Specular:
        return TEXT("Specular");
    case EMaterialPilotSemantic::Gloss:
        return TEXT("Glossiness");
    case EMaterialPilotSemantic::Height:
        return TEXT("Height");
    case EMaterialPilotSemantic::PackedORM:
        return TEXT("Packed ORM");
    case EMaterialPilotSemantic::PackedRMA:
        return TEXT("Packed RMA");
    case EMaterialPilotSemantic::PackedMRA:
        return TEXT("Packed MRA");
    case EMaterialPilotSemantic::PackedMetallicSmoothness:
        return TEXT("Metallic + Smoothness");
    default:
        return TEXT("Unknown");
    }
}

FString MaterialPilotNormalConventionToString(EMaterialPilotNormalConvention Convention)
{
    switch (Convention)
    {
    case EMaterialPilotNormalConvention::DirectX:
        return TEXT("DirectX");
    case EMaterialPilotNormalConvention::OpenGL:
        return TEXT("OpenGL");
    default:
        return TEXT("Project Default");
    }
}

FString MaterialPilotMakeObjectSafeName(const FString& Name)
{
    FString Out;
    Out.Reserve(Name.Len());

    for (TCHAR Char : Name)
    {
        if (FChar::IsAlnum(Char) || Char == TEXT('_'))
        {
            Out.AppendChar(Char);
        }
        else
        {
            Out.AppendChar(TEXT('_'));
        }
    }

    while (Out.Contains(TEXT("__")))
    {
        Out.ReplaceInline(TEXT("__"), TEXT("_"));
    }

    Out.TrimStartAndEndInline();
    Out.RemoveFromStart(TEXT("_"));
    Out.RemoveFromEnd(TEXT("_"));

    return Out.IsEmpty() ? TEXT("MaterialPilotSet") : Out;
}
