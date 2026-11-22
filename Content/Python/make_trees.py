import unreal
DTL = unreal.DustlineTerrainLibrary
ela = unreal.EditorLevelLibrary
unreal.EditorAssetLibrary.make_directory("/Game/Vegetation/Trees")
bark = unreal.EditorAssetLibrary.load_asset("/Game/Vegetation/fir_sapling/MI_fir_sapling_branches")
fol_dense = unreal.EditorAssetLibrary.load_asset("/Game/Vegetation/fir_sapling/MI_fir_sapling_twigs")
fol_sparse = unreal.EditorAssetLibrary.load_asset("/Game/Vegetation/fir_sapling_medium/MI_fir_sapling_medium_twigs")
VARS = [(1, 1100, 22, 17, 9, 650, "sparse"), (2, 1300, 26, 19, 9, 720, "sparse"), (3, 900, 18, 15, 8, 560, "sparse"),
        (4, 1500, 30, 21, 10, 820, "sparse"), (5, 1000, 20, 16, 8, 600, "sparse"), (6, 1200, 24, 18, 9, 690, "sparse")]
meshes = []
for k, (seed, h, tr, wh, cards, crown, kind) in enumerate(VARS):
    name = "SM_Fir_%d" % k
    p = "/Game/Vegetation/Trees/" + name
    if unreal.EditorAssetLibrary.does_asset_exist(p): unreal.EditorAssetLibrary.delete_asset(p)
    m = DTL.create_procedural_conifer("/Game/Vegetation/Trees", name, seed, float(h), float(tr), wh, cards, float(crown), bark, fol_sparse if kind == "sparse" else fol_dense)
    print("TR_MESH", name, m, "tris", m.get_num_triangles(0) if m else None, "bounds", m.get_bounds().box_extent if m else None)
    unreal.EditorAssetLibrary.save_loaded_asset(m)
    meshes.append(m)
# showroom row
ela.load_level('/Game/Maps/Showroom')
for a in ela.get_all_level_actors():
    l = a.get_actor_label()
    if l.startswith("diag_") or l in ("nanite_fir", "fir") or l.startswith("myfir_"): ela.destroy_actor(a)
for k, m in enumerate(meshes):
    a = ela.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0, 2000 + k * 520, 0))
    a.set_actor_label("myfir_%d" % k)
    a.static_mesh_component.set_static_mesh(m)
print("TR_SAVED", ela.save_current_level())
