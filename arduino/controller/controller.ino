//#include <SoftwareSerial.h>

#define SERIAL_BAUD 115200
#define SERIAL_DELAY 5

#define lightSensor A0
//#define gasSensorCO A1
//#define gasSensorAQ A2
#define doorSensor 2
//#define lightSensor 3
#define lockRelayDoor 4
#define doorRelay 5
#define lockRelayButton 6
#define lightRelay 7
#define rangeTestPin 8
//#define xbeeRx 11
//#define xbeeTx 12
//#define piRx 11
//#define piTx 12
#define ledPin 13

bool connectionStatus = false;  // Becomes true after successfully pinging repeater
bool checkRequired = false; // Becomes true when variable updated locally to allow remote update before proceeding

const unsigned long updateInterval = 60000;
unsigned long updateLast = 0;
const int sensorReadInterval = 30000;
unsigned long sensorReadLast = 0;

// Variables that change on device
volatile bool doorState; bool doorStateLast;
bool lightState; bool lightStateLast;
//volatile bool lightState; bool lightStateLast;

// Variables that are changed remotely
bool doorLock = false; bool doorLockLast = doorLock;
bool buttonLock = false; bool buttonLockLast = buttonLock;

//SoftwareSerial XBee(xbeeRx, xbeeTx);
//SoftwareSerial piSerial(piRx, piTx);

void setup() {
  pinMode(lockRelayDoor, OUTPUT); digitalWrite(lockRelayDoor, LOW);
  pinMode(doorRelay, OUTPUT); digitalWrite(doorRelay, LOW);
  pinMode(lockRelayButton, OUTPUT); digitalWrite(lockRelayButton, LOW);
  pinMode(lightRelay, OUTPUT); digitalWrite(lightRelay, LOW);
  pinMode(ledPin, OUTPUT); digitalWrite(ledPin, LOW);

  pinMode(doorSensor, INPUT);
  attachInterrupt(digitalPinToInterrupt(doorSensor), readDoorState, CHANGE);
  //pinMode(lightSensor, INPUT);
  //attachInterrupt(digitalPinToInterrupt(lightSensor), readLightState, CHANGE);

  pinMode(rangeTestPin, INPUT_PULLUP);

  //UNUSED.begin(19200);
  Serial.begin(SERIAL_BAUD);

  // Read initial values of device-specific variables
  readDoorState();
  doorStateLast = doorState;
  //readLightState();
  //lightStateLast = lightState;
  //readGasSensors();

  //UNUSED.print(F("Flushing buffer..."));
  flushBuffer();
  //UNUSED.println(F("complete."));

  connectionStatus = pingRepeater();

  if (connectionStatus == true) {
    makeRequest("settings"); // Get initial values of remotely-set variables
    waitReceive(5000);
  }
}

void loop() {
  if (connectionStatus == false) {
    delay(10000);

    //UNUSED.print(F("Flushing buffer..."));
    flushBuffer();
    //UNUSED.println(F("complete."));

    //UNUSED.println(F("No connection established. Pinging repeater."));
    connectionStatus = pingRepeater();
  }

  else {
    if (checkRequired == false) {
      if (Serial.available()) {
        char c = Serial.read();
        if (c == '@') {
          String commandString = String(c);
          bool validCommand = false;
          while (Serial.available()) {
            c = Serial.read();
            commandString += c;
            if (c == '^') {
              validCommand = true;
              break;
            }
            delay(SERIAL_DELAY);
          }
          //UNUSED.print(F("commandString: ")); //UNUSED.println(commandString);
          //UNUSED.print(F("validCommand: ")); //UNUSED.println(validCommand);
          if (validCommand == true) processCommand(commandString);
        }
      }

      else if ((millis() - sensorReadLast) > sensorReadInterval) {
        //UNUSED.print(F("Reading sensor data..."));
        sensorReadLast = millis();
        //UNUSED.println(F("complete."));
      }

      else if ((millis() - updateLast) > updateInterval) {
        //UNUSED.print(F("Sending data update for all variables..."));
        sendUpdate("all");
        updateLast = millis();
        //UNUSED.println(F("complete."));
      }
    }
    checkChange();  // Check for local variable changes and update OpenHAB if necessary
  }
  //readLightState();

  delay(100);
}

bool pingRepeater() {
  bool connectionValid = false;

  //UNUSED.println(F("Pinging repeater."));

  makeRequest("ping");
  unsigned long pingStart = millis();
  bool messageWaiting = waitReceive(5000);
  int pingDuration = millis() - pingStart;

  if (messageWaiting == true) {
    String pingMessage = "";
    while (Serial.available()) {
      char c = Serial.read();
      pingMessage += c;
      delay(SERIAL_DELAY);
    }
    //UNUSED.print(F("pingMessage: ")); //UNUSED.println(pingMessage);
    if (pingMessage == "@#ping#^") {
      connectionValid = true;
      //UNUSED.print(F("Ping (ms): ")); //UNUSED.println(pingDuration);
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
    //reportChange("lightState", lightState);
    //delay(100);
    reportChange("doorLock", doorLock);
    delay(100);
    reportChange("buttonLock", buttonLock);
  }
  else printError("updateType", "sendUpdate()");
}

void checkChange() {
  // Check all relevant boolean variables for state change
  if (doorState != doorStateLast) {
    reportChange("doorState", doorState);
    doorStateLast = doorState;
  }
  /*
    if (lightState != lightStateLast) {
    reportChange("lightState", lightState);
    lightStateLast = lightState;
    }
  */
  if (doorLock != doorLockLast) {
    triggerAction("doorLock", doorLock);
    reportChange("doorLock", doorLock);
    doorLockLast = doorLock;
  }
  if (buttonLock != buttonLockLast) {
    triggerAction("buttonLock", buttonLock);
    reportChange("buttonLock", buttonLock);
    buttonLockLast = buttonLock;
  }

  if (checkRequired == true) checkRequired = false;
}

void makeRequest(String requestType) {
  // Send settings request command to controller
  Serial.print("^#request/" + requestType + "#@\n");
}

void reportChange(String var, bool val) {
  String reportMessage = "^%" + var + "$" + String(val) + "@\n";
  Serial.print(reportMessage);
}

void processCommand(String command) {
  char separator = command.charAt(1);
  //UNUSED.print(F("separator: ")); //UNUSED.println(separator);

  // Variable Update
  if (separator == '%') {
    String var = command.substring((command.indexOf('%') + 1), command.indexOf('$'));
    //UNUSED.print(F("var: ")); //UNUSED.println(var);
    String val = command.substring((command.indexOf('$') + 1), command.indexOf('^'));
    //UNUSED.print(F("val: ")); //UNUSED.println(val);
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
    //UNUSED.print(F("responseType: ")); //UNUSED.println(responseType);

    if (responseType == "ping") printError("Unrequested ping", "processCommand()");
  }

  else if (separator == '>') {
    //String actionTarget = command.substring((command.indexOf('>') + 1), command.indexOf('^'));
    String actionTarget = command.substring((command.indexOf('>') + 1), command.indexOf('<'));
    //UNUSED.print(F("actionTarget: ")); //UNUSED.println(actionTarget);
    String actionCommand = command.substring((command.indexOf('<') + 1), command.indexOf('^'));
    //UNUSED.print(F("actionCommand: ")); //UNUSED.println(actionCommand);

    if (actionTarget.length() > 0 && actionCommand.length() > 0) {
      if (actionTarget == "door" || actionTarget == "light") {
        int actionInt = actionCommand.toInt();
        if (0 <= actionInt <= 1) triggerAction(actionTarget, actionInt);
        else printError("Invalid actionCommand/actionInt", "processCommand()");
      }
      else if (actionTarget == "doorLock" || actionTarget == "buttonLock") updateVariable(actionTarget, actionCommand);
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
    else printError("Invalid variable", "updateVariable()");
  }
  else printError("Invalid value", "updateVariable()");
}

void triggerAction(String target, bool action) {
  if (target == "door") {
    digitalWrite(doorRelay, HIGH);
    delay(250);
    digitalWrite(doorRelay, LOW);
  }

  else if (target == "light") {
    digitalWrite(lightRelay, HIGH);
    delay(250);
    digitalWrite(lightRelay, LOW);
  }

  else if (target == "doorLock") digitalWrite(lockRelayDoor, action);

  else if (target == "buttonLock") digitalWrite(lockRelayButton, action);
}

void readDoorState() {
  doorState = digitalRead(doorSensor);
}

void readLightState() {
  byte lightVal = analogRead(lightSensor);
  if (lightVal < 50) lightState = false;
  else lightState = true;
}

/*
  void readGasSensors() {
  //
  }
*/

bool waitReceive(int timeout) {
  bool receiveSuccess = true;

  unsigned long waitStart = millis();
  if (!Serial.available()) {
    while (!Serial.available()) {
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
  if (Serial.available()) {
    while (Serial.available()) {
      char c = Serial.read();
      delay(SERIAL_DELAY);
    }
  }
}

void printError(String errorType, String errorFunction) {
  //UNUSED.println(errorType + " in " + errorFunction + ".");
}
