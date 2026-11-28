# Rocas grandes: la textura propia (1k) se estira al escalarlas. Se añade al master M_VegPBR un detalle de roca
# proyectado en coordenadas del mundo (triplanar, 2,5 m) que multiplica el color, solo donde el parámetro WorldRock > 0.
import unreal
mel = unreal.MaterialEditingLibrary
m = unreal.EditorAssetLibrary.load_asset("/Game/Vegetation/M_VegPBR")
base = mel.get_material_property_input_node(m, unreal.MaterialProperty.MP_BASE_COLOR)
print("RD_BASE", base)
def E(cls, x, y, **kw):
    e = mel.create_material_expression(m, cls, x, y)
    for k, v in kw.items(): e.set_editor_property(k, v)
    return e
def C(a, ao, b, bi): mel.connect_material_expressions(a, ao, b, bi)
wr = E(unreal.MaterialExpressionScalarParameter, -1200, -500, parameter_name="WorldRock", default_value=0.0)
wp = E(unreal.MaterialExpressionWorldPosition, -1600, -800)
vn = E(unreal.MaterialExpressionVertexNormalWS, -1600, -650)
def M(a, r=False, g=False, b=False):
    e = E(unreal.MaterialExpressionComponentMask, -1450, -700, r=r, g=g, b=b, a=False); C(a, "", e, ""); return e
X, Y, Z = M(wp, r=True), M(wp, g=True), M(wp, b=True)
def app(a, b):
    e = E(unreal.MaterialExpressionAppendVector, -1350, -700); C(a, "", e, "A"); C(b, "", e, "B"); return e
rock = unreal.EditorAssetLibrary.load_asset("/Game/Terrain/Textures/T_Rock058_Color")
def samp(uv2):
    sc = E(unreal.MaterialExpressionMultiply, -1250, -700, const_b=1.0 / 250.0); C(uv2, "", sc, "A")
    t = E(unreal.MaterialExpressionTextureSample, -1100, -700, texture=rock, sampler_source=unreal.SamplerSourceMode.SSM_WRAP_WORLD_GROUP_SETTINGS); C(sc, "", t, "Coordinates")
    return t
sx, sy, sz = samp(app(Y, Z)), samp(app(X, Z)), samp(app(X, Y))
an = E(unreal.MaterialExpressionAbs, -1450, -600); C(vn, "", an, "")
a2 = E(unreal.MaterialExpressionMultiply, -1350, -600); C(an, "", a2, "A"); C(an, "", a2, "B")
a4 = E(unreal.MaterialExpressionMultiply, -1250, -600); C(a2, "", a4, "A"); C(a2, "", a4, "B")
wx, wy, wz = M(a4, r=True), M(a4, g=True), M(a4, b=True)
def mul(a, b):
    e = E(unreal.MaterialExpressionMultiply, -950, -650); C(a, "", e, "A"); C(b, "", e, "B"); return e
def add(a, b):
    e = E(unreal.MaterialExpressionAdd, -900, -650); C(a, "", e, "A"); C(b, "", e, "B"); return e
num = add(add(mul(sx, wx), mul(sy, wy)), mul(sz, wz))
den = add(add(wx, wy), wz)
tri = E(unreal.MaterialExpressionDivide, -800, -650); C(num, "", tri, "A"); C(den, "", tri, "B")
# luminancia normalizada (media lineal de Rock058 ~0,088) -> factor de detalle alrededor de 1
lum = E(unreal.MaterialExpressionDotProduct, -700, -650); C(tri, "", lum, "A")
lw = E(unreal.MaterialExpressionConstant3Vector, -800, -550, constant=unreal.LinearColor(0.3, 0.59, 0.11, 1)); C(lw, "", lum, "B")
nrm = E(unreal.MaterialExpressionMultiply, -600, -650, const_b=1.0 / 0.088); C(lum, "", nrm, "A")
cl = E(unreal.MaterialExpressionClamp, -500, -650, min_default=0.35, max_default=1.9); C(nrm, "", cl, "")
f = E(unreal.MaterialExpressionLinearInterpolate, -400, -600, const_a=1.0); C(cl, "", f, "B"); C(wr, "", f, "Alpha")
out = E(unreal.MaterialExpressionMultiply, -250, -300); C(base, "RGB", out, "A"); C(f, "", out, "B")
mel.connect_material_property(out, "", unreal.MaterialProperty.MP_BASE_COLOR)
mel.recompile_material(m); unreal.EditorAssetLibrary.save_loaded_asset(m)
for r in ["rock_09", "rock_07", "stone_01", "boulder_01"]:
    for a in unreal.EditorAssetLibrary.list_assets("/Game/Vegetation/%s" % r, recursive=False):
        o = unreal.EditorAssetLibrary.load_asset(a)
        if isinstance(o, unreal.MaterialInstanceConstant):
            mel.set_material_instance_scalar_parameter_value(o, "WorldRock", 0.75)
            mel.update_material_instance(o); unreal.EditorAssetLibrary.save_loaded_asset(o)
            print("RD_MI", a)
print("RD_OK")
