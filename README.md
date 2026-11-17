# Lightweight Encrypted UDP Telemetry Proxy

A high-performance, low-latency, dual-ended UDP proxy written in pure C for securing real-time telemetry and command streams between unmanned aerial vehicles (UAVs) and Ground Control Stations (GCS).

## Overview

Standard telemetry links often transmit raw, unencrypted UDP datagrams over local networks or wireless bridges, leaving flight data vulnerable to passive eavesdropping and packet injection. This utility acts as a transparent cryptographic tunnel between two endpoints without introducing the overhead of TLS/DTLS handshakes or dynamic heap allocations.

### Key Features

* **Zero-Overhead Non-Blocking I/O:** Built directly on POSIX sockets (`AF_INET`, `SOCK_DGRAM`) and `poll()` for deterministic, event-driven packet forwarding with minimal CPU footprint.

* **Compile-Time PSK & Stream Obfuscation:** Uses a pre-shared key (`PSK_KEY` via `key.h`) combined with byte-wise XOR masking and key-derived circular bitwise rotations (`rotl8` / `rotr8`).

* **Bidirectional Tunneling:** Handles simultaneous uplink and downlink traffic over separate local (`127.0.0.1`) and tunnel sockets.

* **Zero External Dependencies:** Written in standard C using only native POSIX networking and `getopt` headers—ideal for companion computers and embedded Linux targets.

## Architecture

```
+-------------+     Plain UDP (14551)     +------------------+      Encrypted UDP      +------------------+     Plain UDP (14550)     +-------------+
|   Local     | ------------------------> |   Proxy Node A   | ======================> |   Proxy Node B   | ------------------------> |   Local     |
| Application | <------------------------ |   (Encrypt/Tx)   | <====================== |   (Decrypt/Rx)   | <------------------------ | Application |
+-------------+     Plain UDP (14550)     +------------------+     Public / RF Link    +------------------+     Plain UDP (14551)     +-------------+
```

1. **Ingress (`local_sock`):** Listens on `0.0.0.0:<local-in-port>` (default `14551`) for incoming plaintext datagrams from the local flight controller bridge or GCS software.

2. **Encryption (`encrypt_data`):** Processes each byte `i` in-place using `k = PSK_KEY[i % key_len]`:
   

   $$
   \text{cipher}[i] = \text{rotl8}\big(\text{data}[i] \oplus k,\ (k \bmod 42) \times 7\big)
   $$

3. **Tunnel Egress (`tunnel_sock`):** Forwards the obfuscated payload to `<tun-out-ip>:<tun-out-port>`.

4. **Decryption & Local Delivery:** Incoming tunnel packets on `<tun-in-port>` are reversed via `rotr8` and XORed with `k`, then forwarded to `127.0.0.1:<local-out-port>` (default `14550`).

## Build & Configuration

### 1. Create `key.h`

Before compiling, create a `key.h` file in the root directory defining your shared secret (`PSK_KEY`). Both endpoints must be compiled with the exact same key:

```
#ifndef KEY_H
#define KEY_H

#define PSK_KEY "YourStrongPreSharedSecretKey"

#endif
```


### 2. Setup & Build

1. **Clone the repository:**

   ```
   git clone https://github.com/Kurwei/uav-proxy.git
   cd uav-proxy
   ```

2. **Create your local key header from the example template:**

   ```
   cp key.h.example key.h
   ```

3. Edit `key.h` and define your `PSK_KEY`.

4. **Compile the binary:**

   ```
   make
   # or compile directly with gcc:
   gcc -O2 -Wall -Wextra -o udp-proxy main.c
   ```

## CLI Options & Usage

### Command-Line Flags

| Flag | Argument | Required | Default | Description | 
| ----- | ----- | ----- | ----- | ----- | 
| `--tun-out-ip` | `<ip>` | **Yes** | — | Remote peer's IPv4 address | 
| `--tun-in-port` | `<port>` | **Yes** | — | Local UDP port listening for encrypted tunnel traffic | 
| `--tun-out-port` | `<port>` | **Yes** | — | Remote peer's UDP port receiving encrypted tunnel traffic | 
| `--local-in-port` | `<port>` | No | `14551` | Local UDP port receiving plaintext packets from the app | 
| `--local-out-port` | `<port>` | No | `14550` | Local UDP port (`127.0.0.1`) to forward decrypted packets to | 
| `--tun-in-ip` | `<ip>` | No | `0.0.0.0` | Local tunnel interface IP address | 

### Example Two-Node Setup

Assume **Node A (UAV Companion Computer)** is at `192.168.1.100` and **Node B (Ground Control Station)** is at `192.168.1.50`.

**On Node A (UAV Side):**

* Receives plaintext MAVLink from autopilot/MAVSDK on port `14551`

* Sends encrypted packets to `192.168.1.50:9000`

* Listens for encrypted return commands on port `9001` and forwards decrypted packets to `127.0.0.1:14550`

```
./udp-proxy \
  --local-in-port 14551 \
  --local-out-port 14550 \
  --tun-in-port 9001 \
  --tun-out-ip 192.168.1.50 \
  --tun-out-port 9000
```

**On Node B (GCS Side):**

* Listens for encrypted telemetry from UAV on port `9000` and forwards decrypted packets to QGroundControl at `127.0.0.1:14550`

* Receives plaintext commands from QGroundControl on port `14551` and sends encrypted packets to `192.168.1.100:9001`

```
./udp-proxy \
  --local-in-port 14551 \
  --local-out-port 14550 \
  --tun-in-port 9000 \
  --tun-out-ip 192.168.1.100 \
  --tun-out-port 9001
```

## Security Disclaimer

This utility is engineered for low-latency telemetry obfuscation, embedded systems experimentation, and network protocol research. While key-derived bitwise rotations and XOR stream transformations protect against passive sniffing and casual tampering, they do not provide cryptographic authenticity or replay protection. For production defense deployments, pair this transport layer with authenticated encryption (e.g., ChaCha20-Poly1305 or AES-GCM).

> **Note:** The custom cipher implemented in this project is intended for educational purposes and low-overhead telemetry obfuscation rather than replacing audited cryptographic standards like ChaCha20-Poly1305 or AES-GCM.

## Roadmap

* \[ \] Add sequence numbers for replay-attack mitigation.

## License

This project is open-source and available under the [MIT License](LICENSE).
