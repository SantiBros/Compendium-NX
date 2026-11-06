#!/bin/bash
# make_release.sh <version> <Compendium.nro> <out dir>
#
# Packs a release the way players install it: one zip with the switch folder, ready to copy onto the root
# of the SD card.
#
#   <out dir>/Compendium-<version>.zip
#     switch/Compendium/
#       Compendium.nro    the app
#       README.txt        the player's guide (tools/package/README.txt, English and Spanish)
#       LICENSES.txt      Compendium's license and the third-party notices
#       inbox/            empty: mod archives go here
#   <out dir>/stage/      the same files, unzipped (CI keeps it as the run's artifact)
#
# Needs only bash, coreutils, sed and CMake (for the zip), so it runs in the devkitPro container too.
set -euo pipefail
version=$1 nro=$2 out=$3
here=$(cd "$(dirname "$0")/.." && pwd)
[ -f "$nro" ] || { echo "no such file: $nro" >&2; exit 1; }

rm -rf "$out/stage"
dir="$out/stage/switch/Compendium"
mkdir -p "$dir/inbox"
cp "$nro" "$dir/Compendium.nro"
sed "s/{VERSION}/$version/g" "$here/tools/package/README.txt" > "$dir/README.txt"
{
    echo "Compendium $version"
    echo "Copyright (C) 2026 SantiBros. Free software under the GNU General Public License v3.0, below."
    echo "Source code: https://github.com/SantiBros/Compendium-NX"
    echo
    cat "$here/resources/THIRD_PARTY_NOTICES.txt"
    echo
    echo "=============================================================================="
    echo "GNU General Public License v3.0 (Compendium's license)"
    echo "=============================================================================="
    cat "$here/LICENSE"
} > "$dir/LICENSES.txt"

zip="Compendium-$version.zip"
rm -f "$out/$zip"
(cd "$out/stage" && cmake -E tar cf "../$zip" --format=zip switch)
sha=$(sha256sum "$dir/Compendium.nro" | cut -d' ' -f1)
echo "$out/$zip"
echo "Compendium.nro SHA-256: $sha"
