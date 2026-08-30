# MeshCore RemoteTerm Display — Freenove FNK0104

Receive-only MeshCore channel viewer for the Freenove ESP32-S3 Display (FNK0104B and FNK0104S).

## What it does

- Connects directly to a RemoteTerm server over Wi-Fi.
- Discovers channels through `GET /api/channels`.
- Loads channel history through `GET /api/messages?type=CHAN&conversation_key=...`.
- Uses `/api/ws` for real-time updates.
- Falls back to periodic REST refreshes every 15 seconds.
- Swipe left/right to change channels.
- Tap the left/right footer edge to change channels.
- Receive-only: there is deliberately no send-message UI or POST request.
- Optional HTTP Basic Auth support.
- HTTP/WS and HTTPS/WSS configuration.
- Uses the EastMesh dark-mode theme: `#222222` background, `#303030` panels,
  `#E6EAF0` text, `#9AA4B2` muted text, and `#36A167` accent green.
- The lower-left gear button opens an on-device channel settings screen.

## Supported hardware

### FNK0104B (default)

- ESP32-S3
- 2.8 inch 240x320 ILI9341
- FT6336U / FT5x06-compatible capacitive touch
- LCD: SCLK 12, MOSI 11, MISO 13, CS 10, DC 46, BL 45
- Touch: SDA 16, SCL 15, INT 17, RST 18, address 0x38

### FNK0104S

The project includes a second PlatformIO environment for the 4.0 inch 320x480 panel. It uses the same board wiring and an ST7796-compatible panel profile. Both environments are maintained and built independently:

```bash
pio run -e fnk0104b
pio run -e fnk0104s
```

The 4-inch target uses the Freenove-compatible RGB colour order and 80 MHz write / 20 MHz read SPI settings. The 2.8-inch target uses the ILI9341 profile and its native BGR order. Display and touch rotations are applied together at runtime.

## Configure

1. Copy:

   `include/config.example.h` -> `include/config.h`

2. Edit the optional compile-time defaults in `include/config.h`, or use the
   device settings page after Wi-Fi is configured.

Example:

```cpp
#define WIFI_SSID "EastMesh"
#define WIFI_PASSWORD "..."
#define REMOTETERM_HOST "192.168.1.50"
#define REMOTETERM_PORT 8000
#define REMOTETERM_TLS false
```

Do not include `http://` in `REMOTETERM_HOST`. When the setup AP is used, its saved values override these defaults.

If RemoteTerm has Basic Auth enabled:

```cpp
#define REMOTETERM_USERNAME "user"
#define REMOTETERM_PASSWORD "password"
```

## Build / upload

Install VS Code + PlatformIO, open this folder, then use the PlatformIO Upload action.

Or from a PlatformIO shell:

```bash
pio run -e fnk0104b -t upload
pio device monitor -b 115200
```

For FNK0104S:

```bash
pio run -e fnk0104s -t upload
```

## Controls

- Swipe left: next channel
- Swipe right: previous channel
- Tap `<` in the footer: previous channel
- Tap `>` in the footer: next channel
- Tap the gear in the lower-left footer: open channel settings

In channel settings, tap rows to toggle channels and tap `SAVE`. The selected
channel keys are stored in NVS and the device restarts. The web settings page
also provides display rotation, channel selection, and Select all / Clear all
controls. Rotation defaults to the corrected clockwise landscape orientation (rotation
value 3) for the 2.8-inch target and 90° landscape
for the 4-inch target.

The newest messages are shown at the bottom of the visible list.

## Offline setup access point

On first boot, or after the configured Wi-Fi fails to connect for 30 seconds, the display starts a temporary configuration network named `RemoteTerm-XXXX`. The password is `configure`. Connect to it and browse to `http://192.168.4.1`; captive-portal detection is supported through a wildcard DNS responder, with a normal browser fallback if the phone does not open it automatically.

Saving the form stores Wi-Fi, RemoteTerm host/port, transport, and optional Basic Auth values in ESP32 non-volatile storage and restarts the device. Credentials are never written into the repository. A later reset/clear-settings control can be added without changing the network protocol.

## Serial configuration

At 115200 baud, the USB serial console accepts configuration commands. Type
`help` for the complete list. Examples:

```text
show
set wifi-ssid EastMesh
set wifi-password your-password
set host 192.168.1.50
set port 8000
set tls off
set rotation 2
save
reboot
```

`show` never prints Wi-Fi or RemoteTerm passwords. Every valid `set` command
now saves immediately to NVS; `save` remains available as an explicit retry,
and `reboot` saves again before restarting. `clear` removes saved NVS settings
but does not restart the device; use `reboot` afterwards to apply compile-time
defaults.

The web settings page also has **Check for OTA update**, which triggers the
same public GitHub release check immediately. Watch the serial console for
the HTTP status, selected release, asset, download length, and any update
error. The device restarts automatically only after a verified image is
written.

## RemoteTerm compatibility

The parser intentionally accepts both bare arrays and envelope responses such as `{ "channels": [...] }` / `{ "messages": [...] }`. WebSocket message events are also parsed defensively because RemoteTerm's event envelope has changed over its development history.

The current RemoteTerm API documents:

- `GET /api/channels`
- `GET /api/messages` with filters including forward-pagination support
- `WS /api/ws`
- channel messages use type `CHAN`
- `conversation_key` identifies the channel

## TLS note

`TLS_INSECURE=true` is convenient for local/self-signed RemoteTerm HTTPS installs. It disables certificate verification on the ESP32. Do not use that setting when crossing untrusted networks.

## First-run troubleshooting

If the display starts but no channels appear:

1. Confirm the RemoteTerm web UI is reachable from another device on the same Wi-Fi.
2. Open `http://REMOTE_TERM_HOST:PORT/docs` and confirm `/api/channels` works.
3. Check the serial monitor for the ESP32 IP address.
4. If Basic Auth is enabled, verify the configured credentials.
5. If using HTTPS with a self-signed certificate, leave `TLS_INSECURE=true` for initial testing.

If the LCD image works but touch direction is wrong on a hardware revision, adjust `_touch`'s `offset_rotation` in `include/display_config.h` (0-7). The shipped profile targets the standard FNK0104B/S orientation in landscape.

## Safety / scope

MeshCore RemoteTerm Display never writes to the RemoteTerm radio and has no message-send implementation. It only uses read endpoints plus the WebSocket feed.

The merged factory image is intended for first installation or recovery. Writing
it from offset `0x0` covers the NVS partition and can remove saved settings.
For normal upgrades use the GitHub Release application asset or another
app-only update method; those preserve NVS. After USB recovery, expect to
re-enter configuration if the NVS namespace was covered by the write.
