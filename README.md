# C++ Decentralized Messenger

A high-performance **P2P & Bluetooth console messenger** written in C++17. This project aims to create a fully autonomous communication tool that works anywhere — with or without a central internet server.

---

## Key Features

### P2P Chat (UDP Hole Punching)
- **Zero-Server Architecture**: Direct communication between users via UDP.
- **NAT Traversal**: Uses **STUN** (Session Traversal Utilities for NAT) to automatically discover public endpoints.
- **Aggressive Hole Punching**: High-frequency handshake packets to bypass strict firewalls.
- **Keep-Alive (Heartbeat)**: Maintains connection stability by preventing NAT session timeouts.

### Bluetooth Chat (Windows Native)
- **Offline Communication**: Works without any internet connection.
- **RFCOMM Protocol**: Uses native Win32 Bluetooth APIs for high-speed local data transfer.
- **Device Discovery**: Scans and connects to nearby Bluetooth adapters automatically.

### Core Tech Stack
- **Standard**: C++17
- **Network Engine**: SFML 3.0.2 (UDP Socket API)
- **Low-level**: Windows Sockets (Winsock2) & Bluetooth API
- **UI**: Lightweight UTF-8 Console Interface

---

## Connection Methods Comparison

| Feature | P2P Chat (UDP) | Bluetooth Chat |
|---------|---------------|----------------|
| **Distance** | Worldwide  | Short Range (~10m)  |
| **Internet Required** | Yes (Direct) | No (Offline)  |
| **NAT Bypass** | Automatic (STUN) | N/A |
| **Platform** | Windows Only | Windows Only |

---

## Requirements

- **OS**: Windows 10 / 11
- **Compiler**: MSVC (cl.exe) from Visual Studio 2022 / Build Tools
- **Libraries**: 
  - **SFML 3.0.2** (included in `/Libraries`)
  - **Windows SDK** (Bluetooth + Winsock)

### Linked Libraries
`sfml-network.lib`, `sfml-system.lib`, `ws2_32.lib`, `bthprops.lib`, `user32.lib`, `advapi32.lib`

---

## Build & Run

### Quick Build (VS Code)
1. Open the project in VS Code.
2. Ensure MSVC environment is active.
3. Press `Ctrl + Shift + B` (uses the pre-configured `tasks.json`).

### Manual Compilation
```cmd
cl /EHsc /std:c++17 /utf-8 /Zi /Fe:messenger.exe main.cpp bluetooth.cpp p2p.cpp ^
/I Libraries\SFML-3.0.2\include ^
/link /LIBPATH:Libraries\SFML-3.0.2\lib ^
sfml-network.lib sfml-system.lib ws2_32.lib bthprops.lib user32.lib advapi32.lib
