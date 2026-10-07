#pragma once

#include "CoreMinimal.h"
#include "MaterialPilotTypes.h"
#include "Materials/MaterialExpressionTextureSample.h"

class UMaterial;
class UMaterialExpression;
class UMaterialInstanceConstant;
class UTexture2D;

class FMaterialPilotBuilder
{
public:
    static FMaterialPilotBuildResult Build(const FMaterialPilotSetAnalysis& Analysis, const FMaterialPilotTarget& Target, const FMaterialPilotBuildOptions& Options);

private:
    static FString MakeDryRunReport(const FMaterialPilotSetAnalysis& Analysis, const FMaterialPilotTarget& Target, const FMaterialPilotBuildOptions& Options);
    static UMaterial* FindOrCreateParentMaterial(const FMaterialPilotSetAnalysis& Analysis, FString& OutReport, bool& bOutCreated);
    static UMaterialInstanceConstant* FindOrCreateMaterialInstance(UMaterial* ParentMaterial, const FMaterialPilotSetAnalysis& Analysis, FString& OutReport, bool& bOutCreated);
    static void BuildParentGraph(UMaterial* ParentMaterial, const FMaterialPilotSetAnalysis& Analysis);
    static void ApplyInstanceParameters(UMaterialInstanceConstant* Instance, const FMaterialPilotSetAnalysis& Analysis, FString& OutReport);
    static void FixTextureSettings(const FMaterialPilotSetAnalysis& Analysis, FString& OutReport);
    static bool AssignToTarget(const FMaterialPilotTarget& Target, UMaterialInstanceConstant* Instance, FString& OutReport);

    static UMaterialExpression* CreateTextureParameter(UMaterial* Material, FName ParameterName, UTexture2D* DefaultTexture, EMaterialSamplerType SamplerType, int32 X, int32 Y);
    static UMaterialExpression* CreateChannelMask(UMaterial* Material, UMaterialExpression* Source, bool bR, bool bG, bool bB, bool bA, int32 X, int32 Y);
    static FString BuildAssetPath(const FString& Folder, const FString& AssetName);
};
