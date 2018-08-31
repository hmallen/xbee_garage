#include <SoftwareSerial.h>
#include <TimeLib.h>

#define hallSensor 2
#define lockRelayDoor 4
#define doorRelay 5
#define lockRelayButton 6
#define rangeTestPin 8
#define xbeeRx 11
#define xbeeTx 12
#define ledPin 13

// Variables that change on device
bool doorState; bool doorStateLast; // 0 = Closed, 1 = Open
time_t lastOpened;

// Variables that are changed remotely
bool doorLock; bool doorLockLast;
bool buttonLock; bool buttonLockLast;
bool doorAlarm; bool doorAlarmLast;

// Variables that need to be communicated in real-time
bool alarmTriggered = false;

SoftwareSerial XBee(xbeeRx, xbeeTx);

void setup() {
  pinMode(lockRelayDoor, OUTPUT); digitalWrite(lockRelayDoor, LOW);
  pinMode(doorRelay, OUTPUT); digitalWrite(doorRelay, LOW);
  pinMode(lockRelayButton, OUTPUT); digitalWrite(lockRelayButton, LOW);
  pinMode(ledPin, OUTPUT); digitalWrite(ledPin, LOW);

  pinMode(hallSensor, INPUT);
  pinMode(rangeTestPin, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(hallSensor), readDoorState, CHANGE);

  // Read initial values of device-specific variables
  readDoorState();
  doorStateLast = doorState;
  //readGasSensors();

  Serial.print(F("Flushing buffer..."));
  flushBuffer();
  Serial.println(F("complete."));

  delay(1000);

  // Get initial values of remotely-set variables
  requestSettings("all");

  Serial.begin(19200);
  XBee.begin(19200);
}

void loop() {
  /*
     Command String Construction:
     Leading Character --> Source
     Trailing Character --> Destination
     ^: Controller
     @: Repeater
     +: Remote (Currently unused/handled by repeater)
     Separators
     %: Variable Name
     $: Variable Value
     Requests
     #: Start/End of Requests/Responses
     /: Request Response Type/Information Divider
     Actions (Unused)
     <: Target (Unused)
     >: Action (Unused)
  */
  if (XBee.available()) {
    char c = XBee.read();
    if (c == '@') {
      String commandString = String(c);
      bool validCommand = false;
      while (XBee.available()) {
        if (c == '^') {
          validCommand = true;
          break;
        }
        commandString += c;
        delay(1);
      }
      Serial.print(F("commandString: ")); Serial.println(commandString);
      Serial.print(F("validCommand: ")); Serial.println(validCommand);
      if (validCommand == true) processCommand(commandString);
    }
  }
  else {
    checkChange();
  }
}

void requestSettings(String requestType) {
  // Send settings request command to controller
  XBee.print("^#request/" + requestType + "#@\n");
}

void checkChange() {
  // Check all relevant boolean variables for state change
  if (doorState != doorStateLast) {
    if (doorState == 1) {
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

void reportChange(String var, bool val) {
  String reportMessage = "^%" + var + "$" + String(val) + "@\n";
  XBee.print(reportMessage);
}

void readDoorState() {
  doorState = digitalRead(hallSensor);
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
  if (var == "doorLock") digitalWrite(lockRelayDoor, val);
  else if (var == "buttonLock") digitalWrite(lockRelayButton, val);
}

void waitReceive(int timeout) {
  bool timeoutError = false;
  unsigned long waitStart = millis();
  if (!XBee.available()) {
    while (!XBee.available()) {
      if ((millis() - waitStart) > timeout) {
        timeoutError = true;
        break;
      }
      delay(1);
    }
  }
  printError("Timeout", "waitReceive()");
}

void flushBuffer() {
  if (XBee.available()) {
    while (XBee.available()) {
      char c = XBee.read();
      delay(1);
    }
  }
}

void printError(String errorType, String errorFunction) {
  Serial.println(errorType + " in " + errorFunction + ".");
}
