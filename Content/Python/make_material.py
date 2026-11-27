import unreal
mel = unreal.MaterialEditingLibrary
at = unreal.AssetToolsHelpers.get_asset_tools()
PATH, NAME = "/Game/Terrain", "M_Terrain"
unreal.EditorAssetLibrary.make_directory(PATH)
if unreal.EditorAssetLibrary.does_asset_exist(PATH + "/" + NAME):
    unreal.EditorAssetLibrary.delete_asset(PATH + "/" + NAME)
mat = at.create_asset(NAME, PATH, unreal.Material, unreal.MaterialFactoryNew())

X = [0]
def node(cls, x=None, y=0):
    X[0] += 1
    return mel.create_material_expression(mat, cls, -1600 + (X[0] % 20) * 60 if x is None else x, y + (X[0] // 20) * 120)
def conn(a, ao, b, bi): mel.connect_material_expressions(a, ao, b, bi)
def const(v):
    n = node(unreal.MaterialExpressionConstant); n.set_editor_property("r", v); return n
def scalar(name, v):
    n = node(unreal.MaterialExpressionScalarParameter); n.set_editor_property("parameter_name", name); n.set_editor_property("default_value", v); return n
def mul(a, b=None, c=None, ao=""):
    n = node(unreal.MaterialExpressionMultiply); conn(a, ao, n, "A")
    if b is not None: conn(b, "", n, "B")
    else: n.set_editor_property("const_b", c)
    return n
def add(a, b=None, c=None):
    n = node(unreal.MaterialExpressionAdd); conn(a, "", n, "A")
    if b is not None: conn(b, "", n, "B")
    else: n.set_editor_property("const_b", c)
    return n
def sub(a, b=None, c=None, ca=None):
    n = node(unreal.MaterialExpressionSubtract)
    if a is not None: conn(a, "", n, "A")
    else: n.set_editor_property("const_a", ca)
    if b is not None: conn(b, "", n, "B")
    else: n.set_editor_property("const_b", c)
    return n
def lerp(a, b, alpha):
    n = node(unreal.MaterialExpressionLinearInterpolate)
    conn(a, "", n, "A"); conn(b, "", n, "B"); conn(alpha, "", n, "Alpha"); return n
def sat(a):
    n = node(unreal.MaterialExpressionSaturate); conn(a, "", n, ""); return n
def one_minus(a):
    n = node(unreal.MaterialExpressionOneMinus); conn(a, "", n, ""); return n
def smooth(value, lo, hi):
    n = node(unreal.MaterialExpressionSmoothStep)
    conn(value, "", n, "Value"); n.set_editor_property("const_min", lo); n.set_editor_property("const_max", hi); return n
def smooth_dyn(value, lo_node, hi_node):
    n = node(unreal.MaterialExpressionSmoothStep)
    conn(value, "", n, "Value"); conn(lo_node, "", n, "Min"); conn(hi_node, "", n, "Max"); return n
def mask(a, r=False, g=False, b=False, ao=""):
    n = node(unreal.MaterialExpressionComponentMask)
    n.set_editor_property("r", r); n.set_editor_property("g", g); n.set_editor_property("b", b); n.set_editor_property("a", False)
    conn(a, ao, n, ""); return n
def noise(pos, scale, seed_off=0.0):
    p = add(pos, None, seed_off) if seed_off else pos
    n = node(unreal.MaterialExpressionNoise)
    n.set_editor_property("scale", scale); n.set_editor_property("quality", 1)
    n.set_editor_property("levels", 4); n.set_editor_property("output_min", 0.0); n.set_editor_property("output_max", 1.0)
    n.set_editor_property("level_scale", 2.0)
    n.set_editor_property("noise_function", unreal.NoiseFunction.NOISEFUNCTION_GRADIENT_ALU)
    conn(p, "", n, "Position"); return n
def tex(path, uv, normal=False):
    t = unreal.EditorAssetLibrary.load_asset(path)
    n = node(unreal.MaterialExpressionTextureSample)
    n.set_editor_property("texture", t)
    n.set_editor_property("sampler_source", unreal.SamplerSourceMode.SSM_WRAP_WORLD_GROUP_SETTINGS)
    if normal: n.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    conn(uv, "", n, "Coordinates"); return n

wp = node(unreal.MaterialExpressionWorldPosition)
xy = mask(wp, r=True, g=True)
z = mask(wp, b=True)
vn = node(unreal.MaterialExpressionVertexNormalWS)
nz = mask(vn, b=True)

snow_amt = scalar("SnowAmount", 0.0)
wet_amt = scalar("Wetness", 0.0)

# --- máscara horneada (misma que usa la física: DustlineWorld) y estado del clima (textura 64x64 que actualiza ADustlineWeather) ---
HALF = 201600.0
uvw = mul(add(xy, None, HALF), None, 1.0 / (2 * HALF))
mtex = node(unreal.MaterialExpressionTextureSample)
mtex.set_editor_property("texture", unreal.EditorAssetLibrary.load_asset("/Game/Terrain/T_SurfaceMask"))
mtex.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
mtex.set_editor_property("sampler_source", unreal.SamplerSourceMode.SSM_CLAMP_WORLD_GROUP_SETTINGS)
conn(uvw, "", mtex, "Coordinates")
wtex = node(unreal.MaterialExpressionTextureSampleParameter2D)
wtex.set_editor_property("parameter_name", "WeatherTex")
wtex.set_editor_property("texture", unreal.EditorAssetLibrary.load_asset("/Game/Weather/T_WeatherDefault"))
wtex.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
wtex.set_editor_property("sampler_source", unreal.SamplerSourceMode.SSM_CLAMP_WORLD_GROUP_SETTINGS)
conn(uvw, "", wtex, "Coordinates")
def ch(n, c):
    m = node(unreal.MaterialExpressionComponentMask)
    for k in "rgba": m.set_editor_property(k, k == c)
    conn(n, "RGBA", m, ""); return m
m_dirt, m_mud, m_tall, m_grav = ch(mtex, "r"), ch(mtex, "g"), ch(mtex, "b"), ch(mtex, "a")
wx_wet, wx_snow, wx_ice = ch(wtex, "r"), ch(wtex, "g"), ch(wtex, "b")

TILES = {"Grass004": 380.0, "Ground071": 450.0, "Ground106": 450.0, "Ground080": 520.0, "Rock058": 700.0, "Snow014": 450.0}
layers = {}
for i, tile in TILES.items():
    uv = mul(xy, None, 1.0 / tile)
    uv2 = add(mul(xy, None, 1.0 / (tile * 7.3)), None, 0.37)
    c1 = tex("/Game/Terrain/Textures/T_%s_Color" % i, uv)
    c2 = tex("/Game/Terrain/Textures/T_%s_Color" % i, uv2)
    col = lerp(c1, c2, const(0.35))
    if i == "Rock058":
        rv = node(unreal.MaterialExpressionConstant3Vector); rv.set_editor_property("constant", unreal.LinearColor(0.78, 0.72, 0.64, 1)); col = mul(col, rv)
    if i == "Snow014": col = mul(col, None, 0.85)
    if i == "Grass004":
        gv = node(unreal.MaterialExpressionConstant3Vector); gv.set_editor_property("constant", unreal.LinearColor(0.60, 0.66, 0.46, 1)); col = mul(col, gv)
        # pradera de hierba alta: más pajiza y oscura
        tv = node(unreal.MaterialExpressionConstant3Vector); tv.set_editor_property("constant", unreal.LinearColor(0.80, 0.70, 0.42, 1))
        col = lerp(col, mul(col, tv), mul(m_tall, None, 0.7))
    if i == "Ground071":
        dv = node(unreal.MaterialExpressionConstant3Vector); dv.set_editor_property("constant", unreal.LinearColor(0.72, 0.66, 0.60, 1)); col = mul(col, dv)
    nm = tex("/Game/Terrain/Textures/T_%s_Normal" % i, uv, normal=True)
    ro = mask(tex("/Game/Terrain/Textures/T_%s_Roughness" % i, uv), r=True)
    layers[i] = (col, nm, ro)
# grava: la roca a escala pequeña (canto rodado) mezclada con la arena
uvg = mul(xy, None, 1.0 / 160.0)
gc = tex("/Game/Terrain/Textures/T_Rock058_Color", uvg)
gt = node(unreal.MaterialExpressionConstant3Vector); gt.set_editor_property("constant", unreal.LinearColor(0.62, 0.60, 0.56, 1))
gcol = lerp(mul(gc, gt), layers["Ground080"][0], const(0.35))
gnm = tex("/Game/Terrain/Textures/T_Rock058_Normal", uvg, normal=True)
gro = mask(tex("/Game/Terrain/Textures/T_Rock058_Roughness", uvg), r=True)
layers["Gravel"] = (gcol, gnm, gro)

# --- pesos: EXACTAMENTE los de DustlineWorld::GroundAt ---
slope = one_minus(nz)
w_rock = smooth(slope, 0.22, 0.42)
flat = smooth(nz, 0.80, 0.95)
wet = sat(add(wx_wet, wet_amt))
a_dirt = mul(m_dirt, one_minus(w_rock))
a_mud = mul(sat(add(m_mud, mul(mul(wet, add(mul(m_dirt, None, 0.45), None, 0.15)), flat))), one_minus(w_rock))
a_sand = smooth(sub(None, z, ca=300.0), 0.0, 280.0)
a_grav = mul(m_grav, sub(None, mul(w_rock, None, 0.5), ca=1.0))
snow_m = add(mul(wx_snow, None, 0.5), mul(snow_amt, None, 0.3))
a_snow = mul(smooth(snow_m, 0.015, 0.07), add(mul(smooth(nz, 0.55, 0.8), None, 0.65), None, 0.35))
a_ice = mul(mul(wx_ice, one_minus(a_snow)), smooth(nz, 0.7, 0.9))

def blend(idx):
    base = layers["Grass004"][idx]
    base = lerp(base, layers["Ground071"][idx], a_dirt)
    base = lerp(base, layers["Ground106"][idx], a_mud)
    base = lerp(base, layers["Ground080"][idx], a_sand)
    base = lerp(base, layers["Gravel"][idx], a_grav)
    base = lerp(base, layers["Rock058"][idx], w_rock)
    base = lerp(base, layers["Snow014"][idx], a_snow)
    return base
col = blend(0); nrm = blend(1); rough = blend(2)

# humedad: suelo más oscuro y brillante; charcos en llano embarrado; hielo liso y vidrioso
wet_eff = mul(wet, one_minus(a_snow))
col = mul(col, lerp(const(1.0), const(0.62), wet_eff))
rough = mul(rough, lerp(const(1.0), const(0.55), wet_eff))
puddle = mul(mul(smooth(wet, 0.55, 0.9), smooth(nz, 0.965, 0.995)), sat(add(a_mud, None, 0.25)))
puddle = mul(puddle, one_minus(a_snow))
col = lerp(col, mul(col, None, 0.45), puddle)
rough = lerp(rough, const(0.03), puddle)
upv = node(unreal.MaterialExpressionConstant3Vector); upv.set_editor_property("constant", unreal.LinearColor(0, 0, 1, 1))
nrm = lerp(nrm, upv, puddle)
icec = node(unreal.MaterialExpressionConstant3Vector); icec.set_editor_property("constant", unreal.LinearColor(0.10, 0.13, 0.16, 1))
col = lerp(col, add(mul(col, None, 0.45), icec), a_ice)
rough = lerp(rough, const(0.06), a_ice)
nrm = lerp(nrm, upv, mul(a_ice, None, 0.7))
nrm_n = node(unreal.MaterialExpressionNormalize); conn(nrm, "", nrm_n, "")

mel.connect_material_property(col, "", unreal.MaterialProperty.MP_BASE_COLOR)
mel.connect_material_property(nrm_n, "", unreal.MaterialProperty.MP_NORMAL)
mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
mat.set_editor_property("used_with_nanite", False)
mel.recompile_material(mat)
unreal.EditorAssetLibrary.save_loaded_asset(mat)
print("MAT_DONE", mel.get_num_material_expressions(mat))

# apply to the landscape in the level (build_world.py lo aplica él mismo)
if not globals().get("DUSTLINE_NO_APPLY"):
    unreal.EditorLevelLibrary.load_level('/Game/Maps/Dustline_World')
    for a in unreal.EditorLevelLibrary.get_all_level_actors():
        if a.get_class().get_name() == "Landscape":
            a.set_editor_property("landscape_material", mat)
            a.set_editor_property("use_dynamic_material_instance", True)
            print("MAT_APPLIED", a.get_actor_label())
    print("MAT_SAVED", unreal.EditorLevelLibrary.save_current_level())
