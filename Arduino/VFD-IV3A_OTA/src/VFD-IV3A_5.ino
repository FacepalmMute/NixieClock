/*
ESP8266 : NTP Time + Timezone
Created 07 May 2016 by Ralf Bohnen - www.Arduinoclub.de
This example code is in the public domain.
*/

//(ESP) WiFi include stuff
#include <ESP8266WiFi.h>
#include <ESP8266WiFiAP.h>
#include <ESP8266WiFiGeneric.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266WiFiScan.h>
#include <ESP8266WiFiSTA.h>
#include <ESP8266WiFiType.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <WiFiServer.h>
#include <WiFiUdp.h>

// RTC  Stuff ###################################################################
#include <DS1307RTC.h>
#include <Time.h>
#include <Wire.h>
#include <TimeLib.h>  //by Paul Stoffregen, not included in the Arduino IDE !!!
#include <Timezone.h> //by Jack Christensen, not included in the Arduino IDE !!!

// Shift Register HV5812 and Ports #############################################
#include "HV5812.h"
#include "multiplexer.h"

// OTA Stuff ###################################################################
// WifiManager + OTA https://github.com/FacepalmMute/WiFiManager
#include <WiFiManager.h>

// Port Pins are defined in HV5812.h ##########################################

uint8_t fieldMux[6]; // field to display

int segmentIndex = 14;

unsigned long previousMillis = 0;
//const long interval = 1000;
volatile bool wasConnected = false;

bool isConnected(long timeOutSec)
{
  timeOutSec = timeOutSec * 1000;
  int z = 0;
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(200);
    Serial.print(".");
    if (z == timeOutSec / 200)
    {
      return false;
    }
    z++;
  }
  return true;
}

//### NTP, TIME & TIMEZONE ###################################################################

//WiFiManager
WiFiManager wifimanager;
//Login for OTA Update option, (already entered login is saved)
const char *ota_user = "admin";
const char *ota_pass = "admin";
char resChar = n; //char for uart communication

//UDP
WiFiUDP Udp;
unsigned int localPort = 123;

//NTP Server
char ntpServerName1[] = "ntp1.t-online.de";
char ntpServerName2[] = "ntp2.ptb.de";

//Timezone
//Central European Time (Frankfurt, Paris)
TimeChangeRule CEST = {"CEST", Last, Sun, Mar, 2, 120}; //Central European Summer Time
TimeChangeRule CET = {"CET ", Last, Sun, Oct, 3, 60};   //Central European Standard Time
Timezone CE(CEST, CET);
TimeChangeRule *tcr; //pointer to the time change rule, use to get the TZ abbrev
time_t utc, local;

const int NTP_PACKET_SIZE = 48;     // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets

bool getNtpTime(char *ntpServerName)
{
  //	Serial.print(F("NTP request..."));
  if (timeStatus() == timeSet)
  {
    //		Serial.println(F("not necessary"));
    return true;
  }

  IPAddress ntpServerIP; // NTP server's ip address

  while (Udp.parsePacket() > 0)
    ; // discard any previously received packets
  Serial.println(F("Transmit NTP Request"));
  // get a random server from the pool
  WiFi.hostByName(ntpServerName, ntpServerIP);
  Serial.print(ntpServerName);
  Serial.print(": ");
  Serial.println(ntpServerIP);
  sendNTPpacket(ntpServerIP);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500)
  {
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE)
    {
      Serial.println(F("Receive NTP Response"));
      if (!Udp.read(packetBuffer, NTP_PACKET_SIZE))
        Serial.printf("ERROR in Udp.read"); // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 = (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      setTime(secsSince1900 - 2208988800UL);
      //setTime(23, 55, 0, 30, 3, 2016); //simulate time for test
      return true;
    }
  }
  Serial.println(F("FATAL ERROR : No NTP Response."));
  return false; // return 0 if unable to get the time
}

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address)
{
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011; // LI, Version, Mode
  packetBuffer[1] = 0;          // Stratum, or type of clock
  packetBuffer[2] = 6;          // Polling Interval
  packetBuffer[3] = 0xEC;       // Peer Clock Precision
                                // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}

static void print_wifi_mac_address()
{
  uint8_t mac[6];
  WiFi.macAddress(mac);
  Serial.print("MAC: ");
  Serial.print(mac[0], HEX);
  Serial.print(":");
  Serial.print(mac[1], HEX);
  Serial.print(":");
  Serial.print(mac[2], HEX);
  Serial.print(":");
  Serial.print(mac[3], HEX);
  Serial.print(":");
  Serial.print(mac[4], HEX);
  Serial.print(":");
  Serial.println(mac[5], HEX);
}
//### TIME Printing only  on demand ###################################################################

void printTime(time_t t)
{
  sPrintI00(hour(t));
  sPrintDigits(minute(t));
  sPrintDigits(second(t));
  Serial.print(' ');
  Serial.print(dayShortStr(weekday(t)));
  Serial.print(' ');
  sPrintI00(day(t));
  Serial.print(' ');
  Serial.print(monthShortStr(month(t)));
  Serial.print(' ');
  Serial.print(year(t));
  Serial.println(' ');
}

//Print an integer in "00" format (with leading zero).
//Input value assumed to be between 0 and 99.
void sPrintI00(int val)
{
  if (val < 10)
    Serial.print('0');
  Serial.print(val, DEC);
  return;
}

//Print an integer in ":00" format (with leading zero).
//Input value assumed to be between 0 and 99.
void sPrintDigits(int val)
{
  Serial.print(':');
  if (val < 10)
    Serial.print('0');
  Serial.print(val, DEC);
}

//############################################################################################
static void print_wifi_status()
{
  Serial.print("WiFi status: ");
  switch (WiFi.status())
  {
  case WL_CONNECTED:
    Serial.println("WL_CONNECTED");
    break;
  case WL_NO_SHIELD:
    Serial.println("WL_NO_SHIELD");
    break;
  case WL_IDLE_STATUS:
    Serial.println("WL_IDLE_STATUS");
    break;
  case WL_NO_SSID_AVAIL:
    Serial.println("WL_NO_SSID_AVAIL");
    break;
  case WL_SCAN_COMPLETED:
    Serial.println("WL_SCAN_COMPLETED");
    break;
  case WL_CONNECT_FAILED:
    Serial.println("WL_CONNECT_FAILED");
    break;
  case WL_CONNECTION_LOST:
    Serial.println("WL_CONNECTION_LOST");
    break;
  case WL_DISCONNECTED:
    Serial.println("WL_DISCONNECTED");
    break;
  default:
    Serial.println("Unknown");
    break;
  }
}

void vfdTestDisplay(void)
{
  long testTime = 0;
  uint8_t digitsCnt = 0;
  while (digitsCnt < 6)
  {
    while ((millis() - testTime < 3000) && (true))
    {
      for (uint8_t i = 0; i < 6; i++)
      {
        fieldMux[i] = i + digitsCnt + 16;
      }
      multiplexer(fieldMux);
    }
    digitsCnt++;
  }
}

void setup()
{
  Serial.begin(115200);

  pinMode(BLK, OUTPUT);   // Blanking Command input
  pinMode(LE, OUTPUT);    // Latch Enable Command input
  pinMode(CLK, OUTPUT);   // Shift Register clk input
  pinMode(SDATA, OUTPUT); // Shift Register data input

  delay(50);
  Serial.println(F("\n\n42nibbles-NTP-Timezone"));

  //Clock setting from RTC
  Serial.print(F("DS1307 real time clock on I2C"));
  time_t rtc_time = RTC.get();
  if (rtc_time != 0)
  {
    setTime(rtc_time);
    Serial.println("\t[OK]");
    Serial.print("UTC date is: ");
    Serial.print(day());
    Serial.print(".");
    Serial.print(month());
    Serial.print(".");
    Serial.println(year());
    Serial.print("UTC time is: ");
    Serial.print(hour());
    Serial.print(":");
    Serial.print(minute());
    Serial.print(":");
    Serial.println(second());
  }
  else
  {
    Serial.println("\t[FAILED]");
  }

  /* Commented out because the mode and the begin starts already in WifiManager
  WiFi.mode(WIFI_AP_STA); //WIFI_STA -> WIFI_AP_STA, Nik changed for OTA
  WiFi.begin(WiFiSSID, WiFiPSK);
  */

  //Setup WiFimanager+OTA
  wifimanager.setOtaUser(ota_user, ota_pass);
  wifimanager.autoConnect("NixieClock");

  if (isConnected(50))
  {
    wasConnected = true;
    Serial.println(F("Starting UDP"));
    Udp.begin(localPort);
    Serial.print(F("Local port: "));
    Serial.println(Udp.localPort());
    Serial.println(F("waiting for sync"));
  }

  digitalWrite(BLK, HIGH); // Blanking low active
  digitalWrite(LE, HIGH);  // Latch Enable idle
  delay(500);
  Serial.println(F("Blanking Inactive!"));

  vfdTestDisplay();

  //Clock setting from RTC
  Serial.print(F("DS1307 real time clock on I2C"));

  if (rtc_time != 0)
  {
    setTime(rtc_time);
    Serial.println("\t[OK]");
    Serial.print("UTC date is: ");
    Serial.print(day());
    Serial.print(".");
    Serial.print(month());
    Serial.print(".");
    Serial.println(year());
    Serial.print("UTC time is: ");
    Serial.print(hour());
    Serial.print(":");
    Serial.print(minute());
    Serial.print(":");
    Serial.println(second());
  }
  else
  {
    Serial.println("\t[FAILED]");
  }

  //OTA init
  //init_OTA(Serial, host);
}

void loop()
{
  if (!isConnected(10) && wasConnected)
  {
    delay(200);
    ESP.restart();
  }
  if (!getNtpTime(ntpServerName1))
  {
    getNtpTime(ntpServerName2);
  }
  local = CE.toLocal(now(), &tcr);

  uart_debug();

  if (second(local) > 55)
  {
    fieldMux[0] = year(local) % 10;
    fieldMux[1] = (year(local) - 2000) / 10; // indexing tenth of year
    fieldMux[2] = month(local) % 10;         // switch on decimal point
    fieldMux[3] = month(local) / 10;
    fieldMux[4] = day(local) % 10; // switch on decimal point
    fieldMux[5] = day(local) / 10;
    multiplexer(fieldMux);
  }
  else
  {
    fieldMux[0] = second(local) % 10;
    fieldMux[1] = second(local) / 10;
    fieldMux[2] = minute(local) % 10; // switch on decimal point
    fieldMux[3] = minute(local) / 10;
    fieldMux[4] = hour(local) % 10; // switch on decimal point
    fieldMux[5] = hour(local) / 10;
    multiplexer(fieldMux);
  }
}


void uart_debug(){
//Easy UART Debug interface
  if (resChar = Serial.read())
  {
    switch (resChar)
    {
    //Reset saved station logins
    case 'r':
      resChar = 'n';
      wifimanager.resetSettings();
      wifimanager.autoConnect("Pixie Uhr");
      break;
    //Restart ESP
    case 'p':
      resChar = 'n';
      ESP.restart();
      break;
    }
  }
}
