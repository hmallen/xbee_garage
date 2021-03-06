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

bool connectionStatus = false;  // Becomes true after successfully pinging repeater

const int updateInterval = 30000;
unsigned long updateLast = 0;
const int sensorReadInterval = 10000;
unsigned long sensorReadLast = 0;
const unsigned long pingInterval = 60000;
unsigned long pingLast = 0;

// Variables that change on device
bool doorState; bool doorStateLast; // 0 = Closed, 1 = Open
time_t lastOpened;

// Variables that are changed remotely
//bool doorLock; bool doorLockLast;
//bool buttonLock; bool buttonLockLast;
//bool doorAlarm; bool doorAlarmLast;
bool doorLock = false; bool doorLockLast = doorLock;
bool buttonLock = false; bool buttonLockLast = buttonLock;
bool doorAlarm = false; bool doorAlarmLast = doorAlarm;

// Variables that need to be communicated in real-time
bool alarmTriggered = false; bool alarmTriggeredLast = alarmTriggered;

SoftwareSerial XBee(xbeeRx, xbeeTx);

void setup() {
  //pinMode(lockRelayDoor, OUTPUT); digitalWrite(lockRelayDoor, LOW);
  //pinMode(doorRelay, OUTPUT); digitalWrite(doorRelay, LOW);
  //pinMode(lockRelayButton, OUTPUT); digitalWrite(lockRelayButton, LOW);
  pinMode(ledPin, OUTPUT); digitalWrite(ledPin, LOW);

  //pinMode(doorSensor, INPUT);
  pinMode(rangeTestPin, INPUT_PULLUP);

  //attachInterrupt(digitalPinToInterrupt(doorSensor), readDoorState, CHANGE);

  Serial.begin(19200);
  XBee.begin(19200);

  // Read initial values of device-specific variables
  readDoorState();
  doorStateLast = doorState;
  //readGasSensors();

  Serial.print(F("Flushing buffer..."));
  flushBuffer();
  Serial.println(F("complete."));

  connectionStatus = pingRepeater();
  Serial.print(F("connectionStatus: ")); Serial.println(connectionStatus);

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
  delay(1000);

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

    else if ((millis() - pingLast) > pingInterval) connectionStatus = pingRepeater();

    else checkChange();
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

  pingLast = millis();

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
    reportChange("doorState", doorState);
    delay(100);
    reportChange("doorLock", doorLock);
    delay(100);
    reportChange("buttonLock", buttonLock);
    delay(100);
    reportChange("doorAlarm", doorAlarm);
    delay(100);
    reportChange("alarmTriggered", alarmTriggered);
  }
  else printError("updateType", "sendUpdate()");
}

void checkChange() {
  // Check all relevant boolean variables for state change
  if (doorState != doorStateLast) {
    if (doorState == 1) {
      if (timeStatus() != timeNotSet) lastOpened = now(); // Current date/time
      if (doorAlarm == true) {
        alarmTriggered = true;
        reportChange("alarmTriggered", alarmTriggered);
      }
    }
    else {
      if (doorAlarm == true) {
        alarmTriggered = false;
        reportChange("alarmTriggered", alarmTriggered);
      }
    }
    reportChange("doorState", doorState);
    doorStateLast = doorState;
  }
  if (doorLock != doorLockLast) {
    toggleLock("door", doorLock);
    reportChange("doorLock", doorLock);
    doorLockLast = doorLock;
  }
  if (buttonLock != buttonLockLast) {
    toggleLock("button", buttonLock);
    reportChange("buttonLock", buttonLock);
    buttonLockLast = buttonLock;
  }
  if (doorAlarm != doorAlarmLast) {
    reportChange("doorAlarm", doorAlarm);
    doorAlarmLast = doorAlarm;
  }
  if (alarmTriggered != alarmTriggeredLast) {
    reportChange("alarmTriggered", alarmTriggered);
    alarmTriggeredLast = alarmTriggered;
  }
}

void makeRequest(String requestType) {
  // Send settings request command to controller
  XBee.print("^#request/" + requestType + "#@\n");
}

void reportChange(String var, bool val) {
  String reportMessage = "^%" + var + "$" + String(val) + "@\n";
  XBee.print(reportMessage);
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

    if (actionTarget == "door") triggerDoor();
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
  //doorState = digitalRead(doorSensor);
  doorState = 0;
}

/*
  void readGasSensors() {
  //
  }
*/

void triggerDoor() {
  //digitalWrite(doorRelay, HIGH);
  digitalWrite(ledPin, HIGH);
  delay(250);
  //digitalWrite(doorRelay, LOW);
  digitalWrite(ledPin, LOW);
}

void toggleLock(String var, bool val) {
  if (var == "doorLock") digitalWrite(ledPin, val); //digitalWrite(lockRelayDoor, val);
  else if (var == "buttonLock") digitalWrite(ledPin, val); //digitalWrite(lockRelayButton, val);
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
  if (receiveSuccess == false) printError("Timeout", "waitReceive()");

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

void rangeTest() {
  Serial.print(F("Beginning 120 second range test..."));
  unsigned long testStart = millis();
  byte count = 0;
  while ((millis() - testStart) < 120000) {
    count++;
    String testMessage = "RANGE TEST #" + String(count) + "\n";
    XBee.print(testMessage);
    delay(1000);
  }
  Serial.println(F("complete."));
}
