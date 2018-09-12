#include <EEPROM.h>
#include <SoftwareSerial.h>
#include <TimeLib.h>

#define SERIAL_DELAY 5

#define doorSensor 2
#define lockRelayDoor 4
#define doorRelay 5
#define lockRelayButton 6
#define rangeTestPin 8
#define xbeeRx 11
#define xbeeTx 12
#define ledPin 13

#define eepromValidBit 50
#define eepromValidBitCount 5
#define eepromDoorLock 55
#define eepromButtonLock 56
#define eepromDoorAlarm 57

bool eepromLoaded = false;  // Becomes true once first set of valid values are saved into or loaded from EEPROM

bool connectionStatus = false;  // Becomes true after successfully pinging repeater

const unsigned long updateInterval = 30000;
unsigned long updateLast = 0;
const int sensorReadInterval = 10000;
unsigned long sensorReadLast = 0;

// Variables that change on device
bool doorState; bool doorStateLast; // 0 = Closed, 1 = Open
time_t lastOpened;

// Variables that are changed remotely
bool doorLock = false; bool doorLockLast = doorLock;
bool buttonLock = false; bool buttonLockLast = buttonLock;
bool doorAlarm = false; bool doorAlarmLast = doorAlarm;

// Variables that need to be communicated in real-time
bool alarmTriggered = false; bool alarmTriggeredLast = alarmTriggered;

SoftwareSerial XBee(xbeeRx, xbeeTx);

void eepromAction(String action, String target = "") {
  if (action == "startup") {
    byte validBits = 0;
    for (byte x = eepromValidBit; x < (eepromValidBit + eepromValidBitCount); x++) {
      if (EEPROM.read(x) == true) validBits++;
    }
    Serial.print(F("Valid Bits: ")); Serial.println(validBits);
    Serial.print(F("Required Bits: ")); Serial.println(eepromValidBitCount);
    if (validBits == eepromValidBitCount) {
      eepromLoaded = true;
    }
  }

  else if (action == "validate") {
    Serial.print(F("Setting valid bits in EEPROM..."));
    for (byte x = eepromValidBit; x < (eepromValidBit + eepromValidBitCount); x++) {
      EEPROM.update(x, true);
    }
    Serial.println(F("complete."));
  }

  else if (action == "update") {
    if (target == "doorLock") EEPROM.update(eepromDoorLock, doorLock);
    else if (target == "buttonLock") EEPROM.update(eepromButtonLock, buttonLock);
    else if (target == "doorAlarm") EEPROM.update(eepromDoorAlarm, doorAlarm);
    else printError("Unrecognized update target", "eepromAction()");
  }

  else if (action == "read") {
    if (eepromLoaded == true) {
      if (target == "doorLock") doorLock = EEPROM.read(eepromDoorLock);
      else if (target == "buttonLock") buttonLock = EEPROM.read(eepromButtonLock);
      else if (target == "doorAlarm") doorAlarm = EEPROM.read(eepromDoorAlarm);
      else printError("Unrecognized read target", "eepromAction()");
    }
    else printError("Attempted to load invalid EEPROM values", "eepromAction()");
  }

  else printError("Unrecognized action", "eepromAction()");
}

void setup() {
  pinMode(lockRelayDoor, OUTPUT); digitalWrite(lockRelayDoor, LOW);
  pinMode(doorRelay, OUTPUT); digitalWrite(doorRelay, LOW);
  pinMode(lockRelayButton, OUTPUT); digitalWrite(lockRelayButton, LOW);
  pinMode(ledPin, OUTPUT); digitalWrite(ledPin, LOW);

  pinMode(doorSensor, INPUT);
  pinMode(rangeTestPin, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(doorSensor), readDoorState, CHANGE);

  Serial.begin(19200);
  XBee.begin(19200);

  // Read initial boolean values from EEPROM, or flag for required update if necessary
  eepromAction("startup");
  if (eepromLoaded == true) {
    Serial.print(F("Loading valid boolean values from EEPROM..."));
    eepromAction("read", "doorLock");
    eepromAction("read", "buttonLock");
    eepromAction("read", "doorAlarm");
  }
  else {
    Serial.print(F("Updating EEPROM values..."));
    eepromAction("update", "doorLock");
    eepromAction("update", "buttonLock");
    eepromAction("update", "doorAlarm");
    Serial.print(F("setting validation bits..."));
    eepromAction("validate");
  }
  Serial.println(F("complete."));

  // Read initial values of device-specific variables
  readDoorState();
  doorStateLast = doorState;
  //readGasSensors();

  Serial.print(F("Flushing buffer..."));
  flushBuffer();
  Serial.println(F("complete."));

  connectionStatus = pingRepeater();

  if (connectionStatus == true) makeRequest("settings"); // Get initial values of remotely-set variables
}

void loop() {
  /*
     Command String Construction:
     Leading Character --> Source
     Trailing Character --> Destination
     ^: Controller
     @: Repeater
     +: Remote (Currently unused/handled by repeater)
     Purpose:
     >: Action
     Requests
     #: Start/End of Requests/Responses
     /: Request Response Type/Information Divider
     Separators
     %: Variable Name
     $: Variable Value
  */
  if (connectionStatus == false) {
    delay(10000);

    Serial.print(F("Flushing buffer..."));
    flushBuffer();
    Serial.println(F("complete."));

    Serial.println(F("No connection established. Pinging repeater."));
    connectionStatus = pingRepeater();
  }

  else {
    if (XBee.available()) {
      char c = XBee.read();
      if (c == '@') {
        String commandString = String(c);
        bool validCommand = false;
        while (XBee.available()) {
          c = XBee.read();
          commandString += c;
          if (c == '^') {
            validCommand = true;
            break;
          }
          delay(SERIAL_DELAY);
        }
        Serial.print(F("commandString: ")); Serial.println(commandString);
        Serial.print(F("validCommand: ")); Serial.println(validCommand);
        if (validCommand == true) processCommand(commandString);
      }
    }

    else if ((millis() - sensorReadLast) > sensorReadInterval) {
      Serial.print(F("Reading sensor data..."));
      // READ SENSORS
      sensorReadLast = millis();
      Serial.println(F("complete."));
    }

    else if ((millis() - updateLast) > updateInterval) {
      Serial.print(F("Sending data update for all variables..."));
      sendUpdate("all");
      updateLast = millis();
      Serial.println(F("complete."));
    }

    else if (timeStatus() == timeNotSet) makeRequest("time");

    else checkChange();

    delay(1000);
  }
}

bool pingRepeater() {
  bool connectionValid = false;

  Serial.println(F("Pinging repeater."));

  makeRequest("ping");
  unsigned long pingStart = millis();
  bool messageWaiting = waitReceive(10000);
  int pingTime = millis() - pingStart;

  if (messageWaiting == true) {
    String pingMessage = "";
    while (XBee.available()) {
      char c = XBee.read();
      pingMessage += c;
      delay(SERIAL_DELAY);
    }
    Serial.print(F("pingMessage: ")); Serial.println(pingMessage);
    if (pingMessage == "@#ping#^") {
      connectionValid = true;
      Serial.print(F("Ping (ms): ")); Serial.println(pingTime);
    }
  }
  else printError("Timeout", "pingRepeater()");

  return connectionValid;
}

void sendUpdate(String updateType) {
  /*
    // Variables that change on device
    bool doorState; bool doorStateLast; // 0 = Closed, 1 = Open
    time_t lastOpened;

    // Variables that are changed remotely
    bool doorLock; bool doorLockLast;
    bool buttonLock; bool buttonLockLast;
    bool doorAlarm; bool doorAlarmLast;

    // Variables that need to be communicated in real-time
    bool alarmTriggered = false; bool alarmTriggeredLast = alarmTriggered;
  */
  if (updateType == "all") {
    reportChange("doorState", String(doorState));
    delay(100);
    reportChange("doorLock", String(doorLock));
    delay(100);
    reportChange("buttonLock", String(buttonLock));
    delay(100);
    reportChange("doorAlarm", String(doorAlarm));
    delay(100);
    reportChange("alarmTriggered", String(alarmTriggered));
  }
  else printError("Invalid updateType", "sendUpdate()");
}

void checkChange() {
  // Check all relevant boolean variables for state change
  if (doorState != doorStateLast) {
    if (doorState == 1) {
      if (timeStatus() != timeNotSet) {
        lastOpened = now(); // Current date/time
        reportChange("lastOpened", String(lastOpened));
      }
      if (doorAlarm == true) {
        alarmTriggered = true;
        reportChange("alarmTriggered", String(alarmTriggered));
      }
    }
    else {
      if (doorAlarm == true) {
        alarmTriggered = false;
        reportChange("alarmTriggered", String(alarmTriggered));
      }
    }
    reportChange("doorState", String(doorState));
    doorStateLast = doorState;
  }
  if (doorLock != doorLockLast) {
    toggleLock("door", doorLock);
    reportChange("doorLock", String(doorLock));
    doorLockLast = doorLock;
  }
  if (buttonLock != buttonLockLast) {
    toggleLock("button", buttonLock);
    reportChange("buttonLock", String(buttonLock));
    buttonLockLast = buttonLock;
  }
  if (doorAlarm != doorAlarmLast) {
    reportChange("doorAlarm", String(doorAlarm));
    doorAlarmLast = doorAlarm;
  }
  if (alarmTriggered != alarmTriggeredLast) {
    reportChange("alarmTriggered", String(alarmTriggered));
    alarmTriggeredLast = alarmTriggered;
  }
}

void makeRequest(String requestType) {
  // Send settings request command to controller
  XBee.print("^#request/" + requestType + "#@\n");
}

void reportChange(String var, String val) {
  String reportMessage = "^%" + var + "$" + String(val) + "@\n";
  XBee.print(reportMessage);

  if (var == "doorLock" || var == "buttonLock" || var == "doorAlarm") eepromAction("update", var);
}

void processCommand(String command) {
  char separator = command.charAt(1);
  Serial.print(F("separator: ")); Serial.println(separator);

  // Variable Update
  if (separator == '%') {
    String var = command.substring((command.indexOf('%') + 1), command.indexOf('$'));
    Serial.print(F("var: ")); Serial.println(var);
    String val = command.substring((command.indexOf('$') + 1), command.indexOf('^'));
    Serial.print(F("val: ")); Serial.println(val);
    updateVariable(var, val);
  }

  else if (separator == '#') {
    String responseType = "";
    if (command.indexOf('/') > 0) {
      responseType = command.substring((command.indexOf('#') + 1), command.indexOf('/'));
    }
    else {
      responseType = command.substring((command.indexOf('#') + 1), command.lastIndexOf('#'));
    }
    Serial.print(F("responseType: ")); Serial.println(responseType);

    if (responseType == "ping") printError("Unrequested ping", "processCommand()");

    else if (responseType == "time") {
      String timeString = command.substring((command.indexOf('/') + 1), command.lastIndexOf('#'));
      Serial.print(F("timeString: ")); Serial.println(timeString);

      syncTime(timeString);
    }
  }

  else if (separator == '>') {
    String actionTarget = command.substring((command.indexOf('>') + 1), command.indexOf('^'));
    Serial.print(F("actionTarget: ")); Serial.println(actionTarget);

    bool toggleVal = false;

    if (actionTarget == "door") triggerDoor();
    else if (actionTarget == "doorLock") {
      if (doorLock == false) toggleVal = true;
      toggleLock("doorLock", toggleVal);
    }
    else if (actionTarget == "buttonLock") {
      if (buttonLock == false) toggleVal = true;
      toggleLock("buttonLock", toggleVal);
    }
    else if (actionTarget == "doorAlarm") doorAlarm = !doorAlarm;
    else printError("Invalid actionTarget", "processCommand()");
  }

  else printError("Invalid separator", "processCommand()");
}

void updateVariable(String var, String val) {
  int valConv = val.toInt();
  if (valConv == 0 || valConv == 1) {
    if (var == "doorLock") doorLock = valConv;
    else if (var == "buttonLock") buttonLock = valConv;
    else if (var == "doorAlarm") doorAlarm = valConv;
    else printError("Invalid variable", "updateVariable()");
  }
  else printError("Invalid value", "updateVariable()");
}

void readDoorState() {
  doorState = digitalRead(doorSensor);
}

/*
  void readGasSensors() {
  //
  }
*/

void triggerDoor() {
  digitalWrite(doorRelay, HIGH);
  delay(250);
  digitalWrite(doorRelay, LOW);
}

void toggleLock(String var, bool val) {
  Serial.print(F("var: ")); Serial.println(var);
  Serial.print(F("val: ")); Serial.println(val);

  if (var == "doorLock") {
    digitalWrite(lockRelayDoor, val);
    doorLock = val;
  }
  else if (var == "buttonLock") {
    digitalWrite(lockRelayButton, val);
    buttonLock = val;
  }
}

String datetimeCurrent(time_t dtCurrent) {
  //time_t dtCurrent = now();

  String datetimeString = formatDigit(month(dtCurrent));
  datetimeString += formatDigit(day(dtCurrent));
  datetimeString += String(year(dtCurrent));
  datetimeString += "-";
  datetimeString += formatDigit(hour(dtCurrent));
  datetimeString += formatDigit(minute(dtCurrent));
  datetimeString += formatDigit(second(dtCurrent));

  return datetimeString;
}

void syncTime(String timeString) {
  // "m8d31y2018H20M50S35
  // OR
  // "T{UNIX_TIME}"

  byte monthInput = timeString.substring((timeString.indexOf('m') + 1), timeString.indexOf('d')).toInt();
  byte dayInput = timeString.substring((timeString.indexOf('d') + 1), timeString.indexOf('y')).toInt();
  int yearInput = timeString.substring((timeString.indexOf('y') + 1), timeString.indexOf('H')).toInt();
  byte hourInput = timeString.substring((timeString.indexOf('H') + 1), timeString.indexOf('M')).toInt();
  byte minuteInput = timeString.substring((timeString.indexOf('M') + 1), timeString.indexOf('S')).toInt();
  byte secondInput = timeString.substring(timeString.indexOf('S') + 1).toInt();

  Serial.print(F("Month:  ")); Serial.println(monthInput);
  Serial.print(F("Day:    ")); Serial.println(dayInput);
  Serial.print(F("Year:   ")); Serial.println(yearInput);
  Serial.print(F("Hour:   ")); Serial.println(hourInput);
  Serial.print(F("Minute: ")); Serial.println(minuteInput);
  Serial.print(F("Second: ")); Serial.println(secondInput);

  setTime(hourInput, minuteInput, secondInput, dayInput, monthInput, yearInput);

  /*
    unsigned long unixTime = timeString.substring(timeString.indexOf('T') + 1);
    Serial.print(F("unixTime: ")); Serial.println(unixTime);

    setTime(unixTime);
  */
}

String formatDigit(byte digit) {
  String digitFormatted = "";
  if (digit < 10 || digit == 0) {
    digitFormatted += "0";
  }
  digitFormatted += String(digit);
  return digitFormatted;
}

bool waitReceive(int timeout) {
  bool receiveSuccess = true;

  unsigned long waitStart = millis();
  if (!XBee.available()) {
    while (!XBee.available()) {
      if ((millis() - waitStart) > timeout) {
        receiveSuccess = false;
        break;
      }
      delay(SERIAL_DELAY);
    }
  }
  printError("Timeout", "waitReceive()");

  return receiveSuccess;
}

void flushBuffer() {
  if (XBee.available()) {
    while (XBee.available()) {
      char c = XBee.read();
      delay(SERIAL_DELAY);
    }
  }
}

void printError(String errorType, String errorFunction) {
  Serial.println(errorType + " in " + errorFunction + ".");
}
