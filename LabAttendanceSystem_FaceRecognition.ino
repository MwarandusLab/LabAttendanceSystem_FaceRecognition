#include <MFRC522.h>
#include <MFRC522Extended.h>
#include <deprecated.h>
#include <require_cpp11.h>

#include <Wire.h>               // LCD Display
#include <SPI.h>                //RFID-RC522 Module
#include <RFID.h>               //RFDI Module
#include <LiquidCrystal_I2C.h>  // LCD Display
#include <SoftwareSerial.h>     //GSM Module

SoftwareSerial mySerial(6, 7);  //GSM Pin Connection

// Pin definitions for RFID module
#define SDA_DIO 10
#define RESET_DIO 9

#define RESTART_BUTTON 3  // Push BUTTON

String message = "";
// RFID and LCD instances
RFID RC522(SDA_DIO, RESET_DIO);
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Tracker variables
int Tracker_1 = 0;
int Tracker_2 = 0;
int Tracker_3 = 0;
int Tracker_4 = 0;
int Tracker_5 = 0;

// Button debounce variables
volatile boolean buttonState = HIGH;
volatile boolean lastButtonState = HIGH;
volatile unsigned long lastDebounceTime = 0;
volatile unsigned long debounceDelay = 50;

// Session state variables
boolean sessionActive = false;
unsigned long sessionStartTime;

// Attendance tracking variables
String students[] = { "IAN ADM001", "JAMES ADM002", "LUCY ADM003", "JOHN ADM004", "JANET ADM005" };
//String admissionNumbers[] = { "ADM001", "ADM002", "ADM003", "ADM004", "ADM005" };
boolean attended[5] = { false, false, false, false, false };
boolean exited[5] = { false, false, false, false, false };
String entryTimes[5];
String exitTimes[5];

// Start time at 8:00 AM
int startHour = 8;
int startMinute = 0;
int currentMinute = 0;

int Sms = 0;
// Define states
enum State { INIT,
             IDLE,
             SESSION_ACTIVE,
             SESSION_ENDED };
State currentState = INIT;

void setup() {
  Serial.begin(9600);
  SPI.begin();
  RC522.init();

  pinMode(RESTART_BUTTON, INPUT_PULLUP);

  lcd.init();
  lcd.backlight();

  mySerial.begin(9600);
  mySerial.println("AT");  //Once the handshake test is successful, it will back to OK
  updateSerial();

  changeState(INIT);
}

void loop() {
  handleButtonPress();
  switch (currentState) {
    case INIT:
      initializeSystem();
      break;
    case IDLE:
      checkAllTrackers();
      break;
    case SESSION_ACTIVE:
      handleSession();
      break;
    case SESSION_ENDED:
      endSession();
      break;
  }
}

void initializeSystem() {
  lcd.clear();
  lcd.setCursor(5, 0);
  lcd.print("SYSTEM");
  lcd.setCursor(1, 1);
  lcd.print("INITIALIZATION");
  delay(2000);
  lcd.clear();
  lcd.setCursor(3, 0);
  lcd.print("Scan ID");
  changeState(IDLE);
}

void checkAllTrackers() {
  rfidScan();
  if (Tracker_1 == 1 && Tracker_2 == 1 && Tracker_3 == 1 && Tracker_4 == 1 && Tracker_5 == 1) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Press Button");
    lcd.setCursor(0, 1);
    lcd.print("Session to Start");
    delay(1000);
    if (Sms == 0) {
      mySerial.println("AT");  //Once the handshake test is successful, it will back to OK
      updateSerial();

      mySerial.println("AT+CMGF=1");  // Configuring TEXT mode
      updateSerial();
      mySerial.println("AT+CMGS=\"+254748613509\"");  //change ZZ with country code and xxxxxxxxxxx with phone number to sms
      updateSerial();
      mySerial.print("Hello Madam, 5 Students are already in the Lab");  //text content
      updateSerial();
      mySerial.write(26);
      delay(1000);
      Sms = 1;
    }
  }
}

void handleSession() {
  unsigned long elapsedTime = millis() - sessionStartTime;
  const unsigned long sessionDuration = 2 * 60 * 1000;  // 2 minutes in milliseconds
  Serial.print("Elapsed Time: ");                       // Debug output
  Serial.println(elapsedTime);                          // Debug output
  delay(500);

  if (currentMinute >= 2) {
    Serial.println("Session Ended");
    changeState(SESSION_ENDED);
  } else {
    updateDisplayWithTime(elapsedTime);
    rfidScan();
  }
}

void updateDisplayWithTime(unsigned long elapsedTime) {
  int minutesPassed = elapsedTime / 60000;
  currentMinute = (startMinute + minutesPassed) % 60;
  int currentHour = startHour + (startMinute + minutesPassed) / 60;

  lcd.clear();
  lcd.setCursor(3, 0);
  lcd.print("Scan ID");
  lcd.setCursor(0, 1);
  lcd.print("Time: ");
  lcd.print(currentHour);
  lcd.print(":");
  if (currentMinute < 10) lcd.print("0");
  lcd.print(currentMinute);
}

void endSession() {
  lcd.clear();
  lcd.setCursor(2, 0);
  lcd.print("Lab Session");
  lcd.setCursor(5, 1);
  lcd.print("Ended");updateSerial();
  updateSerial();
  delay(5000);

  mySerial.println("AT+CMGF=1");  // Configuring TEXT mode
  updateSerial();
  mySerial.println("AT+CMGS=\"+254748613509\"");   // Change ZZ with country code and xxxxxxxxxxx with phone number to SMS
  updateSerial();
  
  // Building the message for students who attended and stayed
  message = "Students who attended and stayed until the end\n";  // Reset message
  for (int i = 0; i < 5; i++) {
    if (attended[i] && !exited[i]) {
      message += students[i] + " - ENT: " + entryTimes[i] + "\n";
    }
  }
  mySerial.println(message);  // Send the message for students who attended and stayed
  updateSerial();
  delay(1000);

  // Reset the message for students who left early
  message = "Students who attended but left early\n";  // Reset message
  for (int i = 0; i < 5; i++) {
    if (attended[i] && exited[i]) {
      message += students[i] + " - ENT: " + entryTimes[i] + ", EXT: " + exitTimes[i] + "\n";
    }
  }
  mySerial.println(message);  // Send the message for students who left early
  updateSerial();
  delay(1000);

  mySerial.write(26);
  delay(2000);

  //Serial.println(message);

  // Reset trackers and session state
  Tracker_1 = Tracker_2 = Tracker_3 = Tracker_4 = Tracker_5 = 0;
  for (int i = 0; i < 5; i++) {
    attended[i] = exited[i] = false;
    entryTimes[i] = exitTimes[i] = "";
  }
  // Reset time to 8:00 AM for the next session
  startHour = 8;
  startMinute = 0;
  currentMinute = 0;
  
  Sms = 0;
  changeState(INIT);
}


void rfidScan() {
  if (RC522.isCard()) {
    RC522.readCardSerial();
    String tagID = "";
    for (int i = 0; i < 5; i++) {
      tagID += String(RC522.serNum[i], DEC);
    }
    Serial.println("Tag ID: " + tagID);

    handleTagID(tagID);
  }
}

void handleTagID(String tagID) {
  if (tagID == "73013180202") {
    processStudent(0);
  } else if (tagID == "71632498013") {
    processStudent(1);
  } else if (tagID == "3521214413106") {
    processStudent(2);
  } else if (tagID == "352113824846") {
    processStudent(3);
  } else if (tagID == "14718716213135") {
    processStudent(4);
  }
}

void processStudent(int studentIndex) {
  unsigned long elapsedTime = millis() - sessionStartTime;
  int minutesPassed = elapsedTime / 60000;
  int currentMinute = (startMinute + minutesPassed) % 60;
  int currentHour = startHour + (startMinute + minutesPassed) / 60;
  String currentTime = String(currentHour) + ":" + (currentMinute < 10 ? "0" : "") + String(currentMinute);

  // Display waiting message
  updateLCD("Verify Face", "");

  // Wait for specific input based on student index
  int expectedInput = studentIndex + 1;  // Expect '1' for first student, '2' for second, etc.
  boolean inputReceived = false;
  unsigned long timeoutStartTime = millis();
  while (millis() - timeoutStartTime < 10000) {  // Wait for up to 10 seconds
    if (Serial.available() > 0) {
      char receivedChar = Serial.read();
      if (receivedChar == expectedInput + '0') {  // Check if received character matches expected
        inputReceived = true;
        break;
      }
    }
    delay(100);  // Adjust delay as needed for responsiveness
  }

  if (inputReceived) {
    if (!attended[studentIndex]) {
      attended[studentIndex] = true;
      entryTimes[studentIndex] = currentTime;
      updateLCD(students[studentIndex], "ENTERING LAB");
      setTracker(studentIndex, 1);
    } else if (!exited[studentIndex]) {
      exited[studentIndex] = true;
      exitTimes[studentIndex] = currentTime;
      updateLCD(students[studentIndex], "EXITING LAB");
      setTracker(studentIndex, 0);
    } else {
      attended[studentIndex] = true;
      exited[studentIndex] = false;
      entryTimes[studentIndex] = currentTime;
      updateLCD(students[studentIndex], "RE-ENTERING LAB");
      setTracker(studentIndex, 1);
    }
  } else {
    // Display timeout message or handle as needed
    updateLCD("  Scan ID", "");
  }
}

void updateLCD(String line1, String line2) {
  lcd.clear();
  lcd.setCursor(2, 0);
  lcd.print(line1);
  lcd.setCursor(0, 1);
  lcd.print(line2);
  delay(2000);
}

void setTracker(int studentIndex, int value) {
  switch (studentIndex) {
    case 0: Tracker_1 = value; break;
    case 1: Tracker_2 = value; break;
    case 2: Tracker_3 = value; break;
    case 3: Tracker_4 = value; break;
    case 4: Tracker_5 = value; break;
  }
}

void handleButtonPress() {
  if (millis() - lastDebounceTime > debounceDelay) {
    buttonState = digitalRead(RESTART_BUTTON);
    if (buttonState == LOW && lastButtonState == HIGH) {
      Serial.println("Button pressed");
      if (!sessionActive && Tracker_1 == 1 && Tracker_2 == 1 && Tracker_3 == 1 && Tracker_4 == 1 && Tracker_5 == 1) {
        changeState(SESSION_ACTIVE);
      }
    }
    lastButtonState = buttonState;
    lastDebounceTime = millis();
  }
}

void changeState(State newState) {
  currentState = newState;
  switch (newState) {
    case INIT:
      initializeSystem();
      break;
    case IDLE:
      lcd.clear();
      lcd.setCursor(3, 0);
      lcd.print("Scan ID");
      break;
    case SESSION_ACTIVE:
      Serial.println("Session started");
      sessionActive = true;
      sessionStartTime = millis();
      lcd.clear();
      lcd.setCursor(3, 0);
      lcd.print("Scan ID");
      lcd.setCursor(0, 1);
      lcd.print("Time: 8:00am");
      delay(1000);
      break;
    case SESSION_ENDED:
      sessionActive = false;
      endSession();
      break;
  }
}
void updateSerial() {
  delay(500);
  while (Serial.available()) {
    mySerial.write(Serial.read());  //Forward what Serial received to Software Serial Port
  }
  while (mySerial.available()) {
    Serial.write(mySerial.read());  //Forward what Software Serial received to Serial Port
  }
}
