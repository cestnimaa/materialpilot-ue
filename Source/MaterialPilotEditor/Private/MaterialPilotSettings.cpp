#include "MaterialPilotSettings.h"

#define LOCTEXT_NAMESPACE "MaterialPilotSettings"

UMaterialPilotSettings::UMaterialPilotSettings()
    : GeneratedParentFolder(TEXT("/Game/AutoMaterial/GeneratedParents"))
    , MaterialInstanceFolder(TEXT("/Game/AutoMaterial/Instances"))
    , bFixTextureSettingsByDefault(true)
    , bAssignToCapturedTargetByDefault(true)
    , BuildConfidenceThreshold(60)
    , DefaultNormalConvention(EMaterialPilotDefaultNormalConvention::DirectX)
{
}

FName UMaterialPilotSettings::GetCategoryName() const
{
    return TEXT("Plugins");
}

FText UMaterialPilotSettings::GetSectionText() const
{
    return LOCTEXT("MaterialPilotSettingsSection", "MaterialPilot UE");
}

#undef LOCTEXT_NAMESPACE
