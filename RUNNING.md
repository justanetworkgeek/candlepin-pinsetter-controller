# Running The Raylib State Machine Viewer

## Prerequisites
- CMake 3.16+
- A C compiler
- raylib development package installed and discoverable by CMake

## Build
```bash
cmake -S . -B build
cmake --build build --config Release
```

## Run
- Windows (MSVC generator):
```bash
build\Release\state_machine_viewer.exe
```

- Other generators/platforms:
```bash
build/state_machine_viewer
```

## Interaction
- Click input buttons on the left panel to simulate switch events.
- Active diagram states are highlighted in green.
- Active outputs (LED + relays) are shown on the right panel.
- Press `R` to reset the simulator to startup defaults.
