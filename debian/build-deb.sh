#!/usr/bin/env bash
# Builds a ghtop .deb for the current host distribution and drops it under
# debian/build/. Supported targets: Ubuntu focal/jammy/noble and Debian
# bookworm/trixie. Run on the matching host (or inside a debootstrap chroot)
# for the distribution you want to target.
#
# The single debian/changelog in this repo carries a generic UNRELEASED
# entry; this script stamps it with the target distribution and a per-distro
# version suffix (~ubuntu20.04 / ~ubuntu22.04 / ~ubuntu24.04 / ~deb12 /
# ~deb13) before invoking dpkg-buildpackage. The original changelog is
# restored afterwards.
#
# Focal note: debhelper 13 (from focal-backports) and a modern cuda-nvml-dev-*
# package (from the NVIDIA CUDA repo) are required in the build environment;
# the script auto-injects the CUDA stub paths so the build picks up the newer
# NVML symbols.
set -euo pipefail

cd "$(dirname "$0")/.."

. /etc/os-release
dist="${VERSION_CODENAME:-${UBUNTU_CODENAME:-unknown}}"
ver_id="${VERSION_ID:-0}"
os_id="${ID:-unknown}"

case "$dist" in
   focal|jammy|noble) suffix="ubuntu${ver_id}" ;;
   bookworm)          suffix="deb12" ;;
   trixie)            suffix="deb13" ;;
   *) echo "Unsupported distribution: $dist (expected focal/jammy/noble/bookworm/trixie)" >&2; exit 1 ;;
esac

base_version="$(dpkg-parsechangelog -SVersion)"
target_version="${base_version}~${suffix}"

# Focal ships libnvidia-ml-dev 10.1 (CUDA 10.1 / driver 418) which lacks the
# nvmlDeviceGet{Compute,Graphics}RunningProcesses_v3 symbols we use. If a
# modern cuda-nvml-dev-* is also installed under /usr/local/cuda-*, prepend
# its include/stub paths so the build picks up the newer NVML header and
# link-time stub instead. The runtime dep stays libnvidia-ml.so.1, resolved
# from whichever NVIDIA driver is installed on the target host.
if [ "$dist" = focal ]; then
   cuda_inc=$(ls -d /usr/local/cuda-*/targets/x86_64-linux/include 2>/dev/null | sort -V | tail -1 || true)
   cuda_stub=$(ls -d /usr/local/cuda-*/targets/x86_64-linux/lib/stubs 2>/dev/null | sort -V | tail -1 || true)
   if [ -n "$cuda_inc" ] && [ -f "$cuda_inc/nvml.h" ] && [ -n "$cuda_stub" ]; then
      export DEB_CPPFLAGS_APPEND="${DEB_CPPFLAGS_APPEND:-} -I${cuda_inc}"
      export DEB_LDFLAGS_APPEND="${DEB_LDFLAGS_APPEND:-} -L${cuda_stub}"
      echo "focal: using NVML from ${cuda_inc%/include}"
   else
      echo "focal: warning: no modern cuda-nvml-dev-* found under /usr/local/cuda-*;" >&2
      echo "       install e.g. cuda-nvml-dev-12-9 from the NVIDIA CUDA repo or the build" >&2
      echo "       will fail with undefined nvmlDeviceGet*RunningProcesses_v3 references." >&2
   fi
fi

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
