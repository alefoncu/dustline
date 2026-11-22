import unreal
names = [n for n in dir(unreal) if 'andscape' in n]
print("PROBE_LANDSCAPE", len(names))
for n in names: print("PROBE_NAME", n)
for cls in ("Landscape", "LandscapeProxy"):
    c = getattr(unreal, cls, None)
    if c:
        for m in dir(c):
            if any(k in m.lower() for k in ("import","heightmap","create","initialize","spawn")):
                print("PROBE_METHOD", cls, m)
