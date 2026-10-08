#pragma once

#include "CoreMinimal.h"

class UMaterialInstanceConstant;
class UPrimitiveComponent;
class USkeletalMesh;
class UStaticMesh;
class UTexture2D;

enum class EMaterialPilotSemantic : uint8
{
    Unknown,
    BaseColor,
    Normal,
    Roughness,
    Metallic,
    AmbientOcclusion,
    Opacity,
    Emissive,
    Specular,
    Gloss,
    Height,
    PackedORM,
    PackedRMA,
    PackedMRA,
    PackedMetallicSmoothness
};

enum class EMaterialPilotNormalConvention : uint8
{
    ProjectDefault,
    DirectX,
    OpenGL
};

enum class EMaterialPilotTargetKind : uint8
{
    None,
    Component,
    StaticMeshAsset,
    SkeletalMeshAsset
};

struct FMaterialPilotTarget
{
    EMaterialPilotTargetKind Kind = EMaterialPilotTargetKind::None;
    TWeakObjectPtr<UPrimitiveComponent> Component;
    TWeakObjectPtr<UStaticMesh> StaticMesh;
    TWeakObjectPtr<USkeletalMesh> SkeletalMesh;
    int32 MaterialSlotIndex = 0;
    FString Label;

    bool IsValid() const;
};

struct FMaterialPilotTextureAnalysis
{
    TWeakObjectPtr<UTexture2D> Texture;
    FString AssetName;
    FString Stem;
    EMaterialPilotSemantic Semantic = EMaterialPilotSemantic::Unknown;
    EMaterialPilotNormalConvention NormalConvention = EMaterialPilotNormalConvention::ProjectDefault;
    FString ChannelRoute;
    int32 Confidence = 0;
    TArray<FString> Reasons;
    bool bUsedByRecipe = false;
    FString Resolution;
    bool bReviewRequired = true;

    FString ToReportLine() const;
};

struct FMaterialPilotSetAnalysis
{
    FString SetName;
    TArray<FMaterialPilotTextureAnalysis> Textures;
    int32 AggregateConfidence = 0;
    FString RecipeSignature;
    TArray<FString> Warnings;

    bool HasRequiredInputs() const;
    bool HasBlockingAmbiguity(int32 RequiredConfidence) const;
    const FMaterialPilotTextureAnalysis* FindSemantic(EMaterialPilotSemantic Semantic) const;
    FString BuildReport() const;
};

struct FMaterialPilotBuildOptions
{
    bool bDryRun = true;
    bool bFixTextureSettings = true;
    bool bAssignToTarget = true;
    int32 RequiredConfidence = 60;
};

struct FMaterialPilotBuildResult
{
    bool bSucceeded = false;
    FString Report;
    TWeakObjectPtr<UMaterialInstanceConstant> MaterialInstance;
};

FString MaterialPilotSemanticToString(EMaterialPilotSemantic Semantic);
FString MaterialPilotNormalConventionToString(EMaterialPilotNormalConvention Convention);
FString MaterialPilotMakeObjectSafeName(const FString& Name);
