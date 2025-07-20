#include "config.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// -------- State config --------
String currentState = "UNSET";
String lastState = "";
const String validStates[] = {"OFF", "READY", "DRIVING", "EMERGENCY", "FINISHED"};
const int numStates = sizeof(validStates) / sizeof(validStates[0]);

// -------- Blinking logic --------
bool ledOn = false;
unsigned long lastToggle = 0;

// -------- State info tracking --------
String throttleStatus = "OFF";
String assiStatus = "OFF";
String ebsStatus = "OFF";

// -------- Serial input buffer --------
String inputBuffer = "";

struct InputStatus {
  bool asms_active;
  bool brakes_engaged;
  bool throttle_engaged;
  bool estop_engaged;
  bool kill_switch;
  bool ebs_engaged;
  bool asb_ok;
  bool ts_active;
};

InputStatus inputs;
InputStatus lastInputs = {};

unsigned long lastJetsonMessageTime = 0;
bool jetsonLost = false;

// -------- setup() --------
void setup() {
  Serial.begin(UART_BAUDRATE);
  delay(100);

  Wire.begin(21, 22);  // Adjust if using different pins

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println("OLED init failed.");
    while (true);
  }

  pinMode(PIN_RELAY, OUTPUT);
  pinMode(PIN_YELLOW, OUTPUT);
  pinMode(PIN_BLUE, OUTPUT);
  pinMode(PIN_EBS, OUTPUT);

  digitalWrite(PIN_RELAY, LOW);
  digitalWrite(PIN_YELLOW, LOW);
  digitalWrite(PIN_BLUE, LOW);
  digitalWrite(PIN_EBS, LOW);

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("ESP32 ready.");
  display.display();

  pinMode(PIN_ASMS_ACTIVE, INPUT_PULLUP);
  pinMode(PIN_BRAKES_ENGAGED, INPUT_PULLUP);
  pinMode(PIN_THROTTLE_ENGAGED, INPUT_PULLUP);
  pinMode(PIN_ESTOP_ENGAGED, INPUT_PULLUP);
  pinMode(PIN_KILL_SWITCH, INPUT_PULLUP);
  pinMode(PIN_EBS_ENGAGED, INPUT);
  pinMode(PIN_ASB_OK, INPUT_PULLUP);
  pinMode(PIN_TS_ACTIVE, INPUT_PULLUP);
}

void updateInputs() {
  inputs.asms_active = digitalRead(PIN_ASMS_ACTIVE) == HIGH;
  inputs.brakes_engaged = digitalRead(PIN_BRAKES_ENGAGED) == HIGH;
  inputs.throttle_engaged = digitalRead(PIN_THROTTLE_ENGAGED) == HIGH;
  inputs.estop_engaged = digitalRead(PIN_ESTOP_ENGAGED) == HIGH;
  inputs.kill_switch = digitalRead(PIN_KILL_SWITCH) == HIGH;
  inputs.ebs_engaged = digitalRead(PIN_EBS_ENGAGED) == HIGH;
  inputs.asb_ok = digitalRead(PIN_ASB_OK) == HIGH;
  inputs.ts_active = digitalRead(PIN_TS_ACTIVE) == HIGH;
}

// -------- loop() --------
unsigned long lastReportTime = 0;

void checkEBS() {
  if (inputs.ebs_engaged) {
    currentState = "AS_EMERGENCY";
  }
}

void loop() {
  handleSerialInput();
  updateBlinkLogic();

  if (millis() - lastJetsonMessageTime > JETSON_TIMEOUT) {
    currentState = "AS_EMERGENCY";
    jetsonLost = true;
  } else {
    jetsonLost = false;
  }

  applyState();
  updateInputs();
  checkEBS();

  unsigned long now = millis();
  bool changed = (
    inputs.asms_active     != lastInputs.asms_active     ||
    inputs.brakes_engaged  != lastInputs.brakes_engaged  ||
    inputs.throttle_engaged!= lastInputs.throttle_engaged||
    inputs.estop_engaged   != lastInputs.estop_engaged   ||
    inputs.kill_switch     != lastInputs.kill_switch     ||
    inputs.ebs_engaged     != lastInputs.ebs_engaged     ||
    inputs.asb_ok          != lastInputs.asb_ok          ||
    inputs.ts_active       != lastInputs.ts_active
  );

  if (changed || now - lastReportTime >= 1000) {
    sendInputStatus();
    lastReportTime = now;
  }

  delay(10);  // Light serial stability delay
}

// -------- handleSerialInput() --------
void handleSerialInput() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n') {
      inputBuffer.trim();
      parseInput(inputBuffer);
      inputBuffer = "";

      lastJetsonMessageTime = millis();

    } else {
      inputBuffer += c;
    }
  }
}

// -------- parseInput() --------
void parseInput(String input) {
  input.trim();
  bool valid = false;

  for (int i = 0; i < numStates; i++) {
    if (input == validStates[i]) {
      currentState = "AS_" + input;  // Internally still use AS_ prefix for logic
      valid = true;
      break;
    }
  }

  if (!valid) {
    displayError("Invalid state");
  }
}

// -------- updateBlinkLogic() --------
void updateBlinkLogic() {
  unsigned long now = millis();
  if (now - lastToggle >= 500) {
    ledOn = !ledOn;
    lastToggle = now;
  }
}

// -------- applyState() --------
void applyState() {
  static bool lastJetsonLost = false;

  // Reset all outputs
  digitalWrite(PIN_RELAY, LOW);
  digitalWrite(PIN_EBS, LOW);
  digitalWrite(PIN_YELLOW, LOW);
  digitalWrite(PIN_BLUE, LOW);
  throttleStatus = "OFF";
  ebsStatus = "OFF";
  assiStatus = "OFF";

  if (currentState == "AS_READY") {
    digitalWrite(PIN_YELLOW, HIGH);
    assiStatus = "YELLOW STEADY";
  }
  else if (currentState == "AS_DRIVING") {
    digitalWrite(PIN_RELAY, HIGH);
    digitalWrite(PIN_YELLOW, ledOn ? HIGH : LOW);
    throttleStatus = "ON";
    assiStatus = "YELLOW FLASHING";
  }
  else if (currentState == "AS_EMERGENCY") {
    digitalWrite(PIN_EBS, HIGH);
    digitalWrite(PIN_BLUE, ledOn ? HIGH : LOW);
    ebsStatus = "ON";
    assiStatus = "BLUE FLASHING";
  }
  else if (currentState == "AS_FINISHED") {
    digitalWrite(PIN_BLUE, HIGH);
    assiStatus = "BLUE STEADY";
  }
  else if (currentState == "AS_OFF") {
    // All outputs remain LOW
  }
  else {
    displayError("Unknown state");
    return;
  }

  if (currentState != lastState || jetsonLost != lastJetsonLost) {
    lastState = currentState;
    lastJetsonLost = jetsonLost;

    // OLED update
    display.clearDisplay();
    display.setCursor(0, 0);

    display.setTextSize(2);

    if (jetsonLost) {
      display.println("Comms LOST");
    } else {
      display.println(currentState);
    }

    display.setTextSize(1);
    display.println();
    display.print("Throttle: "); display.println(throttleStatus);
    display.print("ASSI:     "); display.println(assiStatus);
    display.print("EBS:      "); display.println(ebsStatus);
    display.display();
  }
}

// -------- OLED error helper --------
void displayError(String msg) {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(1);
  display.println("!! ERROR !!");
  display.println(msg);
  display.display();
}

// -------- sendInputStatus() --------
void sendInputStatus() {
  String status = "INPUTS:";
  status += "ASMS=" + String(inputs.asms_active ? 1 : 0) + ",";
  status += "BRK=" + String(inputs.brakes_engaged ? 1 : 0) + ",";
  status += "THR=" + String(inputs.throttle_engaged ? 1 : 0) + ",";
  status += "ESTOP=" + String(inputs.estop_engaged ? 1 : 0) + ",";
  status += "KILL=" + String(inputs.kill_switch ? 1 : 0) + ",";
  status += "EBS=" + String(inputs.ebs_engaged ? 1 : 0) + ",";
  status += "ASB=" + String(inputs.asb_ok ? 1 : 0) + ",";
  status += "TS="   + String(inputs.ts_active ? 1 : 0);

  Serial.println(status);

  lastInputs = inputs;  // Always update last known
}
