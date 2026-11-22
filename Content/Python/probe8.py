import unreal
for n in dir(unreal):
    if "ither" in n: print("P8", n)
d = getattr(unreal, "MaterialExpressionDither", None)
if d: print("P8D", [x for x in dir(d) if not x.startswith("_")][:40])
