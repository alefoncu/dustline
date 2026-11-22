#!/bin/bash
# Envío RÁPIDO (solo cambió código C++, no recursos/mapa): commit + push, recompila el juego e intercambia el ejecutable
# en /Applications. No cocina ni empaqueta (~1-2 min en vez de ~6-10).  Uso: tools/ship_fast.sh "mensaje"
# Si cambiaste recursos (Content/, texturas, mapas) o propiedades UPROPERTY de actores del mapa, usa tools/ship.sh.
set -e
cd "$(dirname "$0")/.."
MSG="${1:?uso: tools/ship_fast.sh \"mensaje\"}"
git add -A
git diff --cached --quiet || git commit -m "$MSG"
git push
UE=/Volumes/SSD/UnrealEngine/UE_5.8
APP=/Applications/Dustline.app
[ -d "$APP" ] || { echo "No hay app instalada: usa tools/ship.sh la primera vez"; exit 1; }
env "UE-LocalDataCachePath=/Volumes/SSD/UE-Cache/DDC" "$UE/Engine/Build/BatchFiles/Mac/Build.sh" Dustline Mac Development -project="$PWD/Dustline.uproject" -NoHotReload
pkill -f "Dustline.app/Contents/MacOS" 2>/dev/null || true; sleep 1
cp Binaries/Mac/Dustline "$APP/Contents/MacOS/Dustline"
codesign --force --sign - "$APP"
touch "$APP"
echo "Instalado (rápido): $APP"
