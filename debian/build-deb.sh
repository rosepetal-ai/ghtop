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

dist="$(. /etc/os-release && echo "${VERSION_CODENAME:-${UBUNTU_CODENAME:-unknown}}")"
ver_id="$(. /etc/os-release && echo "${VERSION_ID:-0.0}")"

case "$dist" in
   jammy|noble) ;;
   *) echo "Unsupported distribution: $dist (expected jammy or noble)" >&2; exit 1 ;;
esac

base_version="$(dpkg-parsechangelog -SVersion)"
target_version="${base_version}~ubuntu${ver_id}"

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

mkdir -p ../ghtop-debs
mv ../ghtop_${target_version}_*.deb     ../ghtop-debs/
mv ../ghtop_${target_version}_*.buildinfo ../ghtop-debs/ 2>/dev/null || true
mv ../ghtop_${target_version}_*.changes   ../ghtop-debs/ 2>/dev/null || true

echo
echo "Built ghtop_${target_version} for ${dist}:"
ls -la ../ghtop-debs/ghtop_${target_version}_*.deb
