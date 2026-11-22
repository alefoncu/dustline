import unreal
LEVEL = '/Game/Maps/Dustline_World'
unreal.EditorLevelLibrary.load_level(LEVEL)
ela = unreal.EditorLevelLibrary
actors = ela.get_all_level_actors()
for a in actors:
    if isinstance(a, (unreal.DirectionalLight, unreal.SkyAtmosphere, unreal.SkyLight, unreal.ExponentialHeightFog, unreal.VolumetricCloud)):
        ela.destroy_actor(a)
sun = ela.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0,0,100000), unreal.Rotator(0,-38,35))
sun.set_actor_label("Sun")
sun.get_component_by_class(unreal.DirectionalLightComponent).set_editor_property("atmosphere_sun_light", True)
sun.get_component_by_class(unreal.DirectionalLightComponent).set_editor_property("intensity", 8.0)
sky = ela.spawn_actor_from_class(unreal.SkyAtmosphere, unreal.Vector(0,0,0)); sky.set_actor_label("SkyAtmosphere")
sl = ela.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0,0,50000)); sl.set_actor_label("SkyLight")
sl.get_component_by_class(unreal.SkyLightComponent).set_editor_property("real_time_capture", True)
fog = ela.spawn_actor_from_class(unreal.ExponentialHeightFog, unreal.Vector(0,0,0)); fog.set_actor_label("HeightFog")
fog.get_component_by_class(unreal.ExponentialHeightFogComponent).set_editor_property("fog_density", 0.004)
clouds = ela.spawn_actor_from_class(unreal.VolumetricCloud, unreal.Vector(0,0,0)); clouds.set_actor_label("Clouds")
print("A_SAVED", ela.save_current_level())
