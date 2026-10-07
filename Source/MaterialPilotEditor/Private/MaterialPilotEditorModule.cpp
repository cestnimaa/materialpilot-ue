#include "MaterialPilotEditorModule.h"

#include "ToolMenus.h"
#include "UI/SMaterialPilotPanel.h"
#include "Widgets/Docking/SDockTab.h"

#define LOCTEXT_NAMESPACE "FMaterialPilotEditorModule"

namespace MaterialPilotEditor
{
    static const FName TabName(TEXT("MaterialPilot"));
}

void FMaterialPilotEditorModule::StartupModule()
{
    FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
        MaterialPilotEditor::TabName,
        FOnSpawnTab::CreateRaw(this, &FMaterialPilotEditorModule::SpawnMaterialPilotTab))
        .SetDisplayName(LOCTEXT("MaterialPilotTabTitle", "MaterialPilot UE"))
        .SetMenuType(ETabSpawnerMenuType::Hidden);

    UToolMenus::RegisterStartupCallback(
        FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FMaterialPilotEditorModule::RegisterMenus));
}

void FMaterialPilotEditorModule::ShutdownModule()
{
    if (UToolMenus::IsToolMenuUIEnabled())
    {
        UToolMenus::UnRegisterStartupCallback(this);
        UToolMenus::UnregisterOwner(this);
    }

    FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(MaterialPilotEditor::TabName);
}

void FMaterialPilotEditorModule::RegisterMenus()
{
    FToolMenuOwnerScoped OwnerScoped(this);

    if (UToolMenu* ToolsMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Tools"))
    {
        FToolMenuSection& Section = ToolsMenu->FindOrAddSection("MaterialPilot");
        Section.AddMenuEntry(
            "MaterialPilot_Open",
            LOCTEXT("MaterialPilotOpenLabel", "MaterialPilot UE"),
            LOCTEXT("MaterialPilotOpenTooltip", "Open MaterialPilot UE texture-to-material automation."),
            FSlateIcon(),
            FUIAction(FExecuteAction::CreateRaw(this, &FMaterialPilotEditorModule::OpenMaterialPilotTab)));
    }
}

TSharedRef<SDockTab> FMaterialPilotEditorModule::SpawnMaterialPilotTab(const FSpawnTabArgs& Args)
{
    return SNew(SDockTab)
        .TabRole(ETabRole::NomadTab)
        [
            SNew(SMaterialPilotPanel)
        ];
}

void FMaterialPilotEditorModule::OpenMaterialPilotTab()
{
    FGlobalTabmanager::Get()->TryInvokeTab(MaterialPilotEditor::TabName);
}

IMPLEMENT_MODULE(FMaterialPilotEditorModule, MaterialPilotEditor)

#undef LOCTEXT_NAMESPACE
