#include "Materials/MaterialPilotBuilder.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Editor.h"
#include "Factories/MaterialFactoryNew.h"
#include "Factories/MaterialInstanceConstantFactoryNew.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionOneMinus.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialInstanceConstant.h"
#include "MaterialPilotSettings.h"
#include "Misc/PackageName.h"
#include "ScopedTransaction.h"
#include "TextureResource.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Components/PrimitiveComponent.h"

#define LOCTEXT_NAMESPACE "MaterialPilotBuilder"

namespace MaterialPilotBuilder
{
    static FName ParamBaseColor(TEXT("MP_BaseColor"));
    static FName ParamNormal(TEXT("MP_Normal"));
    static FName ParamPacked(TEXT("MP_PackedSurface"));
    static FName ParamRoughness(TEXT("MP_Roughness"));
    static FName ParamMetallic(TEXT("MP_Metallic"));
    static FName ParamAO(TEXT("MP_AO"));
    static FName ParamOpacity(TEXT("MP_Opacity"));
    static FName ParamEmissive(TEXT("MP_Emissive"));
    static FName ParamSpecular(TEXT("MP_Specular"));
    static FName ParamGloss(TEXT("MP_Gloss"));

    static UTexture2D* GetTexture(const FMaterialPilotTextureAnalysis* Analysis)
    {
        return Analysis && Analysis->Texture.IsValid() ? Analysis->Texture.Get() : nullptr;
    }

    static bool EnsurePackageFolder(const FString& PackageFolder, FString& OutReport)
    {
        if (!FPackageName::IsValidLongPackageName(PackageFolder))
        {
            OutReport += FString::Printf(TEXT("ERROR: Invalid output folder: %s\n"), *PackageFolder);
            return false;
        }
        return true;
    }

    static bool IsColorSemantic(EMaterialPilotSemantic Semantic)
    {
        return Semantic == EMaterialPilotSemantic::BaseColor ||
            Semantic == EMaterialPilotSemantic::Emissive;
    }

    static bool IsMaskSemantic(EMaterialPilotSemantic Semantic)
    {
        return Semantic == EMaterialPilotSemantic::Roughness ||
            Semantic == EMaterialPilotSemantic::Metallic ||
            Semantic == EMaterialPilotSemantic::AmbientOcclusion ||
            Semantic == EMaterialPilotSemantic::Opacity ||
            Semantic == EMaterialPilotSemantic::Specular ||
            Semantic == EMaterialPilotSemantic::Gloss ||
            Semantic == EMaterialPilotSemantic::Height ||
            Semantic == EMaterialPilotSemantic::PackedORM ||
            Semantic == EMaterialPilotSemantic::PackedRMA ||
            Semantic == EMaterialPilotSemantic::PackedMRA ||
            Semantic == EMaterialPilotSemantic::PackedMetallicSmoothness;
    }
}

FMaterialPilotBuildResult FMaterialPilotBuilder::Build(const FMaterialPilotSetAnalysis& Analysis, const FMaterialPilotTarget& Target, const FMaterialPilotBuildOptions& Options)
{
    FMaterialPilotBuildResult Result;

    if (Options.bDryRun)
    {
        Result.bSucceeded = true;
        Result.Report = MakeDryRunReport(Analysis, Target, Options);
        return Result;
    }

    if (Analysis.HasBlockingAmbiguity(Options.RequiredConfidence))
    {
        Result.Report = TEXT("Cannot build: no usable material textures were detected.\n\n");
        Result.Report += Analysis.BuildReport();
        return Result;
    }

    const FScopedTransaction Transaction(LOCTEXT("MaterialPilotBuildTransaction", "MaterialPilot Build Material"));

    FString Report;
    if (Options.bFixTextureSettings)
    {
        FixTextureSettings(Analysis, Report);
    }

    bool bCreatedParent = false;
    UMaterial* ParentMaterial = FindOrCreateParentMaterial(Analysis, Report, bCreatedParent);
    if (!ParentMaterial)
    {
        Result.Report = Report;
        return Result;
    }

    if (bCreatedParent)
    {
        BuildParentGraph(ParentMaterial, Analysis);
        Report += FString::Printf(TEXT("Created parent material: %s\n"), *ParentMaterial->GetPathName());
    }
    else
    {
        Report += FString::Printf(TEXT("Reused parent material: %s\n"), *ParentMaterial->GetPathName());
    }

    bool bCreatedInstance = false;
    UMaterialInstanceConstant* Instance = FindOrCreateMaterialInstance(ParentMaterial, Analysis, Report, bCreatedInstance);
    if (!Instance)
    {
        Result.Report = Report;
        return Result;
    }

    ApplyInstanceParameters(Instance, Analysis, Report);

    if (Options.bAssignToTarget)
    {
        AssignToTarget(Target, Instance, Report);
    }
    else
    {
        Report += TEXT("Skipped assignment because Assign To Target is disabled.\n");
    }

    Result.bSucceeded = true;
    Result.MaterialInstance = Instance;
    Result.Report = Report;
    return Result;
}

FString FMaterialPilotBuilder::MakeDryRunReport(const FMaterialPilotSetAnalysis& Analysis, const FMaterialPilotTarget& Target, const FMaterialPilotBuildOptions& Options)
{
    const UMaterialPilotSettings* Settings = GetDefault<UMaterialPilotSettings>();

    FString Report;
    Report += TEXT("DRY RUN - no assets were changed.\n\n");
    Report += Analysis.BuildReport();
    Report += TEXT("\nBuild Plan\n");
    Report += FString::Printf(TEXT("- Parent material folder: %s\n"), Settings ? *Settings->GeneratedParentFolder : TEXT("/Game/AutoMaterial/GeneratedParents"));
    Report += FString::Printf(TEXT("- Material instance folder: %s\n"), Settings ? *Settings->MaterialInstanceFolder : TEXT("/Game/AutoMaterial/Instances"));
    Report += FString::Printf(TEXT("- Fix texture settings: %s\n"), Options.bFixTextureSettings ? TEXT("Yes") : TEXT("No"));
    Report += FString::Printf(TEXT("- Assign to captured target: %s\n"), Options.bAssignToTarget ? TEXT("Yes") : TEXT("No"));
    Report += FString::Printf(TEXT("- Target: %s\n"), Target.IsValid() ? *Target.Label : TEXT("None captured"));

    if (Analysis.HasBlockingAmbiguity(Options.RequiredConfidence))
    {
        Report += TEXT("\nSTATUS: CANNOT BUILD - no usable material textures were detected.\n");
    }
    else if (Analysis.Warnings.Num() > 0)
    {
        Report += TEXT("\nSTATUS: READY WITH WARNINGS - build will continue using the best resolved texture set.\n");
    }
    else
    {
        Report += TEXT("\nSTATUS: READY - build will create and assign the material.\n");
    }

    return Report;
}

UMaterial* FMaterialPilotBuilder::FindOrCreateParentMaterial(const FMaterialPilotSetAnalysis& Analysis, FString& OutReport, bool& bOutCreated)
{
    bOutCreated = false;

    const UMaterialPilotSettings* Settings = GetDefault<UMaterialPilotSettings>();
    const FString Folder = Settings ? Settings->GeneratedParentFolder : TEXT("/Game/AutoMaterial/GeneratedParents");
    if (!MaterialPilotBuilder::EnsurePackageFolder(Folder, OutReport))
    {
        return nullptr;
    }

    const FString ParentName = MaterialPilotMakeObjectSafeName(TEXT("M_MP_") + Analysis.RecipeSignature);
    const FString ObjectPath = BuildAssetPath(Folder, ParentName);
    if (UMaterial* Existing = LoadObject<UMaterial>(nullptr, *ObjectPath))
    {
        return Existing;
    }

    FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
    UMaterialFactoryNew* Factory = NewObject<UMaterialFactoryNew>();

    UObject* NewAsset = AssetToolsModule.Get().CreateAsset(ParentName, Folder, UMaterial::StaticClass(), Factory);
    UMaterial* Material = Cast<UMaterial>(NewAsset);
    if (!Material)
    {
        OutReport += FString::Printf(TEXT("ERROR: Could not create parent material %s in %s.\n"), *ParentName, *Folder);
        return nullptr;
    }

    bOutCreated = true;
    FAssetRegistryModule::AssetCreated(Material);
    Material->Modify();
    Material->MarkPackageDirty();
    return Material;
}

UMaterialInstanceConstant* FMaterialPilotBuilder::FindOrCreateMaterialInstance(UMaterial* ParentMaterial, const FMaterialPilotSetAnalysis& Analysis, FString& OutReport, bool& bOutCreated)
{
    bOutCreated = false;

    const UMaterialPilotSettings* Settings = GetDefault<UMaterialPilotSettings>();
    const FString Folder = Settings ? Settings->MaterialInstanceFolder : TEXT("/Game/AutoMaterial/Instances");
    if (!MaterialPilotBuilder::EnsurePackageFolder(Folder, OutReport))
    {
        return nullptr;
    }

    const FString InstanceName = MaterialPilotMakeObjectSafeName(TEXT("MI_") + Analysis.SetName);
    const FString ObjectPath = BuildAssetPath(Folder, InstanceName);
    if (UMaterialInstanceConstant* Existing = LoadObject<UMaterialInstanceConstant>(nullptr, *ObjectPath))
    {
        Existing->Modify();
        Existing->SetParentEditorOnly(ParentMaterial);
        OutReport += FString::Printf(TEXT("Reused material instance: %s\n"), *Existing->GetPathName());
        return Existing;
    }

    FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
    UMaterialInstanceConstantFactoryNew* Factory = NewObject<UMaterialInstanceConstantFactoryNew>();
    Factory->InitialParent = ParentMaterial;

    UObject* NewAsset = AssetToolsModule.Get().CreateAsset(InstanceName, Folder, UMaterialInstanceConstant::StaticClass(), Factory);
    UMaterialInstanceConstant* Instance = Cast<UMaterialInstanceConstant>(NewAsset);
    if (!Instance)
    {
        OutReport += FString::Printf(TEXT("ERROR: Could not create material instance %s in %s.\n"), *InstanceName, *Folder);
        return nullptr;
    }

    bOutCreated = true;
    FAssetRegistryModule::AssetCreated(Instance);
    Instance->Modify();
    Instance->MarkPackageDirty();
    OutReport += FString::Printf(TEXT("Created material instance: %s\n"), *Instance->GetPathName());
    return Instance;
}

void FMaterialPilotBuilder::BuildParentGraph(UMaterial* ParentMaterial, const FMaterialPilotSetAnalysis& Analysis)
{
    if (!ParentMaterial)
    {
        return;
    }

    ParentMaterial->Modify();
    UMaterialEditingLibrary::DeleteAllMaterialExpressions(ParentMaterial);

    ParentMaterial->BlendMode = Analysis.FindSemantic(EMaterialPilotSemantic::Opacity) ? BLEND_Masked : BLEND_Opaque;
    ParentMaterial->SetShadingModel(MSM_DefaultLit);

    const FMaterialPilotTextureAnalysis* BaseColor = Analysis.FindSemantic(EMaterialPilotSemantic::BaseColor);
    const FMaterialPilotTextureAnalysis* Normal = Analysis.FindSemantic(EMaterialPilotSemantic::Normal);
    const FMaterialPilotTextureAnalysis* PackedORM = Analysis.FindSemantic(EMaterialPilotSemantic::PackedORM);
    const FMaterialPilotTextureAnalysis* PackedRMA = Analysis.FindSemantic(EMaterialPilotSemantic::PackedRMA);
    const FMaterialPilotTextureAnalysis* PackedMRA = Analysis.FindSemantic(EMaterialPilotSemantic::PackedMRA);
    const FMaterialPilotTextureAnalysis* Roughness = Analysis.FindSemantic(EMaterialPilotSemantic::Roughness);
    const FMaterialPilotTextureAnalysis* Metallic = Analysis.FindSemantic(EMaterialPilotSemantic::Metallic);
    const FMaterialPilotTextureAnalysis* AO = Analysis.FindSemantic(EMaterialPilotSemantic::AmbientOcclusion);
    const FMaterialPilotTextureAnalysis* Opacity = Analysis.FindSemantic(EMaterialPilotSemantic::Opacity);
    const FMaterialPilotTextureAnalysis* Emissive = Analysis.FindSemantic(EMaterialPilotSemantic::Emissive);
    const FMaterialPilotTextureAnalysis* Specular = Analysis.FindSemantic(EMaterialPilotSemantic::Specular);
    const FMaterialPilotTextureAnalysis* Gloss = Analysis.FindSemantic(EMaterialPilotSemantic::Gloss);

    int32 Row = 0;

    if (BaseColor)
    {
        UMaterialExpression* Expr = CreateTextureParameter(ParentMaterial, MaterialPilotBuilder::ParamBaseColor, MaterialPilotBuilder::GetTexture(BaseColor), SAMPLERTYPE_Color, -900, Row);
        UMaterialEditingLibrary::ConnectMaterialProperty(Expr, TEXT(""), MP_BaseColor);
        Row += 180;
    }

    if (Normal)
    {
        UMaterialExpression* Expr = CreateTextureParameter(ParentMaterial, MaterialPilotBuilder::ParamNormal, MaterialPilotBuilder::GetTexture(Normal), SAMPLERTYPE_Normal, -900, Row);
        UMaterialEditingLibrary::ConnectMaterialProperty(Expr, TEXT(""), MP_Normal);
        Row += 180;
    }

    const FMaterialPilotTextureAnalysis* Packed = PackedORM ? PackedORM : (PackedRMA ? PackedRMA : PackedMRA);
    if (Packed)
    {
        UMaterialExpression* PackedExpr = CreateTextureParameter(ParentMaterial, MaterialPilotBuilder::ParamPacked, MaterialPilotBuilder::GetTexture(Packed), SAMPLERTYPE_Masks, -900, Row);

        if (PackedORM)
        {
            UMaterialEditingLibrary::ConnectMaterialProperty(CreateChannelMask(ParentMaterial, PackedExpr, true, false, false, false, -620, Row - 80), TEXT(""), MP_AmbientOcclusion);
            UMaterialEditingLibrary::ConnectMaterialProperty(CreateChannelMask(ParentMaterial, PackedExpr, false, true, false, false, -620, Row), TEXT(""), MP_Roughness);
            UMaterialEditingLibrary::ConnectMaterialProperty(CreateChannelMask(ParentMaterial, PackedExpr, false, false, true, false, -620, Row + 80), TEXT(""), MP_Metallic);
        }
        else if (PackedRMA)
        {
            UMaterialEditingLibrary::ConnectMaterialProperty(CreateChannelMask(ParentMaterial, PackedExpr, true, false, false, false, -620, Row - 80), TEXT(""), MP_Roughness);
            UMaterialEditingLibrary::ConnectMaterialProperty(CreateChannelMask(ParentMaterial, PackedExpr, false, true, false, false, -620, Row), TEXT(""), MP_Metallic);
            UMaterialEditingLibrary::ConnectMaterialProperty(CreateChannelMask(ParentMaterial, PackedExpr, false, false, true, false, -620, Row + 80), TEXT(""), MP_AmbientOcclusion);
        }
        else if (PackedMRA)
        {
            UMaterialEditingLibrary::ConnectMaterialProperty(CreateChannelMask(ParentMaterial, PackedExpr, true, false, false, false, -620, Row - 80), TEXT(""), MP_Metallic);
            UMaterialEditingLibrary::ConnectMaterialProperty(CreateChannelMask(ParentMaterial, PackedExpr, false, true, false, false, -620, Row), TEXT(""), MP_Roughness);
            UMaterialEditingLibrary::ConnectMaterialProperty(CreateChannelMask(ParentMaterial, PackedExpr, false, false, true, false, -620, Row + 80), TEXT(""), MP_AmbientOcclusion);
        }
        Row += 240;
    }

    if (!Packed && Roughness)
    {
        UMaterialExpression* Expr = CreateTextureParameter(ParentMaterial, MaterialPilotBuilder::ParamRoughness, MaterialPilotBuilder::GetTexture(Roughness), SAMPLERTYPE_Masks, -900, Row);
        UMaterialEditingLibrary::ConnectMaterialProperty(CreateChannelMask(ParentMaterial, Expr, true, false, false, false, -620, Row), TEXT(""), MP_Roughness);
        Row += 180;
    }

    if (!Packed && Gloss)
    {
        UMaterialExpression* Expr = CreateTextureParameter(ParentMaterial, MaterialPilotBuilder::ParamGloss, MaterialPilotBuilder::GetTexture(Gloss), SAMPLERTYPE_Masks, -900, Row);
        UMaterialExpression* Mask = CreateChannelMask(ParentMaterial, Expr, true, false, false, false, -620, Row);
        UMaterialExpression* OneMinus = UMaterialEditingLibrary::CreateMaterialExpression(ParentMaterial, UMaterialExpressionOneMinus::StaticClass(), -380, Row);
        UMaterialEditingLibrary::ConnectMaterialExpressions(Mask, TEXT(""), OneMinus, TEXT("Input"));
        UMaterialEditingLibrary::ConnectMaterialProperty(OneMinus, TEXT(""), MP_Roughness);
        Row += 180;
    }

    if (!Packed && Metallic)
    {
        UMaterialExpression* Expr = CreateTextureParameter(ParentMaterial, MaterialPilotBuilder::ParamMetallic, MaterialPilotBuilder::GetTexture(Metallic), SAMPLERTYPE_Masks, -900, Row);
        UMaterialEditingLibrary::ConnectMaterialProperty(CreateChannelMask(ParentMaterial, Expr, true, false, false, false, -620, Row), TEXT(""), MP_Metallic);
        Row += 180;
    }

    if (!Packed && AO)
    {
        UMaterialExpression* Expr = CreateTextureParameter(ParentMaterial, MaterialPilotBuilder::ParamAO, MaterialPilotBuilder::GetTexture(AO), SAMPLERTYPE_Masks, -900, Row);
        UMaterialEditingLibrary::ConnectMaterialProperty(CreateChannelMask(ParentMaterial, Expr, true, false, false, false, -620, Row), TEXT(""), MP_AmbientOcclusion);
        Row += 180;
    }

    if (Opacity)
    {
        UMaterialExpression* Expr = CreateTextureParameter(ParentMaterial, MaterialPilotBuilder::ParamOpacity, MaterialPilotBuilder::GetTexture(Opacity), SAMPLERTYPE_Masks, -900, Row);
        UMaterialEditingLibrary::ConnectMaterialProperty(CreateChannelMask(ParentMaterial, Expr, true, false, false, false, -620, Row), TEXT(""), MP_OpacityMask);
        Row += 180;
    }

    if (Emissive)
    {
        UMaterialExpression* Expr = CreateTextureParameter(ParentMaterial, MaterialPilotBuilder::ParamEmissive, MaterialPilotBuilder::GetTexture(Emissive), SAMPLERTYPE_Color, -900, Row);
        UMaterialEditingLibrary::ConnectMaterialProperty(Expr, TEXT(""), MP_EmissiveColor);
        Row += 180;
    }

    if (Specular)
    {
        UMaterialExpression* Expr = CreateTextureParameter(ParentMaterial, MaterialPilotBuilder::ParamSpecular, MaterialPilotBuilder::GetTexture(Specular), SAMPLERTYPE_Masks, -900, Row);
        UMaterialEditingLibrary::ConnectMaterialProperty(CreateChannelMask(ParentMaterial, Expr, true, false, false, false, -620, Row), TEXT(""), MP_Specular);
    }

    UMaterialEditingLibrary::LayoutMaterialExpressions(ParentMaterial);
    ParentMaterial->PostEditChange();
    ParentMaterial->MarkPackageDirty();
}

void FMaterialPilotBuilder::ApplyInstanceParameters(UMaterialInstanceConstant* Instance, const FMaterialPilotSetAnalysis& Analysis, FString& OutReport)
{
    if (!Instance)
    {
        return;
    }

    Instance->Modify();

    auto SetTexture = [Instance, &OutReport](FName ParameterName, const FMaterialPilotTextureAnalysis* TextureAnalysis)
    {
        if (!TextureAnalysis || !TextureAnalysis->Texture.IsValid())
        {
            return;
        }

        Instance->SetTextureParameterValueEditorOnly(ParameterName, TextureAnalysis->Texture.Get());
        OutReport += FString::Printf(TEXT("Set %s = %s\n"), *ParameterName.ToString(), *TextureAnalysis->Texture->GetPathName());
    };

    SetTexture(MaterialPilotBuilder::ParamBaseColor, Analysis.FindSemantic(EMaterialPilotSemantic::BaseColor));
    SetTexture(MaterialPilotBuilder::ParamNormal, Analysis.FindSemantic(EMaterialPilotSemantic::Normal));
    SetTexture(MaterialPilotBuilder::ParamRoughness, Analysis.FindSemantic(EMaterialPilotSemantic::Roughness));
    SetTexture(MaterialPilotBuilder::ParamMetallic, Analysis.FindSemantic(EMaterialPilotSemantic::Metallic));
    SetTexture(MaterialPilotBuilder::ParamAO, Analysis.FindSemantic(EMaterialPilotSemantic::AmbientOcclusion));
    SetTexture(MaterialPilotBuilder::ParamOpacity, Analysis.FindSemantic(EMaterialPilotSemantic::Opacity));
    SetTexture(MaterialPilotBuilder::ParamEmissive, Analysis.FindSemantic(EMaterialPilotSemantic::Emissive));
    SetTexture(MaterialPilotBuilder::ParamSpecular, Analysis.FindSemantic(EMaterialPilotSemantic::Specular));
    SetTexture(MaterialPilotBuilder::ParamGloss, Analysis.FindSemantic(EMaterialPilotSemantic::Gloss));

    const FMaterialPilotTextureAnalysis* Packed = Analysis.FindSemantic(EMaterialPilotSemantic::PackedORM);
    if (!Packed)
    {
        Packed = Analysis.FindSemantic(EMaterialPilotSemantic::PackedRMA);
    }
    if (!Packed)
    {
        Packed = Analysis.FindSemantic(EMaterialPilotSemantic::PackedMRA);
    }
    SetTexture(MaterialPilotBuilder::ParamPacked, Packed);

    Instance->PostEditChange();
    Instance->MarkPackageDirty();
}

void FMaterialPilotBuilder::FixTextureSettings(const FMaterialPilotSetAnalysis& Analysis, FString& OutReport)
{
    for (const FMaterialPilotTextureAnalysis& TextureAnalysis : Analysis.Textures)
    {
        UTexture2D* Texture = TextureAnalysis.Texture.Get();
        if (!Texture)
        {
            continue;
        }

        if (!TextureAnalysis.bUsedByRecipe && TextureAnalysis.Semantic != EMaterialPilotSemantic::Height)
        {
            continue;
        }

        const bool bShouldSRGB = MaterialPilotBuilder::IsColorSemantic(TextureAnalysis.Semantic);
        TextureCompressionSettings DesiredCompression = TC_Default;
        if (TextureAnalysis.Semantic == EMaterialPilotSemantic::Normal)
        {
            DesiredCompression = TC_Normalmap;
        }
        else if (MaterialPilotBuilder::IsMaskSemantic(TextureAnalysis.Semantic))
        {
            DesiredCompression = TC_Masks;
        }

        bool bChanged = false;
        Texture->Modify();

        if (Texture->SRGB != bShouldSRGB)
        {
            Texture->SRGB = bShouldSRGB;
            bChanged = true;
        }

        if (Texture->CompressionSettings != DesiredCompression)
        {
            Texture->CompressionSettings = DesiredCompression;
            bChanged = true;
        }

        if (TextureAnalysis.Semantic == EMaterialPilotSemantic::Normal &&
            TextureAnalysis.NormalConvention == EMaterialPilotNormalConvention::OpenGL &&
            !Texture->bFlipGreenChannel)
        {
            Texture->bFlipGreenChannel = true;
            bChanged = true;
        }

        if (bChanged)
        {
            Texture->PostEditChange();
            Texture->MarkPackageDirty();
            OutReport += FString::Printf(TEXT("Updated texture settings: %s\n"), *Texture->GetPathName());
        }
    }
}

bool FMaterialPilotBuilder::AssignToTarget(const FMaterialPilotTarget& Target, UMaterialInstanceConstant* Instance, FString& OutReport)
{
    if (!Target.IsValid() || !Instance)
    {
        OutReport += TEXT("Skipped assignment: no valid target was captured.\n");
        return false;
    }

    switch (Target.Kind)
    {
    case EMaterialPilotTargetKind::Component:
        if (UPrimitiveComponent* Component = Target.Component.Get())
        {
            Component->Modify();
            Component->SetMaterial(Target.MaterialSlotIndex, Instance);
            OutReport += FString::Printf(TEXT("Assigned instance to component: %s slot %d\n"), *Component->GetName(), Target.MaterialSlotIndex);
            return true;
        }
        break;
    case EMaterialPilotTargetKind::StaticMeshAsset:
        if (UStaticMesh* StaticMesh = Target.StaticMesh.Get())
        {
            StaticMesh->Modify();
            StaticMesh->SetMaterial(Target.MaterialSlotIndex, Instance);
            StaticMesh->PostEditChange();
            StaticMesh->MarkPackageDirty();
            OutReport += FString::Printf(TEXT("Assigned instance to static mesh: %s slot %d\n"), *StaticMesh->GetName(), Target.MaterialSlotIndex);
            return true;
        }
        break;
    case EMaterialPilotTargetKind::SkeletalMeshAsset:
        if (USkeletalMesh* SkeletalMesh = Target.SkeletalMesh.Get())
        {
            if (!SkeletalMesh->GetMaterials().IsValidIndex(Target.MaterialSlotIndex))
            {
                OutReport += FString::Printf(TEXT("Assignment failed: skeletal mesh slot %d is invalid.\n"), Target.MaterialSlotIndex);
                return false;
            }

            SkeletalMesh->Modify();
            SkeletalMesh->GetMaterials()[Target.MaterialSlotIndex].MaterialInterface = Instance;
            SkeletalMesh->PostEditChange();
            SkeletalMesh->MarkPackageDirty();
            OutReport += FString::Printf(TEXT("Assigned instance to skeletal mesh: %s slot %d\n"), *SkeletalMesh->GetName(), Target.MaterialSlotIndex);
            return true;
        }
        break;
    default:
        break;
    }

    OutReport += TEXT("Assignment failed: target became invalid.\n");
    return false;
}

UMaterialExpression* FMaterialPilotBuilder::CreateTextureParameter(UMaterial* Material, FName ParameterName, UTexture2D* DefaultTexture, EMaterialSamplerType SamplerType, int32 X, int32 Y)
{
    UMaterialExpressionTextureSampleParameter2D* TextureExpression =
        Cast<UMaterialExpressionTextureSampleParameter2D>(
            UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionTextureSampleParameter2D::StaticClass(), X, Y));

    if (TextureExpression)
    {
        TextureExpression->ParameterName = ParameterName;
        TextureExpression->Texture = DefaultTexture;
        TextureExpression->SamplerType = SamplerType;
    }

    return TextureExpression;
}

UMaterialExpression* FMaterialPilotBuilder::CreateChannelMask(UMaterial* Material, UMaterialExpression* Source, bool bR, bool bG, bool bB, bool bA, int32 X, int32 Y)
{
    UMaterialExpressionComponentMask* Mask =
        Cast<UMaterialExpressionComponentMask>(
            UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionComponentMask::StaticClass(), X, Y));

    if (Mask)
    {
        Mask->R = bR;
        Mask->G = bG;
        Mask->B = bB;
        Mask->A = bA;
        UMaterialEditingLibrary::ConnectMaterialExpressions(Source, TEXT(""), Mask, TEXT(""));
    }

    return Mask;
}

FString FMaterialPilotBuilder::BuildAssetPath(const FString& Folder, const FString& AssetName)
{
    return FString::Printf(TEXT("%s/%s.%s"), *Folder, *AssetName, *AssetName);
}

#undef LOCTEXT_NAMESPACE
