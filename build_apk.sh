#!/usr/bin/env bash
set -euo pipefail

PROJ_DIR="$(cd "$(dirname "$0")" && pwd)"
if [ -z "${JAVA_HOME:-}" ]; then
    if command -v /usr/libexec/java_home >/dev/null 2>&1 && /usr/libexec/java_home -v 17 >/dev/null 2>&1; then
        export JAVA_HOME="$(/usr/libexec/java_home -v 17)"
    elif [ -d "/opt/homebrew/opt/openjdk@17" ]; then
        export JAVA_HOME="/opt/homebrew/opt/openjdk@17"
    elif command -v javac >/dev/null 2>&1; then
        export JAVA_HOME="$(dirname "$(dirname "$(readlink -f "$(command -v javac)")")")"
    fi
fi
export PATH="${JAVA_HOME:+$JAVA_HOME/bin:}$PATH"

find_tool() {
    local pattern="$1"
    shift
    for d in "$@"; do
        if [ -d "$d" ]; then
            local found
            found="$(find "$d" -name "$pattern" -type f 2>/dev/null | head -n 1 || true)"
            if [ -n "$found" ]; then
                echo "$found"
                return 0
            fi
        fi
    done
    return 0
}

AAPT2="${AAPT2:-$(find_tool "aapt2" "$HOME/Library/Android/sdk/build-tools" "${GRADLE_USER_HOME:-$HOME/.gradle}/caches")}"
BUNDLETOOL="${BUNDLETOOL:-$(find_tool "bundletool-*.jar" "$HOME/.gemini/users" "${GRADLE_USER_HOME:-$HOME/.gradle}/caches")}"
APKSIG="${APKSIG:-$(find_tool "apksig-*.jar" "$HOME/.gemini/users" "${GRADLE_USER_HOME:-$HOME/.gradle}/caches")}"
KEYSTORE="${ANDROID_KEYSTORE:-$PROJ_DIR/build-tools/hyperhdr-release.jks}"

BUILD_TMP="$PROJ_DIR/app/build/manual_build"
rm -rf "$BUILD_TMP"
mkdir -p "$BUILD_TMP/r_src" "$BUILD_TMP/app_classes" "$BUILD_TMP/dex" "$BUILD_TMP/signer_classes" "$PROJ_DIR/app/build/outputs/apk/debug"

echo "[1/5] Compiling Android resources and linking AndroidManifest.xml + Web UI assets..."
"$AAPT2" compile --dir "$PROJ_DIR/app/src/main/res" -o "$BUILD_TMP/app_res.zip"
"$AAPT2" link -o "$BUILD_TMP/unsigned.apk" \
    -I "$PROJ_DIR/build-tools/android-fw.jar" \
    --manifest "$PROJ_DIR/app/src/main/AndroidManifest.xml" \
    -A "$PROJ_DIR/app/src/main/assets" \
    --java "$BUILD_TMP/r_src" \
    --min-sdk-version 23 \
    --target-sdk-version 30 \
    --version-code 24000 \
    --version-name "24.0.0-beta.0" \
    "$BUILD_TMP/app_res.zip"

echo "[2/5] Compiling Android stubs and Java sources..."
mkdir -p "$BUILD_TMP/stub_classes"
javac -source 11 -target 11 -g:none \
    -d "$BUILD_TMP/stub_classes" \
    $(find "$PROJ_DIR/build-tools/stubs_src" -name "*.java")
jar cf "$PROJ_DIR/build-tools/android-stubs.jar" -C "$BUILD_TMP/stub_classes" .

javac -source 11 -target 11 -g:none \
    -cp "$PROJ_DIR/build-tools/android-stubs.jar" \
    -d "$BUILD_TMP/app_classes" \
    "$BUILD_TMP/r_src/org/hyperhdr/androidtv/R.java" \
    $(find "$PROJ_DIR/app/src/main/java" -name "*.java")

echo "[3/5] Dexing bytecode with D8..."
JDK_LIB="$JAVA_HOME"
if [ -d "$JAVA_HOME/libexec/openjdk.jdk/Contents/Home" ]; then
    JDK_LIB="$JAVA_HOME/libexec/openjdk.jdk/Contents/Home"
fi
java -cp "$BUNDLETOOL" shadow.bundletool.com.android.tools.r8.D8 \
    --min-api 23 \
    --lib "$JDK_LIB" \
    --lib "$PROJ_DIR/build-tools/android-stubs.jar" \
    --output "$BUILD_TMP/dex" \
    $(find "$BUILD_TMP/app_classes" -name "*.class")

echo "[4/5] Packaging classes.dex and 4-byte aligning uncompressed resources..."
python3 - "$BUILD_TMP/unsigned.apk" "$BUILD_TMP/dex/classes.dex" "$BUILD_TMP/aligned.apk" << 'PYEOF'
import sys, zipfile

unsigned_apk, classes_dex, aligned_apk = sys.argv[1], sys.argv[2], sys.argv[3]
with zipfile.ZipFile(unsigned_apk, "r") as zin:
    entries = [(info, zin.read(info.filename)) for info in zin.infolist()]
with open(classes_dex, "rb") as df:
    dex_bytes = df.read()

with zipfile.ZipFile(aligned_apk, "w") as zout:
    zi_dex = zipfile.ZipInfo("classes.dex")
    zi_dex.date_time = (2026, 10, 23, 0, 0, 0)
    zi_dex.compress_type = zipfile.ZIP_DEFLATED
    zout.writestr(zi_dex, dex_bytes)
    for info, data in entries:
        zi = zipfile.ZipInfo(info.filename)
        zi.date_time = (2026, 10, 23, 0, 0, 0)
        if info.filename == "resources.arsc" or info.filename.endswith(".png"):
            zi.compress_type = zipfile.ZIP_STORED
            header_offset = zout.fp.tell()
            fname_len = len(zi.filename.encode("utf-8"))
            data_start = header_offset + 30 + fname_len
            pad = (4 - (data_start % 4)) % 4
            zi.extra = b"\x00" * pad
        else:
            zi.compress_type = zipfile.ZIP_DEFLATED
            zi.extra = b""
        zout.writestr(zi, data)
PYEOF

echo "[5/5] Signing and verifying APK (V1 + V2 + V3)..."
javac -g:none -cp "$APKSIG" -d "$BUILD_TMP/signer_classes" "$PROJ_DIR/build-tools/signer_src/SignApk.java"
java -cp "$BUILD_TMP/signer_classes:$APKSIG" SignApk \
    "$KEYSTORE" \
    "$BUILD_TMP/aligned.apk" \
    "$PROJ_DIR/HyperHDR-v24.0.0-beta.0-AndroidTV.apk"

rm -rf "$BUILD_TMP"
echo "Done! Output APK: $PROJ_DIR/HyperHDR-v24.0.0-beta.0-AndroidTV.apk"
