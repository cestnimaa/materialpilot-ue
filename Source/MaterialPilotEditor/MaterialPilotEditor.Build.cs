using UnrealBuildTool;

public class MaterialPilotEditor : ModuleRules
{
    public MaterialPilotEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreUObject",
                "Engine"
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "AssetRegistry",
                "AssetTools",
                "ContentBrowser",
                "DeveloperSettings",
                "EditorFramework",
                "InputCore",
                "MaterialEditor",
                "Projects",
                "PropertyEditor",
                "Slate",
                "SlateCore",
                "ToolMenus",
                "UnrealEd"
            }
        );
    }
}
