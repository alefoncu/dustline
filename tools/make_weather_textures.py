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
