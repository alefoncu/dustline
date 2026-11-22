import unreal, os, time
unreal.EditorLevelLibrary.load_level('/Game/Maps/Showroom')
cams = [("mytrees_wide", unreal.Vector(-2300, 2950, 420), unreal.Rotator(0, 12, 0)),
        ("mytrees_close", unreal.Vector(-750, 2350, 300), unreal.Rotator(0, 18, -27))]
S = {"i": 0, "t0": time.time(), "phase": "wait", "fired": 0, "end": None}
def path(n): return "/Volumes/SSD/UE-Cache/sr_%s.png" % n
def tick(dt):
    now = time.time() - S["t0"]
    if S["i"] >= len(cams):
        if S["end"] is None: S["end"] = time.time()
        if time.time() - S["end"] > 3:
            unreal.unregister_slate_post_tick_callback(handle); unreal.SystemLibrary.quit_editor()
        return
    name, loc, rot = cams[S["i"]]
    unreal.EditorLevelLibrary.set_level_viewport_camera_info(loc, rot)
    if S["phase"] == "wait" and now > (60 if S["i"] == 0 else 12):
        unreal.SystemLibrary.execute_console_command(None, "HighResShot 1280x720 filename=%s" % path(name))
        S["phase"] = "saving"; S["fired"] = now
    elif S["phase"] == "saving" and (os.path.exists(path(name)) or now - S["fired"] > 240):
        print("SHOT_DONE", name, os.path.exists(path(name)))
        S["i"] += 1; S["phase"] = "wait"; S["t0"] = time.time()
handle = unreal.register_slate_post_tick_callback(tick)
print("SHOT_REGISTERED")
