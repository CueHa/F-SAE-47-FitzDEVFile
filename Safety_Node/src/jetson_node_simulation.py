import serial
import time

# Serial config
ser = serial.Serial('/dev/ttyUSB0', 115200)
time.sleep(2)  # Let ESP32 boot
ser.reset_input_buffer()  # flush any boot garbage

# Simplified FSM states to cycle through
fsm_states = [
    "OFF",
    "READY",
    "DRIVING",
    "EMERGENCY",
    "FINISHED",
]

index = 0

while True:
    state = fsm_states[index]
    msg = f"{state}\n"  # Only send the simplified state string
    ser.write(msg.encode())  # Send the FSM state
    print(f"> Sent: {msg.strip()}")

    # Wait and read back ACK
    time.sleep(0.1)
    while ser.in_waiting:
        print(f"< Recv: {ser.readline().decode(errors='replace').strip()}")

    time.sleep(1.8)  # Wait before next state
    index = (index + 1) % len(fsm_states)  # Cycle through states
