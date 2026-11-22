#!/bin/bash
# uso: tools/orthoshot.sh <modelo 0-6>  -> /Volumes/SSD/UE-Cache/bd_N_{o34,oside,ofront,orear}.png (vistas casi ortográficas, 560 cm de ancho)
M="$1"; rm -f /Volumes/SSD/UE-Cache/bd_${M}_o*.png
(/Volumes/SSD/Dustline-UE/tools/play.sh "$M" -nomenu -bodytest "-bodymodel=$M" -stock -bodyortho > /Volumes/SSD/UE-Cache/uidemo.log 2>&1 &)
sleep 5; for i in $(seq 1 90); do [ -f /Volumes/SSD/UE-Cache/bd_${M}_orear.png ] && break; sleep 3; done; sleep 2; pkill -f UnrealEditor; sleep 2
