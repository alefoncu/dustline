import unreal
for root in ("/Water", "/Engine/EngineMaterials", "/Engine/EngineResources", "/Engine/Functions"):
    try:
        for p in unreal.EditorAssetLibrary.list_assets(root, recursive=True, include_folder=False):
            l = p.lower()
            if ("water" in l and ("_n" in l or "normal" in l or "wave" in l)) or ("normal" in l and "water" in l):
                print("P4", p)
    except Exception as e:
        print("P4ERR", root, e)
for p in ("/Engine/BasicShapes/Plane", "/Engine/BasicShapes/Cylinder", "/Engine/BasicShapes/Cube"):
    print("P4EXISTS", p, unreal.EditorAssetLibrary.does_asset_exist(p))
