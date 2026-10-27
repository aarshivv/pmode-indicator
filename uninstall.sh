#!/bin/bash
# Remove everything install.sh added. Unloading the module touches nothing on the EC.
set -uo pipefail

cd "$(dirname "$0")"
NAME=pmode-indicator
VERSION=$(sed -n 's/^PACKAGE_VERSION="\(.*\)"/\1/p' dkms.conf)

omarchy plugin disable aarshiv.pmode 2>/dev/null
rm -rf ~/.config/omarchy/plugins/aarshiv.pmode
omarchy-shell shell rescanPlugins >/dev/null 2>&1

sudo modprobe -r pmode_indicator 2>/dev/null
sudo rm -f /etc/modules-load.d/pmode_indicator.conf /usr/local/bin/pmode-status
sudo dkms remove "$NAME/$VERSION" --all 2>/dev/null
sudo rm -rf "/usr/src/$NAME-$VERSION"

echo "Removed. (dkms itself was left installed.)"
