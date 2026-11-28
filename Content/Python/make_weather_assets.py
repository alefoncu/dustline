# Recursos del clima y de la hierba alta: texturas de lluvia/nieve, M_Precip (cortinas de precipitación), M_Grass y las mallas de hierba.
import unreal
mel = unreal.MaterialEditingLibrary
at = unreal.AssetToolsHelpers.get_asset_tools()
EAL = unreal.EditorAssetLibrary
DTL = unreal.DustlineTerrainLibrary
EAL.make_directory("/Game/Weather"); EAL.make_directory("/Game/Vegetation/Grass")

def imp(name, linear=False, mips=True):
    t = unreal.AssetImportTask()
    t.filename = "/Volumes/SSD/Weather-Setup/%s.png" % name; t.destination_path = "/Game/Weather"; t.destination_name = name
    t.automated = True; t.save = False; t.replace_existing = True
    at.import_asset_tasks([t])
    tex = EAL.load_asset("/Game/Weather/" + name)
    tex.set_editor_property("srgb", False)
    if linear: tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_VECTOR_DISPLACEMENTMAP)
    if not mips: tex.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    EAL.save_loaded_asset(tex); return tex
rain = imp("T_Rain"); snow = imp("T_Snow"); imp("T_WeatherDefault", linear=True, mips=False); puff = imp("T_Puff"); rut = imp("T_Rut")

def new_mat(path, name):
    if EAL.does_asset_exist(path + "/" + name): EAL.delete_asset(path + "/" + name)
    return at.create_asset(name, path, unreal.Material, unreal.MaterialFactoryNew())

# ---- M_Precip ----
m = new_mat("/Game/Weather", "M_Precip")
m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
m.set_editor_property("two_sided", True)
m.set_editor_property("use_translucency_vertex_fog", False)   # la niebla no se "come" la lluvia que cae delante de la cámara
def N(mat, cls, x=0, y=0): return mel.create_material_expression(mat, cls, x, y)
def P(mat, name, v):
    n = N(mat, unreal.MaterialExpressionScalarParameter); n.set_editor_property("parameter_name", name); n.set_editor_property("default_value", v); return n
tc = N(m, unreal.MaterialExpressionTextureCoordinate)
tm = N(m, unreal.MaterialExpressionTime)
ap = N(m, unreal.MaterialExpressionAppendVector)
mel.connect_material_expressions(P(m, "SpeedU", 0.0), "", ap, "A"); mel.connect_material_expressions(P(m, "SpeedV", 3.0), "", ap, "B")
mt = N(m, unreal.MaterialExpressionMultiply); mel.connect_material_expressions(tm, "", mt, "A"); mel.connect_material_expressions(ap, "", mt, "B")
uv = N(m, unreal.MaterialExpressionAdd); mel.connect_material_expressions(tc, "", uv, "A"); mel.connect_material_expressions(mt, "", uv, "B")
ts = N(m, unreal.MaterialExpressionTextureSampleParameter2D); ts.set_editor_property("parameter_name", "Tex"); ts.set_editor_property("texture", rain)
ts.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
mel.connect_material_expressions(uv, "", ts, "Coordinates")
tint = N(m, unreal.MaterialExpressionVectorParameter); tint.set_editor_property("parameter_name", "Tint"); tint.set_editor_property("default_value", unreal.LinearColor(0.6, 0.65, 0.7, 1))
em = N(m, unreal.MaterialExpressionMultiply); mel.connect_material_expressions(ts, "R", em, "A"); mel.connect_material_expressions(tint, "", em, "B")
op = N(m, unreal.MaterialExpressionMultiply); mel.connect_material_expressions(ts, "R", op, "A"); mel.connect_material_expressions(P(m, "Opacity", 0.5), "", op, "B")
mel.connect_material_property(em, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
mel.connect_material_property(op, "", unreal.MaterialProperty.MP_OPACITY)
mel.recompile_material(m); EAL.save_loaded_asset(m)

# ---- M_Grass: hojas opacas a dos caras, color por altura de la hoja (color de vértice R) y tono por hoja (G), viento por WPO ----
g = new_mat("/Game/Vegetation/Grass", "M_Grass")
g.set_editor_property("two_sided", True)
vc = N(g, unreal.MaterialExpressionVertexColor)
def C3(mat, r, gg, b):
    n = N(mat, unreal.MaterialExpressionConstant3Vector); n.set_editor_property("constant", unreal.LinearColor(r, gg, b, 1)); return n
base = C3(g, 0.035, 0.045, 0.012); tip = C3(g, 0.20, 0.21, 0.07); tip2 = C3(g, 0.26, 0.22, 0.09)
tipmix = N(g, unreal.MaterialExpressionLinearInterpolate); mel.connect_material_expressions(tip, "", tipmix, "A"); mel.connect_material_expressions(tip2, "", tipmix, "B"); mel.connect_material_expressions(vc, "G", tipmix, "Alpha")
col = N(g, unreal.MaterialExpressionLinearInterpolate); mel.connect_material_expressions(base, "", col, "A"); mel.connect_material_expressions(tipmix, "", col, "B"); mel.connect_material_expressions(vc, "R", col, "Alpha")
mel.connect_material_property(col, "", unreal.MaterialProperty.MP_BASE_COLOR)
r = N(g, unreal.MaterialExpressionConstant); r.set_editor_property("r", 0.78); mel.connect_material_property(r, "", unreal.MaterialProperty.MP_ROUGHNESS)
sp = N(g, unreal.MaterialExpressionConstant); sp.set_editor_property("r", 0.25); mel.connect_material_property(sp, "", unreal.MaterialProperty.MP_SPECULAR)
# viento: desplazamiento ~ sin(t*1.6 + (x+y)/250) * 7 cm * altura^2
wp = N(g, unreal.MaterialExpressionWorldPosition)
mx = N(g, unreal.MaterialExpressionComponentMask); mx.set_editor_property("r", True); mx.set_editor_property("g", False); mx.set_editor_property("b", False); mx.set_editor_property("a", False); mel.connect_material_expressions(wp, "", mx, "")
my = N(g, unreal.MaterialExpressionComponentMask); my.set_editor_property("r", False); my.set_editor_property("g", True); my.set_editor_property("b", False); my.set_editor_property("a", False); mel.connect_material_expressions(wp, "", my, "")
sxy = N(g, unreal.MaterialExpressionAdd); mel.connect_material_expressions(mx, "", sxy, "A"); mel.connect_material_expressions(my, "", sxy, "B")
k1 = N(g, unreal.MaterialExpressionMultiply); mel.connect_material_expressions(sxy, "", k1, "A"); k1.set_editor_property("const_b", 0.0025)
t2 = N(g, unreal.MaterialExpressionTime); k2 = N(g, unreal.MaterialExpressionMultiply); mel.connect_material_expressions(t2, "", k2, "A"); k2.set_editor_property("const_b", 0.27)
ph = N(g, unreal.MaterialExpressionAdd); mel.connect_material_expressions(k1, "", ph, "A"); mel.connect_material_expressions(k2, "", ph, "B")
sn = N(g, unreal.MaterialExpressionSine); mel.connect_material_expressions(ph, "", sn, "")
h2 = N(g, unreal.MaterialExpressionMultiply); mel.connect_material_expressions(vc, "R", h2, "A"); mel.connect_material_expressions(vc, "R", h2, "B")
amp = N(g, unreal.MaterialExpressionMultiply); mel.connect_material_expressions(sn, "", amp, "A"); mel.connect_material_expressions(h2, "", amp, "B")
amp2 = N(g, unreal.MaterialExpressionMultiply); mel.connect_material_expressions(amp, "", amp2, "A"); amp2.set_editor_property("const_b", 7.0)
dirv = C3(g, 0.8, 0.6, 0.0)
off = N(g, unreal.MaterialExpressionMultiply); mel.connect_material_expressions(amp2, "", off, "A"); mel.connect_material_expressions(dirv, "", off, "B")
mel.connect_material_property(off, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
g.set_editor_property("used_with_instanced_static_meshes", True)
mel.recompile_material(g); EAL.save_loaded_asset(g)

# ---- M_Fx: partículas (salpicaduras, barro, nieve, polvo): color y opacidad por vértice ----
fx = new_mat("/Game/Weather", "M_Fx")
fx.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
fx.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
fx.set_editor_property("two_sided", True)
fvc = N(fx, unreal.MaterialExpressionVertexColor)
ft = N(fx, unreal.MaterialExpressionTextureSample); ft.set_editor_property("texture", puff); ft.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
mel.connect_material_expressions(N(fx, unreal.MaterialExpressionTextureCoordinate), "", ft, "Coordinates")
fe = N(fx, unreal.MaterialExpressionMultiply); mel.connect_material_expressions(fvc, "", fe, "A"); mel.connect_material_expressions(P(fx, "Light", 1.0), "", fe, "B")
mel.connect_material_property(fe, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
fo = N(fx, unreal.MaterialExpressionMultiply); mel.connect_material_expressions(ft, "R", fo, "A"); mel.connect_material_expressions(fvc, "A", fo, "B")
mel.connect_material_property(fo, "", unreal.MaterialProperty.MP_OPACITY)
mel.recompile_material(fx); EAL.save_loaded_asset(fx)

# ---- M_Rut: rodadas, modulan (oscurecen) el suelo con la huella de los tacos ----
ru = new_mat("/Game/Weather", "M_Rut")
ru.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MODULATE)
ru.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
ru.set_editor_property("use_translucency_vertex_fog", False)
rvc = N(ru, unreal.MaterialExpressionVertexColor)
rt = N(ru, unreal.MaterialExpressionTextureSample); rt.set_editor_property("texture", rut); rt.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
mel.connect_material_expressions(N(ru, unreal.MaterialExpressionTextureCoordinate), "", rt, "Coordinates")
ra = N(ru, unreal.MaterialExpressionMultiply); mel.connect_material_expressions(rt, "R", ra, "A"); mel.connect_material_expressions(rvc, "A", ra, "B")
one = N(ru, unreal.MaterialExpressionConstant3Vector); one.set_editor_property("constant", unreal.LinearColor(1, 1, 1, 1))
rl = N(ru, unreal.MaterialExpressionLinearInterpolate); mel.connect_material_expressions(one, "", rl, "A"); mel.connect_material_expressions(rvc, "", rl, "B"); mel.connect_material_expressions(ra, "", rl, "Alpha")
mel.connect_material_property(rl, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
mel.recompile_material(ru); EAL.save_loaded_asset(ru)

for k, (seed, h, blades) in enumerate([(11, 85.0, 30), (23, 105.0, 26), (37, 65.0, 34)]):
    p = "/Game/Vegetation/Grass/SM_TallGrass_%d" % k
    if EAL.does_asset_exist(p): EAL.delete_asset(p)
    sm = DTL.create_procedural_grass("/Game/Vegetation/Grass", "SM_TallGrass_%d" % k, seed, h, blades, g)
    print("WX_GRASS", p, sm.get_num_triangles(0) if sm else None)
    EAL.save_loaded_asset(sm)
print("WX_DONE")
