# M_CarGeneric: material de todas las piezas del coche (color, metal, rugosidad, emisivo, Fade de rayos X)
# + capa de barro: Dirt (0..1) cubre de barro lo que queda por debajo de DirtH (cm sobre el suelo, espacio local),
#   con manchas de ruido fijas a la carrocería (no "nadan" al moverse).
import unreal
mel = unreal.MaterialEditingLibrary
at = unreal.AssetToolsHelpers.get_asset_tools()
P = "/Game/Cars/M_CarGeneric"
if unreal.EditorAssetLibrary.does_asset_exist(P): unreal.EditorAssetLibrary.delete_asset(P)
m = at.create_asset("M_CarGeneric", "/Game/Cars", unreal.Material, unreal.MaterialFactoryNew())
def E(cls, x, y, **kw):
    e = mel.create_material_expression(m, cls, x, y)
    for k, v in kw.items(): e.set_editor_property(k, v)
    return e
def vec(n, d, x, y): return E(unreal.MaterialExpressionVectorParameter, x, y, parameter_name=n, default_value=d)
def sc(n, d, x, y): return E(unreal.MaterialExpressionScalarParameter, x, y, parameter_name=n, default_value=d)
def C(a, ao, b, bi): mel.connect_material_expressions(a, ao, b, bi)
color = vec("Color", unreal.LinearColor(0.5, 0.5, 0.5, 1), -1400, 0)
metal = sc("Metallic", 0.0, -1400, 200); rough = sc("Roughness", 0.5, -1400, 300)
emis = sc("Emissive", 0.0, -1400, 450); fade = sc("Fade", 1.0, -1400, 600)
dirt = sc("Dirt", 0.0, -1400, 1000); dirth = sc("DirtH", 45.0, -1400, 1100)
mud = vec("DirtColor", unreal.LinearColor(0.075, 0.058, 0.040, 1), -1400, 1200)

# posición local (cm, origen en el suelo bajo el coche para la carrocería)
wp = E(unreal.MaterialExpressionWorldPosition, -1400, 1400)
lp = E(unreal.MaterialExpressionTransformPosition, -1200, 1400,
       transform_source_type=unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_WORLD,
       transform_type=unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_LOCAL)
C(wp, "", lp, "")
lz = E(unreal.MaterialExpressionComponentMask, -1000, 1400, r=False, g=False, b=True, a=False); C(lp, "", lz, "")
# altura: 1 por debajo de DirtH, se desvanece en 30 cm por encima
dz = E(unreal.MaterialExpressionSubtract, -850, 1300); C(lz, "", dz, "A"); C(dirth, "", dz, "B")
dzs = E(unreal.MaterialExpressionDivide, -700, 1300, const_b=30.0); C(dz, "", dzs, "A")
hsat = E(unreal.MaterialExpressionSaturate, -560, 1300); C(dzs, "", hsat, "")
h = E(unreal.MaterialExpressionOneMinus, -430, 1300); C(hsat, "", h, "")
# manchas: ruido grande (salpicaduras) + fino (grano)
def noise(scale, x, y):
    n = E(unreal.MaterialExpressionNoise, x, y, scale=scale, quality=1, levels=2, output_min=0.0, output_max=1.0,
          noise_function=unreal.NoiseFunction.NOISEFUNCTION_GRADIENT_ALU)
    C(lp, "", n, "Position"); return n
n1 = noise(0.035, -850, 1550); n2 = noise(0.22, -850, 1700)
nn = E(unreal.MaterialExpressionLinearInterpolate, -650, 1600, const_alpha=0.35); C(n1, "", nn, "A"); C(n2, "", nn, "B")
# máscara = saturate(Dirt*1.6 * h * (0.35 + nn) + Dirt*0.25*nn)  (siempre algo de polvo por todo cuando está sucio)
a1 = E(unreal.MaterialExpressionAdd, -480, 1600, const_b=0.35); C(nn, "", a1, "A")
m1 = E(unreal.MaterialExpressionMultiply, -330, 1450); C(h, "", m1, "A"); C(a1, "", m1, "B")
d16 = E(unreal.MaterialExpressionMultiply, -480, 1100, const_b=1.6); C(dirt, "", d16, "A")
m2 = E(unreal.MaterialExpressionMultiply, -200, 1400); C(m1, "", m2, "A"); C(d16, "", m2, "B")
d25 = E(unreal.MaterialExpressionMultiply, -330, 1700, const_b=0.25); C(dirt, "", d25, "A")
m3 = E(unreal.MaterialExpressionMultiply, -200, 1650); C(d25, "", m3, "A"); C(nn, "", m3, "B")
s1 = E(unreal.MaterialExpressionAdd, -80, 1500); C(m2, "", s1, "A"); C(m3, "", s1, "B")
mask = E(unreal.MaterialExpressionSaturate, 40, 1500); C(s1, "", mask, "")
# barro: color con algo de variación (seco más claro arriba)
mudv = E(unreal.MaterialExpressionMultiply, -1100, 1250); C(mud, "", mudv, "A")
mv = E(unreal.MaterialExpressionAdd, -1250, 1250, const_b=0.7); C(n2, "", mv, "A"); C(mv, "", mudv, "B")
bc = E(unreal.MaterialExpressionLinearInterpolate, 200, 0); C(color, "", bc, "A"); C(mudv, "", bc, "B"); C(mask, "", bc, "Alpha")
rg = E(unreal.MaterialExpressionLinearInterpolate, 200, 300, const_b=0.93); C(rough, "", rg, "A"); C(mask, "", rg, "Alpha")
mt = E(unreal.MaterialExpressionLinearInterpolate, 200, 200, const_b=0.0); C(metal, "", mt, "A"); C(mask, "", mt, "Alpha")
mel.connect_material_property(bc, "", unreal.MaterialProperty.MP_BASE_COLOR)
mel.connect_material_property(mt, "", unreal.MaterialProperty.MP_METALLIC)
mel.connect_material_property(rg, "", unreal.MaterialProperty.MP_ROUGHNESS)
em = E(unreal.MaterialExpressionMultiply, -500, 450); C(color, "", em, "A"); C(emis, "", em, "B")
mel.connect_material_property(em, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
# rayos X: tramado por ruido (igual que antes)
wpn = E(unreal.MaterialExpressionNoise, -650, 800, scale=0.45, quality=1, levels=1, output_min=0.0, output_max=1.0,
        noise_function=unreal.NoiseFunction.NOISEFUNCTION_GRADIENT_ALU)
C(wp, "", wpn, "Position")
cen = E(unreal.MaterialExpressionSubtract, -450, 800, const_b=0.5); C(wpn, "", cen, "A")
amp = E(unreal.MaterialExpressionMultiply, -300, 800, const_b=0.7); C(cen, "", amp, "A")
dit = E(unreal.MaterialExpressionAdd, -150, 700); C(fade, "", dit, "A"); C(amp, "", dit, "B")
mel.connect_material_property(dit, "", unreal.MaterialProperty.MP_OPACITY_MASK)
m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
m.set_editor_property("two_sided", True)
mel.recompile_material(m); unreal.EditorAssetLibrary.save_loaded_asset(m)
print("CARMAT_OK", mel.get_num_material_expressions(m))
