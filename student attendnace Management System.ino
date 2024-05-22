#include <Arduino.h>
#include <WiFi.h>
//#include <Firebase_ESP_Client.h>
#include <FirebaseESP32.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <SPI.h>
#include <MFRC522.h>
#include <FirebaseJson.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <TimeLib.h>

#define SDA_PIN   21   // Define the SDA, SCK, MOSI, MISO pins of the RFID reader
#define RST_PIN   36 // Define the reset pin of the RFID reader

MFRC522 mfrc522(SDA_PIN, RST_PIN);    // Create MFRC522 instance

#define FIREBASE_HOST "studentattendance-3eb88-default-rtdb.asia-southeast1.firebasedatabase.app/name"
// Define the authentication token as a firebase_auth_signin_token_t variable
firebase_auth_signin_token_t FIREBASE_AUTH = {
  "bDAcWPFMHudPuOWLSBHNkXSBpi8vFVed4Xxa9kBN" // Your authentication token
};
#define WIFI_SSID "Quarnede WiFi"
#define WIFI_PASSWORD "0011708@#Qua"

FirebaseData firebaseData;
FirebaseConfig firebaseConfig;
FirebaseAuth firebaseAuth;

LiquidCrystal_I2C lcd(0x27, 16, 2);  // Set the LCD address to 0x27 for a 16 chars and 2 line display

#define GREEN_LED_PIN  23 // Pin for Green LED
#define RED_LED_PIN    22 // Pin for Red LED
#define BUZZER_PIN     21 // Pin for Buzzer

void setup() {
  Serial.begin(115200);
  SPI.begin();                      // Init SPI bus
  mfrc522.PCD_Init();               // Init MFRC522

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
 firebaseConfig.host = FIREBASE_HOST;
  firebaseAuth.token = FIREBASE_AUTH;

  Firebase.begin(&firebaseConfig,&firebaseAuth);
  //timeClient.begin();
  
  lcd.init();                       // Initialize the LCD
  lcd.backlight();                  // Turn on backlight

  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(RED_LED_PIN, OUTPUT);
}


void loop() {
  if ( ! mfrc522.PICC_IsNewCardPresent()) {
    return;
  }

  if ( ! mfrc522.PICC_ReadCardSerial()) {
    return;
  }

  String cardID = "";
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    cardID += String(mfrc522.uid.uidByte[i] < 0x10 ? "0" : "");
    cardID += String(mfrc522.uid.uidByte[i], HEX);
  }
  
  // Read student information from Firebase
  String studentName = readStudentNameFromFirebase(cardID);
  
  // Display student info on LCD
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("ID: " + cardID);
  lcd.setCursor(0, 1);
  lcd.print("Name: " + studentName);

  // Check if the student is already marked attendance today
  bool attendanceStatus = checkAttendanceStatus(cardID);

  // Update Firebase with attendance information
  updateFirebaseAttendance(cardID, studentName, attendanceStatus);

  // Indicate attendance status with LEDs and buzzer
  if (attendanceStatus) {
    digitalWrite(GREEN_LED_PIN, HIGH);
    digitalWrite(RED_LED_PIN, LOW);
    tone(BUZZER_PIN, 1000, 1000);
  } else {
    digitalWrite(GREEN_LED_PIN, LOW);
    digitalWrite(RED_LED_PIN, HIGH);
    tone(BUZZER_PIN, 500, 1000);
  }

  delay(2000); // Delay before reading next RFID card
}

String readStudentNameFromFirebase(String cardID) {
  String studentName = "";

  // Construct the path to the node containing student information
  String path = "/students/" + cardID + "/name";

  // Read student's name from Firebase
  if (Firebase.getString(firebaseData, path)) {
    studentName = firebaseData.stringData();
  } else {
    Serial.println("Error reading student name from Firebase");
  }

  return studentName;
}

bool checkAttendanceStatus(String cardID) {
  bool attendanceStatus = false;

  // Get today's date
  String currentDate = String(year()) + "-" + String(month()) + "-" + String(day());

  // Construct the path to the node containing attendance information
  String path = "/attendance/" + currentDate + "/" + cardID;

  // Check if attendance node exists for today
  if (Firebase.getBool(firebaseData, path)) {
    attendanceStatus = firebaseData.boolData();
  }

  return attendanceStatus;
}

void updateFirebaseAttendance(String cardID, String studentName, bool status) {
  // Get current date and time
  String currentDate = String(year()) + "-" + String(month()) + "-" + String(day());
  String currentTime = String(hour()) + ":" + String(minute());

  // Construct the path to the node where attendance will be updated
  String path = "/attendance/" + currentDate + "/" + cardID;

  // Update attendance status in Firebase
  if (Firebase.setBool(firebaseData, path, status)) {
    Serial.println("Attendance updated successfully");
  } else {
    Serial.println("Error updating attendance in Firebase");
  }

  // Construct the path to the node where attendance log will be stored
  String logPath = "/attendance_log/" + currentDate + "/" + currentTime;

  // Construct attendance log data
  String logData = "Name: " + studentName + ", ID: " + cardID + ", Status: " + (status ? "In" : "Out");

  // Update attendance log in Firebase
  if (Firebase.setString(firebaseData, logPath, logData)) {
    Serial.println("Attendance log updated successfully");
  } else {
    Serial.println("Error updating attendance log in Firebase");
  }
}
