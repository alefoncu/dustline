import unreal, math
ela = unreal.EditorLevelLibrary
LEVEL = '/Game/Maps/Dustline_World'
DTL = unreal.DustlineTerrainLibrary

# ---------- 1. level + terrain ----------
unreal.EditorAssetLibrary.make_directory('/Game/Maps')
if unreal.EditorAssetLibrary.does_asset_exist(LEVEL):
    unreal.EditorAssetLibrary.delete_asset(LEVEL)
ela.new_level(LEVEL)
world = ela.get_editor_world()
land = DTL.create_procedural_landscape(world, 2017, 1337, 200.0, 300.0, 380.0, 350000, True, 0.07)
mat = unreal.EditorAssetLibrary.load_asset('/Game/Terrain/M_Terrain')
land.set_editor_property("landscape_material", mat)
land.set_actor_label("Terrain")
loc = DTL.find_spawn_location(world)
print("BW_SPAWN", loc)
ela.spawn_actor_from_class(unreal.PlayerStart, loc)

# ---------- 2. light / atmosphere ----------
sun = ela.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 100000), unreal.Rotator(0, -38, 35))
sun.set_actor_label("Sun"); sun.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
c = sun.get_component_by_class(unreal.DirectionalLightComponent)
c.set_editor_property("atmosphere_sun_light", True); c.set_editor_property("intensity", 4.0)
ela.spawn_actor_from_class(unreal.SkyAtmosphere, unreal.Vector(0, 0, 0)).set_actor_label("SkyAtmosphere")
sl = ela.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 50000)); sl.set_actor_label("SkyLight")
sl.get_component_by_class(unreal.SkyLightComponent).set_editor_property("real_time_capture", True); sl.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
fog = ela.spawn_actor_from_class(unreal.ExponentialHeightFog, unreal.Vector(0, 0, 0)); fog.set_actor_label("HeightFog")
fog.get_component_by_class(unreal.ExponentialHeightFogComponent).set_editor_property("fog_density", 0.004)
ela.spawn_actor_from_class(unreal.VolumetricCloud, unreal.Vector(0, 0, 0)).set_actor_label("Clouds")
# (no post-process volume: default auto exposure looks correct)

# ---------- 3. LODs for heavy meshes ----------
sms = unreal.EditorStaticMeshLibrary
def load_mesh(i, suffix="_1k"):
    return unreal.EditorAssetLibrary.load_asset("/Game/Vegetation/%s/%s%s" % (i, i, suffix))
def make_lods(mesh, tri_threshold=30000):
    if mesh.get_num_triangles(0) < tri_threshold: return
    rs = []
    for pct, screen in ((1.0, 1.0), (0.18, 0.35), (0.05, 0.12), (0.012, 0.04)):
        r = unreal.EditorScriptingMeshReductionSettings(percent_triangles=pct, screen_size=screen); rs.append(r)
    o = unreal.EditorScriptingMeshReductionOptions(auto_compute_lod_screen_size=False, reduction_settings=rs)
    sms.set_lods(mesh, o)
IDS = ["rock_09","rock_07","stone_01","boulder_01","dead_tree_trunk","tree_stump_01","shrub_03","shrub_04","fir_sapling"]
meshes = {}
for i in IDS:
    m = load_mesh(i); meshes[i] = m
    make_lods(m)
    unreal.EditorAssetLibrary.save_loaded_asset(m)

def height_cm(m, axis='z'):
    b = m.get_bounds(); return max(getattr(b.box_extent, axis) * 2.0, 1.0)

# ---------- 4. scatter ----------
def scatter(i, count, seed, tmin_cm, tmax_cm, zmin, zmax, slope, nscale, nthr, align, coll, cull, label=None, zoff=0.0, upaxis=None):
    rot = unreal.Rotator(0, 0, 0)
    axis = 'z'
    if upaxis == 'x':   # model stands along its X axis: rotate it upright
        b = meshes[i].get_bounds()
        rot = unreal.Rotator(0, 90 if b.origin.x >= 0 else -90, 0)   # (roll, pitch, yaw)
        axis = 'x'
        print("BW_UPRIGHT", i, "origin.x", b.origin.x, "->", rot)
    h = height_cm(meshes[i], axis)
    return DTL.scatter_instances(world, meshes[i], count, seed, tmin_cm / h, tmax_cm / h, zmin, zmax, slope, nscale, nthr, align, zoff, cull, coll, rot, label or i)
for k in range(6):
    meshes["SM_Fir_%d" % k] = unreal.EditorAssetLibrary.load_asset("/Game/Vegetation/Trees/SM_Fir_%d" % k)
    scatter("SM_Fir_%d" % k, 4500, 100 + k, 700, 1500, 400, 24000, 0.30, 0.00045, 0.46, False, True, 120000)
scatter("shrub_03", 1800, 21, 70, 140, 250, 22000, 0.35, 0.0009, 0.44, False, False, 14000)
scatter("shrub_04", 1400, 22, 90, 190, 250, 22000, 0.35, 0.0008, 0.46, False, False, 14000)
scatter("rock_09", 700, 31, 120, 320, 100, 36000, 0.75, 0, 0, True, True, 60000, zoff=-10)
scatter("rock_07", 550, 32, 150, 380, 100, 36000, 0.75, 0, 0, True, True, 60000, zoff=-10)
scatter("stone_01", 350, 33, 200, 520, 100, 36000, 0.85, 0, 0, True, True, 70000, zoff=-15)
scatter("boulder_01", 140, 34, 420, 900, 100, 36000, 0.9, 0, 0, True, True, 90000, zoff=-30)
scatter("dead_tree_trunk", 160, 41, 450, 800, 300, 24000, 0.3, 0.0004, 0.5, False, True, 50000)
scatter("tree_stump_01", 220, 42, 80, 160, 300, 24000, 0.3, 0.0004, 0.5, False, True, 30000)

# ---------- 5. water ----------
plane = unreal.EditorAssetLibrary.load_asset("/Engine/BasicShapes/Plane")
wm = unreal.EditorAssetLibrary.load_asset("/Game/Water/M_Water")
w = ela.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0, 0, 0))
w.set_actor_label("WaterSurface")
smc = w.static_mesh_component
smc.set_static_mesh(plane); smc.set_material(0, wm)
w.set_actor_scale3d(unreal.Vector(4200, 4200, 1))
smc.set_editor_property("cast_shadow", False)
smc.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)

ws = ela.get_editor_world().get_world_settings(); ws.set_editor_property("default_game_mode", unreal.DustlineGameMode)
print("BW_SAVED", ela.save_current_level())
