import unreal
unreal.EditorLevelLibrary.load_level('/Game/Maps/Dustline_World')
ela = unreal.EditorLevelLibrary
for a in ela.get_all_level_actors():
    if isinstance(a, unreal.PostProcessVolume): ela.destroy_actor(a)
    if isinstance(a, unreal.DirectionalLight):
        a.get_component_by_class(unreal.DirectionalLightComponent).set_editor_property("intensity", 6.0)
v = ela.spawn_actor_from_class(unreal.PostProcessVolume, unreal.Vector(0, 0, 0))
v.set_actor_label("GlobalPostProcess")
v.set_editor_property("unbound", True)
s = v.get_editor_property("settings")
s.set_editor_property("override_auto_exposure_method", True)
s.set_editor_property("auto_exposure_method", unreal.AutoExposureMethod.AEM_MANUAL)
s.set_editor_property("override_auto_exposure_bias", True)
s.set_editor_property("auto_exposure_bias", 13.5)
s.set_editor_property("override_bloom_intensity", True)
s.set_editor_property("bloom_intensity", 0.2)
v.set_editor_property("settings", s)
print("EXP_SAVED", ela.save_current_level())
