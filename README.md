# MaterialPilot UE

MaterialPilot UE is an Unreal Engine 5 editor plugin that turns selected texture sets into reusable Unreal materials.

Developed by **Nima Ghozatimoslehabadi**.

The first vertical slice focuses on the core workflow from the roadmap:

```text
Select mesh/component -> select textures -> analyze -> dry run -> build material instance -> assign
```

## What It Does

- Opens as a native dockable editor tab from **Tools > MaterialPilot UE**.
- Captures the selected mesh component, Static Mesh asset, or Skeletal Mesh asset as the material target.
- Captures selected `Texture2D` assets from the Content Browser.
- Groups the texture set by common stem.
- Classifies common PBR maps:
  - Base Color / Albedo / Diffuse
  - Normal
  - Roughness
  - Metallic / Metalness
  - Ambient Occlusion
  - Opacity / Mask
  - Emissive
  - Specular
  - Gloss / Smoothness
- Detects packed channel maps:
  - ORM / ARM: R=AO, G=Roughness, B=Metallic
  - RMA: R=Roughness, G=Metallic, B=AO
  - MRA: R=Metallic, G=Roughness, B=AO
- Reports confidence and reasons for every classification.
- Generates or reuses a parent material by recipe signature.
- Creates or reuses a Material Instance Constant for the texture set.
- Sets texture parameters on the material instance.
- Optionally fixes texture settings for sRGB, compression, masks, and normal maps.
- Assigns the generated material instance to the captured target slot.
- Supports dry run reports before changing assets.

## Compatibility

MaterialPilot is delivered as a source C++ editor plugin. The `.uplugin` intentionally does not pin `EngineVersion`, so Unreal can rebuild it for UE 5.x projects and future UE5 versions when the editor APIs remain compatible.

The plugin uses editor-side systems that are stable across UE5:

- ToolMenus and dockable tabs
- Content Browser selection
- AssetTools asset creation
- Material Instance Constants
- Material Editing Library
- Editor transactions
- Developer Settings

## Installation

1. Copy the `MaterialPilot` folder into your Unreal project:

   ```text
   YourProject/Plugins/MaterialPilot
   ```

2. Open the project in Unreal Engine 5.
3. When Unreal asks to rebuild modules, choose **Yes**.
4. Enable **MaterialPilot UE** from **Edit > Plugins** if needed.
5. Restart the editor if Unreal asks.

## Basic Use

1. Select a mesh component in the level, or select a Static Mesh / Skeletal Mesh asset.
2. Open **Tools > MaterialPilot UE**.
3. Click **Capture Selected Target**.
4. Select the texture assets in the Content Browser.
5. Click **Capture Selected Textures**.
6. Click **Analyze** to inspect the detected map roles.
7. Click **Dry Run** to preview generated assets and assignment.
8. Click **Build & Assign** when the report looks correct.

Generated assets are created in:

```text
/Game/AutoMaterial/GeneratedParents
/Game/AutoMaterial/Instances
```

You can change those folders in **Project Settings > Plugins > MaterialPilot UE**.

## Current MVP Scope

This version implements the first strong production slice from the roadmap. It is not yet the complete future product.

Included:

- Single captured target slot
- Selected texture-set workflow
- Explainable token-based classifier
- Packed ORM/RMA/MRA support
- Texture setting cleanup
- Generated parent material cache
- Material instance creation/reuse
- Assignment with editor transaction support
- Dry-run report

Planned next:

- Multi-slot mapper
- Batch folder mode
- Custom profile/rule editor
- Existing studio master-material mapping
- UDIM and virtual texture awareness
- Advanced height/displacement recipes
- Automated Unreal test suite

## Notes

- The tool does not use cloud AI.
- It does not overwrite existing user materials.
- It marks changed assets dirty so you can review and save them normally in Unreal.
- Ambiguous low-confidence texture inputs should be reviewed before building.

## License

This project is released under the MIT License. See [LICENSE](LICENSE) for details.
