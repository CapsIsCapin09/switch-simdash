# SimDash

A native SimHub digital dash for a modded Nintendo Switch (Atmosphere, sysMMC or emuMMC). It runs as a normal homebrew app from the Homebrew Menu. No web browser involved.

It has two modes, picked from its start menu:

- **Custom dash**: SimDash draws its own dashboard from live SimHub data (gear, speed, RPM lights, lap times, position, fuel, TC/ABS/bias, flags, tyre temps and pressures, water/oil temps).
- **SimHub dash (Moonlight)**: hands over to [Moonlight-Switch](https://github.com/XITRIX/Moonlight-Switch), which streams a SimHub dashboard from your PC. You pick and change dashboards in SimHub exactly like with a tablet.

## Download

Grab `switch-simdash.nro` from the latest [Actions build](../../actions/workflows/build.yml) (open the newest green run, download the **SimDash** artifact and unzip it) or from [Releases](../../releases).

Copy it to `/switch/switch-simdash.nro` on the SD card.

## Custom dash setup

### On the PC

1. Download `PropertyServer.dll` from the [SimHub Property Server releases](https://github.com/pre-martin/SimHubPropertyServer/releases) (needs SimHub 9.6 or newer).
2. Close SimHub, copy the DLL into the SimHub install folder (usually `C:\Program Files (x86)\SimHub`), then start SimHub.
3. When SimHub asks about the new plugin, enable it (or turn it on under **Settings > Plugins**).
4. Allow SimHub through Windows Firewall on private networks if Windows asks. The plugin listens on TCP port **18082**.
5. Find the PC's local IP: open a command prompt, run `ipconfig`, and note the IPv4 address (for example `192.168.1.50`).

### On the Switch

1. Launch SimDash from the Homebrew Menu.
2. Choose **PC address** and type the PC's IP.
3. Choose **Custom dash**.

Controls in the dash:

| Button | Action |
| --- | --- |
| L / R, left / right, or tap the screen | Switch page (Race, Car) |
| Y | Change PC address |
| B | Back to menu |
| + | Exit |

Settings are saved to `/config/simdash/config.ini`.

## SimHub dash (Moonlight) setup

This streams a real SimHub dashboard to the Switch. It takes more setup, but any dash you can show in SimHub works, and you switch dashes from SimHub.

### On the PC

1. Install [Sunshine](https://github.com/LizardByte/Sunshine/releases) and open its web page at `https://localhost:47990` to create a login.
2. Install a virtual display driver so the dash gets its own screen, for example [Virtual Display Driver](https://github.com/VirtualDrivers/Virtual-Display-Driver/releases). Set the new display to **1280x720** in Windows display settings and choose **Extend these displays**.
3. In Sunshine's web page, open **Troubleshooting**, find the virtual display's `device_id` under "Currently available display devices", paste it into **Configuration > Audio/Video > Output Name**, save and apply.
4. In SimHub, open **Dash Studio**, pick your dashboard and start it on the virtual display (full screen). Changing the dash there changes what the Switch shows.

### On the Switch

1. Download `Moonlight-Switch.nro` from [Moonlight-Switch releases](https://github.com/XITRIX/Moonlight-Switch/releases) and copy it to `/switch/Moonlight-Switch.nro`. (SimDash also looks in `/switch/Moonlight-Switch/` and `/switch/moonlight/`, or set `moonlight_path=` in the config file.)
2. Launch SimDash and choose **SimHub dash (Moonlight)**. Moonlight opens.
3. The first time, add your PC, enter the PIN in Sunshine's web page to pair, then start the **Desktop** app.

Tips:

- Launch through a game (hold **R** while starting a game to open the Homebrew Menu) rather than the Album. Moonlight is more stable there and gets more memory.
- Set **System Settings > Sleep Mode > Auto-Sleep** to **Never** while using the Switch as a dash.

## Limits

- The Property Server plugin sends updates 10 times per second. SimDash smooths the RPM lights between updates, but the custom dash is not as instant as a wired dash. The Moonlight mode runs at up to 60 fps.
- Property names follow SimHub's `DataCorePlugin` data, so values depend on what each game provides.

## Building

The GitHub Actions workflow builds the `.nro` with devkitPro's `devkita64` Docker image. To build locally, install devkitPro with `switch-dev`, `switch-sdl2` and `switch-sdl2_ttf`, then run `make`.

To try the dash on a Linux desktop without a Switch:

```sh
sudo apt install libsdl2-dev libsdl2-ttf-dev
make -f Makefile.desktop
python3 tools/fake_simhub.py &      # pretend SimHub with moving data
SIMDASH_HOST=127.0.0.1 ./simdash-desktop
```

Arrow keys, Enter, Esc and Q stand in for the Switch buttons.
