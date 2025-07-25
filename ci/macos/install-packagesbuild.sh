#! /bin/bash

set -e

if which packagesbuild; then
	exit 0
fi

packages_url='http://www.nagater.net/obs-studio/Packages.dmg'
packages_hash='6afdd25386295974dad8f078b8f1e41cabebd08e72d970bf92f707c7e48b16c9'

for ((retry=5; retry>0; retry--)); do
	curl -o Packages.dmg $packages_url
	sha256sum -c - <<<"$packages_hash Packages.dmg" && break
done
if ((retry == 0)); then
	echo "Error: Failed to get Packages.dmg" >&2
	exit 1
fi

hdiutil attach -noverify Packages.dmg
packages_volume="$(hdiutil info -plist | grep '<string>/Volumes/Packages' | sed 's/.*<string>\(\/Volumes\/[^<]*\)<\/string>/\1/')"

sudo installer -pkg "${packages_volume}/packages/Packages.pkg" -target /
hdiutil detach "${packages_volume}"
