#!/bin/bash
# Removes the SimDash kiosk and puts the normal Ubuntu desktop login back.
set -e
if [ "$(id -u)" != 0 ]; then echo "Run with sudo: sudo $0"; exit 1; fi
KUSER="${SUDO_USER:-}"

if [ -f /etc/gdm3/custom.conf.simdash-bak ]; then
  mv /etc/gdm3/custom.conf.simdash-bak /etc/gdm3/custom.conf
fi
if [ -n "$KUSER" ]; then
  AS=/var/lib/AccountsService/users/$KUSER
  if [ -f "$AS.simdash-bak" ]; then mv "$AS.simdash-bak" "$AS"
  elif [ -f "$AS" ]; then sed -i '/^X\?Session=simdash-kiosk$/d' "$AS"; fi
fi
rm -f /etc/lightdm/lightdm.conf.d/50-simdash-kiosk.conf
rm -f /usr/share/xsessions/simdash-kiosk.desktop /usr/local/bin/simdash-kiosk
rm -rf /usr/share/simdash-kiosk
rm -f /etc/NetworkManager/conf.d/99-simdash-wifi-powersave-off.conf
echo "Kiosk removed (settings kept in /etc/simdash-kiosk.conf). Reboot: sudo reboot"
