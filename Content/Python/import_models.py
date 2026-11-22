import unreal
SRC = "/Volumes/SSD/Models-Setup"
IDS = ["rock_09","rock_07","stone_01","boulder_01","dead_tree_trunk","tree_stump_01","shrub_03","shrub_04","fir_sapling"]
at = unreal.AssetToolsHelpers.get_asset_tools()
tasks = []
for i in IDS:
    t = unreal.AssetImportTask()
    t.filename = "%s/%s/%s_1k.fbx" % (SRC, i, i)
    t.destination_path = "/Game/Vegetation/%s" % i
    t.automated = True; t.save = False; t.replace_existing = True
    o = unreal.FbxImportUI()
    o.set_editor_property("import_mesh", True)
    o.set_editor_property("import_as_skeletal", False)
    o.set_editor_property("import_materials", True)
    o.set_editor_property("import_textures", True)
    o.set_editor_property("automated_import_should_detect_type", False)
    o.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
    o.static_mesh_import_data.set_editor_property("combine_meshes", True)
    o.static_mesh_import_data.set_editor_property("generate_lightmap_u_vs", False)
    t.options = o
    tasks.append(t)
at.import_asset_tasks(tasks)
for i in IDS:
    for p in unreal.EditorAssetLibrary.list_assets("/Game/Vegetation/%s" % i):
        a = unreal.EditorAssetLibrary.load_asset(p)
        extra = ""
        if isinstance(a, unreal.StaticMesh):
            extra = "tris=%d slots=%s" % (a.get_num_triangles(0) if hasattr(a,'get_num_triangles') else -1, [str(s.material_slot_name) for s in a.static_materials])
        print("IM", a.get_class().get_name(), p, extra)
print("IM_SAVED", unreal.EditorAssetLibrary.save_directory("/Game/Vegetation", only_if_is_dirty=False, recursive=True))
