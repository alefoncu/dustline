import unreal, os, time
LEVEL = '/Game/Maps/Dustline_World'
unreal.EditorLevelLibrary.load_level(LEVEL)
cams = []
def inst_pos(label, idx):
    for a in unreal.EditorLevelLibrary.get_all_level_actors():
        if a.get_actor_label() == label:
            comp = a.get_components_by_class(unreal.HierarchicalInstancedStaticMeshComponent)[0]
            return comp.get_instance_transform(min(idx, comp.get_instance_count() - 1), True).translation
def all_fir():
    pts = []
    for a in unreal.EditorLevelLibrary.get_all_level_actors():
        if a.get_actor_label().startswith("SM_Fir_"):
            comp = a.get_components_by_class(unreal.HierarchicalInstancedStaticMeshComponent)[0]
            for i in range(comp.get_instance_count()):
                t = comp.get_instance_transform(i, True).translation; pts.append((t.x, t.y, t.z))
    return pts
pts = all_fir()
cell = {}
for (x, y, z) in pts:
    k = (int(x // 3000), int(y // 3000)); cell.setdefault(k, []).append((x, y, z))
bk = max(cell, key=lambda k: len(cell[k])); bn = len(cell[bk])
lst = cell[bk]; best = (sum(t[0] for t in lst) / bn, sum(t[1] for t in lst) / bn, sum(t[2] for t in lst) / bn)
print("FOREST_DENSEST", best, bn, "of", len(pts))
p = unreal.Vector(best[0], best[1], best[2])
cams.append(("forest_top", unreal.Vector(p.x, p.y, p.z + 6500), unreal.Rotator(0, -62, 0)))
cams.append(("forest_oblique", unreal.Vector(p.x - 4200, p.y - 4200, p.z + 3800), unreal.Rotator(0, -24, 45)))
only = os.environ.get("SHOT_ONLY")
if only: cams = [c for c in cams if c[0] == only]

S = {"i": 0, "t0": time.time(), "phase": "wait", "fired": 0}
WARMUP = 90.0
def path(n): return "/Volumes/SSD/UE-Cache/shot_%s.png" % n
def tick(dt):
    now = time.time() - S["t0"]
    if S["i"] >= len(cams):
        if "end" not in S: S["end"] = time.time()
        if time.time() - S["end"] > 3:
            unreal.unregister_slate_post_tick_callback(handle)
            unreal.SystemLibrary.quit_editor()
        return
    name, loc, rot = cams[S["i"]]
    unreal.EditorLevelLibrary.set_level_viewport_camera_info(loc, rot)
    if S["phase"] == "wait" and now > (WARMUP if S["i"] == 0 else 15):
        unreal.SystemLibrary.execute_console_command(None, "HighResShot 1280x720 filename=%s" % path(name))
        S["phase"] = "saving"; S["fired"] = now
    elif S["phase"] == "saving" and (os.path.exists(path(name)) or now - S["fired"] > 240):
        print("SHOT_DONE", name, os.path.exists(path(name)))
        S["i"] += 1; S["phase"] = "wait"; S["t0"] = time.time()
        if S["i"] >= len(cams): S["end"] = 0; S["t0"] = time.time() - 10
handle = unreal.register_slate_post_tick_callback(tick)
print("SHOT_REGISTERED")
