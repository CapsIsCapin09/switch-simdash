#!/bin/bash
# Turns Switchroot L4T Ubuntu into a SimHub dash kiosk:
# auto-login straight into a full-screen browser, no screen blanking,
# Wi-Fi power saving off. Safe to run again. Undo with uninstall-kiosk.sh.
#
#   sudo ./install-kiosk.sh [dash-url]
set -e

HERE="$(cd "$(dirname "$0")" && pwd)"

if [ "$(id -u)" != 0 ]; then
  echo "Run with sudo: sudo $0 $*"; exit 1
fi
KUSER="${SUDO_USER:-}"
if [ -z "$KUSER" ] || [ "$KUSER" = root ]; then
  echo "Run this with sudo from your normal user account (the one that will auto-login)."; exit 1
fi

echo "==> Installing packages"
apt-get update
apt-get install -y openbox unclutter x11-xserver-utils x11-utils python3 curl
if ! command -v chromium >/dev/null && ! command -v chromium-browser >/dev/null && ! command -v firefox >/dev/null; then
  apt-get install -y chromium || apt-get install -y chromium-browser || apt-get install -y firefox
fi

echo "==> Installing kiosk files"
install -m 755 "$HERE/simdash-kiosk" /usr/local/bin/simdash-kiosk
install -d /usr/share/simdash-kiosk
install -m 644 "$HERE/loader.html" /usr/share/simdash-kiosk/loader.html
install -d /usr/share/xsessions
install -m 644 "$HERE/simdash-kiosk.desktop" /usr/share/xsessions/simdash-kiosk.desktop
if [ ! -f /etc/simdash-kiosk.conf ]; then
  install -m 644 "$HERE/simdash-kiosk.conf" /etc/simdash-kiosk.conf
fi
if [ -n "${1:-}" ]; then
  sed -i "s|^DASH_URL=.*|DASH_URL=\"$1\"|" /etc/simdash-kiosk.conf
fi

echo "==> Turning off Wi-Fi power saving"
install -d /etc/NetworkManager/conf.d
install -m 644 "$HERE/wifi-powersave-off.conf" /etc/NetworkManager/conf.d/99-simdash-wifi-powersave-off.conf

echo "==> Setting up auto-login for $KUSER into the SimDash Kiosk session"
if [ -d /etc/gdm3 ]; then
  CONF=/etc/gdm3/custom.conf
  [ -f "$CONF.simdash-bak" ] || cp "$CONF" "$CONF.simdash-bak" 2>/dev/null || true
  python3 - "$CONF" "$KUSER" <<'PY'
import configparser, sys
path, user = sys.argv[1], sys.argv[2]
c = configparser.ConfigParser(strict=False, interpolation=None)
c.optionxform = str
c.read(path)
if not c.has_section("daemon"):
    c.add_section("daemon")
c["daemon"]["AutomaticLoginEnable"] = "true"
c["daemon"]["AutomaticLogin"] = user
c["daemon"]["WaylandEnable"] = "false"
with open(path, "w") as f:
    c.write(f)
PY
  install -d /var/lib/AccountsService/users
  AS=/var/lib/AccountsService/users/$KUSER
  [ -f "$AS.simdash-bak" ] || cp "$AS" "$AS.simdash-bak" 2>/dev/null || true
  python3 - "$AS" <<'PY'
import configparser, sys
path = sys.argv[1]
c = configparser.ConfigParser(strict=False, interpolation=None)
c.optionxform = str
c.read(path)
if not c.has_section("User"):
    c.add_section("User")
c["User"]["Session"] = "simdash-kiosk"
c["User"]["XSession"] = "simdash-kiosk"
with open(path, "w") as f:
    c.write(f)
PY
fi
if [ -d /etc/lightdm ]; then
  install -d /etc/lightdm/lightdm.conf.d
  cat > /etc/lightdm/lightdm.conf.d/50-simdash-kiosk.conf <<CONF
[Seat:*]
autologin-user=$KUSER
autologin-user-timeout=0
autologin-session=simdash-kiosk
user-session=simdash-kiosk
CONF
  groupadd -f autologin
  gpasswd -a "$KUSER" autologin >/dev/null
fi
if [ ! -d /etc/gdm3 ] && [ ! -d /etc/lightdm ]; then
  echo "!! No GDM or LightDM found. Auto-login was not set up; pick 'SimDash Kiosk' at the login screen."
fi

echo "==> Stopping the lock screen and blanking in the normal desktop too"
sudo -u "$KUSER" dbus-launch gsettings set org.gnome.desktop.session idle-delay 0 2>/dev/null || true
sudo -u "$KUSER" dbus-launch gsettings set org.gnome.desktop.screensaver lock-enabled false 2>/dev/null || true

echo
echo "Done. Dash URL: $(grep ^DASH_URL /etc/simdash-kiosk.conf)"
echo "Reboot to start the kiosk:  sudo reboot"
echo "Get a terminal back:        Ctrl+Alt+F3 on a USB keyboard, or SSH in"
echo "Back to the normal desktop: sudo $HERE/uninstall-kiosk.sh"
