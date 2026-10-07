#include "Selection/MaterialPilotSelection.h"

#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "ContentBrowserModule.h"
#include "Editor.h"
#include "AssetRegistry/AssetData.h"
#include "Engine/Selection.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "IContentBrowserSingleton.h"
#include "Modules/ModuleManager.h"
#include "TextureResource.h"
#include "Engine/Texture2D.h"

FMaterialPilotTarget FMaterialPilotSelection::CaptureTargetFromEditorSelection()
{
    if (!GEditor)
    {
        return FMaterialPilotTarget();
    }

    if (USelection* SelectedComponents = GEditor->GetSelectedComponents())
    {
        for (FSelectionIterator It(*SelectedComponents); It; ++It)
        {
            if (UObject* Object = *It)
            {
                FMaterialPilotTarget Target = TargetFromObject(Object);
                if (Target.IsValid())
                {
                    return Target;
                }
            }
        }
    }

    if (USelection* SelectedActors = GEditor->GetSelectedActors())
    {
        for (FSelectionIterator It(*SelectedActors); It; ++It)
        {
            if (AActor* Actor = Cast<AActor>(*It))
            {
                TArray<UPrimitiveComponent*> Components;
                Actor->GetComponents<UPrimitiveComponent>(Components);
                for (UPrimitiveComponent* Component : Components)
                {
                    if (IsValid(Component) && Component->GetNumMaterials() > 0)
                    {
                        FMaterialPilotTarget Target;
                        Target.Kind = EMaterialPilotTargetKind::Component;
                        Target.Component = Component;
                        Target.MaterialSlotIndex = 0;
                        Target.Label = FString::Printf(TEXT("%s / %s Slot 0"), *Actor->GetActorLabel(), *Component->GetName());
                        return Target;
                    }
                }
            }
        }
    }

    FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
    TArray<FAssetData> SelectedAssets;
    ContentBrowserModule.Get().GetSelectedAssets(SelectedAssets);
    for (const FAssetData& AssetData : SelectedAssets)
    {
        FMaterialPilotTarget Target = TargetFromObject(AssetData.GetAsset());
        if (Target.IsValid())
        {
            return Target;
        }
    }

    return FMaterialPilotTarget();
}

TArray<UTexture2D*> FMaterialPilotSelection::CaptureTexturesFromContentBrowserSelection()
{
    TArray<UTexture2D*> Textures;

    FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
    TArray<FAssetData> SelectedAssets;
    ContentBrowserModule.Get().GetSelectedAssets(SelectedAssets);

    for (const FAssetData& AssetData : SelectedAssets)
    {
        if (UTexture2D* Texture = Cast<UTexture2D>(AssetData.GetAsset()))
        {
            Textures.Add(Texture);
        }
    }

    return Textures;
}

FMaterialPilotTarget FMaterialPilotSelection::TargetFromObject(UObject* Object)
{
    FMaterialPilotTarget Target;

    if (UPrimitiveComponent* Component = Cast<UPrimitiveComponent>(Object))
    {
        if (Component->GetNumMaterials() > 0)
        {
            Target.Kind = EMaterialPilotTargetKind::Component;
            Target.Component = Component;
            Target.MaterialSlotIndex = 0;
            Target.Label = FString::Printf(TEXT("%s Slot 0"), *Component->GetName());
        }
        return Target;
    }

    if (UStaticMesh* StaticMesh = Cast<UStaticMesh>(Object))
    {
        if (StaticMesh->GetStaticMaterials().Num() > 0)
        {
            Target.Kind = EMaterialPilotTargetKind::StaticMeshAsset;
            Target.StaticMesh = StaticMesh;
            Target.MaterialSlotIndex = 0;
            Target.Label = FString::Printf(TEXT("%s Slot 0"), *StaticMesh->GetName());
        }
        return Target;
    }

    if (USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(Object))
    {
        if (SkeletalMesh->GetMaterials().Num() > 0)
        {
            Target.Kind = EMaterialPilotTargetKind::SkeletalMeshAsset;
            Target.SkeletalMesh = SkeletalMesh;
            Target.MaterialSlotIndex = 0;
            Target.Label = FString::Printf(TEXT("%s Slot 0"), *SkeletalMesh->GetName());
        }
        return Target;
    }

    return Target;
}
