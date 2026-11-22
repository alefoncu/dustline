import unreal
SRC = "/Volumes/SSD/Textures-Setup/tex"
DEST = "/Game/Terrain/Textures"
IDS = ["Grass004", "Ground071", "Ground106", "Ground080", "Rock058", "Snow014"]
unreal.EditorAssetLibrary.make_directory(DEST)
at = unreal.AssetToolsHelpers.get_asset_tools()
tasks = []
maps = {"Color": "Color", "NormalDX": "Normal", "Roughness": "Roughness"}
for i in IDS:
    for suffix, kind in maps.items():
        t = unreal.AssetImportTask()
        t.filename = "%s/%s/%s_2K-JPG_%s.jpg" % (SRC, i, i, suffix)
        t.destination_path = DEST
        t.destination_name = "T_%s_%s" % (i, kind)
        t.automated = True; t.save = False; t.replace_existing = True
        tasks.append(t)
at.import_asset_tasks(tasks)
for i in IDS:
    for kind in maps.values():
        p = "%s/T_%s_%s" % (DEST, i, kind)
        tex = unreal.EditorAssetLibrary.load_asset(p)
        if not tex:
            print("IMP_MISSING", p); continue
        if kind == "Color":
            tex.set_editor_property("srgb", True)
        elif kind == "Normal":
            tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
            tex.set_editor_property("srgb", False)
        else:
            tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
            tex.set_editor_property("srgb", False)
        unreal.EditorAssetLibrary.save_loaded_asset(tex)
print("IMP_DONE", len(unreal.EditorAssetLibrary.list_assets(DEST)))
