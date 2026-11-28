#!/bin/bash
# uso: play.sh <modelo 0-5> [extra...]   (juego en ventana, mapa Dustline_World; -autodrive -dshot=ruta para pruebas)
MODEL="$1"; shift
# un servidor de trazas colgado de una sesión anterior bloquea el arranque: se cierra antes
pkill -9 -f UnrealTraceServer 2>/dev/null
exec env "UE-LocalDataCachePath=/Volumes/SSD/UE-Cache/DDC" /Volumes/SSD/UnrealEngine/UE_5.8/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor /Volumes/SSD/Dustline-UE/Dustline.uproject /Game/Maps/Dustline_World -game -windowed -ResX=1280 -ResY=720 -nosplash -nosound -NoVerifyGC "-dmodel=$MODEL" -ExecCmds="r.Shadow.Virtual.Enable 0,r.DynamicGlobalIlluminationMethod 0,r.ReflectionMethod 2" "$@"
