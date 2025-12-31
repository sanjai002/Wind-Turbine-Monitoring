# Nx_WebServer (STWIN.box) — Network + Web Server Setup

This document explains **where to change IP / port / Wi‑Fi settings**, how the **NetX Duo + ThreadX** startup works, and how to serve the included web pages from an SD card.

This is for the project under:

- `Projects/STWIN.box/Applications/NetXDuo/Nx_WebServer`

---

## What you get (quick contract)

- **Wi‑Fi**: MXCHIP EMW3080B module via SPI (BSP + NetX Duo Wi‑Fi driver)
- **IP addressing**: DHCP by default
- **HTTP server**: NetX Duo Web HTTP Server (`NX_WEB_HTTP_SERVER`)
- **Port**: 80 by default
- **Web content**: served from SD card (FileX + SD driver)
- **Extra runtime endpoints**: a few simple “API-like” paths handled in a request callback

---

## Key “edit here” locations

### 1) Change Wi‑Fi SSID / password

File:
- `Core/Inc/mx_wifi_conf.h`

Macros:
- `WIFI_SSID`
- `WIFI_PASSWORD`

✅ **EDIT HERE (Wi‑Fi credentials)**
- Change `WIFI_SSID` and `WIFI_PASSWORD` to your router values.

Notes:
- These are plain string macros. Example values currently are `"stwintest"`.
- If your network uses different security requirements, those are typically handled inside the MXCHIP middleware/driver configuration. This app-level project exposes SSID/password only.

---

### 1b) Send data to Raspberry Pi (destination IP / port)

Yes — for the **UDP telemetry** part of this firmware, you normally want to set the **destination IP** to your Raspberry Pi (on the same network as the board).

There are two separate networking “roles” in this project:

- **HTTP web server**: your PC/phone connects to the board’s IP (the board is the server).
- **UDP telemetry**: the board sends packets *to a receiver* (your RPi) (the board is the client/sender).

#### Where the destination is configured

1) **Immediate/default behavior (current code): broadcast**

File:
- `Core/Src/app_threadx.c`

Current call:
- `Telemetry_Start(feature_queue, 0xFFFFFFFF);`

`0xFFFFFFFF` is IPv4 broadcast (`255.255.255.255`). This will send to *everyone* on the local network (if the network allows broadcast).

2) **Telemetry module defaults**

Files:
- `NetXDuo/App/app_telemetry.h`
  - `TELEMETRY_DEFAULT_IP_ADDR` (default broadcast)
  - `TELEMETRY_UDP_PORT_RX` (destination UDP port, default `5000`)
- `NetXDuo/App/app_telemetry.c`
  - `telemetry_ctx.receiver_ip` / `telemetry_ctx.receiver_port`
  - `telemetry_ctx.use_broadcast = 1;` (default broadcast)

#### How to switch to unicast (send only to the RPi)

You have two simple options.

✅ **EDIT HERE (send to RPi)**

Option A (recommended, very easy): keep using broadcast
- Leave it as-is.
- RPi just listens on UDP port `5000`.

Option B (recommended for reliability): set unicast to the RPi IP

1) Choose the RPi IP address (example: `192.168.1.50`).
2) In firmware you must set:
   - destination IP = RPi IP
   - destination port = `5000` (or your chosen port)

Where to change:
- Destination port is already defined in:
  - `NetXDuo/App/app_telemetry.h` → `TELEMETRY_UDP_PORT_RX`

- Destination IP is passed when starting telemetry:
  - `Core/Src/app_threadx.c` → `Telemetry_Start(feature_queue, ...)`

Important note (so you don’t get surprised):
- In embedded code, IP addresses are ultimately stored as a 32-bit number, so you can’t literally pass the string `"192.168.1.50"`.
- The **easy way** is: you keep the doc’s “raw IP” (like `192.168.1.50`) and then convert it to the 32-bit value in code.

If you want, tell me your RPi IP and I’ll update the firmware to accept a dotted IP (e.g., `"192.168.1.50"`) and convert it internally (so you never deal with hex).

---

### 2) Change the HTTP port

File:
- `NetXDuo/App/app_netxduo.h`

Macro:
- `CONNECTION_PORT` (default `80`)

✅ **EDIT HERE (HTTP port)**
- Change `CONNECTION_PORT` if you don’t want port 80.

Where used:
- `NetXDuo/App/app_netxduo.c` in:
  - `nx_web_http_server_create(..., CONNECTION_PORT, ...)`
  - `/GetNetInfo` response embeds `CONNECTION_PORT`

---

### 3) DHCP vs Static IPv4

#### DHCP (DEFAULT — recommended)
Files:
- `NetXDuo/App/app_netxduo.c`

✅ **DEFAULT BEHAVIOR**
- This project uses **DHCP by default**.
- You normally do **not** need to change anything here.

How it works:
1. IP instance is created with **NULL address + NULL netmask**:
   - `nx_ip_create(&IpInstance, ..., NULL_ADDRESS, NULL_ADDRESS, ...)`
2. DHCP client created:
   - `nx_dhcp_create(&DHCPClient, &IpInstance, ...)`
3. DHCP started:
   - `nx_dhcp_start(&DHCPClient)`
4. App waits for address assignment using a ThreadX semaphore released by the address-change callback:
   - `nx_ip_address_change_notify(&IpInstance, ip_address_change_notify_callback, ...)`
   - `tx_semaphore_get(&Semaphore, TX_WAIT_FOREVER)`
5. Final IPv4 settings:
   - `nx_ip_address_get(&IpInstance, &IpAddress, &NetMask)`

The assigned IPv4 address is printed using:
- `PRINT_IP_ADDRESS(IpAddress)` (macro in `NetXDuo/App/app_netxduo.h`)

#### Static IPv4 (OPTIONAL — only if you really need it)
If your network doesn’t allow DHCP (or you want a fixed address), you can switch to static IPv4.

Option A (recommended): set a fixed IPv4 address after `nx_ip_create()` and before starting the server.
- Replace `NULL_ADDRESS` parameters passed to `nx_ip_create()` with your own IP and netmask.

Option B: keep `nx_ip_create()` as-is and call:
- `nx_ip_address_set(&IpInstance, ip, netmask)`

In either case:
- Remove/disable the DHCP creation + start path (`nx_dhcp_create`, `nx_dhcp_start`)
- Remove/adjust the `tx_semaphore_get()` wait (since there will be no DHCP callback)

Where to do it:
- In `NetXDuo/App/app_netxduo.c`, inside `App_Main_Thread_Entry()`.

---

## How the web server serves files

File:
- `NetXDuo/App/app_netxduo.c`

### SD card is mandatory for HTML/CSS/JS content
The HTTP server uses FileX media `sdio_disk` as its file system backend:

- Media open:
  - `fx_media_open(&sdio_disk, "STM32_SDIO_DISK", fx_stm32_sd_driver, ...)`

If the SD card is missing or not formatted correctly, you’ll see:
- `FX media opening failed`

### Copy Web_Content to SD card
Folder:
- `Web_Content/`

Copy its files to the **root** of an SD card formatted as **FAT32**:
- `index.html`
- `dashboard.html`
- `assets/*` (css/js)

Pages:
- `http://<board-ip>/index.html`
- `http://<board-ip>/dashboard.html`

### MIME types
The project adds a small MIME map table so the browser treats assets correctly:
- `css → text/css`
- `svg → image/svg+xml`
- `png → image/png`
- `jpg → image/jpg`

---

## Built-in request endpoints (callback)

File:
- `NetXDuo/App/app_netxduo.c`

Function:
- `webserver_request_notify_callback(...)`

These paths are handled specially:

- `GET /GetTXData`
  - Returns ThreadX performance counters (resumptions/suspensions/idle/etc.)
- `GET /GetNXData`
  - Returns NetX TCP statistics (bytes sent/received, connections, …)
- `GET /GetNetInfo`
  - Returns `<ip>,<port>` using `IpAddress` and `CONNECTION_PORT`
- `GET /GetTxCount`
  - Returns basic per-thread run counters
- `GET /GetNXPacket`
  - Returns available packet count from `AppPool.nx_packet_pool_available`
- `GET /GetNXPacketlen`
  - Returns length of the first packet in the available list
- `GET /LedOn`
  - Resumes the LED Thread
- `GET /LedOff`
  - Suspends the LED Thread and turns the LED off

Notes:
- The callback builds a small response using:
  - `nx_web_http_server_callback_generate_response_header(...)`
  - `_nxe_packet_data_append(...)`
  - `nx_web_http_server_callback_packet_send(...)`

---

## Startup flow (ThreadX ↔ NetX Duo)

Two relevant entry points:

1) `Core/Src/main.c`
- Initializes hardware
- Initializes Wi‑Fi GPIOs via BSP: `BSP_WIFI_MX_GPIO_Init()`
- Enters ThreadX: `MX_ThreadX_Init()`

2) `Core/Src/app_threadx.c`
- Creates/starts application threads for:
  - audio acquisition
  - feature extraction
  - telemetry
- Waits for IP assignment by polling `IpAddress` and then starts telemetry pipeline

Important detail:
- The IP address that the rest of the application uses is the global `IpAddress` defined in `NetXDuo/App/app_netxduo.c` and declared in `NetXDuo/App/app_netxduo.h`.

---

## Pin / peripheral “runtime truth” reminders

This project’s actual used pins come from **BSP init code** and **HAL MSP init code** (not solely from the `.ioc`). Examples already confirmed in this repo:

- USART2:
  - PD5 = USART2_TX (AF7)
  - PD6 = USART2_RX (AF7)

- Wi‑Fi SPI (from BSP):
  - SPI1_SCK  = PG2 (AF5)
  - SPI1_MISO = PG3 (AF5)
  - SPI1_MOSI = PG4 (AF5)
  - Flow      = PG15 (LPTIM1_CH1 AF1)
  - Notify EXTI (from `Core/Inc/main.h`):
    - PE7 = `MXCHIP_NOTIFY_Pin`

- On-board DMIC (IMP34DT05TR) via ADF1:
  - CKOUT = PE9  (AF3_ADF1)
  - DATIN = PE10 (AF3_ADF1)

---

## Common troubleshooting

### HTTP pages don’t load
- Check UART logs for:
  - `Fx media successfully opened.`
  - `HTTP WEB Server successfully started.`
- If you see `FX media opening failed`, fix the SD card (FAT32 + copy `Web_Content`).

### No IP address appears
- The app uses DHCP. Make sure:
  - SSID/password are correct (`Core/Inc/mx_wifi_conf.h`)
  - Access point provides DHCP
- `Core/Src/app_threadx.c` will eventually timeout waiting for IP.

### Wi‑Fi IRQ not firing
- EXTI callback in `Core/Src/main.c` specifically checks `MXCHIP_NOTIFY_Pin`.
- Confirm the BSP Wi‑Fi init config matches your hardware.

---

## Raspberry Pi (RPi) receiver setup (UDP telemetry)

This firmware sends **64-byte UDP packets** (`AudioTelemetryPacket_t`) to a receiver.

- Default destination port: `5000` (`TELEMETRY_UDP_PORT_RX` in `NetXDuo/App/app_telemetry.h`)
- Default mode: **broadcast** unless you switch to unicast

### 1) Get the Raspberry Pi IP address

On the Raspberry Pi, find its IPv4 address on the Wi‑Fi/Ethernet interface that is on the same network as the STWIN.box.

Example commands (run on the RPi):

```bash
ip -4 addr
```

Pick the address like `192.168.x.x`.

### 2) Listen for incoming UDP packets

#### Option A: quick test listener with netcat

```bash
nc -ul -p 5000 | hexdump -C
```

You should see data arriving (64 bytes per packet, but your terminal output may split it across lines).

#### Option B: tiny Python receiver (prints node_id + seq + timestamp)

Create a file `udp_rx.py` on the RPi:

```python
import socket
import struct

PORT = 5000

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind(("0.0.0.0", PORT))
print(f"Listening on UDP :{PORT} ...")

while True:
    data, addr = sock.recvfrom(2048)
    if len(data) != 64:
        print(f"{addr} -> len={len(data)} (expected 64)")
        continue

  # Decode a few fields from the 64-byte packet.
    version = data[0]
    seq = struct.unpack_from("<H", data, 2)[0]
    timestamp_ms = struct.unpack_from("<I", data, 4)[0]
  node_id = data[52]  # node id (1/2/3)

    print(f"{addr} v={version} node={node_id} seq={seq} t={timestamp_ms}ms")
```

Run it:

```bash
python3 udp_rx.py
```

### 3) Firewall / routing notes

- Make sure UDP port `5000` is not blocked by the RPi firewall.
- **Broadcast mode** (`255.255.255.255`) may be blocked or filtered by some Wi‑Fi networks/routers.
  - If you don’t see packets on the RPi, switch to **unicast** (send directly to the RPi IP).

---

## Multi-node setup (other two nodes)

This firmware already supports identifying multiple sensor nodes in the telemetry stream via `node_id` inside the 64‑byte packet.

### 1) Give each node a unique node_id

Packet field:
- `AudioTelemetryPacket_t.node_id` (in `Core/Inc/audio_features.h`)

Where it’s currently set:
- `Core/Src/feature_extraction.c`
  - `static uint32_t node_id = 1;  /* Default: Node 1 */`
  - Later the packet is populated with `pkt->node_id = node_id;`

✅ **EDIT HERE (node id)**
- For node 2, change it to `2`
- For node 3, change it to `3`

What you must do for 3 nodes:
- On **node 1 firmware**: set `node_id = 1`
- On **node 2 firmware**: set `node_id = 2`
- On **node 3 firmware**: set `node_id = 3`

If you keep them all as `1`, the RPi can’t reliably distinguish which physical node sent the data.

### 2) Decide how nodes send to the RPi (broadcast vs unicast)

You have two workable topologies:

#### Option A (recommended): unicast all nodes to the same RPi IP + same port
- Each node uses the **same destination**: `<rpi-ip>:5000`
- RPi script receives all nodes and separates by `node_id`

To do this:
- Switch telemetry to unicast (disable broadcast)
- Set the destination IP to the RPi’s IP

#### Option B: keep broadcast (simple, but less reliable)
- Each node transmits to `255.255.255.255:5000`
- RPi listens on `5000` and receives whatever the network permits

Broadcast caveats:
- Some routers/APs block broadcast or isolate clients.
- You may lose packets or receive nothing depending on Wi‑Fi settings.

### 3) (Optional) Give each node a different UDP destination port

If you want to separate streams by port instead of `node_id`, you can change:
- `TELEMETRY_UDP_PORT_RX` in `NetXDuo/App/app_telemetry.h`

Example:
- Node 1 → port 5000
- Node 2 → port 5001
- Node 3 → port 5002

Then run 3 listeners on the RPi (or one script that binds multiple ports).

---

## Quick checklist (what to change for your network)

- Wi‑Fi SSID/password:
  - `Core/Inc/mx_wifi_conf.h`
- Web server port:
  - `NetXDuo/App/app_netxduo.h` → `CONNECTION_PORT`
- DHCP vs static IP:
  - `NetXDuo/App/app_netxduo.c` → `App_Main_Thread_Entry()`
- Web files:
  - Copy `Web_Content/` to SD card root
