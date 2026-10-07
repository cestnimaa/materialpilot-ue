#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FMaterialPilotEditorModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

private:
    void RegisterMenus();
    TSharedRef<class SDockTab> SpawnMaterialPilotTab(const class FSpawnTabArgs& Args);
    void OpenMaterialPilotTab();
};
