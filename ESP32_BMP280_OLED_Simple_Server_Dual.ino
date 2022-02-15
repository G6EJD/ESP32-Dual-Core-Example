// An OLED display driven by an ESP32 tht reads a BME280 sensor for temperature, humidity and pressure
// The programme uses the two ESp32 cores, one to measure the BME280 and the other to run a webserver

// ESP32_BMP280_OLED_Simple_Server_Dual

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>

WebServer server(80);

#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
Adafruit_BME280 bme;              // initialize Adafruit BME280 library
#define BME280_I2C_ADDRESS  0x76  // define device I2C address: 0x76 or 0x77
//BMP280  Pin SDA   >>>>>   ESP32  Pin=GPIO 21 *****  ESP8266  Pin=GPIO4(D2) *****
//BMP280  Pin SCL   >>>>>   ESP32  Pin=GPIO 22 *****  ESP8266  Pin=GPIO5(D1) *****

#define SEALEVELPRESSURE_HPA (1013.25)
// My elevation = 122 ft  or  + 3.13 Hpa
float temperature, humidity, pressure, altitude;

#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>
// initialize SSD1306 OLED display library
#define SCREEN_WIDTH 128  // OLED display width, in pixels
#define SCREEN_HEIGHT 64  // OLED display height, in pixels
//#define OLED_RESET   5  // Reset pin # GPIO5 (D1)
#define OLED_RESET   - 1  // Reset pin # (or -1 if sharing reset pin)

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

//  Configure Wi-Fi Network
const char*     ssid = "Your SSID here";          // your network SSID (name)
const char* password = "Your WIFI password Here"; // your network password

#define LED_BUILTIN 2
const char* ServerName = "SensorDisplay";
String local_name = "http://" + String(ServerName) + ".local/";   // http://pressure.local/
TaskHandle_t Task1; // MainLoop, Core 0
TaskHandle_t Task2; // Server,   Core 1

//-----------------------------
void setup() {
  Serial.begin(115200);
  delay(400);          // Time to press "Clear output" in Serial Monitor
  Serial.println("\nProgram ~ " + Filename() + "\n");
  delay(300);          // Wait a little bit
  Wire.begin();
  pinMode(LED_BUILTIN, OUTPUT);

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with I2C address = 0x3C
  display.clearDisplay();               // clear the display buffer.
  display.setTextSize(1);               // text size = 1
  display.setTextColor(WHITE, BLACK);   // set text color to white and black background
  display.setTextWrap(false);           // disable text wrap
  display.setCursor(0, 4);              // move cursor to position (0, 4) pixel
  display.print("Checking Sensor");     // On OLED
  display.display();                    // Update the display

  if (!bme.begin(0x76)) {   // Initialize the BME280  define I2C address: 0x76 or if Adafruit use 0x77
    Serial.println("Could not find a valid BMP280 sensor, check wiring!");
    display.println("  No BMP");        // On OLED
    display.display();                  // Update the display
    while (1) {
      delay(1000);                      // Stay here
    }
  }

  Serial.println("BMP280 sensor ~ Connected OK");
  display.println("  OK");              // On OLED
  display.display();                    // Update the display

  delay(300);    // Wait a little bit

  Serial.print("Connecting to ");
  Serial.print(ssid);
  WiFi.begin(ssid, password);              // Connect to Wi-Fi network
  while (WiFi.status() != WL_CONNECTED) {  // Connecting to Wi-Fi network
    delay(500);    // Wait a little bit
    Serial.print(".");
  }
  Serial.println(" ~ Connected OK");

  long rssi = WiFi.RSSI() + 100;     // Print the Signal Strength:
  Serial.println("Signal Strength ~ " + String(rssi));

  WiFi.mode(WIFI_STA);

  if (!MDNS.begin(ServerName)) {     // Start mDNS with ServerName
    Serial.println("Error setting up MDNS responder!");
    while (1) {
      delay(1000);                   // Stay here
    }
  }
  Serial.print("MDNS started ~\tUse IP Address\t: ");
  Serial.println(WiFi.localIP());
  Serial.println("\t\tOr Url Address \t: " + local_name);

  display.println(String(ssid));                   // Display Network On OLED
  display.println("Signal = " + String(rssi));     // Display Signal On OLED
  display.println(WiFi.localIP());                 // Display IP addr On OLED
  display.println(local_name);                     // Display local_name On OLED
  display.display();                               // Update the display
  delay(5000);                                     // Time to read display
  display.clearDisplay();                          // Clear the display
  OLED_display();                                  // Display Parameters On OLED

  Serial.println("\nTemperature and Pressure Readings");
  Serial.println("\tT: " + String(temperature * 1.8 + 32) + " deg F");
  Serial.println("\tT: " + String(temperature) + " deg C");
  Serial.println("\tH: " + String(humidity) + " %");
  Serial.println("\tP: " + String(pressure) + " hPa");
  Serial.println("\tA: " + String(altitude) + " m");
  Serial.println("\tA: " + String(altitude * 3.281) + " ft");

  //Task1 = MainLoop() function, with priority 1 and executed on core 0
  xTaskCreatePinnedToCore( MainLoop, "Task1", 10000, NULL, 1, &Task1, 0);
  delay(200);          // Wait a short while
  //Task2 = Server() function, with priority 1 and executed on core 1
  xTaskCreatePinnedToCore( ServerHandling, "Task2", 10000, NULL, 1, &Task2, 1);
  delay(200);          // Wait a short while
  Serial.println(F("Dual Core Tasks Initialized"));

  server.on("/", handle_root);      // Handles server Connection
  server.begin();                   // Server Start

  Serial.println("HTTP server started");
  Serial.println("Set Up Complete");
  Serial.println("*******************************************");
}

void loop() {
  // Nothing to do here
}

//-----------------------------

//  *********** Main_Loop  TASK 1   for Getting data and Display On OLED
void MainLoop( void * pvParameters ) {
  for (;;) {
    OLED_display();   // Display Parameters On OLED
    delay(6000);      // Every 6 Seconds
  }
}
//-----------------------------

//  *********** Go_Server HTML  TASK 2   Handles Connection
void ServerHandling ( void * pvParameters ) {
  for (;;) {
    server.handleClient();               // Handles request
  }
}
//-----------------------------

void handle_root() {                    // Handles Connection
  digitalWrite(LED_BUILTIN, HIGH);
  Serial.println("New Connection");
  String html_code = Web_Page(html_code);
  server.send(200, "text/html", html_code);
  digitalWrite(LED_BUILTIN, LOW);
}

String Web_Page(String html_code) {      // Creates Code
  html_code = "<!DOCTYPE html><HTML>";
  String color;
  if (temperature <= 0 ) color = "<FONT COLOR='Blue'>"; else color = "";
  if (temperature >= 32.2 ) color = "<FONT COLOR='Red'>"; else color = "";
  html_code += "<HEAD>";
  html_code += "<meta name=\'viewport\' content=\'width=device-width, initial-scale=1.0, user-scalable=no\'>";
  //html_code += "<meta http-equiv='refresh' content='10'>";      // Refresh every 10 sec
  html_code += "<TITLE>ESP32 Station</TITLE>";
  html_code += "<STYLE>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: left;}</STYLE></HEAD>";
  html_code += "<BODY><CENTER><h1>ESP32 Station</h1>";
  html_code += "Connected to : " + String(ssid) + "<BR>";
  String IP_address = WiFi.localIP().toString();
  html_code += "IP Address : " + IP_address + "<BR>";
  html_code += "Url Address : " + local_name + "<BR>";
  html_code += "<TABLE BORDER='2' CELLPADDING='5'><FONT SIZE='+1'>";
  html_code += "<TR><TD><FONT COLOR='Cyan'>Temperature:</FONT></TD><TD>" + color + String(temperature * 1.8 + 32) + " &deg;F</TD></TR>";
  html_code += "<TR><TD><FONT COLOR='Blue'>Temperature:</FONT></TD><TD>" + color +  String(temperature) + " &deg;C</TD></TR>";
  html_code += "<TR><TD><FONT COLOR='Green'>Humidity:</FONT></TD><TD>" + String(humidity) + " %</TD></TR>";
  html_code += "<TR><TD><FONT COLOR='Magenta'>Pressure:</FONT></TD><TD>" + String(pressure) + " hPa</TD></TR>";
  html_code += "<TR><TD><FONT COLOR='Orange'>Pressure Altitude:</FONT></TD><TD>" + String(altitude * 3.281) + " Ft</TD></TR></TABLE>";
  html_code += "<BR><IMG SRC ='https://media.giphy.com/media/5upWi4wtfGc7u/giphy.gif'>";
  html_code += "</CENTER></BODY></HTML>";
  return html_code;
}

void OLED_display() {          // Display on SSD1306 OLED display
  temperature = bme.readTemperature();
  humidity = bme.readHumidity();
  pressure = bme.readPressure() / 100.0F;
  //altitude = bme.readAltitude(1013.25); // get altitude (this should be adjusted to your local forecast)
  altitude = bme.readAltitude(pressure + 3.13); // get altitude (this should be adjusted to your local forecast)
  // Display On OLED
  display.setCursor(0, 4);  // move cursor to position (0, 4) pixel
  display.println("T: " + String(temperature * 1.8 + 32) + " deg F");
  display.println("T: " + String(temperature) + " deg C");
  display.println("H: " + String(humidity) + " %");
  display.println("P: " + String(pressure) + " hPa");
  display.println("A: " + String(altitude) + " m");
  display.println("A: " + String(altitude * 3.281) + " Ft");
  display.display();       // Update the display
}

String Filename() {
  return String(__FILE__).substring(String(__FILE__).lastIndexOf("\\") + 1);
}
