# Los masters de vegetación no tenían el uso "Instanced Static Meshes": en el juego (instancias del scatter) Unreal
# usaba el material por defecto (gris). Se activa el uso, se recompila y se vuelven a guardar las instancias.
import unreal
mel = unreal.MaterialEditingLibrary
for p in ["/Game/Vegetation/M_VegPBR", "/Game/Vegetation/M_VegFoliage"]:
    m = unreal.EditorAssetLibrary.load_asset(p)
    for flag in ["used_with_instanced_static_meshes"]:
        m.set_editor_property(flag, True)
    mel.recompile_material(m); unreal.EditorAssetLibrary.save_loaded_asset(m, False)
    print("VU_MASTER", p, m.get_editor_property("used_with_instanced_static_meshes"))
for a in unreal.EditorAssetLibrary.list_assets("/Game/Vegetation", recursive=True):
    o = unreal.EditorAssetLibrary.load_asset(a)
    if isinstance(o, unreal.MaterialInstanceConstant):
        mel.update_material_instance(o); unreal.EditorAssetLibrary.save_loaded_asset(o, False); print("VU_MI", a)
    elif isinstance(o, unreal.Material) and not o.get_editor_property("used_with_instanced_static_meshes"):
        o.set_editor_property("used_with_instanced_static_meshes", True); mel.recompile_material(o); unreal.EditorAssetLibrary.save_loaded_asset(o, False); print("VU_MAT", a)
print("VU_OK")
