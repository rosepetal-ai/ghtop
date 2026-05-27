#!/usr/bin/env bash
# Builds a ghtop .deb for the current host distribution and drops it under
# ../ghtop-debs/. Run on a noble (24.04) host to produce a noble .deb, or
# inside an ubuntu:22.04 docker container to produce a jammy .deb.
#
# The single debian/changelog in this repo carries a generic UNRELEASED
# entry; this script stamps it with the target distribution and a per-distro
# version suffix (~ubuntu22.04 / ~ubuntu24.04) before invoking
# dpkg-buildpackage. The original changelog is restored afterwards.
set -euo pipefail

cd "$(dirname "$0")/.."

. /etc/os-release
dist="${VERSION_CODENAME:-${UBUNTU_CODENAME:-unknown}}"
ver_id="${VERSION_ID:-0}"
os_id="${ID:-unknown}"

case "$dist" in
   jammy|noble) suffix="ubuntu${ver_id}" ;;
   bookworm)    suffix="deb12" ;;
   trixie)      suffix="deb13" ;;
   *) echo "Unsupported distribution: $dist (expected jammy/noble/bookworm/trixie)" >&2; exit 1 ;;
esac

base_version="$(dpkg-parsechangelog -SVersion)"
target_version="${base_version}~${suffix}"

# Stash original changelog, stamp distro + version, restore on exit.
# dpkg-source --after-build may delete stray *.orig in the source tree, so
# keep the backup outside debian/.
backup="$(mktemp)"
trap 'mv "$backup" debian/changelog' EXIT
cp debian/changelog "$backup"

DEBEMAIL="${DEBEMAIL:-dev@rosepetal.ai}" \
DEBFULLNAME="${DEBFULLNAME:-Rosepetal}" \
   dch --force-bad-version --newversion "$target_version" --distribution "$dist" \
       "Automated build for ${dist}." >/dev/null
dch --release "" >/dev/null

dpkg-buildpackage -us -uc -b

mkdir -p debian/build
mv ../ghtop_${target_version}_*.deb     debian/build/
mv ../ghtop_${target_version}_*.buildinfo debian/build/ 2>/dev/null || true
mv ../ghtop_${target_version}_*.changes   debian/build/ 2>/dev/null || true

echo
echo "Built ghtop_${target_version} for ${dist}:"
ls -la debian/build/ghtop_${target_version}_*.deb
