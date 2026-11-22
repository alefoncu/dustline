import unreal
LEVEL = '/Game/Maps/Dustline_World'
unreal.EditorAssetLibrary.make_directory('/Game/Maps')
if unreal.EditorAssetLibrary.does_asset_exist(LEVEL):
    unreal.EditorAssetLibrary.delete_asset(LEVEL)
unreal.EditorLevelLibrary.new_level(LEVEL)
world = unreal.EditorLevelLibrary.get_editor_world()
print("T_WORLD", world)
ls = unreal.DustlineTerrainLibrary.create_procedural_landscape(world, 2017, 1337, 200.0, 300.0, 380.0, 350000, True)
print("T_LANDSCAPE", ls)
loc = unreal.DustlineTerrainLibrary.find_spawn_location(world)
ok = loc.z > -1e8
print("T_SPAWN", ok, loc)
h = unreal.DustlineTerrainLibrary.get_terrain_height(world, 0.0, 0.0)
print("T_H0", h)
if ok:
    ps = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.PlayerStart, loc)
saved = unreal.EditorLevelLibrary.save_current_level()
print("T_SAVED", saved)
