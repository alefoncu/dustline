#!/bin/bash
# Uso: ue.sh <script.py> [flags extra]  -> ejecuta un script Python en el editor de Unreal (sin ventana),
# con la caché de derivados en el SSD para no llenar el disco del Mac.
SCRIPT="$1"; shift
exec env "UE-LocalDataCachePath=/Volumes/SSD/UE-Cache/DDC" \
  /Volumes/SSD/UnrealEngine/UE_5.8/Engine/Binaries/Mac/UnrealEditor-Cmd \
  /Volumes/SSD/Dustline-UE/Dustline.uproject \
  -run=pythonscript -script="$SCRIPT" -unattended -nosplash -stdout -FullStdOutLogOutput "$@"
