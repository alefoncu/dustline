import unreal, os, glob
mel = unreal.MaterialEditingLibrary
at = unreal.AssetToolsHelpers.get_asset_tools()
SRC = "/Volumes/SSD/Models-Setup"; I = "fir_sapling_medium"; DEST = "/Game/Vegetation/%s" % I
t = unreal.AssetImportTask()
t.filename = "%s/%s/%s_1k.fbx" % (SRC, I, I); t.destination_path = DEST
t.automated = True; t.save = False; t.replace_existing = True
o = unreal.FbxImportUI()
o.set_editor_property("import_mesh", True); o.set_editor_property("import_as_skeletal", False)
o.set_editor_property("import_materials", False); o.set_editor_property("import_textures", False)
o.set_editor_property("automated_import_should_detect_type", False)
o.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
o.static_mesh_import_data.set_editor_property("combine_meshes", True)
o.static_mesh_import_data.set_editor_property("generate_lightmap_u_vs", False)
t.options = o
at.import_asset_tasks([t])
mesh = unreal.EditorAssetLibrary.load_asset("%s/%s_1k" % (DEST, I))
print("NT_MESH", mesh, "tris", mesh.get_num_triangles(0), "extent", mesh.get_bounds().box_extent, "origin", mesh.get_bounds().origin)

# --- Nanite ---
ns = mesh.get_editor_property("nanite_settings")
ns.set_editor_property("enabled", True)
mesh.set_editor_property("nanite_settings", ns)
print("NT_NANITE_SET", mesh.get_editor_property("nanite_settings").get_editor_property("enabled"))

# --- textures + material instances (same recipe as other vegetation) ---
m_pbr = unreal.EditorAssetLibrary.load_asset("/Game/Vegetation/M_VegPBR")
m_fol = unreal.EditorAssetLibrary.load_asset("/Game/Vegetation/M_VegFoliage")
tasks = []
for f in sorted(glob.glob("%s/%s/textures/*" % (SRC, I))):
    b = os.path.basename(f)
    if "_disp_" in b: continue
    tk = unreal.AssetImportTask(); tk.filename = f; tk.destination_path = DEST + "/Tex"
    tk.destination_name = os.path.splitext(b)[0]; tk.automated = True; tk.save = False; tk.replace_existing = True
    tasks.append(tk)
at.import_asset_tasks(tasks)
texs = {}
for tk in tasks:
    tx = unreal.EditorAssetLibrary.load_asset(DEST + "/Tex/" + tk.destination_name)
    if not tx: continue
    n = tk.destination_name
    if "_nor_" in n:
        tx.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        tx.set_editor_property("srgb", False); tx.set_editor_property("flip_green_channel", True)
    elif "_rough_" in n or "_alpha_" in n:
        tx.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS); tx.set_editor_property("srgb", False)
    texs[n] = tx; unreal.EditorAssetLibrary.save_loaded_asset(tx)
for k, sm in enumerate(mesh.static_materials):
    slot = str(sm.material_slot_name)
    find = lambda kind: next((t for n, t in texs.items() if n.startswith(slot + "_" + kind)), None)
    alpha = find("alpha")
    mi = at.create_asset("MI_" + slot, DEST, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mi.set_editor_property("parent", m_fol if alpha else m_pbr)
    for pn, tx in (("Diffuse", find("diff")), ("Normal", find("nor")), ("Rough", find("rough")), ("Alpha", alpha)):
        if tx is not None: mel.set_material_instance_texture_parameter_value(mi, pn, tx)
    mel.update_material_instance(mi); unreal.EditorAssetLibrary.save_loaded_asset(mi)
    mesh.set_material(k, mi)
    print("NT_MAT", slot, "foliage" if alpha else "pbr")
unreal.EditorAssetLibrary.save_loaded_asset(mesh)

# --- place in showroom ---
ela = unreal.EditorLevelLibrary
ela.load_level('/Game/Maps/Showroom')
b = mesh.get_bounds()
print("NT_BOUNDS", b.box_extent, b.origin)
up = 'x' if b.box_extent.x > b.box_extent.z * 1.4 else 'z'
rot = unreal.Rotator(0, 90 if b.origin.x >= 0 else -90, 0) if up == 'x' else unreal.Rotator(0, 0, 0)
h = getattr(b.box_extent, up) * 2.0
sc = 700.0 / h
base = getattr(b.origin, up) - getattr(b.box_extent, up)
for a in ela.get_all_level_actors():
    if a.get_actor_label() == "nanite_fir": ela.destroy_actor(a)
act = ela.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0, -2600, -base * sc), rot)
act.set_actor_label("nanite_fir")
act.static_mesh_component.set_static_mesh(mesh); act.set_actor_scale3d(unreal.Vector(sc, sc, sc))
print("NT_PLACED up", up, "native_h", round(h, 1), "scale", round(sc, 3))
print("NT_SAVED", ela.save_current_level())
