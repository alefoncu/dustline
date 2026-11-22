import unreal, math
mel = unreal.MaterialEditingLibrary
at = unreal.AssetToolsHelpers.get_asset_tools()
ela = unreal.EditorLevelLibrary
unreal.EditorAssetLibrary.make_directory("/Game/Cars")
P = "/Game/Cars/M_CarGeneric"
if unreal.EditorAssetLibrary.does_asset_exist(P): unreal.EditorAssetLibrary.delete_asset(P)
m = at.create_asset("M_CarGeneric", "/Game/Cars", unreal.Material, unreal.MaterialFactoryNew())
def vec(n, d, x, y):
    e = mel.create_material_expression(m, unreal.MaterialExpressionVectorParameter, x, y); e.set_editor_property("parameter_name", n); e.set_editor_property("default_value", d); return e
def sc(n, d, x, y):
    e = mel.create_material_expression(m, unreal.MaterialExpressionScalarParameter, x, y); e.set_editor_property("parameter_name", n); e.set_editor_property("default_value", d); return e
color = vec("Color", unreal.LinearColor(0.5, 0.5, 0.5, 1), -800, 0)
metal = sc("Metallic", 0.0, -800, 200); rough = sc("Roughness", 0.5, -800, 300)
emis = sc("Emissive", 0.0, -800, 450); fade = sc("Fade", 1.0, -800, 600)
mel.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)
mel.connect_material_property(metal, "", unreal.MaterialProperty.MP_METALLIC)
mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
em = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -500, 450)
mel.connect_material_expressions(color, "", em, "A"); mel.connect_material_expressions(emis, "", em, "B")
mel.connect_material_property(em, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
wp = mel.create_material_expression(m, unreal.MaterialExpressionWorldPosition, -900, 800)
nz = mel.create_material_expression(m, unreal.MaterialExpressionNoise, -650, 800)
nz.set_editor_property("scale", 0.45); nz.set_editor_property("quality", 1); nz.set_editor_property("levels", 1)
nz.set_editor_property("output_min", 0.0); nz.set_editor_property("output_max", 1.0)
nz.set_editor_property("noise_function", unreal.NoiseFunction.NOISEFUNCTION_GRADIENT_ALU)
mel.connect_material_expressions(wp, "", nz, "Position")
cen = mel.create_material_expression(m, unreal.MaterialExpressionSubtract, -450, 800); cen.set_editor_property("const_b", 0.5)
mel.connect_material_expressions(nz, "", cen, "A")
amp = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -300, 800); amp.set_editor_property("const_b", 0.7)
mel.connect_material_expressions(cen, "", amp, "A")
dit = mel.create_material_expression(m, unreal.MaterialExpressionAdd, -150, 700)
mel.connect_material_expressions(fade, "", dit, "A"); mel.connect_material_expressions(amp, "", dit, "B")
mel.connect_material_property(dit, "", unreal.MaterialProperty.MP_OPACITY_MASK)
m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
m.set_editor_property("two_sided", True)
mel.recompile_material(m); unreal.EditorAssetLibrary.save_loaded_asset(m)
print("CA_MAT", mel.get_num_material_expressions(m))

# ---- garage level ----
L = "/Game/Maps/Garage"
if unreal.EditorAssetLibrary.does_asset_exist(L): unreal.EditorAssetLibrary.delete_asset(L)
ela.new_level(L)
plane = unreal.EditorAssetLibrary.load_asset("/Engine/BasicShapes/Plane")
g = unreal.EditorAssetLibrary.load_asset("/Game/Showroom/M_ShowGround")
fl = ela.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0, 0, 0)); fl.set_actor_label("Floor")
fl.static_mesh_component.set_static_mesh(plane); fl.static_mesh_component.set_material(0, g); fl.set_actor_scale3d(unreal.Vector(300, 300, 1))
sun = ela.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 5000), unreal.Rotator(0, -40, 130)); sun.set_actor_label("Sun")
c = sun.get_component_by_class(unreal.DirectionalLightComponent); c.set_editor_property("atmosphere_sun_light", True); c.set_editor_property("intensity", 4.0)
ela.spawn_actor_from_class(unreal.SkyAtmosphere, unreal.Vector(0, 0, 0))
sl = ela.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 1000)); sl.get_component_by_class(unreal.SkyLightComponent).set_editor_property("real_time_capture", True)
car = ela.spawn_actor_from_class(unreal.DustlineCar, unreal.Vector(0, 0, 0)); car.set_actor_label("Car")
print("CA_CAR", car, car.get_editor_property("setup").get_editor_property("model"))
print("CA_SAVED", ela.save_current_level())
