#!/bin/bash
# Regenera el mapa Dustline_World (terreno, luz, material, vegetacion, agua) y el modo de juego.
# Requiere UE 5.8.4 en /Volumes/SSD/UnrealEngine/UE_5.8 y el modulo C++ compilado.
set -e
cd "$(dirname "$0")/.."
rm -f Content/Maps/Dustline_World.umap
python3 tools/make_weather_textures.py /Volumes/SSD/Weather-Setup
tools/ue.sh "$PWD/Content/Python/make_weather_assets.py" -nullrhi
tools/ue.sh "$PWD/Content/Python/build_world.py" -nullrhi
