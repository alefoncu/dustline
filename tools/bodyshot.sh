#!/bin/bash
# uso: tools/bodyshot.sh <modelo 0-6>  -> /Volumes/SSD/UE-Cache/bd_sheet_N.png (4 ángulos del coche de serie)
M="$1"; rm -f /Volumes/SSD/UE-Cache/bd_${M}_*.png
(/Volumes/SSD/Dustline-UE/tools/play.sh "$M" -nomenu -bodytest "-bodymodel=$M" -stock > /Volumes/SSD/UE-Cache/uidemo.log 2>&1 &)
sleep 5; for i in $(seq 1 90); do [ -f /Volumes/SSD/UE-Cache/bd_${M}_r34.png ] && break; sleep 3; done; sleep 2; pkill -f UnrealEditor
python3 - <<PY
from PIL import Image
sh=Image.new('RGB',(1280,720))
for k,t in enumerate(['f34','side','front','r34']):
    im=Image.open(f'/Volumes/SSD/UE-Cache/bd_${M}_{t}.png').convert('RGB'); w,h=im.size
    im=im.crop((int(w*0.18),int(h*0.12),int(w*0.82),int(h*0.88))).resize((640,360)); sh.paste(im,((k%2)*640,(k//2)*360))
sh.save('/Volumes/SSD/UE-Cache/bd_sheet_${M}.png')
PY
