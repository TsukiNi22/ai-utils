#!/usr/bin/env bash
# claude-cluster for Android: its APK built with the Android SDK alone (aapt2, javac, d8, apksigner; no Gradle).
# Usage: android/build.sh <version> <claude-cluster binary> [out dir]   (default: ~/.local/share/claude-cluster/app)
# The signing key is made once (~/.config/claude-cluster/android/release.jks): the updates of the app need the same one.
set -euo pipefail

version="${1:?version}"
binary="${2:?claude-cluster binary (its icon)}"
out="${3:-${XDG_DATA_HOME:-$HOME/.local/share}/claude-cluster/app}"
here="$(cd "$(dirname "$0")" && pwd)"
sdk="${ANDROID_HOME:-${ANDROID_SDK_ROOT:-$HOME/Android/Sdk}}"

# the newest platform and build tools installed
platform="$(ls -d "$sdk"/platforms/android-* 2> /dev/null | sort -V | tail -1 || true)"
tools="$(ls -d "$sdk"/build-tools/* 2> /dev/null | sort -V | tail -1 || true)"
if [[ -z "$platform" || -z "$tools" || ! -x "$tools/aapt2" ]] || ! command -v javac > /dev/null || ! command -v keytool > /dev/null; then
    echo "skip the Android app: the Android SDK (platforms, build-tools) and a JDK are needed (ANDROID_HOME)"
    exit 0
fi
android="$platform/android.jar"
IFS=. read -r major minor patch <<< "$version"
# its Android versionCode, always growing: up to 999 patches a minor and 999 minors a major (1.4.123 < 1.5.0)
code=$(( major * 1000000 + minor * 1000 + ${patch:-0} ))

work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
command mkdir -p "$work/res/mipmap-mdpi" "$work/res/mipmap-hdpi" "$work/res/mipmap-xhdpi" "$work/res/mipmap-xxhdpi" "$work/res/mipmap-xxxhdpi" \
    "$work/gen/com/claudecluster/app" "$work/classes" "$out"

# its icon: ^_^ of claude-cluster on a dark rounded square
"$binary" icon happy > "$work/icon.svg" < /dev/null
for pair in mdpi:48 hdpi:72 xhdpi:96 xxhdpi:144 xxxhdpi:192; do
    density="${pair%%:*}"
    size="${pair##*:}"
    if command -v magick > /dev/null; then
        magick -background none "$work/icon.svg" -resize "$((size * 82 / 100))x$((size * 82 / 100))" -gravity center \
            \( -size "${size}x${size}" xc:none -fill '#1e2128' -draw "roundrectangle 0,0,$((size - 1)),$((size - 1)),$((size / 5)),$((size / 5))" \) \
            +swap -composite "$work/res/mipmap-$density/ic_launcher.png"
    elif command -v rsvg-convert > /dev/null; then
        rsvg-convert -w "$size" -h "$size" "$work/icon.svg" -o "$work/res/mipmap-$density/ic_launcher.png"
    else
        echo "skip the Android app: magick or rsvg-convert is needed for its icon"
        exit 0
    fi
done

# the notifications: the status bar icon is a flat alpha mask (white on transparent: the terminals and its face cut out of the front one,
# 24 dp at every density) and the large icon the coloured logo (the launcher one, 256 px)
if command -v magick > /dev/null; then
    sed -e 's/#8a93a6/#ffffff/g; s/#6d8ef0/#ffffff/g; s/#15171b/#000000/g; s/ opacity="[.0-9]*"//g' "$work/icon.svg" > "$work/stat.svg"
    for pair in mdpi:24 hdpi:36 xhdpi:48 xxhdpi:72 xxxhdpi:96; do
        density="${pair%%:*}"
        size="${pair##*:}"
        command mkdir -p "$work/res/drawable-$density"
        magick -background none -density 384 "$work/stat.svg" -resize "${size}x${size}" -gravity center -extent "${size}x${size}" "$work/stat.png"
        # alpha = its alpha x its brightness (the black face: a hole), the colour white
        magick "$work/stat.png" \( +clone -alpha extract \) \( -clone 0 -alpha off -colorspace Gray \) -delete 0 -compose Multiply -composite "$work/stat-mask.png"
        magick -size "${size}x${size}" xc:white "$work/stat-mask.png" -alpha off -compose CopyOpacity -composite "$work/res/drawable-$density/ic_stat.png"
    done
    command mkdir -p "$work/res/drawable-nodpi"
    magick -background none -density 768 "$work/icon.svg" -resize 210x210 -gravity center \
        \( -size 256x256 xc:none -fill '#1e2128' -draw "roundrectangle 0,0,255,255,52,52" \) +swap -composite "$work/res/drawable-nodpi/ic_large.png"
fi

# resources, then the code (its version as BuildConfig), then the dex, then signed
"$tools/aapt2" compile --dir "$work/res" -o "$work/res.zip"
"$tools/aapt2" link -I "$android" --manifest "$here/AndroidManifest.xml" -o "$work/base.apk" --java "$work/gen" \
    --min-sdk-version 29 --target-sdk-version 35 --version-code "$code" --version-name "$version" "$work/res.zip"
printf 'package com.claudecluster.app;\npublic final class BuildConfig {\n    public static final String VERSION = "%s";\n    public static final int CODE = %d;\n    public static final String BUILT = "%s";\n}\n' "$version" "$code" "$(date +%Y-%m-%d\ %H:%M)" \
    > "$work/gen/com/claudecluster/app/BuildConfig.java"
javac --release 17 -nowarn -classpath "$android" -d "$work/classes" $(find "$here/src" "$work/gen" -name '*.java') 2> "$work/javac.log" \
    || { cat "$work/javac.log"; exit 1; }
"$tools/d8" --release --min-api 29 --lib "$android" --output "$work" $(find "$work/classes" -name '*.class')
(cd "$work" && zip -q -j base.apk classes.dex)
"$tools/zipalign" -f -p 4 "$work/base.apk" "$work/aligned.apk"
key="${XDG_CONFIG_HOME:-$HOME/.config}/claude-cluster/android/release.jks"
if [[ ! -f "$key" ]]; then
    command mkdir -p "$(dirname "$key")"
    keytool -genkeypair -keystore "$key" -alias claude-cluster -keyalg EC -groupname secp256r1 -validity 10000 \
        -storepass claude-cluster -keypass claude-cluster -dname "CN=claude-cluster" > /dev/null 2>&1
    chmod 600 "$key"
fi
"$tools/apksigner" sign --ks "$key" --ks-pass pass:claude-cluster --key-pass pass:claude-cluster --out "$out/claude-cluster.apk" "$work/aligned.apk"
rm -f "$out/claude-cluster.apk.idsig"
echo "$version" > "$out/version.txt"
echo "android app $version built -> $out/claude-cluster.apk"
