#include "UI/SMaterialPilotPanel.h"

#include "Classification/MaterialPilotClassifier.h"
#include "Materials/MaterialPilotBuilder.h"
#include "MaterialPilotSettings.h"
#include "Selection/MaterialPilotSelection.h"
#include "Engine/Texture2D.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/CoreStyle.h"

#define LOCTEXT_NAMESPACE "SMaterialPilotPanel"

void SMaterialPilotPanel::Construct(const FArguments& InArgs)
{
    const UMaterialPilotSettings* Settings = GetDefault<UMaterialPilotSettings>();

    ReportText = TEXT("Capture a mesh/component target and selected textures, then Analyze. Dry Run shows the build plan before any asset changes.");

    ChildSlot
    [
        SNew(SScrollBox)
        + SScrollBox::Slot()
        .Padding(12.0f)
        [
            SNew(SVerticalBox)

            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 0.0f, 0.0f, 10.0f)
            [
                SNew(STextBlock)
                .Text(LOCTEXT("Title", "MaterialPilot UE"))
                .Font(FCoreStyle::GetDefaultFontStyle("Bold", 18))
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 0.0f, 0.0f, 12.0f)
            [
                SNew(STextBlock)
                .Text(LOCTEXT("Subtitle", "Smart texture-to-material assembly for Unreal Engine 5."))
                .AutoWrapText(true)
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 6.0f)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(0.0f, 0.0f, 8.0f, 0.0f)
                [
                    SNew(SButton)
                    .Text(LOCTEXT("CaptureTarget", "Capture Selected Target"))
                    .OnClicked(this, &SMaterialPilotPanel::CaptureTarget)
                ]
                + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                [
                    SNew(STextBlock)
                    .Text(this, &SMaterialPilotPanel::GetTargetText)
                    .AutoWrapText(true)
                ]
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 6.0f)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(0.0f, 0.0f, 8.0f, 0.0f)
                [
                    SNew(SButton)
                    .Text(LOCTEXT("CaptureTextures", "Capture Selected Textures"))
                    .OnClicked(this, &SMaterialPilotPanel::CaptureTextures)
                ]
                + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                [
                    SNew(STextBlock)
                    .Text(this, &SMaterialPilotPanel::GetTextureText)
                    .AutoWrapText(true)
                ]
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 8.0f)
            [
                SNew(SSeparator)
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 6.0f)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(0.0f, 0.0f, 8.0f, 0.0f)
                [
                    SNew(SButton)
                    .Text(LOCTEXT("Analyze", "Analyze"))
                    .OnClicked(this, &SMaterialPilotPanel::AnalyzeTextures)
                ]
                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(0.0f, 0.0f, 8.0f, 0.0f)
                [
                    SNew(SButton)
                    .Text(LOCTEXT("DryRun", "Dry Run"))
                    .OnClicked(this, &SMaterialPilotPanel::RunDryRun)
                ]
                + SHorizontalBox::Slot()
                .AutoWidth()
                [
                    SNew(SButton)
                    .Text(LOCTEXT("BuildAssign", "Build & Assign"))
                    .OnClicked(this, &SMaterialPilotPanel::BuildAndAssign)
                ]
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 6.0f)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(0.0f, 0.0f, 18.0f, 0.0f)
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    [
                        SAssignNew(FixTextureSettingsCheckBox, SCheckBox)
                        .IsChecked(Settings && Settings->bFixTextureSettingsByDefault ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
                    ]
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .Padding(6.0f, 0.0f, 0.0f, 0.0f)
                    [
                        SNew(STextBlock)
                        .Text(LOCTEXT("FixTextureSettings", "Fix texture settings"))
                    ]
                ]
                + SHorizontalBox::Slot()
                .AutoWidth()
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    [
                        SAssignNew(AssignTargetCheckBox, SCheckBox)
                        .IsChecked(Settings && Settings->bAssignToCapturedTargetByDefault ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
                    ]
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .Padding(6.0f, 0.0f, 0.0f, 0.0f)
                    [
                        SNew(STextBlock)
                        .Text(LOCTEXT("AssignTarget", "Assign to captured target"))
                    ]
                ]
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 8.0f)
            [
                SNew(SSeparator)
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 8.0f, 0.0f, 4.0f)
            [
                SNew(STextBlock)
                .Text(LOCTEXT("ReportLabel", "Analysis / Report"))
                .Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SNew(SBox)
                .MinDesiredHeight(320.0f)
                [
                    SNew(STextBlock)
                    .Text(this, &SMaterialPilotPanel::GetReportText)
                    .AutoWrapText(true)
                ]
            ]
        ]
    ];
}

FReply SMaterialPilotPanel::CaptureTarget()
{
    Target = FMaterialPilotSelection::CaptureTargetFromEditorSelection();
    ReportText = Target.IsValid()
        ? FString::Printf(TEXT("Captured target: %s"), *Target.Label)
        : TEXT("No valid mesh, mesh component, Static Mesh asset, or Skeletal Mesh asset was selected.");
    return FReply::Handled();
}

FReply SMaterialPilotPanel::CaptureTextures()
{
    CapturedTextures.Reset();
    for (UTexture2D* Texture : FMaterialPilotSelection::CaptureTexturesFromContentBrowserSelection())
    {
        CapturedTextures.Add(Texture);
    }

    Analysis = FMaterialPilotSetAnalysis();
    ReportText = FString::Printf(TEXT("Captured %d texture(s)."), CapturedTextures.Num());
    return FReply::Handled();
}

FReply SMaterialPilotPanel::AnalyzeTextures()
{
    EnsureAnalysis();
    return FReply::Handled();
}

FReply SMaterialPilotPanel::RunDryRun()
{
    if (!EnsureAnalysis())
    {
        return FReply::Handled();
    }

    const FMaterialPilotBuildResult Result = FMaterialPilotBuilder::Build(Analysis, Target, MakeBuildOptions(true));
    ReportText = Result.Report;
    return FReply::Handled();
}

FReply SMaterialPilotPanel::BuildAndAssign()
{
    if (!EnsureAnalysis())
    {
        return FReply::Handled();
    }

    const FMaterialPilotBuildResult Result = FMaterialPilotBuilder::Build(Analysis, Target, MakeBuildOptions(false));
    ReportText = Result.Report;
    return FReply::Handled();
}

FText SMaterialPilotPanel::GetTargetText() const
{
    return Target.IsValid()
        ? FText::FromString(Target.Label)
        : LOCTEXT("NoTarget", "No target captured.");
}

FText SMaterialPilotPanel::GetTextureText() const
{
    if (CapturedTextures.Num() == 0)
    {
        return LOCTEXT("NoTextures", "No textures captured.");
    }

    TArray<FString> Names;
    for (const TWeakObjectPtr<UTexture2D>& Texture : CapturedTextures)
    {
        if (Texture.IsValid())
        {
            Names.Add(Texture->GetName());
        }
    }

    return FText::FromString(FString::Printf(TEXT("%d texture(s): %s"), Names.Num(), *FString::Join(Names, TEXT(", "))));
}

FText SMaterialPilotPanel::GetReportText() const
{
    return FText::FromString(ReportText);
}

FMaterialPilotBuildOptions SMaterialPilotPanel::MakeBuildOptions(bool bDryRun) const
{
    const UMaterialPilotSettings* Settings = GetDefault<UMaterialPilotSettings>();

    FMaterialPilotBuildOptions Options;
    Options.bDryRun = bDryRun;
    Options.bFixTextureSettings =
        FixTextureSettingsCheckBox.IsValid() &&
        FixTextureSettingsCheckBox->IsChecked();
    Options.bAssignToTarget =
        AssignTargetCheckBox.IsValid() &&
        AssignTargetCheckBox->IsChecked();
    Options.RequiredConfidence = Settings ? Settings->BuildConfidenceThreshold : 60;
    return Options;
}

bool SMaterialPilotPanel::EnsureAnalysis()
{
    TArray<UTexture2D*> Textures;
    for (const TWeakObjectPtr<UTexture2D>& Texture : CapturedTextures)
    {
        if (Texture.IsValid())
        {
            Textures.Add(Texture.Get());
        }
    }

    if (Textures.Num() == 0)
    {
        ReportText = TEXT("No textures captured. Select textures in the Content Browser and click Capture Selected Textures.");
        return false;
    }

    Analysis = FMaterialPilotClassifier::AnalyzeTextures(Textures);
    ReportText = Analysis.BuildReport();
    return true;
}

#undef LOCTEXT_NAMESPACE
