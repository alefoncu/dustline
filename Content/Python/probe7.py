import unreal
for p in ("/Engine/EngineResources/WhiteSquareTexture", "/Engine/EngineMaterials/DefaultNormal", "/Engine/EngineMaterials/DefaultDiffuse", "/Engine/EngineMaterials/DefaultWhiteGrid", "/Engine/EngineResources/Black"):
    print("P7", p, unreal.EditorAssetLibrary.does_asset_exist(p), unreal.EditorAssetLibrary.load_asset(p))
for n in unreal.EditorAssetLibrary.list_assets("/Engine/EngineMaterials", recursive=False)[:40]: print("P7L", n)
for n in unreal.EditorAssetLibrary.list_assets("/Engine/EngineResources", recursive=False)[:40]: print("P7R", n)
