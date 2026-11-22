import unreal
ela = unreal.EditorLevelLibrary
mel = unreal.MaterialEditingLibrary
at = unreal.AssetToolsHelpers.get_asset_tools()
LEVEL = '/Game/Maps/Showroom'

# ---- ground material (world-aligned grass) ----
gp = "/Game/Showroom/M_ShowGround"
unreal.EditorAssetLibrary.make_directory("/Game/Showroom")
if unreal.EditorAssetLibrary.does_asset_exist(gp): unreal.EditorAssetLibrary.delete_asset(gp)
g = at.create_asset("M_ShowGround", "/Game/Showroom", unreal.Material, unreal.MaterialFactoryNew())
wp = mel.create_material_expression(g, unreal.MaterialExpressionWorldPosition, -900, 0)
xy = mel.create_material_expression(g, unreal.MaterialExpressionComponentMask, -700, 0)
xy.set_editor_property("r", True); xy.set_editor_property("g", True)
mel.connect_material_expressions(wp, "", xy, "")
uv = mel.create_material_expression(g, unreal.MaterialExpressionMultiply, -500, 0); uv.set_editor_property("const_b", 1.0 / 400.0)
mel.connect_material_expressions(xy, "", uv, "A")
def T(path, y, normal=False):
    n = mel.create_material_expression(g, unreal.MaterialExpressionTextureSample, -300, y)
    n.set_editor_property("texture", unreal.EditorAssetLibrary.load_asset(path))
    n.set_editor_property("sampler_source", unreal.SamplerSourceMode.SSM_WRAP_WORLD_GROUP_SETTINGS)
    if normal: n.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    mel.connect_material_expressions(uv, "", n, "Coordinates"); return n
mel.connect_material_property(T("/Game/Terrain/Textures/T_Grass004_Color", 0), "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
mel.connect_material_property(T("/Game/Terrain/Textures/T_Grass004_Normal", 300, True), "RGB", unreal.MaterialProperty.MP_NORMAL)
mel.connect_material_property(T("/Game/Terrain/Textures/T_Grass004_Roughness", 600), "R", unreal.MaterialProperty.MP_ROUGHNESS)
mel.recompile_material(g); unreal.EditorAssetLibrary.save_loaded_asset(g)

# ---- level ----
if unreal.EditorAssetLibrary.does_asset_exist(LEVEL): unreal.EditorAssetLibrary.delete_asset(LEVEL)
ela.new_level(LEVEL)
plane = unreal.EditorAssetLibrary.load_asset("/Engine/BasicShapes/Plane")
a = ela.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0, 0, 0)); a.set_actor_label("Ground")
a.static_mesh_component.set_static_mesh(plane); a.static_mesh_component.set_material(0, g); a.set_actor_scale3d(unreal.Vector(300, 300, 1))

def place(mid, y, target_cm, label, up='z', rot_pitch=0.0):
    m = unreal.EditorAssetLibrary.load_asset("/Game/Vegetation/%s/%s_1k" % (mid, mid))
    b = m.get_bounds()
    h = getattr(b.box_extent, up) * 2.0
    sc = target_cm / h
    base_local = getattr(b.origin, up) - getattr(b.box_extent, up)
    rot = unreal.Rotator(0, rot_pitch, 0)
    act = ela.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0, y, -base_local * sc), rot)
    act.set_actor_label(label)
    act.static_mesh_component.set_static_mesh(m)
    act.set_actor_scale3d(unreal.Vector(sc, sc, sc))
    print("SR_PLACED", label, "h_native", round(h, 1), "scale", round(sc, 3), "extent", b.box_extent, "origin", b.origin)
    return act
place("fir_sapling", -1600, 1100, "fir", up='x', rot_pitch=90.0)
place("shrub_03", -800, 110, "shrub_03")
place("shrub_04", -250, 130, "shrub_04")
place("rock_09", 400, 180, "rock_09")
place("boulder_01", 1000, 320, "boulder_01")
place("dead_tree_trunk", 1900, 120, "trunk")   # target: lying trunk ~1.2 m tall footprint
# human-height reference (180 cm)
cyl = unreal.EditorAssetLibrary.load_asset("/Engine/BasicShapes/Cylinder")
r = ela.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(-500, 700, 90)); r.set_actor_label("Ref180cm")
r.static_mesh_component.set_static_mesh(cyl); r.set_actor_scale3d(unreal.Vector(0.4, 0.4, 1.8))

# ---- lights ----
sun = ela.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 5000), unreal.Rotator(0, -42, 60)); sun.set_actor_label("Sun")
c = sun.get_component_by_class(unreal.DirectionalLightComponent)
c.set_editor_property("atmosphere_sun_light", True); c.set_editor_property("intensity", 4.0)
ela.spawn_actor_from_class(unreal.SkyAtmosphere, unreal.Vector(0, 0, 0))
sl = ela.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 1000)); sl.get_component_by_class(unreal.SkyLightComponent).set_editor_property("real_time_capture", True)
print("SR_SAVED", ela.save_current_level())
