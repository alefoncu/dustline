#!/usr/bin/env python3
"""Icono de Dustline (estilo macOS): squircle, atardecer, montañas y la 'dustline' (estela de polvo)."""
import math, os, sys
from PIL import Image, ImageDraw, ImageFilter, ImageChops

OUT = sys.argv[1]
S = 2                      # supersampling
N = 1024 * S
BODY = 824 * S             # tamaño del squircle según la plantilla de macOS
OFF = (N - BODY) // 2

def lerp(a, b, t): return a + (b - a) * t
def lerp_c(c0, c1, t): return tuple(int(round(lerp(c0[i], c1[i], t))) for i in range(len(c0)))

def squircle_mask(size, n=5.0):
    m = Image.new("L", (size, size), 0)
    d = ImageDraw.Draw(m)
    r = size / 2
    pts = []
    for i in range(720):
        a = 2 * math.pi * i / 720
        c, s = math.cos(a), math.sin(a)
        x = r + r * math.copysign(abs(c) ** (2 / n), c)
        y = r + r * math.copysign(abs(s) ** (2 / n), s)
        pts.append((x, y))
    d.polygon(pts, fill=255)
    return m

# ---------- contenido (lienzo BODY x BODY) ----------
W = BODY
art = Image.new("RGBA", (W, W), (0, 0, 0, 255))
d = ImageDraw.Draw(art)

# cielo: índigo -> magenta -> naranja en el horizonte
horizon = int(W * 0.60)
stops = [(0.00, (14, 12, 44)), (0.35, (66, 24, 92)), (0.62, (176, 52, 96)), (0.82, (255, 112, 58)), (1.00, (255, 178, 84))]
for y in range(W):
    t = min(y / horizon, 1.0)
    for i in range(len(stops) - 1):
        if stops[i][0] <= t <= stops[i + 1][0]:
            u = (t - stops[i][0]) / (stops[i + 1][0] - stops[i][0])
            c = lerp_c(stops[i][1], stops[i + 1][1], u)
            break
    d.line([(0, y), (W, y)], fill=c + (255,))

# sol / resplandor
glow = Image.new("RGBA", (W, W), (0, 0, 0, 0))
g = ImageDraw.Draw(glow)
sx, sy = int(W * 0.5), horizon - int(W * 0.015)
for i in range(90):
    r = int(W * 0.46 * (1 - i / 90))
    col = (255, 205, 120, 5)
    g.ellipse([sx - r, sy - r, sx + r, sy + r], fill=col)
rr = int(W * 0.115)
g.ellipse([sx - rr, sy - rr, sx + rr, sy + rr], fill=(255, 234, 176, 255))
glow = glow.filter(ImageFilter.GaussianBlur(W * 0.012))
art = Image.alpha_composite(art, glow)
d = ImageDraw.Draw(art)

def ridge(base, amp, seed, rough, color, jag=0.0):
    pts = [(0, W)]
    x = 0
    ph = [seed * 1.7, seed * 2.9, seed * 4.3]
    while x <= W:
        t = x / W
        y = base - amp * (0.55 * math.sin(t * 5.2 + ph[0]) + 0.30 * math.sin(t * 11.0 + ph[1]) + 0.15 * math.sin(t * 23.0 + ph[2]) * rough)
        pts.append((x, y))
        x += max(2, W // 400)
    pts.append((W, W))
    d.polygon(pts, fill=color)

ridge(horizon - W * 0.01, W * 0.12, 1.0, 1.0, (88, 34, 92, 255))
ridge(horizon + W * 0.03, W * 0.10, 2.3, 1.0, (52, 22, 70, 255))
ridge(horizon + W * 0.09, W * 0.07, 4.1, 0.8, (30, 14, 48, 255))
# suelo
d.polygon([(0, horizon + W * 0.16), (W, horizon + W * 0.13), (W, W), (0, W)], fill=(20, 10, 32, 255))

# ---------- la "dustline": estela serpenteante en perspectiva ----------
def road_points(n=240):
    pts = []
    y0, y1 = W * 1.02, horizon + W * 0.035
    for i in range(n + 1):
        t = i / n
        y = lerp(y0, y1, t ** 0.82)
        persp = (1 - t)
        x = W * 0.5 + W * 0.30 * math.sin(t * 3.6 + 0.35) * persp ** 0.9 - W * 0.04 * persp
        w = W * 0.20 * persp ** 1.55 + W * 0.004
        pts.append((x, y, w))
    return pts

pts = road_points()
left = [(x - w, y) for x, y, w in pts]
right = [(x + w, y) for x, y, w in reversed(pts)]

# resplandor cálido de la estela
halo = Image.new("RGBA", (W, W), (0, 0, 0, 0))
ImageDraw.Draw(halo).polygon(left + right, fill=(255, 150, 80, 200))
halo = halo.filter(ImageFilter.GaussianBlur(W * 0.025))
art = Image.alpha_composite(art, halo)

# cinta principal con degradado: crema abajo -> naranja arriba
ribbon = Image.new("RGBA", (W, W), (0, 0, 0, 0))
ImageDraw.Draw(ribbon).polygon(left + right, fill=(255, 255, 255, 255))
grad = Image.new("RGBA", (W, W))
gd = ImageDraw.Draw(grad)
for y in range(W):
    t = min(max((y - horizon) / (W - horizon), 0), 1)
    c = lerp_c((255, 170, 96), (255, 244, 224), t ** 0.7)
    gd.line([(0, y), (W, y)], fill=c + (255,))
ribbon = ImageChops.multiply(ribbon, grad)
art = Image.alpha_composite(art, ribbon)

# línea discontinua central
dash = Image.new("RGBA", (W, W), (0, 0, 0, 0))
dd = ImageDraw.Draw(dash)
n = 14
for k in range(n):
    t0, t1 = k / n, (k + 0.45) / n
    seg = []
    for tt in (t0, t1):
        i = min(int(tt * (len(pts) - 1)), len(pts) - 1)
        seg.append(pts[i])
    (xa, ya, wa), (xb, yb, wb) = seg
    wa2, wb2 = wa * 0.07, wb * 0.07
    if wa < W * 0.02:
        continue
    dd.polygon([(xa - wa2, ya), (xa + wa2, ya), (xb + wb2, yb), (xb - wb2, yb)], fill=(150, 60, 70, 150))
art = Image.alpha_composite(art, dash)

# luz superior (cristal): degradado blanco suave arriba-izquierda
sheen = Image.new("RGBA", (W, W), (0, 0, 0, 0))
sd = ImageDraw.Draw(sheen)
for y in range(int(W * 0.55)):
    a = int(46 * (1 - y / (W * 0.55)) ** 2)
    sd.line([(0, y), (W, y)], fill=(255, 255, 255, a))
art = Image.alpha_composite(art, sheen)

# ---------- composición final ----------
mask = squircle_mask(BODY)
body = Image.new("RGBA", (BODY, BODY), (0, 0, 0, 0))
body.paste(art, (0, 0), mask)

# borde interior fino
edge = Image.new("RGBA", (BODY, BODY), (0, 0, 0, 0))
ed = ImageDraw.Draw(edge)
inner = squircle_mask(BODY - 4 * S)
ring = ImageChops.subtract(mask, Image.new("L", (BODY, BODY), 0).copy()) if False else mask
shrunk = Image.new("L", (BODY, BODY), 0)
shrunk.paste(inner, (2 * S, 2 * S))
rim = ImageChops.subtract(mask, shrunk).point(lambda v: int(v * 0.35))
edge.putalpha(rim)
edge.paste((255, 255, 255, 255), (0, 0), rim)
body = Image.alpha_composite(body, edge)

canvas = Image.new("RGBA", (N, N), (0, 0, 0, 0))
shadow = Image.new("RGBA", (N, N), (0, 0, 0, 0))
shadow.paste((0, 0, 0, 120), (OFF, OFF + 14 * S), mask)
shadow = shadow.filter(ImageFilter.GaussianBlur(14 * S))
canvas = Image.alpha_composite(canvas, shadow)
canvas.paste(body, (OFF, OFF), body)
final = canvas.resize((1024, 1024), Image.LANCZOS)
final.save(OUT)
print("ok", OUT)
