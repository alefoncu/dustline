import unreal, os, time, math
unreal.EditorLevelLibrary.load_level('/Game/Maps/Garage')
car = [a for a in unreal.EditorLevelLibrary.get_all_level_actors() if a.get_actor_label() == "Car"][0]
V = unreal.Vector; R = unreal.Rotator
def look(cam, tgt):
    d = tgt - cam; yaw = math.degrees(math.atan2(d.y, d.x)); pitch = math.degrees(math.atan2(d.z, math.hypot(d.x, d.y)))
    return R(0, pitch, yaw)
SH = {
 "g_ext":   dict(model=0, xray=False, cam=V(-520, -470, 170), tgt=V(0, 0, 95)),
 "g_xray":  dict(model=0, xray=True,  cam=V(330, -300, 240),  tgt=V(145, 0, 70)),
 "g_under": dict(model=0, xray=False, cam=V(-300, -420, 40),  tgt=V(0, 0, 40)),
 "panda_v8": dict(model=2, xray=True, over=dict(engine_cc=7000.0, turbo=3.0), cam=V(290, -270, 220), tgt=V(110, 0, 60)),
 "panda_stock": dict(model=2, xray=True, cam=V(290, -270, 220), tgt=V(110, 0, 60)),
 "crawler_ext": dict(model=6, xray=False, cam=V(-720, -600, 210), tgt=V(0, 0, 110)),
 "buggy_ext": dict(model=4, xray=False, cam=V(-560, -480, 170), tgt=V(0, 0, 70)),
}
SH["lineup"] = dict(model=0, xray=False, cam=V(-1500, 0, 380), tgt=V(0, 0, 70))
SH["lineup_xray"] = dict(model=0, xray=True, cam=V(-1500, 0, 520), tgt=V(0, 0, 40))
name = os.environ.get("SHOT_ONLY", "g_ext"); cfg = SH[name]
if name.startswith("lineup"):
    car.set_actor_location(V(0, 0, -5000), False, False)
    for i in range(7):
        c2 = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.DustlineCar, V(0, (i - 3) * 330, 0))
        if os.environ.get("SHOT_NOKEN"): c2.set_editor_property("use_kenney", False)
        c2.apply_setup(unreal.DustlineCar.stock_setup(i)); c2.set_x_ray(cfg["xray"])
s = unreal.DustlineCar.stock_setup(cfg["model"])
for k, v in cfg.get("over", {}).items(): s.set_editor_property(k, v)
if os.environ.get("SHOT_NOKEN"): car.set_editor_property("use_kenney", False)
car.apply_setup(s); car.set_x_ray(cfg["xray"])
loc, rot = cfg["cam"], look(cfg["cam"], cfg["tgt"])
S = {"t0": time.time(), "phase": "wait", "fired": 0}
path = "/Volumes/SSD/UE-Cache/car_%s.png" % (name + os.environ.get("SHOT_SUFFIX", ""))
def tick(dt):
    now = time.time() - S["t0"]
    unreal.EditorLevelLibrary.set_level_viewport_camera_info(loc, rot)
    if S["phase"] == "wait" and now > 70:
        unreal.SystemLibrary.execute_console_command(None, "HighResShot 1280x720 filename=%s" % path); S["phase"] = "saving"; S["fired"] = now
    elif S["phase"] == "saving" and (os.path.exists(path) or now - S["fired"] > 240):
        print("SHOT_DONE", name, os.path.exists(path)); S["phase"] = "end"; S["t1"] = time.time()
    elif S["phase"] == "end" and time.time() - S["t1"] > 3:
        unreal.unregister_slate_post_tick_callback(handle); unreal.SystemLibrary.quit_editor()
handle = unreal.register_slate_post_tick_callback(tick)
print("SHOT_REGISTERED", name)
