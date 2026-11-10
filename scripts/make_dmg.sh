#!/usr/bin/env bash
# Builds the macOS DMG installer: a .pkg that installs the standalone app, the VST3 and the AU
# in one go, plus a read-me. Run on macOS after a Release build:
#
#   scripts/make_dmg.sh [artefacts-dir] [output-dir]
#
# artefacts-dir defaults to build/app/ViridianForge_artefacts/Release, output-dir to build/dist.
#
# Signing: set MACOS_APP_SIGN_ID ("Developer ID Application: ...") and MACOS_PKG_SIGN_ID
# ("Developer ID Installer: ...") to sign with your certificates; set MACOS_NOTARY_PROFILE
# (a notarytool keychain profile) to notarise too. Without them the bundles are ad-hoc signed
# and macOS will ask the user to confirm the first time (see the read-me in the DMG).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
ART="${1:-$ROOT/build/app/ViridianForge_artefacts/Release}"
OUT="${2:-$ROOT/build/dist}"
PRODUCT="Viridian Forge"
IDENT="com.viridianaudioworks.forge"
VERSION="$(sed -n 's/^project(ViridianForge VERSION \([0-9.]*\).*/\1/p' "$ROOT/CMakeLists.txt")"
[ -n "$VERSION" ] || VERSION="0.0.0"

APP="$ART/Standalone/$PRODUCT.app"
VST3="$ART/VST3/$PRODUCT.vst3"
AU="$ART/AU/$PRODUCT.component"
[ -d "$APP" ]  || { echo "missing $APP (build Release first)"; exit 1; }
[ -d "$VST3" ] || { echo "missing $VST3"; exit 1; }

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
PAYLOAD="$WORK/payload"
mkdir -p "$PAYLOAD/Applications" "$PAYLOAD/Library/Audio/Plug-Ins/VST3" "$PAYLOAD/Library/Audio/Plug-Ins/Components"

ditto "$APP" "$PAYLOAD/Applications/$PRODUCT.app"
ditto "$VST3" "$PAYLOAD/Library/Audio/Plug-Ins/VST3/$PRODUCT.vst3"
HAS_AU=0
if [ -d "$AU" ]; then
    ditto "$AU" "$PAYLOAD/Library/Audio/Plug-Ins/Components/$PRODUCT.component"
    HAS_AU=1
else
    rmdir "$PAYLOAD/Library/Audio/Plug-Ins/Components"
fi

# --- Sign the bundles (Developer ID if given, otherwise ad-hoc so Apple Silicon will load them)
SIGN_ID="${MACOS_APP_SIGN_ID:--}"
sign() {
    if [ "$SIGN_ID" = "-" ]; then
        codesign --force --deep --sign - "$1"
    else
        codesign --force --deep --options runtime --timestamp --sign "$SIGN_ID" "$1"
    fi
}
sign "$PAYLOAD/Applications/$PRODUCT.app"
sign "$PAYLOAD/Library/Audio/Plug-Ins/VST3/$PRODUCT.vst3"
[ "$HAS_AU" = 1 ] && sign "$PAYLOAD/Library/Audio/Plug-Ins/Components/$PRODUCT.component"

# --- Installer scripts: refresh the AU cache so DAWs see the plugin right away
mkdir -p "$WORK/scripts"
cat > "$WORK/scripts/postinstall" <<'EOF'
#!/bin/sh
killall -9 AudioComponentRegistrar >/dev/null 2>&1 || true
exit 0
EOF
chmod +x "$WORK/scripts/postinstall"

# --- Component package: install exactly where we put things (no bundle relocation)
pkgbuild --analyze --root "$PAYLOAD" "$WORK/components.plist"
n=$(/usr/libexec/PlistBuddy -c "Print" "$WORK/components.plist" | grep -c "BundleIsRelocatable" || true)
for ((i = 0; i < n; i++)); do
    /usr/libexec/PlistBuddy -c "Set :$i:BundleIsRelocatable false" "$WORK/components.plist"
done
pkgbuild --root "$PAYLOAD" --component-plist "$WORK/components.plist" --scripts "$WORK/scripts" \
         --identifier "$IDENT.pkg" --version "$VERSION" --install-location / "$WORK/component.pkg"

# --- Product archive with a title and the welcome text
mkdir -p "$WORK/resources"
cat > "$WORK/resources/welcome.txt" <<EOF
$PRODUCT $VERSION

This installs:
  - $PRODUCT.app into /Applications
  - $PRODUCT.vst3 into /Library/Audio/Plug-Ins/VST3
$( [ "$HAS_AU" = 1 ] && echo "  - $PRODUCT.component (Audio Unit) into /Library/Audio/Plug-Ins/Components" )

Rescan plug-ins in your DAW after installing. Your NAM captures, IRs and settings are not touched.
EOF
cat > "$WORK/distribution.xml" <<EOF
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
    <title>$PRODUCT $VERSION</title>
    <welcome file="welcome.txt" mime-type="text/plain"/>
    <options customize="never" require-scripts="false" hostArchitectures="arm64,x86_64"/>
    <domains enable_localSystem="true"/>
    <choices-outline><line choice="default"><line choice="$IDENT.pkg"/></line></choices-outline>
    <choice id="default"/>
    <choice id="$IDENT.pkg" visible="false"><pkg-ref id="$IDENT.pkg"/></choice>
    <pkg-ref id="$IDENT.pkg" version="$VERSION" onConclusion="none">component.pkg</pkg-ref>
</installer-gui-script>
EOF

DMGROOT="$WORK/dmg"
mkdir -p "$DMGROOT"
PKG="$DMGROOT/Install $PRODUCT.pkg"
PB_ARGS=(--distribution "$WORK/distribution.xml" --package-path "$WORK" --resources "$WORK/resources")
if [ -n "${MACOS_PKG_SIGN_ID:-}" ]; then
    productbuild "${PB_ARGS[@]}" --sign "$MACOS_PKG_SIGN_ID" "$PKG"
else
    productbuild "${PB_ARGS[@]}" "$PKG"
fi

cat > "$DMGROOT/Read Me.txt" <<EOF
$PRODUCT $VERSION for macOS (Apple Silicon and Intel, macOS 11 or later)

INSTALL
Double-click "Install $PRODUCT.pkg" and follow the steps. It installs the standalone app,
the VST3 plug-in$( [ "$HAS_AU" = 1 ] && echo " and the Audio Unit") for all users. Then rescan plug-ins in your DAW.

IF MACOS SAYS IT CANNOT BE OPENED
This build is not notarised by Apple yet. Right-click (or Control-click) the installer and
choose Open, then Open again. If it is still blocked: System Settings > Privacy & Security,
scroll down and click "Open Anyway". The same applies the first time you open the app.

UNINSTALL
Delete /Applications/$PRODUCT.app, /Library/Audio/Plug-Ins/VST3/$PRODUCT.vst3 and
/Library/Audio/Plug-Ins/Components/$PRODUCT.component.
Your libraries (NAM captures, IRs) are in ~/Library/$PRODUCT and are kept.
EOF
cp "$ROOT/THIRD_PARTY_NOTICES.md" "$DMGROOT/Third-Party Notices.md"

# --- Disk image
mkdir -p "$OUT"
DMG="$OUT/$(echo "$PRODUCT" | tr ' ' '-')-$VERSION-macOS.dmg"
rm -f "$DMG"
# hdiutil occasionally fails with "Resource busy" on CI machines: retry a few times.
for attempt in 1 2 3 4 5; do
    if hdiutil create -volname "$PRODUCT $VERSION" -srcfolder "$DMGROOT" -fs HFS+ -format UDZO -ov "$DMG"; then break; fi
    [ "$attempt" = 5 ] && { echo "hdiutil failed"; exit 1; }
    sleep $((attempt * 5))
done

if [ "$SIGN_ID" != "-" ]; then
    codesign --force --timestamp --sign "$SIGN_ID" "$DMG"
fi
if [ -n "${MACOS_NOTARY_PROFILE:-}" ]; then
    xcrun notarytool submit "$DMG" --keychain-profile "$MACOS_NOTARY_PROFILE" --wait
    xcrun stapler staple "$DMG"
fi

echo "Built $DMG"
