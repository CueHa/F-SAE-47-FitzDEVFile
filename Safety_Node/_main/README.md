# Safety Node – F:SAE:47

This module implements the Jetson–ESP32 safety watchdog logic. It ensures reliable state enforcement for:

- Throttle enable/disable
- EBS triggering
- Emergency state locking
- Jetson heartbeat timeout

## Subsystems

- `esp32/config.h`: config.h for the esp32.ino
- `esp32/esp32.ino`: Handles hardware I/O and control logic.
- `jetson_node_simulation.py`: Mocks Jetson communication for testing.

## How to Run

1. Flash `esp32.ino` to ESP32 (uses pins defined in `config.h`)
2. Run `jetson_node_simulation.py` to test serial input
