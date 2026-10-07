#pragma once

#include "CoreMinimal.h"
#include "MaterialPilotTypes.h"

class UTexture2D;

class FMaterialPilotClassifier
{
public:
    static FMaterialPilotSetAnalysis AnalyzeTextures(const TArray<UTexture2D*>& Textures);

private:
    static FMaterialPilotTextureAnalysis AnalyzeTexture(UTexture2D* Texture);
    static TArray<FString> Tokenize(const FString& Name);
    static FString BuildStem(const FString& AssetName, const TArray<FString>& Tokens);
    static bool HasToken(const TArray<FString>& Tokens, const TArray<FString>& Candidates);
    static bool HasTokenContaining(const TArray<FString>& Tokens, const FString& Needle);
    static void FinalizeRecipe(FMaterialPilotSetAnalysis& Analysis);
};
