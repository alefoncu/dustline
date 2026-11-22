#!/bin/bash
# Instala el paquete ya generado (Saved/StagedBuilds/Mac/Dustline.app) en /Applications.
set -e
cd "$(dirname "$0")/.."
SRC="$PWD/Saved/StagedBuilds/Mac/Dustline.app"
APP="/Applications/Dustline.app"
[ -d "$SRC" ] || { echo "No hay paquete en $SRC (ejecuta tools/ship.sh)"; exit 1; }
pkill -f "Dustline.app/Contents/MacOS" 2>/dev/null || true; sleep 2
rm -rf "/Applications/Dustline.app"
ditto "$SRC" "$APP"
cp tools/AppIcon.icns "$APP/Contents/Resources/AppIcon.icns"
PB=/usr/libexec/PlistBuddy
$PB -c "Delete :CFBundleIconName" "$APP/Contents/Info.plist" 2>/dev/null || true
$PB -c "Set :CFBundleIdentifier com.dustline.game" "$APP/Contents/Info.plist"
$PB -c "Set :CFBundleShortVersionString 0.1" "$APP/Contents/Info.plist"
$PB -c "Add :CFBundleDisplayName string Dustline" "$APP/Contents/Info.plist" 2>/dev/null || true
codesign --force --deep --sign - "$APP"
touch "$APP"
/System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister -f "$APP" >/dev/null 2>&1 || true
echo "Instalado: $APP"
