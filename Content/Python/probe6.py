import unreal
mel = unreal.MaterialEditingLibrary
for mp in ("M_VegPBR", "M_VegFoliage"):
    m = unreal.EditorAssetLibrary.load_asset("/Game/Vegetation/" + mp)
    for prop in (unreal.MaterialProperty.MP_BASE_COLOR, unreal.MaterialProperty.MP_NORMAL, unreal.MaterialProperty.MP_ROUGHNESS, unreal.MaterialProperty.MP_OPACITY_MASK):
        n = mel.get_material_property_input_node(m, prop)
        out = mel.get_material_property_input_node_output_name(m, prop)
        print("P6", mp, str(prop).split('.')[-1], "->", n.get_class().get_name() if n else None, getattr(n, 'get_editor_property', lambda x: None)('parameter_name') if n and 'Parameter' in n.get_class().get_name() else "", "out:", out)
mi = unreal.EditorAssetLibrary.load_asset("/Game/Vegetation/fir_sapling_medium/MI_fir_sapling_medium_twigs")
print("P6 MI parent", mi.get_editor_property("parent").get_name())
for tp in mi.get_editor_property("texture_parameter_values"):
    print("P6 MI tex", tp.parameter_info.name, tp.parameter_value.get_name() if tp.parameter_value else None)
for sp in mi.get_editor_property("scalar_parameter_values"):
    print("P6 MI scalar", sp.parameter_info.name, sp.parameter_value)
t = unreal.EditorAssetLibrary.load_asset("/Game/Vegetation/fir_sapling_medium/Tex/fir_sapling_medium_twigs_diff_1k")
print("P6 TEX", t.get_name(), "srgb", t.get_editor_property("srgb"), "comp", t.get_editor_property("compression_settings"), t.blueprint_get_size_x(), "lod_group", t.get_editor_property("lod_group"))
