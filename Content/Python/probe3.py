import unreal
unreal.EditorLevelLibrary.load_level('/Game/Maps/Dustline_World')
for a in unreal.EditorLevelLibrary.get_all_level_actors():
    n = a.get_class().get_name()
    o, e = a.get_actor_bounds(False)
    print("P3", n, a.get_actor_label(), "origin", o, "extent", e)
    if n == "Landscape":
        comps = a.get_components_by_class(unreal.LandscapeComponent)
        print("P3_COMPS", len(comps), "scale", a.get_actor_scale3d(), "loc", a.get_actor_location())
