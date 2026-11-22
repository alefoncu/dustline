#!/bin/bash
# Compila y empaqueta Dustline.app
set -e
cd "$(dirname "$0")"
swift build -c release
APP="Dustline.app"
rm -rf "$APP"
mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Resources"
cp .build/release/Dustline "$APP/Contents/MacOS/Dustline"
cat > "$APP/Contents/Info.plist" <<PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
<key>CFBundleName</key><string>Dustline</string>
<key>CFBundleDisplayName</key><string>Dustline</string>
<key>CFBundleIdentifier</key><string>com.dustline.game</string>
<key>CFBundleExecutable</key><string>Dustline</string>
<key>CFBundlePackageType</key><string>APPL</string>
<key>CFBundleVersion</key><string>1</string>
<key>CFBundleShortVersionString</key><string>0.1</string>
<key>LSMinimumSystemVersion</key><string>13.0</string>
<key>NSHighResolutionCapable</key><true/>
<key>LSApplicationCategoryType</key><string>public.app-category.games</string>
</dict></plist>
PLIST
codesign --force --sign - "$APP" >/dev/null 2>&1 || true
echo "Listo: $(pwd)/$APP"
