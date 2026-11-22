import unreal
for lvl in ('/Game/Maps/Dustline_World',):
    unreal.EditorLevelLibrary.load_level(lvl)
    ws = unreal.EditorLevelLibrary.get_editor_world().get_world_settings()
    ws.set_editor_property("default_game_mode", unreal.DustlineGameMode)
    print("GM_SAVED", unreal.EditorLevelLibrary.save_current_level(), ws.get_editor_property("default_game_mode"))
