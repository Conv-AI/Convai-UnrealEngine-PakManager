import unreal

def create_primary_asset_label(asset_path: str, asset_name: str, chunk_id: int, priority: int):
    """
    Creates a UPrimaryAssetLabel asset at a specified destination path with custom ChunkId and Priority.

    Parameters:
        asset_path (str): The virtual content folder where the asset should be created (e.g. "/TestPy/AssetLabels").
        asset_name (str): The name to assign to the asset.
        chunk_id (int): The value to set for ChunkId in PrimaryAssetRules.
        priority (int): The value to set for Priority in PrimaryAssetRules.
    """
    # Ensure the destination path exists; if not, create it.
    if not unreal.EditorAssetLibrary.does_directory_exist(asset_path):
        unreal.EditorAssetLibrary.make_directory(asset_path)

    # Set up a generic DataAssetFactory, specifying that it will create a PrimaryAssetLabel.
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("SupportedClass", unreal.PrimaryAssetLabel)

    # Create the asset using the AssetToolsHelpers.
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    new_asset = asset_tools.create_asset(asset_name, asset_path, unreal.PrimaryAssetLabel, factory)

    if new_asset:
        # Set basic label properties for the asset.
        new_asset.set_editor_property("bLabelAssetsInMyDirectory", True)
        new_asset.set_editor_property("bIsRuntimeLabel", True)

        # Create an instance of PrimaryAssetRules and assign the properties using set_editor_property.
        asset_rules = unreal.PrimaryAssetRules()
        asset_rules.set_editor_property("Priority", priority)
        asset_rules.set_editor_property("ChunkId", chunk_id)
        asset_rules.set_editor_property("bApplyRecursively", True)
        asset_rules.set_editor_property("CookRule", unreal.PrimaryAssetCookRule.ALWAYS_COOK)

        # Apply the rules to the asset.
        new_asset.set_editor_property("rules", asset_rules)

        # Save the asset and log the output.
        asset_full_path = f"{asset_path}/{asset_name}"
        unreal.EditorAssetLibrary.save_asset(asset_full_path)
        unreal.log("✅ PrimaryAssetLabel '{}' created at '{}'".format(asset_name, asset_full_path))
    else:
        unreal.log_error("Failed to create PrimaryAssetLabel asset.")

    return new_asset

# Example usage: Creates an asset named "PAL_Test" under /TestPy/CustomFolder with ChunkId 10 and Priority 0.
# create_primary_asset_label("/TestPy/CustomFolder", "PAL_Test", 10, 0)
