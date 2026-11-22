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
snow_line = scalar("SnowLineCm", 26000.0)

TILES = {"Grass004": 380.0, "Ground071": 450.0, "Ground106": 450.0, "Ground080": 520.0, "Rock058": 700.0, "Snow014": 450.0}
layers = {}
for i, tile in TILES.items():
    uv = mul(xy, None, 1.0 / tile)
    uv2 = add(mul(xy, None, 1.0 / (tile * 7.3)), None, 0.37)
    c1 = tex("/Game/Terrain/Textures/T_%s_Color" % i, uv)
    c2 = tex("/Game/Terrain/Textures/T_%s_Color" % i, uv2)
    col = lerp(c1, c2, const(0.35))
    if i == "Rock058":   # source rock is blue-grey and too bright in sun: warm it up and darken a little
        rv = node(unreal.MaterialExpressionConstant3Vector); rv.set_editor_property("constant", unreal.LinearColor(0.78, 0.72, 0.64, 1))
        col = mul(col, rv)
    if i == "Snow014": col = mul(col, None, 0.85)
    if i == "Grass004":   # too saturated / lime: darker olive
        gv = node(unreal.MaterialExpressionConstant3Vector); gv.set_editor_property("constant", unreal.LinearColor(0.60, 0.66, 0.46, 1)); col = mul(col, gv)
    if i == "Ground071":  # dirt patches too orange
        dv = node(unreal.MaterialExpressionConstant3Vector); dv.set_editor_property("constant", unreal.LinearColor(0.72, 0.66, 0.60, 1)); col = mul(col, dv)
    nm = tex("/Game/Terrain/Textures/T_%s_Normal" % i, uv, normal=True)
    ro = mask(tex("/Game/Terrain/Textures/T_%s_Roughness" % i, uv), r=True)
    layers[i] = (col, nm, ro)

# --- weights ---
slope = one_minus(nz)                                  # 0 flat .. 1 vertical
w_rock = smooth(slope, 0.22, 0.42)
n_dirt = noise(wp, 0.0007, 0.0)
w_dirt = mul(smooth(n_dirt, 0.52, 0.64), one_minus(w_rock))
n_mud = noise(wp, 0.0011, 5000.0)
low = smooth(sub(None, z, ca=3000.0), 0.0, 1800.0)      # lowlands (below ~30 m)
mud_perm = mul(smooth(n_mud, 0.60, 0.70), low)
mud_wet = mul(mul(wet_amt, None, 0.9), low)
w_mud = mul(sat(add(mud_perm, mud_wet)), one_minus(w_rock))
w_sand = smooth(sub(None, z, ca=300.0), 0.0, 280.0)     # shoreline band around water level
alt_snow = smooth_dyn(z, snow_line, add(snow_line, None, 4000.0))
flat = smooth(nz, 0.62, 0.82)
w_snow = sat(add(mul(alt_snow, add(flat, None, 0.25)), mul(snow_amt, flat)))

def blend(idx):
    base = layers["Grass004"][idx]
    base = lerp(base, layers["Ground071"][idx], w_dirt)
    base = lerp(base, layers["Ground106"][idx], w_mud)
    base = lerp(base, layers["Ground080"][idx], w_sand)
    base = lerp(base, layers["Rock058"][idx], w_rock)
    base = lerp(base, layers["Snow014"][idx], w_snow)
    return base
col = blend(0); nrm = blend(1); rough = blend(2)

# wetness: darker, glossier ground (not on snow)
wet_eff = mul(wet_amt, one_minus(w_snow))
col = mul(col, lerp(const(1.0), const(0.62), wet_eff))
rough = mul(rough, lerp(const(1.0), const(0.55), wet_eff))
nrm_n = node(unreal.MaterialExpressionNormalize); conn(nrm, "", nrm_n, "")

mel.connect_material_property(col, "", unreal.MaterialProperty.MP_BASE_COLOR)
mel.connect_material_property(nrm_n, "", unreal.MaterialProperty.MP_NORMAL)
mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
mat.set_editor_property("used_with_nanite", False)
mel.recompile_material(mat)
unreal.EditorAssetLibrary.save_loaded_asset(mat)
print("MAT_DONE", mel.get_num_material_expressions(mat))

# apply to the landscape in the level
unreal.EditorLevelLibrary.load_level('/Game/Maps/Dustline_World')
for a in unreal.EditorLevelLibrary.get_all_level_actors():
    if a.get_class().get_name() == "Landscape":
        a.set_editor_property("landscape_material", mat)
        print("MAT_APPLIED", a.get_actor_label())
print("MAT_SAVED", unreal.EditorLevelLibrary.save_current_level())
