# Genera las texturas de lluvia y nieve (sin recursos de terceros). Uso: python3 tools/make_weather_textures.py [carpeta]
from PIL import Image, ImageDraw, ImageFilter
import random, sys, os
out = sys.argv[1] if len(sys.argv) > 1 else "/Volumes/SSD/Weather-Setup"
os.makedirs(out, exist_ok=True)
random.seed(7)
W, H = 256, 512
im = Image.new('L', (W, H), 0); d = ImageDraw.Draw(im)
for i in range(140):
    x = random.uniform(0, W); y = random.uniform(0, H); L = random.uniform(25, 90); a = random.randint(60, 200); w = random.choice([1, 1, 1, 2])
    for k in range(-1, 2):
        y0 = y + k * H; d.line([(x, y0), (x, y0 + L)], fill=a, width=w)
        if x < 3: d.line([(x + W, y0), (x + W, y0 + L)], fill=a, width=w)
im = im.filter(ImageFilter.GaussianBlur(0.6))
Image.merge('RGBA', (im, im, im, im)).save(os.path.join(out, 'T_Rain.png'))
S = 512
im = Image.new('L', (S, S), 0); d = ImageDraw.Draw(im)
for i in range(420):
    x = random.uniform(0, S); y = random.uniform(0, S); r = random.choice([1.2, 1.5, 2, 2, 2.5, 3, 4]); a = random.randint(120, 255)
    for ox in (-S, 0, S):
        for oy in (-S, 0, S):
            d.ellipse([x + ox - r, y + oy - r, x + ox + r, y + oy + r], fill=a)
im = im.filter(ImageFilter.GaussianBlur(1.1))
Image.merge('RGBA', (im, im, im, im)).save(os.path.join(out, 'T_Snow.png'))
Image.new('RGBA', (4, 4), (0, 0, 0, 0)).save(os.path.join(out, 'T_WeatherDefault.png'))
print("weather textures ->", out)
# partícula suave (salpicaduras, barro, nieve, polvo) y huella de neumático con tacos (rodadas)
P = 128
im = Image.new('L', (P, P), 0); d = ImageDraw.Draw(im)
for r in range(P // 2, 0, -1):
    v = int(255 * (1 - (r / (P / 2))) ** 1.6); d.ellipse([P / 2 - r, P / 2 - r, P / 2 + r, P / 2 + r], fill=v)
for i in range(30):   # algo de textura irregular
    x = random.uniform(30, 98); y = random.uniform(30, 98); rr = random.uniform(4, 12)
    d.ellipse([x - rr, y - rr, x + rr, y + rr], fill=random.randint(150, 255))
im = im.filter(ImageFilter.GaussianBlur(4))
Image.merge('RGBA', (im, im, im, im)).save(os.path.join(out, 'T_Puff.png'))
RW, RH = 128, 256
im = Image.new('L', (RW, RH), 0); d = ImageDraw.Draw(im)
for k in range(8):   # tacos en V cada 32 px
    y = k * 32
    d.polygon([(8, y + 4), (60, y + 16), (60, y + 26), (8, y + 14)], fill=255)
    d.polygon([(120, y + 4), (68, y + 16), (68, y + 26), (120, y + 14)], fill=255)
edge = Image.new('L', (RW, RH), 0); de = ImageDraw.Draw(edge)
for x in range(RW):
    t = min(x, RW - 1 - x) / 14.0
    de.line([(x, 0), (x, RH)], fill=int(255 * min(1.0, t)))
from PIL import ImageChops
base = Image.new('L', (RW, RH), 150)
im = ImageChops.multiply(ImageChops.lighter(base, im), edge).filter(ImageFilter.GaussianBlur(1.5))
Image.merge('RGBA', (im, im, im, im)).save(os.path.join(out, 'T_Rut.png'))
print("fx textures ->", out)
