#pragma once

#include "CoreMinimal.h"
#include "MaterialPilotTypes.h"
#include "Widgets/SCompoundWidget.h"

class SCheckBox;
class UTexture2D;

class SMaterialPilotPanel : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SMaterialPilotPanel) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

private:
    FReply CaptureTarget();
    FReply CaptureTextures();
    FReply AnalyzeTextures();
    FReply RunDryRun();
    FReply BuildAndAssign();

    FText GetTargetText() const;
    FText GetTextureText() const;
    FText GetReportText() const;

    FMaterialPilotBuildOptions MakeBuildOptions(bool bDryRun) const;
    bool EnsureAnalysis();

private:
    FMaterialPilotTarget Target;
    TArray<TWeakObjectPtr<UTexture2D>> CapturedTextures;
    FMaterialPilotSetAnalysis Analysis;
    FString ReportText;

    TSharedPtr<SCheckBox> FixTextureSettingsCheckBox;
    TSharedPtr<SCheckBox> AssignTargetCheckBox;
};
