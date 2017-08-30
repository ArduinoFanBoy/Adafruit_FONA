/* This code is an example FONA sketch for either 2G or 3G that sends some data to dweet.io
 *  every time you press the reset button or wake up the MCU from its sleep. It uses the IMEI
 *  number (which is globally unique) to generate a device ID for the cloud API (dweet.io)
 *  so that the user doesn't have to manually assign an ID, allowing the user to upload the
 *  same code for multiple devices. Additionally, for AVR-based MCU's this code puts the
 *  MCU to sleep at the end to conserve power.
 *  
 *  Github fork: https://github.com/botletics/Adafruit_FONA
 *  
 *  Written by: Timothy Woo (botletics.com)
 *  Last Updated: 8/20/2017
  */

#include "Adafruit_FONA.h"
#include <avr/sleep.h>
#include <avr/power.h>

// Default
#define FONA_RX 2
#define FONA_TX 3
#define FONA_RST 4
//#define PWR_KEY 5 // Optional

// For Feather FONA (SIM800) specifically
//#define FONA_RX  9
//#define FONA_TX  8
//#define FONA_RST 4
//#define FONA_RI  7

// Using SoftwareSerial:
#include <SoftwareSerial.h>
SoftwareSerial fonaSS = SoftwareSerial(FONA_TX, FONA_RX);
SoftwareSerial *fonaSerial = &fonaSS;

// Hardware serial is also possible!
//  HardwareSerial *fonaSerial = &Serial1;

// Use this for FONA 800 and 808s
Adafruit_FONA fona = Adafruit_FONA(FONA_RST);
// Use this one for FONA 3G. Run the "FONA3G_setBaud" example before running code for 3G or it may not work.
//Adafruit_FONA_3G fona = Adafruit_FONA_3G(FONA_RST);

uint8_t readline(char *buff, uint8_t maxbuff, uint16_t timeout = 0);
char imei[16] = {0}; // Use this for device ID
char replybuffer[255]; // Large buffer for replies
uint8_t type;
uint16_t VBAT = 0; // Battery voltage
uint16_t battLevel = 0; // Battery level (percentage)
const byte LED = 13;
const int phoneNum = 1234567890; // Phone number to call

void setup() {
  Serial.begin(115200);

  pinMode(LED, OUTPUT);

  // Flash LED twice and turn on the FONA
  digitalWrite(LED, HIGH);
  delay(50);
  digitalWrite(LED, LOW);
  delay(50);
  digitalWrite(LED, HIGH);
  delay(50);
  digitalWrite(LED, LOW);
  
  fonaSerial->begin(4800); // This function includes a reset, which turns on the FONA even if it was asleep
  if (! fona.begin(*fonaSerial)) {
    Serial.println(F("Couldn't find FONA"));
    while (1);
  }
  type = fona.type();
  Serial.println(F("FONA is OK"));
  Serial.print(F("Found "));
  switch (type) {
    case FONA800L:
      Serial.println(F("FONA 800L")); break;
    case FONA800H:
      Serial.println(F("FONA 800H")); break;
    case FONA808_V1:
      Serial.println(F("FONA 808 (v1)")); break;
    case FONA808_V2:
      Serial.println(F("FONA 808 (v2)")); break;
    case FONA3G_A:
      Serial.println(F("FONA 3G (American)")); break;
    case FONA3G_E:
      Serial.println(F("FONA 3G (European)")); break;
    default: 
      Serial.println(F("???")); break;
  }
  
  // Print module IMEI number.
  uint8_t imeiLen = fona.getIMEI(imei);
  if (imeiLen > 0) {
    Serial.print("Module IMEI: "); Serial.println(imei);
  }

  // Optionally configure a GPRS APN, username, and password.
  // You might need to do this to access your network's GPRS/data
  // network.  Contact your provider for the exact APN, username,
  // and password values.  Username and password are optional and
  // can be removed, but APN is required.
  //fona.setGPRSNetworkSettings(F("your APN"), F("your username"), F("your password"));
  //fona.setGPRSNetworkSettings(F("phone"); // This worked fine for a standard AT&T 3G SIM card (US)
  //fona.setGPRSNetworkSettings(F("m2m.com.attz")); // Might need to use this for AT&T IoT SIM card (data only, US)
  //fona.setGPRSNetworkSettings(F("wholesale")); // For Ting SIM card (sold on Adafruit site)
  //fona.setGPRSNetworkSettings(F("M2Mglobal")); // T-Mobile IoT SIM card

  // Optionally configure HTTP gets to follow redirects over SSL.
  // Default is not to follow SSL redirects, however if you uncomment
  // the following line then redirects over SSL will be followed.
  //fona.setHTTPSRedirect(true);
}

void loop() {
  // Connect to cell network and verify connection
  // If unsuccessful, keep retrying every 2s until a connection is made
  while (!netStatus()) {
    Serial.println(F("Failed to connect to cell network, retrying..."));
    delay(2000); // Retry every 2s
  }
  Serial.println(F("Connected to cell network!"));
  delay(1000); // At least 1s to help GPRS enable successfully

  // Measure battery level
//  VBAT = readVcc(); // Voltage
  battLevel = readVcc(); // Percentage

  // Measure something else
  float temperature = analogRead(A0); // Change this to suit your needs
  
  // Turn on GPRS
  while (!fona.enableGPRS(true)) {
    Serial.println(F("Failed to enable GPRS, retrying..."));
    delay(2000); // Retry every 2s
  }
  Serial.println(F("Enabled GPRS!"));

  // Post something like temperature and battery level to the web API
  // Construct URL and post the data to the web API
  char URL[150]; // Make sure this is long enough for your request URL
  char tempBuff[16];
  char battLevelBuff[16];

  // Format the floating point numbers as needed
  dtostrf(temperature, 1, 0, tempBuff); // float_val, min_width, digits_after_decimal, char_buffer
  dtostrf(battLevel, 1, 0, battLevelBuff);

  // Use IMEI as device ID in the example below
  sprintf(URL, "http://dweet.io/dweet/for/%s?temp=%s&batt=%s", imei, tempBuff, battLevelBuff);
  
  if (!fona.postData("GET", URL)) {
    Serial.println(F("Failed to post data, retrying..."));
  }

  // Disable GPRS
  if (!fona.enableGPRS(false)) Serial.println(F("Failed to disable GPRS"));

  // Power off FONA
  if (!fona.powerDown()) Serial.println(F("Failed to power down FONA!"));
  
  // Alternative to the AT command method above:
  // If your FONA has a PWR_key pin connected to your MCU, you can pulse the PWR_key
  // LOW for a little bit, then pull it back HIGH, like this:
//  digitalWrite(PWR_KEY, LOW);
//  delay(600); // Minimum of 64ms to turn on and 500ms to turn off for FONA3G. Check spec sheet for other types
//  digitalWrite(PWR_KEY, HIGH);
  
  // Shut down the MCU
  MCU_powerDown();
}

// Read the battery level percentage
float readVcc() {
//  if (!fona.getBattVoltage(&VBAT)) Serial.println(F("Failed to read Batt"));
//  else Serial.print(F("VBAT = ")); Serial.print(VBAT); Serial.println(F(" mV"));

  // Read battery percentage
  if (!fona.getBattPercent(&battLevel)) Serial.println(F("Failed to read batt"));
  else Serial.print(F("BAT % = ")); Serial.print(battLevel); Serial.println(F("%"));

  return battLevel;
}

bool netStatus() {
  int n = fona.getNetworkStatus();
  
  Serial.print(F("Network status ")); Serial.print(n); Serial.print(F(": "));
  if (n == 0) Serial.println(F("Not registered"));
  if (n == 1) Serial.println(F("Registered (home)"));
  if (n == 2) Serial.println(F("Not registered (searching)"));
  if (n == 3) Serial.println(F("Denied"));
  if (n == 4) Serial.println(F("Unknown"));
  if (n == 5) Serial.println(F("Registered roaming"));

  if (!(n == 1 || n == 5)) return false;
  else return true;
}

// Turn off the MCU completely. Can only wake up from RESET button
// However, this can be altered to wake up via a pin change interrupt
void MCU_powerDown() {
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  ADCSRA = 0; // Turn off ADC
  power_all_disable ();  // Power off ADC, Timer 0 and 1, serial interface
  sleep_enable();
  sleep_cpu();
}
