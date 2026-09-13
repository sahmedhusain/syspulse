# 📊 SysPulse

[![C++](https://img.shields.io/badge/C++-14-00599C?style=flat&logo=c%2B%2B)](https://cplusplus.com/)
[![Dear ImGui](https://img.shields.io/badge/GUI-Dear%20ImGui-blueviolet)](#-system-architecture)
[![Cross Platform](https://img.shields.io/badge/Platform-Windows%20%7C%20macOS%20%7C%20Linux-green)](#-setup--execution)
[![License](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE.md)

**SysPulse** is a lightweight, cross-platform hardware utility and system performance dashboard implemented in C++. Powered by **Dear ImGui** and SDL2/OpenGL3, SysPulse interfaces directly with OS kernel APIs to display real-time CPU utilization, thermal sensor readings, fan speeds, memory allocations, process tables, and network throughput graphs.

---

## ⚡ Key Highlights

- **Native Platform API Bridges**: Zero-overhead OS polling using platform-native system calls:
  - **macOS (Apple Silicon & Intel)**: `sysctl`, `mach`, `libproc`, and IOKit hardware bridges.
  - **Linux**: `/proc` and `/sys` filesystem polling drivers.
  - **Windows (MinGW/MSVC)**: Win32 API (`psapi`, `iphlpapi`, `GetSystemTimes`).
- **Immediate-Mode GUI Performance**: Built with Dear ImGui for hardware-accelerated 60 FPS HUD rendering.
- **Interactive Process Manager**: Sortable process grid with search filtering, PID tracking, memory consumption metrics, and CPU usage percentages.
- **Dynamic Graphical Overlays**: Real-time vector graphs tracking CPU history, thermal sensor temperatures (°C), and cooling fan RPMs with adjustable FPS sample rates.
- **Auto-Scaling Network Monitor**: Dual-channel RX/TX network traffic visualization automatically scaling units from Bytes to Gigabytes.

---

## 📋 Table of Contents

- [Key Highlights](#-key-highlights)
- [System Architecture](#-system-architecture)
- [Hardware Polling & Rendering Cycle](#-hardware-polling--rendering-cycle)
- [Setup & Execution](#-setup--execution)
- [Project Directory Structure](#-project-directory-structure)
- [License](#-license)

---

## 🖼️ Interface Demonstrations

| CPU & System Hardware Telemetry | Memory Allocation & Process Table |
| :---: | :---: |
| ![System Hardware Telemetry](system.gif) | ![Memory & Process Table](mem.gif) |

---

## 🏗️ System Architecture

```mermaid
graph TD
    subgraph OSKernel["Operating System Kernel APIs"]
        macOS["macOS: mach / sysctl / IOKit"]
        Linux["Linux: /proc & /sys Virtual Filesystems"]
        Win32["Windows: psapi / iphlpapi / Win32"]
    end

    subgraph Core["SysPulse C++ Telemetry Core"]
        SystemMod[system.cpp - CPU & Thermals]
        MemMod[mem.cpp - RAM & SWAP]
        NetMod[network.cpp - Network RX/TX]
    end

    subgraph UI["Dear ImGui + SDL2 / OpenGL3 Layer"]
        Window[SDL2 High-DPI Window Manager]
        ImGuiEngine[ImGui Immediate Mode HUD]
        Graphs[Vector Trend Line Plots & Sortable Tables]
    end

    macOS --> SystemMod
    Linux --> SystemMod
    Win32 --> SystemMod
    
    macOS --> MemMod
    Linux --> MemMod
    Win32 --> MemMod
    
    macOS --> NetMod
    Linux --> NetMod
    Win32 --> NetMod

    SystemMod --> ImGuiEngine
    MemMod --> ImGuiEngine
    NetMod --> ImGuiEngine
    ImGuiEngine --> Window --> Graphs
```

---

## 📐 Hardware Polling & Rendering Cycle

```mermaid
sequenceDiagram
    participant OS as OS Kernel (mach / /proc / Win32)
    participant Core as SysPulse C++ Backend
    participant ImGui as Dear ImGui Renderer
    participant GPU as OpenGL3 / SDL2 Window

    loop 60 FPS Frame Loop
        Core->>OS: Poll Hardware Stats (sysctl / GetSystemTimes)
        OS-->>Core: Raw CPU, Memory, & Network Structs
        Core->>Core: Compute Delta Percentages & Append Graph History
        Core->>ImGui: NewFrame() & Pass Struct Data
        ImGui->>ImGui: Render System, Memory, & Network Tabs
        ImGui->>GPU: Draw Frame Buffer (OpenGL3)
        GPU-->>GPU: Render Visual HUD on Screen
    end
```

---

## 🚀 Setup & Execution

### Prerequisites

- **C++ Compiler**: `g++` or `clang++` (C++14 support).
- **SDL2**: Library and headers installed.

#### Installing SDL2 Dependencies:

- **macOS**:
  ```bash
  brew install sdl2
  ```
- **Linux (Debian/Ubuntu)**:
  ```bash
  sudo apt-get install libsdl2-dev
  ```
- **Windows (MSYS2)**:
  ```bash
  pacman -S mingw-w64-x86_64-SDL2
  ```

---

### Build & Run

1. **Clone Repository**:
   ```bash
   git clone https://github.com/sahmedhusain/syspulse.git
   cd syspulse
   ```

2. **Compile Application**:
   ```bash
   make clean
   make
   ```

3. **Launch SysPulse**:
   ```bash
   ./syspulse
   ```

---

## 📂 Project Directory Structure

```
syspulse/
├── Makefile              # Cross-platform build script (macOS, Linux, Windows)
├── README.md             # Project documentation
├── header.h              # Unified cross-platform telemetry structs & API declarations
├── main.cpp              # Application entrypoint & ImGui window coordinator
├── system.cpp            # CPU, fan speed, & thermal sensor polling logic
├── mem.cpp               # RAM, SWAP, & process table gathering logic
├── network.cpp           # Bandwidth RX/TX network traffic tracking logic
└── imgui/                # Dear ImGui library & OpenGL3/SDL2 backends
```

---

## 📄 License

Distributed under the MIT License. See [LICENSE](LICENSE.md) for details.