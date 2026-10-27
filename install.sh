#!/bin/bash
# Install the read-only EVO-X2 EC driver (via DKMS), the status script,
# and the Omarchy bar widget. Run as your normal user; asks for sudo.
set -euo pipefail

cd "$(dirname "$0")"
NAME=pmode-indicator
VERSION=$(sed -n 's/^PACKAGE_VERSION="\(.*\)"/\1/p' dkms.conf)
SRC=/usr/src/$NAME-$VERSION

echo "==> Building and verifying locally (no EC writes allowed)"
make clean >/dev/null
make

echo "==> Installing dkms if missing"
pacman -Q dkms &>/dev/null || sudo pacman -S --needed --noconfirm dkms

echo "==> Registering $NAME $VERSION with DKMS"
sudo dkms remove "$NAME/$VERSION" --all &>/dev/null || true
sudo rm -rf "$SRC"
sudo mkdir -p "$SRC"
sudo cp -r Kbuild Makefile dkms.conf src "$SRC/"
sudo dkms install "$NAME/$VERSION"

echo "==> Loading module now and on every boot"
echo pmode_indicator | sudo tee /etc/modules-load.d/pmode_indicator.conf >/dev/null
# Swap any test-loaded (insmod) copy for the DKMS-installed one
sudo modprobe -r pmode_indicator 2>/dev/null || true
sudo modprobe pmode_indicator

echo "==> Installing status script"
sudo install -m 0755 bin/pmode-status /usr/local/bin/pmode-status

echo "==> Installing bar widget"
mkdir -p ~/.config/omarchy/plugins
rm -rf ~/.config/omarchy/plugins/aarshiv.pmode
cp -r omarchy-plugin/aarshiv.pmode ~/.config/omarchy/plugins/
# The shell needs a moment to register a newly copied plugin
for _ in 1 2 3 4 5; do
  omarchy-shell shell rescanPlugins >/dev/null 2>&1 || true
  omarchy plugin list 2>/dev/null | grep -q '^aarshiv.pmode ' && break
  sleep 1
done
omarchy plugin enable aarshiv.pmode --section right
# Hot reload doesn't recreate a widget already on the bar; restart to load new code
omarchy restart shell >/dev/null 2>&1 || true

echo
pmode-status
