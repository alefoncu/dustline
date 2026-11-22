#!/bin/bash
# Flujo completo tras un cambio:  tools/ship.sh "mensaje del commit" [--world]
#  1) commit + push a GitHub  2) (opcional) regenera el mundo  3) compila+cocina+empaqueta  4) instala en /Applications
set -e
cd "$(dirname "$0")/.."
MSG="${1:?uso: tools/ship.sh \"mensaje\" [--world]}"
git add -A
git diff --cached --quiet || git commit -m "$MSG"
git push
[ "$2" = "--world" ] && tools/rebuild_world.sh
UE=/Volumes/SSD/UnrealEngine/UE_5.8
env "UE-LocalDataCachePath=/Volumes/SSD/UE-Cache/DDC" "$UE/Engine/Build/BatchFiles/RunUAT.sh" BuildCookRun \
  -project="$PWD/Dustline.uproject" -noP4 -platform=Mac -clientconfig=Development -build -cook \
  -map=/Game/Maps/Dustline_World -stage -pak -iostore -compressed -nodebuginfo -utf8output
tools/install_app.sh
