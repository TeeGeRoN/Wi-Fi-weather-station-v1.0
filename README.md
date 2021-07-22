# Wi-Fi-weather-station
## Weather station v1.0 on the module esp8266. NTP-sync. Graphs with Google Charts. Database.
Brief description
-----------------
This is the first version of the weather station based on **esp8266** with *[Google Charts][1]* support and time synchronization using *[NTP][0]*. Atmospheric pressure and temperature are measured and recorded to a memory card periodically. Graphs are built on the local web server according to the latest data. The project is in the active development stage.

Explanations and recommendations from the author
------------------------------------------------
An atmospheric pressure sensor can measure the temperature itself, but for better accuracy (compared to the DS18B20), I preferred to use a separate module. Also, for a full-fledged weather station, I recommend to use  temperature and humidity sensors, such as [DHT11][2] / [DHT22][3]. Unfortunately,  I don’t have them at the moment so I end up using the DS18B20.

Also you can pick a smarter solution -  buy an atmospheric pressure sensor [bme280][4], which allows you to measure temperature and humidity and does it more accurately than its predecessor.

Components used in this project
-------------------------------
-	**power supply** 5v(0.5-1 A will suffice)
-	Wi-Fi module **[NodeMCU][5]** v3 (esp8266)
-	atmospheric pressure sensor **[BMP280][6]**
-	digital temperature sensor **[DS18B20][7]**
-	**1 resistor** with a nominal value of **4-10 kО** (in my case it is 4.7)
-	**[MicroSD card Adapter][8]**
-	MicroSD **memory card** up to 2 GB
-	bread board and wires (optionally)

Connection
----------
You can see the connection diagram below.
![scheme_WWSTv1 0](https://user-images.githubusercontent.com/77700004/126721919-7db97246-f44e-4fd6-b1de-d429b55e4c25.png)

The temperature sensor DS18B20 needs 5V power supply. The resistor must be placed between the signal output and the power supply of the sensor. The signal output is connected to D4(GPIO2).</br>
For the SD card adapter to work properly you need 5V. Also you can connect a memory card directly without using an adapter. The card requires 3.3V and this is exactly the voltage at which the ESP8266 works. However, this approach is inconvenient, so I recommend not to choose this option. 6 contacts are used for connection of SD Adapter, interaction is made using the SPI interface(see NodeMCU [pinout][5]). MISO -> D6(GPIO12), MOSI -> D7(GPIO13), SCK -> D5(GPIO14) and CS - to any free output. I connected to D8 or GPIO15.</br>
The BMP280 module can be connected to the board with two I2C or SPI interfaces. I used the first one. SCL -> D1(GPIO5), SDA -> D2(GPIO4). This module requires 3.3V! 

Don't forget to connect the ground of all sensors and the power supply to the **GND**! Even if you powering the sensors from an external power supply, and you power the board from a USB, you still need to connect the ground of the power supply to the ground of the board!

Pay attention! Your memory card must be formatted in **FAT32** format!

Firmware
--------
I left a lot of comments in the code, so I think it will be easy to understand if you want to use some part of it in your own project. 
Libraries that I used in the firmware:
-	TimeLib.h
-	ESP8266WiFi.h
-	ESP8266WebServer.h
-	SPI.h
-	SD.h
-	WiFiUdp.h
-	OneWire.h
-	DallasTemperature.h
-	Adafruit_BMP280.h
-	Adafruit_Sensor.h
-	time.h(Only required for the `ctime` function, which is used to display the normalized date and time format in the Serial port. Once you have completed debugging through the Serial port, it is not required)

What you may want to pay attention to:
```c++
const char ssid[] = "network_name";       // your network SSID (name)
const char pass[] = "password";    // your network password
```
Enter the authentication data for your Wi-Fi network. If you do not have a router, but have mobile internet on your phone, you can run an Access Point on it. All devices that will be connected to this network will be able to reach out to the local server, which will be started from NodeMCU, which will display indicators from sensors and graphics.
```c++
static const char ntpServerName[] = "pool.ntp.org";
const int timeZone = 3;               // time zone
```
NTP server address and time zone for your area.
```c++
IPAddress ip(192,168,1,25);  // IP address to view graphs
IPAddress gateway(192,168,1,1);
IPAddress subnet(255,255,255,0);
```
You can change the last number in you IP address(where your graphics page will be displayed) to start a server, for example at IP address 192.168.1.104. Also enter your gateway and subnet(I recommend to look at your router settings first).
```c++
const uint16_t len = 144;  
```
How many values you need to store in *volatile memory* to display on the graph. 

This graph will show exactly this number of points. For example, you are going to take measurements once a minute. Accordingly, your chart will show the last 2 hours. For me, 144 points was enough, taking into account  that I record data every 10 minutes, my graphs show the figures for the last 24 hours. But you can set the value that suits you best. When all 144 points are recorded, space for new data will be freed using the FIFO (first in, first out) principle.
```c++
void handleRoot () {...}
```
It describes everything that will be displayed on the site, including connecting and displaying graphs using [Google Charts][1]. [The Google Chart Tools API][9] is a feature-rich set of data visualization tools. It makes it relatively easy to build graphs and charts on the site.
```c++
/*---------- Sensor polling timer settings  ----------*/
const long intervalSens = 1*60*1000; // 1 time per minute
```
Here we set the value in milliseconds, how often you need to take measurements. To show how it works, I set the measurements to 1 time per minute, but when the system is running normally, the measurements take place every 10 minutes. If you want to do the same, you need to multiply this number by 10 instead of 1.
```c++
/*---------- Resynchronization timer settings ----------*/
const long intervalReSync = 60*1000;
```
After the board starts, you connect to the Internet and request a NTP-server to synchronize time. However, if you are unable to connect to the network immediately, or the NTP-server does not respond, an automatic attempt will be made to reconnect to the NTP-server and synchronize the time. Here you specify how long it takes to try to sync again (I determined once a minute).

***`void setup()`***
```c++
setSyncProvider(getNtpTime);         // setting the time synchronization function
setSyncInterval(24*60*60);              // time interval for scheduled synchronization(sec)
```
Here you specify the function that the system will use to synchronize the time and define the time interval between the scheduled resynchronization (in seconds!). That is, in my case, it will re-sync every 24 hours.
```c++
while (!bmp280.begin(BMP280_ADDRESS - 1))        
  {                                                       
    Serial.println(F("Could not find a valid BMP280 sensor, check wiring!"));
    delay(2000);
  }
```
Checks whether the atmospheric pressure sensor is working. But in case it doesn't work for one reason or another (you plugged it in incorrectly or it just failed), this code snippet won't allow your board to run until you fix the sensor. Delete this piece of code if you don't need it.

In ***`void loop()`*** you have only a command to boot the server and 2 timers – to call the sync retry and for the measurement function.

I would like to pay special attention to the `timeStatus()` function from the TimeLib library, which I used in one of the conditions of the resynchronization timer. 
This function indicates if time has been set and recently synchronized:
 - return 0, if the time has never been set, the clock started at Jan 1 1970;
 - return 1, if the time had been set but a sync attempt did not succeed;
 - return 2, if the time is set and is synced.
`kick()` - sensor polling function. It contains a call to the function of writing data to a file - "text database".

`writeDB()` - function which stores the information to the database. It opens (or creates) a folder with the desired year, which contains a folder with the desired month. Inside which are stored files with the name in the format "dayOfTheMonth.txt". As a result, we get a file with the data as in the screenshot below.

![DB_small_WWSTv1 0](https://user-images.githubusercontent.com/77700004/126720848-522fdb3d-6099-49ea-952f-551bce5b380f.png)

`getNtpTime()` - a function that is used to request and receive packets from the NTP server and process the received data.

Unloading in NodeNCU
--------------------

Once you have configured the firmware for yourself, you can unload on the board. I use the [Arduino IDE][10] to flash the module. In the program settings, select the board and its version, the port to which NodeMCU is connected, upload speed, flash size. You can find these settings in the *"tools"* section. Unload the firmware.

![ArduinoIDE_tools_WWSTv1 0](https://user-images.githubusercontent.com/77700004/126720890-71cb37d9-29de-4b0e-b4ac-3b3110359305.png)

Open the serial port monitor and set the *speed* to *115200*. If you did all of this correctly, you will see a message similar to this.

![Com-port_WWSTv1 0](https://user-images.githubusercontent.com/77700004/126720920-ade92889-71d9-48d0-90ee-eec97c525b47.png)

Well,  we can see that our board was synchronized on the NTP-server, set real time, launched a memory card without errors and recorded the first measurements of atmospheric pressure and temperature on it.

Let's just wait for a moment until the system takes at least 10-15 measurements to see them clearly on the graphs.

![GraphsWWSTv1 0](https://user-images.githubusercontent.com/77700004/126720944-55c21e1c-b5e6-4399-87eb-468c233dfd91.png)

When you hover the mouse cursor over any of the points (or clicking on the point if you are viewing these graphs from a smartphone), you get a popup with information about this point (for example, the temperature at a particular time).

![popup_WWSTv1 0](https://user-images.githubusercontent.com/77700004/126720970-2700ea5e-0e7a-4b8a-8734-ae36516016d3.png)

Now remove the memory card from the adapter and check that all data is recorded properly.

![path_WWSTv1 0](https://user-images.githubusercontent.com/77700004/126720994-2866e8a3-aafe-4b19-af99-e0cb5aaecbe5.png)

As you can see, the program correctly creates the path to the file and the file itself, which stores the data that we wrote there.

![DB_big_WWSTv1 0](https://user-images.githubusercontent.com/77700004/126721021-c9f23d37-b545-489f-a140-4e4cb4985d0c.png)

If you have similar results - my congratulations, everything works as intended. Now I will describe the problems I faced myself, maybe they will help you solve yours.

Mistakes
--------
1)	If your esp8266 is constantly rebooted, and in the serial port you receive similar messages:

![Error_wdtreset_WWSTv1 0](https://user-images.githubusercontent.com/77700004/126721047-ce74ed72-a81b-41dd-93b0-d8a001cbe24d.png)

In this case, make sure that:
- your memory card is formatted in the fat32 file system;
- you have correctly specified the GPIO pin to which the CS input of the adapter is connected;
- if the first 2 points are fulfilled, and the problem is still present, clear the flash memory on NodeMCU using the program nodemcu flasher and unload the firmware again.
2)  If you see the date as of 01.01.1970, most likely, you could not connect to the NTP server(you will see a message about this in the serial port:

![Error_ntp_WWSTv1 0](https://user-images.githubusercontent.com/77700004/126721074-ef1af92a-a620-4f62-a407-b530fcd1bf9f.png)

Check if you are connected to the Wi-Fi network, if the router has an Internet connection and if you have correctly specified the working NTP-server. The program will try to reconnect after the time period you specified in the resynchronization timer, if after 3-5 attempts it fails to do so, I recommend checking everything I described above in this paragraph.

3)	If you can't load the page specified for displaying graphs (for example 192.168.1.25) - check your IP, gateway, subnet settings.

Results
-------
With a small budget, time, desire and connection to the Internet, you can organize your own system for monitoring certain events and recording the results for further processing.

Using the Google Chart Tools API, you can quickly and easily customize charts with your own data from your sensors. Also, in addition to charts, the Google Chart Tools API functionality includes:
-	Dynamic icons;
-	Cards;
-	Dials and displays;
-	Formulas;
-	QR codes;
-	Ability to create your own visualization tools and use third-party ones.

This is the first version of the project, presented to you as a constructor that you can disassemble and use the necessary elements in your projects. It has a number of shortcomings and nuances, which I will fix in future updates. For example, to display a new data on a page, you will need to reload this page. But  you  can't transfer data asynchronously in current version of the project.

The main idea at this stage was to show the functionality and capabilities of the system. I did not waste time for creating a powerful user interface, but you can build visualization you want.

Equally important is the implementation of a program for data analysis, processing and review, and I am currently working on it.

***I hope it was interesting to you. Good luck!***

[0]: https://en.wikipedia.org/wiki/Network_Time_Protocol
[1]: https://developers.google.com/chart
[2]: https://www.mouser.com/datasheet/2/758/DHT11-Technical-Data-Sheet-Translated-Version-1143054.pdf
[3]: https://www.sparkfun.com/datasheets/Sensors/Temperature/DHT22.pdf
[4]: https://www.mouser.com/datasheet/2/783/BST-BME280-DS002-1509607.pdf
[5]: https://components101.com/development-boards/nodemcu-esp8266-pinout-features-and-datasheet
[6]: https://cdn-shop.adafruit.com/datasheets/BST-BMP280-DS001-11.pdf
[7]: https://datasheets.maximintegrated.com/en/ds/DS18B20.pdf
[8]: https://curtocircuito.com.br/datasheet/modulo/cartao_micro_SD.pdf
[9]: https://developers.google.com/chart/interactive/docs/quick_start
[10]: https://www.arduino.cc/en/software
