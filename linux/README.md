# SimHub dash on Switch with Linux (L4T Ubuntu)

This turns your Switch into a dedicated SimHub dash by dual-booting [Switchroot L4T Ubuntu](https://wiki.switchroot.org/wiki/linux) next to Atmosphere. Ubuntu boots straight into a full-screen browser showing SimHub's web dash (`http://192.168.1.145:8888`), with auto-login, no screen blanking and Wi-Fi power saving off. Horizon and your emuMMC stay; you pick which one to boot in hekate.

Any SimHub dashboard works, exactly like on a tablet.

> [!WARNING]
> **Step 2 wipes the whole SD card, including a partition-based emuMMC.** Do the backup in step 1 first and check it opens. Nothing in steps 3 to 6 deletes data.

## What you need

- Your V1 Switch with hekate (6.x or newer) and Atmosphere, as now.
- A microSD card with room for your emuMMC (about 30 GB on a V1), Linux (16 GB or more) and your games. 128 GB or larger is comfortable.
- A PC with a microSD reader, about 70 GB of free space for the backup, [7-Zip](https://www.7-zip.org/), and [NxNandManager](https://github.com/eliboa/NxNandManager/releases) if your emuMMC is a partition.
- For first-time Linux setup: Joy-Cons attached (Ubuntu's setup has an on-screen keyboard and the touch screen works), or a USB keyboard on the dock.

## 1. Back up the SD card

1. Power off the Switch and put the SD card in the PC.
2. Copy **every file and folder** on the card to a folder on the PC, for example `C:\SwitchBackup\files`.
3. Find out what kind of emuMMC you have. Boot hekate, tap **emuMMC**, and look at the info line:
   - **SD File** (an `emuMMC/SD00` folder full of files): it is already in the files you copied. Nothing more to do.
   - **SD Partition**: it lives outside the files, so back it up separately. Put the SD card in the PC, open **NxNandManager as administrator**, choose **File > Open drive**, pick the SD card's emuMMC, and use **Backup** to dump **BOOT0**, **BOOT1** and **RAWNAND** to `C:\SwitchBackup\emummc`. Keep NxNandManager open on the result until it says the dump finished and the hash matched.
4. Optional but recommended: make a full image of the card with [Win32 Disk Imager](https://sourceforge.net/projects/win32diskimager/) (**Read** into a `.img` file). With this you can always put the card back exactly as it was.

## 2. Partition the SD card in hekate (wipes the card)

1. Download the newest **L4T Ubuntu Noble** `.7z` from the [Switchroot install guide](https://wiki.switchroot.org/wiki/linux/l4t-ubuntu-noble-installation-guide) on the PC. Don't extract it yet.
2. Put the SD card back in the Switch and boot into hekate.
3. Tap **Tools > Partition SD Card**.
4. Set the sliders:
   - **emuMMC (RAW)**: *only if* your emuMMC was a partition, set it to **29 GiB (Full)**, the V1's eMMC size. Leave it at 0 for a file-based emuMMC.
   - **Linux (EXT4)**: 16 GiB or more.
   - Leave Android at 0. Whatever is left stays as the normal FAT32 storage.
5. Tap **Next Step**, read hekate's warning, then **Start** and wait. Hekate keeps its own `bootloader` folder, but treat the rest of the card as erased.

## 3. Copy files back and flash Linux

1. When partitioning finishes, hekate offers **SD UMS**. Tap it and connect the Switch to the PC by USB. The FAT32 part shows up as a drive. (Or power off and use the card reader.)
2. Copy your backup files from `C:\SwitchBackup\files` back to the card. Let it replace files if asked.
3. Extract the Ubuntu `.7z` to the root of the card with 7-Zip. It adds `bootloader` and `switchroot` folders. Let it merge with what is there.
4. Eject the drive, go back to **Tools > Partition SD Card** in hekate and tap **Flash Linux**. Confirm and wait.

## 4. Put your emuMMC back

- **File-based**: nothing to do. Your `emuMMC` folder came back with the files in step 3, and hekate finds it again.
- **Partition-based**:
  1. In hekate, tap **emuMMC > Create emuMMC > SD Partition > Part 1** to set up the partition hekate made in step 2. This creates a fresh copy of your sysMMC, not your old one.
  2. Power off, put the SD card in the PC, open NxNandManager as administrator, **File > Open drive**, pick the SD card's emuMMC, and use **Restore** with your `BOOT0`, `BOOT1` and `RAWNAND` dumps from step 1.
  3. Boot Atmosphere on emuMMC and check that your games and saves are there before going on.

## 5. First Ubuntu boot

1. In hekate, tap **More Configs** and pick **L4T Ubuntu Noble**. The first boot takes a few minutes.
2. Follow the setup screens: language, keyboard, **Wi-Fi** (pick your home network, 5 GHz if you have it), and your user name and password. Remember the password.
3. Open **Terminal** and update: `sudo apt update && sudo apt full-upgrade -y`, then reboot.

## 6. Install the SimDash kiosk

In Ubuntu's Terminal:

```sh
cd ~
wget https://github.com/CapsIsCapin09/switch-simdash/releases/download/linux-kiosk-v1.0.0/simdash-linux-kiosk.tar.gz
tar xzf simdash-linux-kiosk.tar.gz
cd simdash-linux-kiosk
sudo ./install-kiosk.sh http://192.168.1.145:8888/
sudo reboot
```

After the reboot, Ubuntu logs in by itself and shows a black **Waiting for SimHub…** screen until SimHub answers, then the SimHub dash list. Tap a dash to open it. The browser restarts on its own if it ever closes.

### On the PC

- In SimHub, the web dash server runs on port 8888 by default. Check it with `http://192.168.1.145:8888` in a phone browser on the same Wi-Fi.
- If the phone can't open it, allow SimHub through Windows Firewall on **Private** networks.
- Give the PC a fixed address (a DHCP reservation in your router) so `192.168.1.145` never changes.

### Open one dash every time

Open the dash you want on a PC or phone browser, copy its full address, then on the Switch:

```sh
sudo nano /etc/simdash-kiosk.conf     # set DASH_URL="that address"
sudo reboot
```

The same file sets the browser (`chromium` or `firefox`), page zoom, and mouse pointer hiding.

## Daily use

- **Boot the dash**: hekate > More Configs > L4T Ubuntu Noble.
- **Boot games**: hekate > Launch > Atmosphere emuMMC, as now.
- **Boot straight to the dash**: in hekate, **Options > Auto Boot** and pick the Ubuntu entry. Hold **Volume -** while powering on to get the hekate menu instead.
- **Get out of the kiosk**: press **Ctrl+Alt+F3** on a USB keyboard for a text login, or SSH in from the PC (`sudo apt install openssh-server` first). To go back to the normal Ubuntu desktop for good, run `sudo ~/simdash-linux-kiosk/uninstall-kiosk.sh` and reboot.
- **Shut down**: hold the power button, or run `sudo poweroff` over SSH.

## Performance and latency tips

- **Stay on the charger or dock.** Linux on the Switch drains the battery quickly, and some power profiles slow down on battery.
- **Use 5 GHz Wi-Fi** with the router close by. The installer turns off Wi-Fi power saving, which removes most lag spikes.
- **Pick lighter dashes.** Dashes with lots of animated gauges, blur or big images cost the most. Dashes designed at 1280x720 fit the Switch screen without scaling.
- **Lower the dash refresh rate** in SimHub's dash settings if the Switch stutters. A steady 30 fps feels better than an uneven 60.
- **Switchroot's power profiles**: Switchroot Ubuntu has a performance mode in its top-bar menu on the normal desktop. Set it there once, before installing the kiosk.
- **Chromium is the default** because it is usually faster than Firefox on the Switch's GPU. If a dash renders wrong, set `BROWSER="firefox"` in `/etc/simdash-kiosk.conf`.
- The kiosk writes a log to `~/.cache/simdash-kiosk.log` if something doesn't start.

## Files

| File | Installed to | What it does |
| --- | --- | --- |
| `install-kiosk.sh` | | Installs packages and everything below, sets auto-login. Safe to run again. |
| `uninstall-kiosk.sh` | | Undoes it and restores the normal login. |
| `simdash-kiosk` | `/usr/local/bin/` | The kiosk session: no blanking, hides the pointer, runs the browser full screen and restarts it. |
| `simdash-kiosk.desktop` | `/usr/share/xsessions/` | Adds **SimDash Kiosk** as a login session. |
| `simdash-kiosk.conf` | `/etc/` | Dash URL and options. |
| `loader.html` | `/usr/share/simdash-kiosk/` | "Waiting for SimHub" screen that opens the dash once SimHub answers. |
| `wifi-powersave-off.conf` | `/etc/NetworkManager/conf.d/` | Turns off Wi-Fi power saving. |
