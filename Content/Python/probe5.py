import unreal
for i in ["fir_sapling","shrub_03","shrub_04","rock_09","boulder_01","dead_tree_trunk"]:
    m = unreal.EditorAssetLibrary.load_asset("/Game/Vegetation/%s/%s_1k" % (i, i))
    b = m.get_bounds()
    print("P5", i, "extent(cm)", round(b.box_extent.x,1), round(b.box_extent.y,1), round(b.box_extent.z,1), "origin", round(b.origin.z,1), "lods", m.get_num_lods())
    for k, sm in enumerate(m.static_materials):
        mi = sm.material_interface
        if mi:
            try:
                bm = mi.get_base_material()
                print("P5M", i, k, mi.get_name(), "base", bm.get_name() if bm else None, "blend", bm.get_editor_property("blend_mode") if bm else None, "2sided", bm.get_editor_property("two_sided") if bm else None)
            except Exception as e:
                print("P5M", i, k, mi.get_name(), "ERR", e)
