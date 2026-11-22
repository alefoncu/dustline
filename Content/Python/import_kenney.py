import unreal
SRC = "/Volumes/SSD/Models-Cars/kenney_car-kit/Models/FBX format"
NAMES = ["suv", "suv-luxury", "sedan-sports", "hatchback-sports", "truck", "truck-flat", "van", "tractor", "tractor-shovel", "kart-oobi", "wheel-default", "wheel-dark", "wheel-truck", "wheel-tractor-back"]
DEST = "/Game/Cars/Kenney"
at = unreal.AssetToolsHelpers.get_asset_tools()
unreal.EditorAssetLibrary.make_directory(DEST)
t = unreal.AssetImportTask(); t.filename = SRC + "/Textures/colormap.png"; t.destination_path = DEST; t.destination_name = "T_Colormap"
t.automated = True; t.save = False; t.replace_existing = True
at.import_asset_tasks([t])
tex = unreal.EditorAssetLibrary.load_asset(DEST + "/T_Colormap")
tex.set_editor_property("filter", unreal.TextureFilter.TF_NEAREST); tex.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
unreal.EditorAssetLibrary.save_loaded_asset(tex)
tasks = []
for n in NAMES:
    k = unreal.AssetImportTask(); k.filename = "%s/%s.fbx" % (SRC, n); k.destination_path = "%s/%s" % (DEST, n.replace("-", "_"))
    k.automated = True; k.save = False; k.replace_existing = True
    o = unreal.FbxImportUI()
    o.set_editor_property("import_mesh", True); o.set_editor_property("import_as_skeletal", False)
    o.set_editor_property("import_materials", False); o.set_editor_property("import_textures", False)
    o.set_editor_property("automated_import_should_detect_type", False)
    o.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
    o.static_mesh_import_data.set_editor_property("combine_meshes", False)
    o.static_mesh_import_data.set_editor_property("generate_lightmap_u_vs", False)
    k.options = o; tasks.append(k)
at.import_asset_tasks(tasks)
for n in NAMES:
    for p in unreal.EditorAssetLibrary.list_assets("%s/%s" % (DEST, n.replace("-", "_"))):
        a = unreal.EditorAssetLibrary.load_asset(p)
        if isinstance(a, unreal.StaticMesh):
            b = a.get_bounds()
            print("KN", n, a.get_name(), "tris", a.get_num_triangles(0), "ext", round(b.box_extent.x), round(b.box_extent.y), round(b.box_extent.z), "orig", round(b.origin.x), round(b.origin.y), round(b.origin.z), "slots", len(a.static_materials))
print("KN_SAVED", unreal.EditorAssetLibrary.save_directory(DEST, only_if_is_dirty=False, recursive=True))
