import unreal
eal = unreal.EditorAssetLibrary
names = ["rock_09/rock_09_1k","rock_07/rock_07_1k","stone_01/stone_01_1k","boulder_01/boulder_01_1k","dead_tree_trunk/dead_tree_trunk_1k","tree_stump_01/tree_stump_01_1k","shrub_03/shrub_03_1k","shrub_04/shrub_04_1k"] + ["Trees/SM_Fir_%d"%k for k in range(6)]
for n in names:
    m = eal.load_asset("/Game/Vegetation/"+n)
    if not m: print("PC_MISSING", n); continue
    b = m.get_bounds()
    bs = m.get_editor_property("body_setup")
    flag = bs.get_editor_property("collision_trace_flag") if bs else None
    agg = bs.get_editor_property("agg_geom") if bs else None
    cx = len(agg.get_editor_property("convex_elems")) if agg else -1
    sp = len(agg.get_editor_property("sphyl_elems")) if agg else -1
    bx = len(agg.get_editor_property("box_elems")) if agg else -1
    print("PC", n, "tris", m.get_num_triangles(0), "lods", m.get_num_lods(), "ext", b.box_extent, "org", b.origin, "flag", flag, "convex", cx, "sphyl", sp, "box", bx)
