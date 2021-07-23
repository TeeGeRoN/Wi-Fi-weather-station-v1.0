/*
 * WiFi-weather_station_v1.0
 * 
 * Wi-Fi - weather station on esp8266 version 1.0. 
 * Time synchronization via NTP server, Web page, 
 * graphing using Google Chart Tools API, 
 * recording values in the database.
 * 
 * In the current version of the project, 
 * data cannot be transmitted asynchronously. 
 * To display new data, you need to reload the page.
 * 
 * by TeeGeRoN
 */
#include <time.h>
#include <TimeLib.h>

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

/*---------- SD Card ----------*/
#include <SPI.h>
#include <SD.h>
#define chipSelect D8       // CS to D8
File myFile;

/*---------- Wifi connection ----------*/
const char ssid[] = "network_name";       // your network SSID (name)
const char pass[] = "password";       // your network password

/*---------- NTP Server ----------*/
#include <WiFiUdp.h>
static const char ntpServerName[] = "pool.ntp.org";
const int timeZone = 3;       // time zone 
WiFiUDP Udp;
unsigned int localPort = 8888;        // local port to listen for UDP packets
time_t getNtpTime();
String DigitsSD(int digits);
void sendNTPpacket(IPAddress &address);

/*----------DS20B18----------*/
#include <OneWire.h>
#include <DallasTemperature.h>
#define ONE_WIRE_BUS D4       // DS20B18 signal output  to D4
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

/*----------BMP280----------*/
#include <Adafruit_BMP280.h>
#include <Adafruit_Sensor.h>
Adafruit_BMP280 bmp280;

IPAddress ip(192,168,1,25);       // IP address to view graphs
IPAddress gateway(192,168,1,1);
IPAddress subnet(255,255,255,0);
ESP8266WebServer server(80);

const uint16_t len = 144;       // how many values ​​you need to store in volatile            
uint16_t tick=0;                // memory to display on the graph
time_t tnow[len];
float h[len], t[len];

void handleRoot() {
  String trendstr;
  
  trendstr = F("<html>\
  <head>\
    <script type='text/javascript' src='https://www.gstatic.com/charts/loader.js'></script>\
    <meta http-equiv='refresh' content='1000'/>\
    <title>TeeGeRoN`s Project</title>\
    <script type='text/javascript'>\
      google.charts.load('current', {'packages':['corechart']});\
      google.charts.setOnLoadCallback(drawChartTemp);\
\
      function drawChartTemp() {\
        var data = new google.visualization.DataTable();\
      data.addColumn('datetime', 'Time');\
      data.addColumn('number', 'Temp, *C');\
\
      data.addRows([");
  uint16_t k, y=0;
  for (int i=1; i <= len; i++){
    k = tick-1 + i;
    if (t[k]>0){
      if (y>0) trendstr += ",";
      y ++;
      if (k>len-1) k = k - len;
      trendstr += "[new Date(";
      trendstr += String(tnow[k]-(timeZone*3600));        // time zone
      trendstr += "*1000), ";
      trendstr += t[k];
      trendstr += "]";
    }
  }
    trendstr += F("]);\
\
        var options = {width: '100%',\
          title: 'Temperature',\
          curveType: 'function',\
          legend: { position: 'bottom' },\
          hAxis: {format: 'dd.MM.yyyy HH:mm',\
          gridlines: {\
            count: 10,\
          },\
        }\
        };\
\
        var chart = new google.visualization.LineChart(document.getElementById('curve_chart_temp'));\
        var formatter = new google.visualization.DateFormat({pattern: 'dd.MM.yyyy HH:mm'});\
        formatter.format(data, 0);\
        chart.draw(data, options);\
      }\
\
      google.charts.load('current', {'packages':['corechart']});\
      google.charts.setOnLoadCallback(drawChartPress);\
\
      function drawChartPress() {\
        var data = new google.visualization.DataTable();\
      data.addColumn('datetime', 'Time');\
      data.addColumn('number', 'Pressure, mm Hg');\
\
      data.addRows([");
  uint16_t a, x=0;
  for (int j=1; j <= len; j++){
    a = tick-1 + j;
    if (t[a]>0){
      if (x>0) trendstr += ",";
      x ++;
      if (a>len-1) a = a - len;
      trendstr += "[new Date(";
      trendstr += String(tnow[a]-(timeZone*3600));        // time zone 
      trendstr += "*1000), ";
      trendstr += h[a];
      trendstr += "]";
    }
  }
    trendstr += F("]);\
\
        var options = {width: '100%',\
          title: 'Pressure',\
          curveType: 'function',\
          legend: { position: 'bottom' },\
          hAxis: {format: 'dd.MM.yyyy HH:mm',\
          gridlines: {\
            count: 10,\
          },\
        }\
        };\
\
        var chart = new google.visualization.LineChart(document.getElementById('curve_chart_press'));\
        var formatter = new google.visualization.DateFormat({pattern: 'dd.MM.yyyy HH:mm'});\
        formatter.format(data, 0);\
        chart.draw(data, options);\
      }\
    </script>\
  </head>\
  <body>\
    <div id='curve_chart_temp' style='width: 60%; height: 500px'></div><hr />\
    <div id='curve_chart_press' style='width: 60%; height: 500px'></div>\
  </body>\
</html>");
  server.send ( 200, F("text/html"), trendstr );
}
/*---------- Sensor polling timer settings  ----------*/
const long intervalSens = 10*60*1000;       // 1 time in 10 minutes
unsigned long previousMillisSens = 0;

/*---------- Resynchronization timer settings ----------*/
const long intervalReSync = 60*1000;
unsigned long previousMillisReSync = 0;

void setup()
{
  Serial.begin(115200);
  Serial.println();
  Serial.println("TeeGeRoN's project.");
  Serial.println("Wi-Fi weather station version 1.0. NTP-sync. Graphs. Database.");
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, pass);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  server.on("/", handleRoot);
  server.begin();
  Serial.println("HTTP server started");
 
  Serial.println("Starting UDP");
  Udp.begin(localPort);
  Serial.print("Local port: ");
  Serial.println(Udp.localPort());
  Serial.println("---waiting for sync---");
  setSyncProvider(getNtpTime);        // setting the time synchronization function
  setSyncInterval(60*60*12);        // time interval for scheduled synchronization(sec) 
  WiFi.config(ip, gateway, subnet);
  sensors.begin();
  
  Serial.print("Initializing SD card...");
  if (!SD.begin(chipSelect)) {
    Serial.println("initialization failed!");
    return;
  }
  Serial.println("initialization done.");
 
  while (!bmp280.begin(BMP280_ADDRESS - 1))    // comment out this part of the code 
  {                                            // if you don't want to check if the bmp280 works
    Serial.println(F("Could not find a valid BMP280 sensor, check wiring!"));
    delay(2000);
  }
  kick();
}


void loop(){
  server.handleClient();
  /*---------- Timer for resynchronization function ----------*/
  unsigned long currentMillisReSync = millis();
  if (!timeStatus() && (currentMillisReSync - previousMillisReSync >= intervalReSync)){       
  //checks if the clock has been synchronized and if not
    previousMillisReSync = currentMillisReSync;
    setSyncProvider(getNtpTime);        // synchronizes
  }
  /*---------- Timer for sensor polling function ----------*/
  unsigned long currentMillisSens = millis();
  if (currentMillisSens - previousMillisSens >= intervalSens) {
    previousMillisSens = currentMillisSens;
    kick();
  }
}

/*---------- Sensor polling function ----------*/
void kick(){
  sensors.requestTemperatures();        // Polls the sensors
  h[tick] = (bmp280.readPressure() / 133.32) ;
  t[tick] = sensors.getTempCByIndex(0); 
  tnow[tick] = now();
  Serial.print(String(ctime(&tnow[tick])));
  Serial.print("Pressure: ");
  Serial.print(h[tick]);
  Serial.println(" mm Hg");
  Serial.print("Temperature: ");
  Serial.print(t[tick]);
  Serial.println("°C");
  writeDB();
  if (tick<len-1) tick ++; else tick = 0;
}

/*---------- Returns min/sec in `:01` format ----------*/
String DigitsSD(int digits)
{
  String result = "";
  if (digits < 10) {
    result += "0";
  }
   result += digits;
   return result;
}

String months[] = {
  "January", "February", 
  "March", "April", 
  "May", "June", 
  "July", "August", 
  "September", "October", 
  "November", "December"
  };
/*---------- Record to database ----------*/
void writeDB()
{
  myFile = SD.open(
    "/" + String(year()) + "/" + months[month() - 1] + "/" + String(day()) + ".txt", FILE_WRITE);
  Serial.print("Writing to DB...");
  myFile.print(DigitsSD(day()));
  myFile.print(".");
  myFile.print(DigitsSD(month()));
  myFile.print(".");
  myFile.print(year());
  myFile.print("\t");
  myFile.print(DigitsSD(hour()));
  myFile.print(":");
  myFile.print(DigitsSD(minute()));
  myFile.print(":");
  myFile.print(DigitsSD(second()));
  myFile.print("\t");
  myFile.print(h[tick]);
  myFile.print("\t");
  myFile.println(t[tick]);
  myFile.close();
  Serial.println("done.");
}


/*---------- NTP code ------------*/
const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets

time_t getNtpTime()
{
  IPAddress ntpServerIP; // NTP server's ip address

  while (Udp.parsePacket() > 0) ; // discard any previously received packets
  Serial.println("Transmit NTP Request");
  // get a random server from the pool
  WiFi.hostByName(ntpServerName, ntpServerIP);
  Serial.print(ntpServerName);
  Serial.print(": ");
  Serial.println(ntpServerIP);
  sendNTPpacket(ntpServerIP);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      Serial.println("Receive NTP Response");
      Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
    }
  }
  Serial.println("No NTP Response :-(");
  return 0; // return 0 if unable to get the time
}

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address)
{
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
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
