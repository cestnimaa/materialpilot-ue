#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "MaterialPilotSettings.generated.h"

UENUM()
enum class EMaterialPilotDefaultNormalConvention : uint8
{
    DirectX,
    OpenGL
};

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "MaterialPilot UE"))
class MATERIALPILOTEDITOR_API UMaterialPilotSettings : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    UMaterialPilotSettings();

    virtual FName GetCategoryName() const override;
    virtual FText GetSectionText() const override;

    UPROPERTY(Config, EditAnywhere, Category = "Output")
    FString GeneratedParentFolder;

    UPROPERTY(Config, EditAnywhere, Category = "Output")
    FString MaterialInstanceFolder;

    UPROPERTY(Config, EditAnywhere, Category = "Build")
    bool bFixTextureSettingsByDefault;

    UPROPERTY(Config, EditAnywhere, Category = "Build")
    bool bAssignToCapturedTargetByDefault;

    UPROPERTY(Config, EditAnywhere, Category = "Analysis", meta = (ClampMin = "0", ClampMax = "100"))
    int32 BuildConfidenceThreshold;

    UPROPERTY(Config, EditAnywhere, Category = "Analysis")
    EMaterialPilotDefaultNormalConvention DefaultNormalConvention;
};
