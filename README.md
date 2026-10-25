# SimDash

A native SimHub digital dash for a modded Nintendo Switch (Atmosphere, sysMMC or emuMMC). It runs as a normal homebrew app from the Homebrew Menu. No web browser involved.

It has two modes, picked from its start menu:

- **Custom dash**: SimDash draws its own dashboard from live SimHub data (gear, speed, RPM lights, lap times, position, fuel, TC/ABS/bias, flags, tyre temps and pressures, water/oil temps).
- **SimHub dash**: shows any real SimHub dashboard, exactly as designed. The small **SimDash Streamer** program on the PC captures the dash window and sends it to the Switch as a live picture. You pick and change dashboards in SimHub like with a tablet.

## Download

Download the files from the latest [Release](../../releases/latest):

- `switch-simdash.nro`: copy it to `/switch/` on the SD card.
- `SimDashStreamer.exe`: put it anywhere on the PC (only needed for the SimHub dash mode).

## Custom dash setup

### On the PC

1. Download `PropertyServer.dll` from the [SimHub Property Server releases](https://github.com/pre-martin/SimHubPropertyServer/releases) (needs SimHub 9.6 or newer).
2. Close SimHub, copy the DLL into the SimHub install folder (usually `C:\Program Files (x86)\SimHub`), then start SimHub.
3. When SimHub asks about the new plugin, enable it (or turn it on under **Settings > Plugins**).
4. Open the Property Server settings (under Additional Plugins, or the left menu) and change **TCP Listen Address** from `Loopback` to `Any`, then fully restart SimHub (quit it from the tray icon too, not just the window). With `Loopback` only the PC itself can connect, and SimDash shows "Can't reach SimHub". To check, run `netstat -an | findstr 18082` in Command Prompt: it should show `0.0.0.0:18082`, not `127.0.0.1:18082`.
5. Allow SimHub through Windows Firewall on private networks if Windows asks. The plugin listens on TCP port **18082**. Your Wi‑Fi/Ethernet network should be set to **Private** in Windows.
6. Find the PC's local IP: open a command prompt, run `ipconfig`, and note the IPv4 address (for example `192.168.1.50`).

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

## SimHub dash setup

### On the PC

1. Open your dashboard in a window on the PC, either way works:
   - In SimHub, go to **Dash Studio** and press the dash's **▶ play** button.
   - Or open `http://localhost:8888` in a browser on the PC (SimHub's built-in dash server) and pick the dash.

   Make the window roughly 16:9 (for example 1280x720) so it fills the Switch screen. It can sit behind other windows, but it must not be minimized.
2. Run `SimDashStreamer.exe`. The first time, Windows asks about the firewall: allow it on **Private** networks. (It needs .NET Framework 4.8, which Windows 10 and 11 already have.)
3. In the dropdown, pick the SimHub dash window. Windows with "SimHub" or "Dash" in the title are listed first. You can also pick a whole monitor, for example a spare screen showing the dash.
4. Note the PC address shown at the bottom of the Streamer window.

The Streamer remembers your choice. Closing it to the tray keeps it running; double-click the tray icon to bring it back.

### On the Switch

1. Launch SimDash and make sure **PC address** is set to the PC's IP (the same one the custom dash uses).
2. Choose **SimHub dash**. The picture appears within a second or two.
3. Press **Y** to show the frame rate, **B** to go back to the menu.

Changing the dash in SimHub changes what the Switch shows. If the picture is choppy, lower the frames per second or the picture quality in the Streamer.

Tip: set **System Settings > Sleep Mode > Auto-Sleep** to **Never** while using the Switch as a dash.

## Limits

- The Property Server plugin sends updates 10 times per second. SimDash smooths the RPM lights between updates, but the custom dash is not as instant as a wired dash.
- The SimHub dash mode sends JPEG pictures over Wi‑Fi. Expect up to about 30 fps and a delay of roughly a tenth of a second, depending on your Wi‑Fi and the Switch's decoding speed.
- Property names follow SimHub's `DataCorePlugin` data, so values depend on what each game provides.

## Building

The GitHub Actions workflow builds the `.nro` with devkitPro's `devkita64` Docker image and the Streamer on a Windows runner. To build the `.nro` locally, install devkitPro with `switch-dev`, `switch-sdl2`, `switch-sdl2_ttf` and `switch-libjpeg-turbo`, then run `make`. To build the Streamer, run `dotnet build streamer/SimDashStreamer.csproj -c Release` on Windows.

To try the dash on a Linux desktop without a Switch:

```sh
sudo apt install libsdl2-dev libsdl2-ttf-dev libjpeg-turbo8-dev
make -f Makefile.desktop
python3 tools/fake_simhub.py &      # pretend SimHub with moving data
python3 tools/fake_streamer.py &    # pretend SimDash Streamer (needs Pillow)
SIMDASH_HOST=127.0.0.1 ./simdash-desktop
```

Arrow keys, Enter, Esc and Q stand in for the Switch buttons.
