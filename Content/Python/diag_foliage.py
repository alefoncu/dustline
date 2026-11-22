import unreal
mel = unreal.MaterialEditingLibrary
at = unreal.AssetToolsHelpers.get_asset_tools()
ela = unreal.EditorLevelLibrary
D = "/Game/Vegetation/fir_sapling_medium"
tw = lambda k: unreal.EditorAssetLibrary.load_asset("%s/Tex/fir_sapling_medium_twigs_%s_1k" % (D, k))
br = lambda k: unreal.EditorAssetLibrary.load_asset("%s/Tex/fir_sapling_medium_branches_%s_1k" % (D, k))
pbr = unreal.EditorAssetLibrary.load_asset("/Game/Vegetation/M_VegPBR")

def mi(name, parent, diff, nor, rough, alpha=None):
    p = "/Game/Showroom/" + name
    if unreal.EditorAssetLibrary.does_asset_exist(p): unreal.EditorAssetLibrary.delete_asset(p)
    m = at.create_asset(name, "/Game/Showroom", unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    m.set_editor_property("parent", parent)
    for pn, t in (("Diffuse", diff), ("Normal", nor), ("Rough", rough), ("Alpha", alpha)):
        if t: mel.set_material_instance_texture_parameter_value(m, pn, t)
    mel.set_material_instance_scalar_parameter_value(m, "UseConst", 1.0)
    mel.update_material_instance(m); return m

# C: constant green, masked by the alpha texture (no diffuse texture at all)
cp = "/Game/Showroom/M_ConstGreen"
if unreal.EditorAssetLibrary.does_asset_exist(cp): unreal.EditorAssetLibrary.delete_asset(cp)
c = at.create_asset("M_ConstGreen", "/Game/Showroom", unreal.Material, unreal.MaterialFactoryNew())
col = mel.create_material_expression(c, unreal.MaterialExpressionConstant3Vector, -400, 0); col.set_editor_property("constant", unreal.LinearColor(0.03, 0.12, 0.02, 1))
al = mel.create_material_expression(c, unreal.MaterialExpressionTextureSample, -400, 300); al.set_editor_property("texture", tw("alpha"))
mel.connect_material_property(col, "", unreal.MaterialProperty.MP_BASE_COLOR)
mel.connect_material_property(al, "R", unreal.MaterialProperty.MP_OPACITY_MASK)
c.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED); c.set_editor_property("two_sided", True)
mel.recompile_material(c); unreal.EditorAssetLibrary.save_loaded_asset(c)

# E: unlit emissive of the diffuse texture (shows the raw texture colours regardless of lighting)
ep = "/Game/Showroom/M_RawDiffuse"
if unreal.EditorAssetLibrary.does_asset_exist(ep): unreal.EditorAssetLibrary.delete_asset(ep)
e = at.create_asset("M_RawDiffuse", "/Game/Showroom", unreal.Material, unreal.MaterialFactoryNew())
ts = mel.create_material_expression(e, unreal.MaterialExpressionTextureSample, -400, 0); ts.set_editor_property("texture", tw("diff"))
mel.connect_material_property(ts, "RGB", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
e.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
mel.recompile_material(e); unreal.EditorAssetLibrary.save_loaded_asset(e)

mats = [
  ("A_opaque_twigs", mi("MI_diagA", pbr, tw("diff"), tw("nor_gl"), tw("rough"))),
  ("B_foliage",      unreal.EditorAssetLibrary.load_asset(D + "/MI_fir_sapling_medium_twigs")),
  ("C_const_green",  c),
  ("D_raw_diffuse",  e),
]
ela.load_level('/Game/Maps/Showroom')
for a in ela.get_all_level_actors():
    if a.get_actor_label().startswith("diag_"): ela.destroy_actor(a)
plane = unreal.EditorAssetLibrary.load_asset("/Engine/BasicShapes/Plane")
for k, (label, m) in enumerate(mats):
    a = ela.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0, 3000 + k * 230, 140), unreal.Rotator(0, 90, 0))
    a.set_actor_label("diag_" + label)
    a.static_mesh_component.set_static_mesh(plane); a.static_mesh_component.set_material(0, m)
    a.set_actor_scale3d(unreal.Vector(2.0, 2.0, 2.0))
print("DG_SAVED", ela.save_current_level())
