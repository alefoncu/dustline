import unreal
m = unreal.EditorAssetLibrary.load_asset("/Game/Vegetation/M_VegPBR")
m.set_editor_property("two_sided", True)
unreal.MaterialEditingLibrary.recompile_material(m)
print("TS", unreal.EditorAssetLibrary.save_loaded_asset(m), m.get_editor_property("two_sided"))
