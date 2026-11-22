import unreal
mel = unreal.MaterialEditingLibrary
at = unreal.AssetToolsHelpers.get_asset_tools()
unreal.EditorAssetLibrary.make_directory("/Game/Water")
# normal texture
t = unreal.AssetImportTask()
t.filename = "/Volumes/SSD/Water-Setup/T_Water_Waves_N.png"; t.destination_path = "/Game/Water"; t.destination_name = "T_Water_Waves_N"
t.automated = True; t.save = False; t.replace_existing = True
at.import_asset_tasks([t])
tex = unreal.EditorAssetLibrary.load_asset("/Game/Water/T_Water_Waves_N")
tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
tex.set_editor_property("srgb", False)
unreal.EditorAssetLibrary.save_loaded_asset(tex)

NAME = "M_Water"
if unreal.EditorAssetLibrary.does_asset_exist("/Game/Water/" + NAME):
    unreal.EditorAssetLibrary.delete_asset("/Game/Water/" + NAME)
m = at.create_asset(NAME, "/Game/Water", unreal.Material, unreal.MaterialFactoryNew())
m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_SINGLE_LAYER_WATER)

def N(cls, x=0, y=0): return mel.create_material_expression(m, cls, x, y)
def C3(r, g, b, x=0, y=0):
    n = N(unreal.MaterialExpressionConstant3Vector, x, y); n.set_editor_property("constant", unreal.LinearColor(r, g, b, 1)); return n
def S(v, x=0, y=0):
    n = N(unreal.MaterialExpressionConstant, x, y); n.set_editor_property("r", v); return n
wp = N(unreal.MaterialExpressionWorldPosition, -1400, 0)
xy = N(unreal.MaterialExpressionComponentMask, -1200, 0)
xy.set_editor_property("r", True); xy.set_editor_property("g", True)
mel.connect_material_expressions(wp, "", xy, "")
def layer(scale, sx, sy, y):
    mul = N(unreal.MaterialExpressionMultiply, -1000, y); mul.set_editor_property("const_b", 1.0 / scale)
    mel.connect_material_expressions(xy, "", mul, "A")
    pan = N(unreal.MaterialExpressionPanner, -800, y)
    pan.set_editor_property("speed_x", sx); pan.set_editor_property("speed_y", sy)
    mel.connect_material_expressions(mul, "", pan, "Coordinate")
    ts = N(unreal.MaterialExpressionTextureSample, -600, y)
    ts.set_editor_property("texture", tex)
    ts.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    ts.set_editor_property("sampler_source", unreal.SamplerSourceMode.SSM_WRAP_WORLD_GROUP_SETTINGS)
    mel.connect_material_expressions(pan, "", ts, "Coordinates")
    return ts
a = layer(2400.0, 0.012, 0.007, -200)
b = layer(850.0, -0.010, 0.016, 100)
add = N(unreal.MaterialExpressionAdd, -400, -50)
mel.connect_material_expressions(a, "", add, "A"); mel.connect_material_expressions(b, "", add, "B")
nz = N(unreal.MaterialExpressionNormalize, -250, -50); mel.connect_material_expressions(add, "", nz, "")
mel.connect_material_property(nz, "", unreal.MaterialProperty.MP_NORMAL)
mel.connect_material_property(C3(0.01, 0.07, 0.09), "", unreal.MaterialProperty.MP_BASE_COLOR)
mel.connect_material_property(S(0.04), "", unreal.MaterialProperty.MP_ROUGHNESS)
mel.connect_material_property(S(0.5), "", unreal.MaterialProperty.MP_SPECULAR)
out = N(unreal.MaterialExpressionSingleLayerWaterMaterialOutput, 200, 300)
mel.connect_material_expressions(C3(0.004, 0.012, 0.014, -100, 300), "", out, "ScatteringCoefficients")
mel.connect_material_expressions(C3(0.0090, 0.0025, 0.0016, -100, 400), "", out, "AbsorptionCoefficients")
mel.connect_material_expressions(S(0.4, -100, 500), "", out, "PhaseG")
mel.recompile_material(m)
unreal.EditorAssetLibrary.save_loaded_asset(m)
print("WATER_DONE", mel.get_num_material_expressions(m))
