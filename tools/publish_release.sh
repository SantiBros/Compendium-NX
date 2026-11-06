#!/bin/bash
# publish_release.sh: run by CI after a push to main that built and tested, with this build's zip in dist/.
#
# - The version in VERSION has notes (docs/releases/v<version>.md) and no release yet: create the release, with
#   the tag v<version> on this commit, Compendium-<version>.zip as its file and the notes as its text. Versions
#   0.0.x are pre-releases.
# - Every release that has notes in docs/releases/: its file stays the one that was released, except that a release
#   from before the zip (only a Compendium.nro) gets the zip, made from its own .nro, in place of the bare .nro. Its
#   text follows the notes file, so notes are fixed by committing them.
#
# In the notes, {NRO_SHA256} stands for the SHA-256 of the release's Compendium.nro.
# Needs the gh CLI with GH_TOKEN, and unzip, run in a checkout of the repository.
set -euo pipefail
current=$(head -n1 VERSION | tr -d '[:space:]')
work=$(mktemp -d)

# The notes of tag $1 with {NRO_SHA256} filled in from the zip $2, written to $work/notes.md.
fill_notes() {
    local notes="docs/releases/$1.md" sha=""
    if grep -qF '{NRO_SHA256}' "$notes"; then
        sha=$(unzip -p "$2" switch/Compendium/Compendium.nro | sha256sum | cut -d' ' -f1)
    fi
    sed "s/{NRO_SHA256}/$sha/g" "$notes" > "$work/notes.md"
}

# Create the current version's release.
tag="v$current"
if [ -f "docs/releases/$tag.md" ] && ! gh release view "$tag" > /dev/null 2>&1; then
    fill_notes "$tag" "dist/Compendium-$current.zip"
    pre=()
    [[ $current == 0.0.* ]] && pre=(--prerelease)
    gh release create "$tag" "dist/Compendium-$current.zip" --target "$GITHUB_SHA" --title "Compendium $tag" \
        --notes-file "$work/notes.md" "${pre[@]}"
    echo "$tag: released"
fi

# Keep every release that has notes up to date.
for notes in docs/releases/v*.md; do
    tag=$(basename "$notes" .md)
    version=${tag#v}
    zip="Compendium-$version.zip"
    gh release view "$tag" > /dev/null 2>&1 || continue
    assets=$(gh release view "$tag" --json assets --jq '.assets[].name')
    dir="$work/$tag"
    mkdir -p "$dir"
    if grep -qxF "$zip" <<< "$assets"; then
        gh release download "$tag" --pattern "$zip" --dir "$dir"
    elif grep -qxF Compendium.nro <<< "$assets"; then
        gh release download "$tag" --pattern Compendium.nro --dir "$dir"
        bash tools/make_release.sh "$version" "$dir/Compendium.nro" "$dir"
        gh release upload "$tag" "$dir/$zip"
        gh release delete-asset "$tag" Compendium.nro --yes
        echo "$tag: its Compendium.nro is now packed as $zip"
    else
        echo "$tag: no $zip and no Compendium.nro, left as it is"
        continue
    fi
    fill_notes "$tag" "$dir/$zip"
    if [ "$(gh release view "$tag" --json body --jq .body)" != "$(cat "$work/notes.md")" ]; then
        gh release edit "$tag" --notes-file "$work/notes.md"
        echo "$tag: release notes updated"
    fi
done
echo "releases up to date"
