import unreal, os, glob
mel = unreal.MaterialEditingLibrary
at = unreal.AssetToolsHelpers.get_asset_tools()
SRC = "/Volumes/SSD/Models-Setup"
IDS = ["rock_09","rock_07","stone_01","boulder_01","dead_tree_trunk","tree_stump_01","shrub_03","shrub_04","fir_sapling","fir_sapling_medium"]

# ---------- master materials ----------
def master(name, foliage):
    path = "/Game/Vegetation/" + name
    if unreal.EditorAssetLibrary.does_asset_exist(path): unreal.EditorAssetLibrary.delete_asset(path)
    m = at.create_asset(name, "/Game/Vegetation", unreal.Material, unreal.MaterialFactoryNew())
    def P(pname, normal=False, y=0):
        n = mel.create_material_expression(m, unreal.MaterialExpressionTextureSampleParameter2D, -600, y)
        n.set_editor_property("parameter_name", pname)
        if normal:
            n.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
            n.set_editor_property("texture", unreal.EditorAssetLibrary.load_asset("/Engine/EngineMaterials/BaseFlattenNormalMap"))
        else:
            n.set_editor_property("texture", unreal.EditorAssetLibrary.load_asset("/Engine/EngineResources/WhiteSquareTexture"))
        return n
    d = P("Diffuse", y=0); nrm = P("Normal", True, 300); r = P("Rough", y=600)
    if foliage:   # pale source texture -> tint to fir green
        tint = mel.create_material_expression(m, unreal.MaterialExpressionVectorParameter, -600, -300)
        tint.set_editor_property("parameter_name", "Tint"); tint.set_editor_property("default_value", unreal.LinearColor(0.12, 0.27, 0.085, 1))
        mt = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -300, -100)
        mel.connect_material_expressions(d, "RGB", mt, "A"); mel.connect_material_expressions(tint, "", mt, "B")
        mel.connect_material_property(mt, "", unreal.MaterialProperty.MP_BASE_COLOR)
    else:
        mel.connect_material_property(d, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(nrm, "RGB", unreal.MaterialProperty.MP_NORMAL)
    cr = mel.create_material_expression(m, unreal.MaterialExpressionScalarParameter, -600, 1200)
    cr.set_editor_property("parameter_name", "ConstRough"); cr.set_editor_property("default_value", 0.8)
    uc = mel.create_material_expression(m, unreal.MaterialExpressionScalarParameter, -600, 1350)
    uc.set_editor_property("parameter_name", "UseConst"); uc.set_editor_property("default_value", 0.0)
    lp = mel.create_material_expression(m, unreal.MaterialExpressionLinearInterpolate, -300, 700)
    mel.connect_material_expressions(r, "R", lp, "A"); mel.connect_material_expressions(cr, "", lp, "B"); mel.connect_material_expressions(uc, "", lp, "Alpha")
    mel.connect_material_property(lp, "", unreal.MaterialProperty.MP_ROUGHNESS)
    if foliage:
        a = P("Alpha", y=900)
        mel.connect_material_property(a, "R", unreal.MaterialProperty.MP_OPACITY_MASK)
        m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
        m.set_editor_property("two_sided", True)
    mel.recompile_material(m)
    unreal.EditorAssetLibrary.save_loaded_asset(m)
    return m
m_pbr = master("M_VegPBR", False)
m_fol = master("M_VegFoliage", True)

# ---------- textures ----------
def import_tex(i):
    tasks = []
    for f in sorted(glob.glob("%s/%s/textures/*" % (SRC, i))):
        b = os.path.basename(f)
        if "_disp_" in b or "_mask_" in b or "_arm_" in b: continue
        t = unreal.AssetImportTask()
        t.filename = f; t.destination_path = "/Game/Vegetation/%s/Tex" % i
        t.destination_name = os.path.splitext(b)[0]
        t.automated = True; t.save = False; t.replace_existing = True
        tasks.append(t)
    at.import_asset_tasks(tasks)
    out = {}
    for t in tasks:
        name = t.destination_name
        tex = unreal.EditorAssetLibrary.load_asset("%s/%s" % (t.destination_path, name))
        if not tex: continue
        if "_nor_" in name:
            tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
            tex.set_editor_property("srgb", False)
            tex.set_editor_property("flip_green_channel", True)   # Poly Haven normals are OpenGL (Y+)
        elif "_rough_" in name or "_alpha_" in name:
            tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
            tex.set_editor_property("srgb", False)
        out[name] = tex
        unreal.EditorAssetLibrary.save_loaded_asset(tex)
    return out

# ---------- instances per material slot ----------
for i in IDS:
    texs = import_tex(i)
    mesh = unreal.EditorAssetLibrary.load_asset("/Game/Vegetation/%s/%s_1k" % (i, i))
    for k, sm in enumerate(mesh.static_materials):
        slot = str(sm.material_slot_name)
        def find(kind):
            for n, t in texs.items():
                if n.startswith(slot + "_" + kind): return t
            return None
        diff, nor, rough, alpha = find("diff"), find("nor"), find("rough"), find("alpha")
        if diff is None:   # e.g. *_dead slots share the base slot textures
            base = slot.replace("_dead", "")
            find2 = lambda kind: next((t for n, t in texs.items() if n.startswith(base + "_" + kind)), None)
            diff, nor, rough, alpha = find2("diff"), find2("nor"), find2("rough"), find2("alpha")
        parent = m_fol if alpha else m_pbr
        mi_name = "MI_" + slot
        p = "/Game/Vegetation/%s/%s" % (i, mi_name)
        if unreal.EditorAssetLibrary.does_asset_exist(p): unreal.EditorAssetLibrary.delete_asset(p)
        mi = at.create_asset(mi_name, "/Game/Vegetation/%s" % i, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        mi.set_editor_property("parent", parent)
        for pname, t in (("Diffuse", diff), ("Normal", nor), ("Rough", rough), ("Alpha", alpha)):
            if t is not None:
                mel.set_material_instance_texture_parameter_value(mi, pname, t)
        # 16-bit PNG roughness maps read as 0 (mirror): use a constant matte roughness for those
        use_const = 1.0 if (rough is None or str(rough.get_name()) and any(os.path.exists("%s/%s/textures/%s.png" % (SRC, i, str(rough.get_name())))) for _ in [0]) else 0.0
        mel.set_material_instance_scalar_parameter_value(mi, "UseConst", use_const)
        mel.set_material_instance_scalar_parameter_value(mi, "ConstRough", 0.85)
        print("VMCHK", slot, "diffuse bound:", mel.get_material_instance_texture_parameter_value(mi, "Diffuse"), "useconst", use_const)
        mel.update_material_instance(mi)
        unreal.EditorAssetLibrary.save_loaded_asset(mi)
        mesh.set_material(k, mi)
        print("VM", i, slot, "->", parent.get_name(), "diff" if diff else "-", "nor" if nor else "-", "rough" if rough else "-", "alpha" if alpha else "-")
    unreal.EditorAssetLibrary.save_loaded_asset(mesh)
print("VM_DONE")
