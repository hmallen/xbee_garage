#include <SoftwareSerial.h>

#define xbeeRx 2
#define xbeeTx 3
#define ledRed 4
#define ledGreen 5
#define ledBlue 6
#define alarmButton 8
//#define buzzerPin 9

const byte alarmCycles = 3;
const int heartbeatTimeout = 31000;

bool doorAlarm = false;
unsigned long heartbeatLast = 0;

SoftwareSerial XBee(xbeeRx, xbeeTx);

void setup() {
  pinMode(ledRed, OUTPUT); digitalWrite(ledRed, LOW);
  pinMode(ledGreen, OUTPUT); digitalWrite(ledGreen, LOW);
  pinMode(ledBlue, OUTPUT); digitalWrite(ledBlue, LOW);

  pinMode(alarmButton, INPUT_PULLUP);

  Serial.begin(19200);
  XBee.begin(19200);

  flushBuffer(true);  // Flush any characters that may be in XBee receive buffer

  Serial.println(F("Setup complete. Remote ready."));
}

void loop() {
  // Handle serial input from user
  if (Serial.available()) {
    while (Serial.available()) {
      XBee.write(Serial.read());
      delay(5);
    }
  }

  // Handle messages from control unit
  if (XBee.available()) {
    bool bypassProcessing = false;
    String messageString = "";
    byte count = 0;
    while (XBee.available()) {
      char c = XBee.read();
      if (count == 0 && c == '@') {
        //flushBuffer(true);
        bypassProcessing = true;
      }
      else if (count == 0 && c == '#') {
        //flushBuffer(true);
        bypassProcessing = true;
      }
      if (c != '\n' && c != '\r') messageString += c;
      count++;
      delay(5);
    }

    if (bypassProcessing == false) {
      //Serial.print(F("Command Received: ")); Serial.println(messageString);

      if (messageString.startsWith("^") && messageString.endsWith("@")) {
        //Serial.println(F("Processing command."));
        processMessage(messageString);
      }

      else {
        displayError("Invalid command: " + messageString);
        //flushBuffer(true);
      }
    }
  }

  if (doorAlarm == true) {
    ledAlarm("door", alarmCycles);

    if (digitalRead(alarmButton) == LOW) {
      Serial.println(F("Alarm disabled."));
      doorAlarm = false;
      while (digitalRead(alarmButton) == LOW) {
        delay(5);
      }
      delay(100);
    }
  }

  if ((millis() - heartbeatLast) > heartbeatTimeout) ledAlarm("heartbeat", alarmCycles);

  delay(100);
}

void processMessage(String command) {
  // Format: ^{IDENTIFIER}{ACTION}@
  char identifier = command.charAt(1);
  //Serial.print(F("Identifier: ")); Serial.println(identifier);
  char action = command.charAt(2);
  //Serial.print(F("Action: ")); Serial.println(action);

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

  else if (identifier == 'M') {
    if (action == 'S') displayMessage("status", parseMessage(command));
    else if (action == 'G') displayMessage("general", parseMessage(command));
    else if (action == 'E') displayMessage("error", parseMessage(command));
    else if (action == 'T') displayMessage("time", parseMessage(command));
    else if (action == 'I') displayMessage("input", parseMessage(command));
    else displayError("Invalid ACTION encountered while processing STATUS command.");
  }

  else if (identifier == 'H') {
    if (action == 'B') {
      ledHeartbeat();
      heartbeatLast = millis();
    }
    else displayError("Invalid ACTION encountered while processing HEARTBEAT command.");
  }

  else displayError("Invalid IDENTIFIER encountered while processing command.");
}

void acknowledgeAlert() {
  XBee.println(F("@AK^"));
}

void ledHeartbeat() {
  digitalWrite(ledGreen, HIGH);
  delay(50);
  digitalWrite(ledGreen, LOW);
}

void ledAlarm(String alarmType, byte cycles) {
  if (alarmType == "door") {
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
  else if (alarmType == "heartbeat") {
    for (byte x = 0; x < cycles; x++) {
      delay(50);
      digitalWrite(ledRed, HIGH);
      delay(50);
      digitalWrite(ledRed, LOW);
    }
  }
  else displayError("Unrecognized alarm type passed to ledAlarm().");
}

void displayMessage(String messageType, String message) {
  if (messageType == "status") {
    //Serial.println(F("Current Status"));
    //Serial.println(F("=============="));
    Serial.println(message);
  }
  else if (messageType == "general") {
    Serial.println(message);
  }
  else if (messageType == "error") {
    Serial.print(F("ERROR (Controller): ")); Serial.println(message);
  }
  else if (messageType == "time") {
    Serial.print(F("Current Date/Time: ")); Serial.println(message);
  }
  else if (messageType == "input") {
    Serial.println(message);
  }
  else {
    displayError("Unrecognized message type passed to displayMessage().");
  }
}

void displayError(String errorMessage) {
  Serial.print(F("ERROR (Remote): ")); Serial.println(errorMessage);
}

String parseMessage(String messageRaw) {
  String messageParsed = messageRaw;
  messageParsed.remove(0, 3);
  messageParsed.remove(messageParsed.indexOf('@'));
  //Serial.print(F("messageParsed: ")); Serial.println(messageParsed);
  return messageParsed;
}

// XBee Buffer Flush Function
void flushBuffer(bool timeout) {
  Serial.print(F("Flushing XBee serial receive buffer..."));

  int flushTimeout;
  if (timeout == true) {
    Serial.print(F("delaying for safety..."));
    flushTimeout = 500;
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
