#include <SoftwareSerial.h>

#define xbeeRx 2
#define xbeeTx 3
#define ledRed 4
#define ledGreen 5
#define ledBlue 6
#define alarmButton 8
//#define buzzerPin 9

const byte alarmCycles = 3;

bool doorAlarm = false;

SoftwareSerial XBee(xbeeRx, xbeeTx);

void setup() {
  pinMode(ledRed, OUTPUT); digitalWrite(ledRed, LOW);
  pinMode(ledGreen, OUTPUT); digitalWrite(ledGreen, LOW);
  pinMode(ledBlue, OUTPUT); digitalWrite(ledBlue, LOW);

  pinMode(alarmButton, INPUT_PULLUP);

  Serial.begin(9600);
  XBee.begin(9600);

  flushBuffer(true);  // Flush any characters that may be in XBee receive buffer

  Serial.println(F("Setup complete. Remote ready."));
}

void loop() {
  // Handle serial input from user
  if (Serial.available()) {
    while (Serial.available()) {
      XBee.write(Serial.read());
    }
  }

  // Handle messages from control unit
  if (XBee.available()) {
    String messageString = "";
    while (XBee.available()) {
      char c = XBee.read();
      if (c != '\n' && c != '\r') messageString += c;
      delay(5);
    }

    Serial.print(F("Command Received: ")); Serial.println(messageString);

    if (messageString.startsWith("^") && messageString.endsWith("@")) {
      //Serial.println(F("Processing command."));
      processMessage(messageString);
    }

    else if (messageString.startsWith("@") && messageString.endsWith("^")) {
      //Serial.println(F("Command echo received from repeater. Flushing buffer."));
      flushBuffer(false);
    }

    else if (messageString.startsWith("*")) flushBuffer(false);

    else {
      displayError("Invalid command.");
      flushBuffer(true);
    }
  }

  if (doorAlarm == true) {
    ledAlarm(alarmCycles);

    if (digitalRead(alarmButton) == LOW) {
      Serial.println(F("Alarm disabled."));
      doorAlarm = false;
      while (digitalRead(alarmButton) == LOW) {
        delay(5);
      }
      delay(100);
    }
  }

  delay(100);
}

void processMessage(String command) {
  // Format: ^{IDENTIFIER}{ACTION}@
  char identifier = command.charAt(1);
  Serial.print(F("Identifier: ")); Serial.println(identifier);
  char action = command.charAt(2);
  Serial.print(F("Action: ")); Serial.println(action);

  if (identifier == 'A') {
    if (action == 'D') {
      // Trigger door alarm
      if (doorAlarm == false) {
        doorAlarm = true;
        acknowledgeAlert();
        Serial.println(F("Acknowledged door alert."));
      }
      else displayError("Door alarm already active.");
    }
    else displayError("Invalid ACTION encountered while processing ALARM command.");
  }
  else displayError("Invalid IDENTIFIER encountered while processing command.");
}

void acknowledgeAlert() {
  XBee.println(F("@AK^"));
}

void ledAlarm(byte cycles) {
  for (byte x = 0; x < cycles; x++) {
    digitalWrite(ledRed, HIGH);
    delay(100);
    digitalWrite(ledRed, LOW);
    digitalWrite(ledGreen, HIGH);
    delay(100);
    digitalWrite(ledGreen, LOW);
    digitalWrite(ledBlue, HIGH);
    delay(100);
    digitalWrite(ledBlue, LOW);
  }
}

void displayError(String errorMessage) {
  Serial.print(F("*ERROR* ")); Serial.println(errorMessage);
}

// XBee Buffer Flush Function
void flushBuffer(bool timeout) {
  Serial.print(F("Flushing XBee serial receive buffer..."));

  int flushTimeout;
  if (timeout == true) {
    Serial.print(F("delaying for safety..."));
    flushTimeout = 1000;
  }
  else {
    flushTimeout = 0;
  }

  for (unsigned long charLast = millis(); (millis() - charLast) < flushTimeout; ) {
    if (XBee.available()) {
      Serial.print(F("resetting timer..."));
      charLast = millis();
      while (XBee.available()) {
        char c = XBee.read();
        delay(5);
      }
    }
  }
  Serial.println(F("buffer cleared."));
}
