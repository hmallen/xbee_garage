#include <SoftwareSerial.h>
#include <TimeLib.h>

#define SERIAL_DELAY 5

#define carbonMonoxideSensor A1
#define airQualitySensor A2
#define doorSensor 2
#define lockRelayDoor 4
#define doorRelay 5
#define lockRelayButton 6
#define lightRelay 7
#define rangeTestPin 8
#define xbeeRx 11
#define xbeeTx 12
#define ledPin 13

bool connectionStatus = false;  // Becomes true after successfully pinging repeater
bool checkRequired = false; // Becomes true when variable updated locally to allow remote update before proceeding

const unsigned long updateInterval = 30000;
unsigned long updateLast = 0;
const int sensorReadInterval = 10000;
unsigned long sensorReadLast = 0;

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
  pinMode(lockRelayDoor, OUTPUT); digitalWrite(lockRelayDoor, LOW);
  pinMode(doorRelay, OUTPUT); digitalWrite(doorRelay, LOW);
  pinMode(lockRelayButton, OUTPUT); digitalWrite(lockRelayButton, LOW);
  pinMode(ledPin, OUTPUT); digitalWrite(ledPin, LOW);

  pinMode(doorSensor, INPUT);
  pinMode(rangeTestPin, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(doorSensor), readDoorState, CHANGE);

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

  if (connectionStatus == true) {
    makeRequest("settings"); // Get initial values of remotely-set variables
    waitReceive(10000);
  }
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
    if (checkRequired == false) {
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

      else if (timeStatus() == timeNotSet) {
        makeRequest("time");
        bool messageWaiting = waitReceive(10000);
      }

      //else checkChange();
    }
  }

  checkChange();  // Check for local variable changes and update OpenHAB if necessary
  delay(100);
}

bool pingRepeater() {
  bool connectionValid = false;

  Serial.println(F("Pinging repeater."));

  makeRequest("ping");
  unsigned long pingStart = millis();
  bool messageWaiting = waitReceive(10000);
  int pingDuration = millis() - pingStart;

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
      Serial.print(F("Ping (ms): ")); Serial.println(pingDuration);
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
    //toggleLock("door", doorLock);
    triggerAction("doorLock", doorLock)
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

  if (checkRequired == true) checkRequired = false;
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
    //String actionTarget = command.substring((command.indexOf('>') + 1), command.indexOf('^'));
    String actionTarget = command.substring((command.indexOf('>') + 1), command.indexOf('<'));
    Serial.print(F("actionTarget: ")); Serial.println(actionTarget);
    String actionCommand = command.substring((command.indexOf('<') + 1), command.indexOf('^'));
    Serial.print(F("actionCommand: ")); Serial.println(actionCommand)

    if (actionTarget.length() > 0 && actionCommand.length() > 0) {
      if (actionTarget == "door") triggerAction(actionTarget, actionInt);
      else if (actionTarget == "doorLock" || actionTarget == "buttonLock" || actionTarget == "doorAlarm") {
        updateVariable(actionTarget, actionCommand);
      }
      else printError("Invalid actionTarget", "processCommand()");
    }
    else printError("Error parsing action target/command", "processCommand()");
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

void triggerAction(String target, bool action = -1) {
  if (target == "door") {
    digitalWrite(doorRelay, HIGH);
    delay(250);
    digitalWrite(doorRelay, LOW);
  }

  else if (action == -1) printError("Action boolean not provided", "triggerAction()");

  else {
    else if (target == "doorLock") {
      //
    }
    else if (target == "buttonLock") {
      //
    }
    else if (target == "doorAlarm") {
      //
    }
  }
}

void toggleLock(String var, bool val) {
  if (var == "doorLock") digitalWrite(lockRelayDoor, val);
  else if (var == "buttonLock") digitalWrite(lockRelayButton, val);
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
