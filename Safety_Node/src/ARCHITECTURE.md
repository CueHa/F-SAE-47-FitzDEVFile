# Safety Node – Architecture

## Overview

The Safety Node enforces autonomous shutdown in response to:

- Loss of Jetson communication
- Triggered EBS line
- Emergency signals (estop, kill switch, ASB not OK)

## ESP32 Responsibilities

- Monitor physical inputs
- Apply state machine logic
- Control outputs (relay, EBS, LEDs)
- Report input status over UART
- Lock state to EMERGENCY if:
  - EBS is HIGH
  - Jetson is silent for >2000 ms

## Jetson Responsibilities

- Run FSM and publish `<state>` messages over UART
- Listen for `INPUTS:` for state decision making
