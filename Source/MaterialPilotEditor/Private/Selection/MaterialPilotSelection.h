#pragma once

#include "CoreMinimal.h"
#include "MaterialPilotTypes.h"

class UTexture2D;

class FMaterialPilotSelection
{
public:
    static FMaterialPilotTarget CaptureTargetFromEditorSelection();
    static TArray<UTexture2D*> CaptureTexturesFromContentBrowserSelection();

private:
    static FMaterialPilotTarget TargetFromObject(UObject* Object);
};
