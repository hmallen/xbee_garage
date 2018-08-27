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
int heartbeatInterval = 10000;

bool lockStateDoor = false;
bool lockStateButton = false;

time_t lastOpened;

bool doorAlarm = false;
bool waitingAcknowledge = false;
unsigned long acknowledgeTime;

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

  Serial.begin(9600);
  XBee.begin(9600);

  flushBuffer(true);  // Flush any characters that may be in XBee receive buffer
}

void loop() {
  /*
     See xbee_message_reference.txt for
     details of message format, etc.
  */

  if (XBee.available()) {
    String commandString = "";
    while (XBee.available()) {
      char c = XBee.read();
      if (c != '\n' && c != '\r') commandString += c;
      delay(5);
    }

    Serial.print(F("Command Received: ")); Serial.println(commandString);

    if (commandString.startsWith("@") && commandString.endsWith("^")) {
      //Serial.println(F("Processing command."));
      processCommand(commandString);
    }

    else if (commandString.startsWith("^") && commandString.endsWith("@")) {
      //Serial.println(F("Command echo received from repeater. Flushing buffer."));
      flushBuffer(false);
    }

    else if (commandString.startsWith("*")) flushBuffer(false);

    else {
      sendError("Invalid command.");
      flushBuffer(true);
    }
  }

  if (waitingAcknowledge == true && (millis() - acknowledgeTime) > 5000) {
    sendError("Timeout while waiting for door alarm acknowledgement. Remote unit may be out of range.");
    acknowledgeTime = millis();
  }

  if ((millis() - heartbeatLast) > heartbeatInterval) {
    XBee.println(F("^HB@"));
    heartbeatLast = millis();
  }

  if (digitalRead(rangeTestPin) == HIGH) {
    while (digitalRead(rangeTestPin) == HIGH) {
      delay(5);
    }
    rangeTest();
  }
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
      else XBee.println(F("Door already open."));
    }
    else if (action == 'C') {
      // Check if door already closed, and trigger if not
      if (doorOpen == true) triggerDoor();
      else XBee.println(F("Door already closed."));
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
    else sendError("Invalid ACTION encountered while processing DOOR command.");
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
    else sendError("Invalid ACTION encountered while processing BUTTON command.");
  }

  else if (identifier == 'T') {
    if (action == 'G') timeFunction("get");
    else if (action == 'S') timeFunction("set");
    else sendError("Invalid ACTION encountered while processing TIME command.");
  }

  else if (identifier == 'A') {
    if (action == 'A') {
      // Activate door alarm if currently inactive
      if (doorAlarm == false) doorAlarm = true;
      else sendError("Door alarm already active.");
    }
    else if (action == 'D') {
      // Deactivate door alarm if currently active
      if (doorAlarm == true) doorAlarm = false;
      else sendError("Door alarm already inactive.");
    }
    else if (action == 'K') {
      if (waitingAcknowledge == true) waitingAcknowledge = false;
      else sendError("Acknowledgement received unexpectedly. An error may have occurred.");
    }
    else sendError("Invalid ACTION encountered while processing ALARM command.");
  }
  else sendError("Invalid IDENTIFIER encountered while processing command.");
}

void sendStatus() {
  XBee.println(F("^SM"));  // Start-Status-Message

  // Send current status of door and option settings
  XBee.print(F("Door State:  "));
  if (doorOpen == true) XBee.println(F("OPEN"));
  else XBee.println(F("CLOSED"));

  if (timeNotSet == false) {
    XBee.print(F("Last Opened: ")); XBee.println(lastOpened);
  }
  else XBee.println(F("Time not set. No last opened time recorded."));

  XBee.print(F("Door Lock:   "));
  if (lockStateDoor == true) XBee.println(F("ENGAGED"));
  else XBee.println(F("DISENGAGED"));

  XBee.print(F("Button Lock: "));
  if (lockStateButton == true) XBee.println(F("ENGAGED"));
  else XBee.println(F("DISENGAGED"));

  XBee.print(F("Door Alarm:  "));
  if (doorAlarm == true) XBee.println(F("ACTIVE"));
  else XBee.println(F("INACTIVE"));

  XBee.println(F("@"));  // End of status message
}

void triggerDoor() {
  XBee.print(F("Triggering garage door..."));
  digitalWrite(doorRelay, HIGH);
  delay(100);
  digitalWrite(doorRelay, LOW);
  XBee.println(F("complete."));
}

void toggleLockout(String lock, bool lockAction) {
  if (lock == "door") {
    if (lockAction == true) {
      if (lockStateDoor == false) {
        digitalWrite(lockoutRelay, HIGH);
        lockStateDoor = true;
        XBee.println(F("Door locked."));
      }
      else XBee.println(F("Door already locked."));
    }
    else {
      if (lockStateDoor == true) {
        digitalWrite(lockoutRelay, LOW);
        lockStateDoor = false;
        XBee.println(F("Door unlocked."));
      }
      else XBee.println(F("Door already unlocked."));
    }
  }
  else if (lock == "button") {
    if (lockAction == true) {
      if (lockStateButton == false) {
        digitalWrite(lockoutRelayButton, HIGH);
        lockStateButton = true;
        XBee.println(F("Button locked."));
      }
      else XBee.println(F("Button already locked."));
    }
    else {
      if (lockStateButton == true) {
        digitalWrite(lockoutRelayButton, LOW);
        lockStateButton = false;
        XBee.println(F("Button unlocked."));
      }
      else XBee.println(F("Button already unlocked."));
    }
  }
}

// Time Functions
void timeFunction(String timeAction) {
  if (timeAction == "get") {
    if (timeNotSet == false) {
      XBee.println(F("^MT"));
      XBee.print(hour()); XBee.print(F(":"));
      XBee.print(formatDigit(minute())); XBee.print(F(":"));
      XBee.print(formatDigit(second())); XBee.print(F(" "));
      XBee.print(month()); XBee.print(F("/"));
      XBee.print(day()); XBee.print(F("/"));
      XBee.println(year());
      XBee.println(F("@"));
    }
    else sendError("Time has not been set.");
  }
  else if (timeAction == "set") {
    // Month
    int monthInput = getInput("Input current month (1-12):");
    if (monthInput < 1 || monthInput > 12) {
      sendError("Invalid month input. Returning to main program.");
      return;
    }
    // Day
    int dayInput = getInput("Input current day (1-31):");
    if (dayInput < 1 || dayInput > 31) {
      sendError("Invalid day input. Returning to main program.");
      return;
    }
    // Year
    int yearInput = getInput("Input current year (ex. 2018):");
    if (yearInput < 2018) {
      sendError("Invalid year input. Returning to main program.");
      return;
    }
    // Hour
    int hourInput = getInput("Input current hour (0-23):");
    if (hourInput < 0 || hourInput > 23) {
      sendError("Invalid hour input. Returning to main program.");
      return;
    }
    // Minute
    int minuteInput = getInput("Input current minute (0-59):");
    if (minuteInput < 0 || minuteInput > 59) {
      sendError("Invalid minute input. Returning to main program.");
      return;
    }
    // Second
    int secondInput = getInput("Input current second (0-59):");
    if (secondInput < 0 || secondInput > 59) {
      sendError("Invalid second input. Returning to main program.");
      return;
    }

    // Set date/time
    setTime(
      hourInput, minuteInput, secondInput,
      dayInput, monthInput, yearInput
    );
    XBee.println(F("Date/time set successfully."));
  }
  else sendError("Unrecognized action encountered in timeFunction().");
}

String formatDigit(int digit) {
  String digitString = "";
  if (digit < 10 || digit == 0) {
    digitString += "0";
  }
  digitString += String(digit);
  return digitString;
}

// Get serial input from user, returning converted value
int getInput(String requestMessage) {
  String serialInput = "";
  //int intReturn;

  XBee.println(F("^MI"));
  XBee.println(requestMessage);
  XBee.print(F("Response must start and end with the # symbol and contain no spaces."));
  XBee.println(F(" (ex. #12#)"));
  XBee.println(F("@"));

  unsigned long waitStart = millis();
  while (!XBee.available()) {
    if ((millis() - waitStart) > 30000) {
      sendError("Timeout while waiting for serial input.");
      return -1;
    }
    delay(5);
  }

  while (XBee.available()) {
    char c = XBee.read();
    if (isDigit(c) == true) {
      serialInput += c;
    }
    delay(5);
  }

  if (serialInput.startsWith("#") == true && serialInput.endsWith("#") == true) {
    serialInput.remove(0, 1);
    serialInput.remove(serialInput.indexOf('#'));
  }
  else {
    sendError("Invalid input received while setting time.");
    return -1;
  }

  return serialInput.toInt();
}

void sendAlarm(String alarmType) {
  if (alarmType == "door") {
    XBee.println(F("^AD@"));
  }
}

void sendError(String errorMessage) {
  XBee.println(F("^EM"));
  //XBee.print(F("*ERROR* "));
  XBee.println(errorMessage);
  XBee.println(F("@"));
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

// Interrupt Functions
void doorCheck() {
  doorOpen = digitalRead(hallSensor);

  if (doorOpen == true) {
    if (timeNotSet == false) lastOpened = now();

    if (doorAlarm == true) {
      sendAlarm("door");
      acknowledgeTime = millis();
      waitingAcknowledge = true;
    }
  }
}

// Auxillary Functions
void rangeTest() {
  XBee.println(F("Beginning 120 second range test."));

  unsigned long testStart = millis();
  for (byte x = 0; (millis() - testStart) < 120000; x++) {
    for (byte y = 0; y < 3; y++) {
      delay(100);
      digitalWrite(ledPin, HIGH);
      delay(100);
      digitalWrite(ledPin, LOW);
    }
    XBee.print(F("RANGE TEST #")); XBee.println(x + 1);
    delay(1000);
  }

  heartbeatLast = millis();

  XBee.println(F("Range test complete."));
}

