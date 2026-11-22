#!/bin/bash
NAME="$1"; rm -f /Volumes/SSD/UE-Cache/car_$NAME$SHOT_SUFFIX.png
(SHOT_ONLY="$NAME" nohup env "UE-LocalDataCachePath=/Volumes/SSD/UE-Cache/DDC" /Volumes/SSD/UnrealEngine/UE_5.8/Engine/Binaries/Mac/UnrealEditor /Volumes/SSD/Dustline-UE/Dustline.uproject -ExecCmds="r.Shadow.Virtual.Enable 0,r.DynamicGlobalIlluminationMethod 0,r.ReflectionMethod 2,py /Volumes/SSD/Dustline-UE/Content/Python/shot4.py" -ResX=1280 -ResY=720 -nosplash -nosound -NoVerifyGC > /Volumes/SSD/UE-Cache/shot-run.log 2>&1 &)
sleep 1
for i in $(seq 1 150); do
  FREE=$(df -m /System/Volumes/Data | tail -1 | awk '{print $4}')
  if [ "$FREE" -lt 250 ]; then pkill -9 -f UnrealEditor; echo "ABORT low disk $FREE MB"; break; fi
  [ -f /Volumes/SSD/UE-Cache/car_$NAME$SHOT_SUFFIX.png ] && { sleep 3; break; }
  pgrep -f "UnrealEditor.*Dustline.uproject" >/dev/null || break
  sleep 5
done
pkill -f "UnrealEditor.*Dustline.uproject"; sleep 4
ls -la /Volumes/SSD/UE-Cache | grep "car_$NAME$SHOT_SUFFIX"; df -h /System/Volumes/Data | tail -1
