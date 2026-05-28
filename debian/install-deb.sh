#!/usr/bin/env bash
set -Eeuo pipefail

VERSION="3.6.0-r1"
APP="ghtop"
# Use github.com/.../raw/... (redirects to media.githubusercontent.com) instead of
# raw.githubusercontent.com directly: the latter has a ~5min edge cache keyed by
# branch ref that frequently serves a stale .deb right after a push.
BASE_URL="https://github.com/rosepetal-ai/ghtop/raw/refs/heads/main/debian/build"

log()  { printf '[%s] %s\n' "$APP" "$*"; }
warn() { printf '[%s][warn] %s\n' "$APP" "$*" >&2; }
die()  { printf '[%s][error] %s\n' "$APP" "$*" >&2; exit 1; }

if [ "${EUID:-$(id -u)}" -eq 0 ]; then
  SUDO=()
else
  SUDO=(sudo)
fi

run_root() { "${SUDO[@]}" "$@"; }

require_sudo() {
  [ "${#SUDO[@]}" -gt 0 ] && sudo -v || true
}

detect_suffix() {
  [ -f /etc/os-release ] || die "Cannot detect OS: /etc/os-release not found"
  # shellcheck source=/dev/null
  . /etc/os-release

  case "$ID" in
    ubuntu)
      case "$VERSION_ID" in
        20.04) echo "ubuntu20.04" ;;
        22.04) echo "ubuntu22.04" ;;
        24.04) echo "ubuntu24.04" ;;
        *) die "Unsupported Ubuntu version: $VERSION_ID (supported: 20.04, 22.04, 24.04)" ;;
      esac
      ;;
    debian)
      case "${VERSION_ID:-}" in
        13) echo "deb13" ;;
        *) die "Unsupported Debian version: ${VERSION_ID:-unknown} (supported: 13)" ;;
      esac
      ;;
    *)
      die "Unsupported OS: $ID (supported: ubuntu 22.04/24.04, debian 13)"
      ;;
  esac
}

require_sudo

suffix=$(detect_suffix)
deb_name="ghtop_${VERSION}~${suffix}_amd64.deb"
url="${BASE_URL}/${deb_name}"
tmp_deb=$(mktemp /tmp/ghtop-XXXXXX.deb)

log "Detected: $suffix"
log "Downloading $deb_name..."
curl -fSL "$url" -o "$tmp_deb"

log "Installing..."
if ! run_root dpkg -i "$tmp_deb"; then
  log "Fixing missing dependencies..."
  run_root apt-get install -f -y
fi

rm -f "$tmp_deb"
log "ghtop $VERSION installed successfully."
log "Run: ghtop"

