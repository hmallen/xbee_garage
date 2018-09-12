#include <SoftwareSerial.h>
#include <TimeLib.h>

#define hallSensor 2
#define lockoutRelay 4
#define doorRelay 5
#define lockoutRelayButton 6
#define rangeTestPin 8
#define xbeeRx 11
#define xbeeTx 12
#define ledPin 13

// Interrupt variables
volatile bool doorOpen;

// Standard variables
unsigned long heartbeatLast = 0;
int heartbeatInterval = 30000;

bool lockStateDoor = false;
bool lockStateButton = false;

time_t lastOpened;

bool doorAlarmActive = false;
bool waitingAcknowledge = false;
unsigned long acknowledgeTime;

// State Variables (for change reference)
// - Need to update heartbeatLast and lastOpened "in-place"
bool doorOpenLast = doorOpen;
bool lockStateDoorLast = lockStateDoor;
bool lockStateButtonLast = lockStateButton;
bool doorAlarmActiveLast = doorAlarmActive;

SoftwareSerial XBee(xbeeRx, xbeeTx);

void setup() {
  // Outputs
  pinMode(lockoutRelay, OUTPUT); digitalWrite(lockoutRelay, LOW);
  pinMode(doorRelay, OUTPUT); digitalWrite(doorRelay, LOW);
  pinMode(lockoutRelayButton, OUTPUT); digitalWrite(lockoutRelayButton, LOW);
  pinMode(ledPin, OUTPUT); digitalWrite(ledPin, LOW);
  // Inputs
  pinMode(hallSensor, INPUT);
  pinMode(rangeTestPin, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(hallSensor), doorCheck, CHANGE);
  doorCheck();  // Read/set initial state of door

  Serial.begin(19200);
  XBee.begin(19200);

  flushBuffer(true);  // Flush any characters that may be in XBee receive buffer

  timeFunction("sync");
}

void loop() {
  /*
     See xbee_message_reference.txt for
     details of message format, etc.
  */

  if (XBee.available()) {
    bool bypassProcessing = false;
    String commandString = "";
    byte count = 0;
    while (XBee.available()) {
      char c = XBee.read();
      if (count == 0 && c == '^') {
        flushBuffer(true);
        bypassProcessing = true;
      }
      if (c != '\n' && c != '\r') commandString += c;
      count++;
      delay(5);
    }

    if (bypassProcessing == false) {
      Serial.print(F("Command Received: ")); Serial.println(commandString);

      if (commandString.startsWith("@") && commandString.endsWith("^")) {
        //Serial.println(F("Processing command."));
        processCommand(commandString);
      }

      //else if (commandString.startsWith("^") && commandString.endsWith("@")) {
      //Serial.println(F("Command echo received from repeater. Flushing buffer."));
      //flushBuffer(false);
      //}

      //else if (commandString.startsWith("*")) flushBuffer(false);

      else {
        sendError("Invalid command.");
        //flushBuffer(true);
      }
    }
  }

  if (waitingAcknowledge == true && (millis() - acknowledgeTime) > 5000) {
    sendError("Acknowledgement timeout. Remote may be out of range.");
    acknowledgeTime = millis();
  }

  if ((millis() - heartbeatLast) > heartbeatInterval) {
    //XBee.println(F("^HB@"));
    XBee.print(F("^HB@"));
    heartbeatLast = millis();
    XBee.print(F("&U%heartbeatLast$"));
    XBee.print(heartbeatLast);
    XBee.print(F("&"));
  }

  if (digitalRead(rangeTestPin) == HIGH) {
    while (digitalRead(rangeTestPin) == HIGH) {
      delay(5);
    }
    rangeTest();
  }

  checkUpdates(false);
}

void processCommand(String command) {
  // Format: @{IDENTIFIER}{ACTION}^
  char identifier = command.charAt(1);
  Serial.print(F("Identifier: ")); Serial.println(identifier);
  char action = command.charAt(2);
  Serial.print(F("Action: ")); Serial.println(action);

  if (identifier == 'D') {
    if (action == 'O') {
      // Check if door already open, and trigger if not
      if (doorOpen == false) triggerDoor();
      //else XBee.println(F("Door already open."));
      else XBee.print(F("^MGDoor already open.@"));
    }
    else if (action == 'C') {
      // Check if door already closed, and trigger if not
      if (doorOpen == true) triggerDoor();
      //else XBee.println(F("Door already closed."));
      else XBee.print(F("^MGDoor already closed.@"));
    }
    else if (action == 'F') {
      // Force trigger of door, regardless of current state
      triggerDoor();
    }
    else if (action == 'L') {
      // Lock door
      toggleLockout("door", true);
    }
    else if (action == 'U') {
      // Unlock door
      toggleLockout("door", false);
    }
    else if (action == 'S') sendStatus();
    else sendError("Invalid ACTION while processing DOOR command.");
  }

  else if (identifier == 'B') {
    if (action == 'L') {
      // Lock button
      toggleLockout("button", true);
    }
    else if (action == 'U') {
      // Unlock button
      toggleLockout("button", false);
    }
    else sendError("Invalid ACTION while processing BUTTON command.");
  }

  else if (identifier == 'T') {
    if (action == 'G') timeFunction("get");
    else if (action == 'S') timeFunction("sync");
    else sendError("Invalid ACTION while processing TIME command.");
  }

  else if (identifier == 'A') {
    if (action == 'A') {
      // Activate door alarm if currently inactive
      if (doorAlarmActive == false) doorAlarmActive = true;
      else sendError("Door alarm already active.");
    }
    else if (action == 'D') {
      // Deactivate door alarm if currently active
      if (doorAlarmActive == true) doorAlarmActive = false;
      else sendError("Door alarm already inactive.");
    }
    else if (action == 'K') {
      if (waitingAcknowledge == true) waitingAcknowledge = false;
      else sendError("Unexpected acknowledgement received. An error may have occurred.");
    }
    else sendError("Invalid ACTION while processing ALARM command.");
  }

  else if (identifier == 'U') {
    if (action == 'A') {
      checkUpdates(true);
    }
    else sendError("Invalid ACTION while processing UPDATE command.");
  }

  else sendError("Invalid IDENTIFIER while processing command.");
}

void sendStatus() {
  String statusMessage = "^MSDoor:        ";
  if (doorOpen == true) statusMessage += "OPEN";
  else statusMessage += "CLOSED";
  statusMessage += "@";
  XBee.print(statusMessage);

  statusMessage = "^MSLast Opened: ";
  if (timeNotSet == false) statusMessage += String(lastOpened);
  else statusMessage += "Time not set.";
  statusMessage += "@";
  XBee.print(statusMessage);

  statusMessage = "^MSDoor Lock:   ";
  if (lockStateDoor == true) statusMessage += "ENGAGED";
  else statusMessage += "DISENGAGED";
  statusMessage += "@";
  XBee.print(statusMessage);

  statusMessage = "^MSButton Lock: ";
  if (lockStateButton == true) statusMessage += "ENGAGED";
  else statusMessage += "DISENGAGED";
  statusMessage += "@";
  XBee.print(statusMessage);

  statusMessage = "^MSDoor Alarm:  ";
  if (doorAlarmActive == true) statusMessage += "ACTIVE";
  else statusMessage += "INACTIVE";
  statusMessage += "@";
  XBee.print(statusMessage);
}

void checkUpdates(bool sendAll) {
  /*
    bool doorOpenLast = doorOpen;
    bool lockStateDoorLast = lockStateDoor;
    bool lockStateButtonLast = lockStateButton;
    bool doorAlarmActiveLast = doorAlarmActive;

    Command Format:
    %doorOpen$true
    1) % - Variable Name Start
    2) Variable Name
    3) $ - Variable Value Start
    4) Variable Value
  */
  String updateString = "&U";
  if (doorOpen != doorOpenLast || sendAll == true) {
    updateString += "%doorOpen$" + String(doorOpen);
    doorOpenLast = doorOpen;
  }
  if (lockStateDoor != lockStateDoorLast || sendAll == true) {
    updateString += "%lockStateDoor$" + String(lockStateDoor);
    lockStateDoorLast = lockStateDoor;
  }
  if (lockStateButton != lockStateButtonLast || sendAll == true) {
    updateString += "%lockStateButton$" + String(lockStateButton);
    lockStateButtonLast = lockStateButton;
  }
  if (doorAlarmActive != doorAlarmActiveLast || sendAll == true) {
    updateString += "%doorAlarmActive$" + String(doorAlarmActive);
    doorAlarmActiveLast = doorAlarmActive;
  }
  updateString += "&";

  if (updateString.length() > 3) XBee.print(updateString);
}

void triggerDoor() {
  XBee.print(F("^MGTriggering garage door.@"));
  digitalWrite(doorRelay, HIGH);
  delay(250);
  digitalWrite(doorRelay, LOW);
}

void toggleLockout(String lock, bool lockAction) {
  if (lock == "door") {
    if (lockAction == true) {
      if (lockStateDoor == false) {
        digitalWrite(lockoutRelay, HIGH);
        lockStateDoor = true;
        //XBee.println(F("Door locked."));
        XBee.print(F("^MGDoor lock engaged.@"));
      }
      //else XBee.println(F("Door already locked."));
      else XBee.print(F("^MGDoor lock already engaged.@"));
    }
    else {
      if (lockStateDoor == true) {
        digitalWrite(lockoutRelay, LOW);
        lockStateDoor = false;
        //XBee.println(F("Door unlocked."));
        XBee.print(F("^MGDoor lock disengaged.@"));
      }
      //else XBee.println(F("Door already unlocked."));
      else XBee.print(F("^MGDoor lock already disengaged.@"));
    }
  }
  else if (lock == "button") {
    if (lockAction == true) {
      if (lockStateButton == false) {
        digitalWrite(lockoutRelayButton, HIGH);
        lockStateButton = true;
        //XBee.println(F("Button locked."));
        XBee.print(F("^MGButton lock engaged.@"));
      }
      //else XBee.println(F("Button already locked."));
      else XBee.print(F("^MGButton lock already engaged.@"));
    }
    else {
      if (lockStateButton == true) {
        digitalWrite(lockoutRelayButton, LOW);
        lockStateButton = false;
        //XBee.println(F("Button unlocked."));
        XBee.print(F("^MGButton lock disengaged.@"));
      }
      //else XBee.println(F("Button already unlocked."));
      else XBee.print(F("^MGButton lock already disengaged.@"));
    }
  }
}

// Time Functions
void timeFunction(String timeAction) {
  if (timeAction == "get") {
    if (timeNotSet == false) {
      String timeMessage = "^MT";
      timeMessage += formatDigit(hour()) + ":";
      timeMessage += formatDigit(minute()) + ":";
      timeMessage += formatDigit(second()) + " - ";
      timeMessage += String(dayStr(weekday())) + " ";
      timeMessage += String(day()) + " ";
      timeMessage += String(monthStr(month())) + " ";
      timeMessage += String(year());

      XBee.print(timeMessage);
    }
    else sendError("Time has not been set.");
  }
  else if (timeAction == "sync") {
    syncTime();

    //XBee.println(F("Date/time set successfully."));
    XBee.print(F("^MGDate/time set successfully.@"));
  }
  else sendError("Unrecognized action in timeFunction().");
}

// Sync date/time from RPi repeater by request
void syncTime() {
  XBee.print("#TS#");

  bool requestTimeout = false;

  if (!XBee.available()) {
    Serial.print(F("Waiting for date/time serial data..."));
    unsigned long requestTime = millis();
    while (!XBee.available()) {
      if ((millis() - requestTime) > 30000) {
        sendError("Time sync error reported by controller.");
        requestTimeout = true;
        break;
      }
      delay(5);
    }
    Serial.println(F("received."));
  }

  if (requestTimeout == false) {
    String timeString = "";
    while (XBee.available()) {
      char c = XBee.read();
      timeString += c;
      delay(5);
    }

    if (!timeString.startsWith("#") || !timeString.endsWith("#")) {
      sendError("Invalid time string received after sync request.");
    }
    else {
      Serial.println(F("Setting date/time."));

      // setTime(H, M, S, d, m, y)
      setTime(
        timeString.substring((timeString.indexOf('H') + 1), timeString.indexOf('M')).toInt(), // Hour
        timeString.substring((timeString.indexOf('M') + 1), timeString.indexOf('S')).toInt(), // Minute
        timeString.substring((timeString.indexOf('S') + 1), timeString.lastIndexOf('#')).toInt(), // Second
        timeString.substring((timeString.indexOf('d') + 1), timeString.indexOf('y')).toInt(), // Day
        timeString.substring((timeString.indexOf('m') + 1), timeString.indexOf('d')).toInt(), // Month
        timeString.substring((timeString.indexOf('y') + 1), timeString.indexOf('H')).toInt()  // Year
      );
    }
  }
  else Serial.println(F("Request for date/time timed-out."));
}

String formatDigit(int digit) {
  String digitString = "";
  if (digit < 10 || digit == 0) {
    digitString += "0";
  }
  digitString += String(digit);
  return digitString;
}

void sendAlarm(String alarmType) {
  if (alarmType == "door") XBee.print(F("^AD@"));
}

void sendError(String errorMessage) {
  String errorString = "^ME";
  errorString += errorMessage;
  errorString += "@";

  XBee.print(errorString);
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

// Interrupt Functions
void doorCheck() {
  doorOpen = digitalRead(hallSensor);

  if (doorOpen == true) {
    if (timeNotSet == false) lastOpened = now();

    if (doorAlarmActive == true) {
      sendAlarm("door");
      acknowledgeTime = millis();
      waitingAcknowledge = true;
    }
  }
}

// Auxillary Functions
void rangeTest() {
  //XBee.println(F("Beginning 120 second range test."));
  XBee.print(F("^MGBeginning 120 second range test.@"));

  unsigned long testStart = millis();
  for (byte x = 0; (millis() - testStart) < 120000; x++) {
    for (byte y = 0; y < 3; y++) {
      delay(100);
      digitalWrite(ledPin, HIGH);
      delay(100);
      digitalWrite(ledPin, LOW);
    }
    //XBee.print(F("RANGE TEST #")); XBee.println(x + 1);
    String testMessage = "^MGRANGE TEST #" + String(x + 1) + "@";
    XBee.print(testMessage);
    delay(1000);
  }

  heartbeatLast = millis();

  //XBee.println(F("Range test complete."));
  XBee.print(F("^MGRange test complete.@"));
}
