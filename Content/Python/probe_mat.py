import unreal
for p in ["/Game/Cars/M_CarGeneric","/Game/Cars/M_KenneyBody","/Game/Vegetation/M_VegFoliage"]:
    m = unreal.EditorAssetLibrary.load_asset(p)
    print("PM", p, "blend", m.get_editor_property("blend_mode"), "shading", m.get_editor_property("shading_models") if hasattr(m,"shading_models") else "", "twosided", m.get_editor_property("two_sided"))
    for e in unreal.MaterialEditingLibrary.get_used_textures(m) if False else []: pass
    print("PM params", [str(x) for x in unreal.MaterialEditingLibrary.get_scalar_parameter_names(m)], [str(x) for x in unreal.MaterialEditingLibrary.get_vector_parameter_names(m)])
