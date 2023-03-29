/* 
 *  Setup the board on Arduino IDE:
 *  Board TTGO LoRa32 OLED V1
 *  Upload Speed: 921.600
 *  TTGO SDA = pin 21 
 *  TTGO SCL = pin 22


+--------------------------------------+------------------------+-------------------------------------+
| Multiple funcions defined            |       Button 1	        |      Button 2                       | 
+--------------------------------------+------------------------+-------------------------------------+
| quick press on station mode	       |    increase frequency	| decrease frequency                  | 
+--------------------------------------+------------------------+-------------------------------------+
| quick press on volume mode	       |    increase volume	| decrease volume                     | 
+--------------------------------------+------------------------+-------------------------------------+
| Press and hold button for 5 seconds  |    Select volume mode	| Change between two preset frequenc  | 
+--------------------------------------+------------------------+-------------------------------------+
| Press and hold button for 10 seconds | Select NPT mode	| Change screen color                 | 
+--------------------------------------+------------------------+-------------------------------------+
| Press and hold button for 20 seconds | 	 NA             | go to deep sleep, save current vol. |  
|                                      |                        | and frequency in the flash memory   | 
+--------------------------------------+------------------------+-------------------------------------+
| quick press on deep sleep mode       |  wake up ESP32         | 	         NA                   | 
+--------------------------------------+------------------------+-------------------------------------+

*/

#include <driver/rtc_io.h>
#include <TFT_eSPI.h>     //https://github.com/Xinyuan-LilyGO/TTGO-T-Display
#include <SPI.h>
#include "iconradio.h"
#include <Wire.h>
#include <radio.h>       //https://github.com/mathertel/Radio
#include <RDA5807M.h>    //http://www.mathertel.de
#include <RDSParser.h> 
#include <Preferences.h> //https://randomnerdtutorials.com/esp32-save-data-permanently-preferences/
Preferences preferences;

////////////////////////////NTP DEFINITIONS //////////////////////////

#include <ArduinoJson.h>         //https://github.com/bblanchon/ArduinoJson.git
#include <NTPClient.h>           //https://github.com/taranais/NTPClient
#include <WiFi.h>
#include <WiFiUdp.h>
#include <HTTPClient.h>

const char* ssid     = "xxx";         //EDIT
const char* password = "xxx";        //EDIT

String town="Nyon";                           //EDIT
String Country="CH";                          //EDIT
const String endpoint = "http://api.openweathermap.org/data/2.5/weather?q="+town+","+Country+"&units=metric&APPID=";

const String key = "xxxxx"; //EDIT

String payload="";  //whole json 
String tmp="" ;     //temperatur
String hum="" ;     //humidity

int npt = 0;
  
StaticJsonDocument<1000> doc;

// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

// Variables to save date and time
String formattedDate;
String dayStamp;
String monthStamp;
String yearStamp;
String Date;
String Fhum;
String Ftemp;
String timeStamp;

////////////////////////////END NTP DEFINITIONS //////////////////////////


TFT_eSPI tft = TFT_eSPI();
RDA5807M radio;
RDSParser rds;

#define BUTTON1PIN 35
#define BUTTON2PIN 0
#define PINRDA 17                   ///to Power the RDA5807 module
#define PINAMP 12                   ///to Power the Amplifier module
#define background 0xB635

#define ADC_EN              14      //ADC_EN is the ADC detection enable port
#define ADC_PIN             34
    /*
    ADC_EN is the ADC detection enable port
    If the USB port is used for power supply, it is turned on by default.
    If it is powered by battery, it needs to be set to high level
    */

int vref = 1100;
float battery_voltage;
String voltage;
String voltage1; 

int count = 0;

#define BUTTON_GPIO GPIO_NUM_35  /// Define button to deep sleep mode

unsigned long tempoInicio1 = 0;
unsigned long tempoBotao1 = 0;

bool estadoBotao1;
bool estadoBotaoAnt1;

unsigned long tempoInicio2 = 0;
unsigned long tempoBotao2 = 0;

bool estadoBotao2;
bool estadoBotaoAnt2;

float frequency;
float memo1 = 93.0;   // pre set station
float memo2 = 100.1;  // pre set station
int volume = 1;
int color;

String station;
String stationant;
String txt;
String txtant;
 
int state = 0;       // Choose modes Frequency or Volume or NTP
bool state1 = true;  //Selec preset stations

void setup() {
  Serial.begin(115200);
  preferences.begin("my-app", false); // opens a storage space with a defined namespace to save Data Permanently
  pinMode(BUTTON1PIN, INPUT_PULLUP);
  pinMode(BUTTON2PIN, INPUT_PULLUP);
  pinMode(PINRDA, OUTPUT);
  pinMode(PINAMP, OUTPUT);
  
  digitalWrite (PINRDA, HIGH);
  digitalWrite (PINAMP, HIGH);
  pinMode(ADC_EN, OUTPUT);
  digitalWrite(ADC_EN, HIGH);

  tft.begin();
  tft.init();
  tft.setRotation(1); //Landscape 1 and 3
  tft.fillScreen(TFT_BLACK);
  tft.setSwapBytes(true);
  tft.pushImage(72,8,96,96, iconradio);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  delay(3000);

///read datas permanently saved in the flash memory
  frequency = preferences.getFloat("frequency", 93.0); 
  volume = preferences.getInt("volume", 0);
  color = preferences.getInt("color", 0);

  radio.init();
  radio.setBandFrequency(RADIO_BAND_FM, frequency*100);
  radio.setMono(false);
  radio.setMute(false);
  radio.setBassBoost(true);
  radio.setVolume(volume); 

  screen();
   
  radio.debugEnable();

  radio.attachReceiveRDS(RDS_process);
  rds.attachTextCallback(DisplayText);
  delay(500); 
  
}

void DisplayText(char *text)
{
  txt = text;
  char s[12];
  radio.formatFrequency(s, sizeof(s));
}

void RDS_process(uint16_t block1, uint16_t block2, uint16_t block3, uint16_t block4) 
 {rds.processData(block1, block2, block3, block4);}

void loop() {
  
 estadoBotao1 = !digitalRead(BUTTON1PIN);
 estadoBotao2 = !digitalRead(BUTTON2PIN);
 rtc_gpio_hold_en(BUTTON_GPIO);
 esp_sleep_enable_ext0_wakeup(BUTTON_GPIO, LOW); /// function to wake up the system when press button 1  
  if( state == 0){ FREQ ();} 
  if( state == 1){ VOL ();}
  if( state == 2){ getData();}
  if (txt != txtant) {
   if (color == 0) {
     tft.fillRect (0, 84, 240, 51, TFT_BLACK); //tft.fillRect (X0, Y0, Long, High, TFT_BLACK)
     tft.setTextColor(TFT_GREEN, TFT_BLACK);
     tft.setCursor(2,90);
     tft.setTextSize(2);
     tft.println(txt);
     txt = txtant;
    }
    else {
     tft.fillRect (0, 84, 240, 51, TFT_BLACK); //tft.fillRect (X0, Y0, Long, High, TFT_BLACK)
     tft.setTextColor(TFT_CYAN, TFT_BLACK);
     tft.setCursor(5,90);
     tft.setTextSize(2);
     tft.println(txt);
     txt = txtant;  
    }
  }
  radio.checkRDS();
  delay(20);
} ////////////////////////////end of loop


void screen () {      //display frequency on screen

  if (color == 0) {
 
   tft.setTextSize(1);
   tft.fillScreen(TFT_BLACK);
   tft.setTextColor(TFT_WHITE, TFT_BLACK);
   tft.drawString("FM Radio", 5, 0, 4);
   tft.drawLine(0, 25, 250, 25, TFT_BLUE);
   tft.setTextColor(TFT_YELLOW, TFT_BLACK);
   tft.drawString("Freq:", 5, 45, 2);
   tft.drawString(String(frequency), 55,30,7);    
  }
  else {
   tft.setTextSize(1);
   tft.fillScreen(background);
   tft.setTextColor(TFT_BLACK, background);
   tft.drawString("FM Radio", 5, 2, 4);
   tft.drawLine(0, 25, 250, 25, TFT_BLUE);
   tft.setTextColor(TFT_BLACK, background);
   tft.drawString("Freq:", 5, 45, 2);
   tft.drawString(String(frequency), 50,30,7); //limited in one decimal place
  }
}


void screenvolume () {  //display volume on screen

count = 0;

if (color == 0) {
  
  tft.fillScreen(TFT_BLACK);
  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("FM Radio", 5, 0, 4);
  tft.setTextColor(TFT_RED, TFT_BLACK);
  tft.drawString("F: ", 130, 0, 4);
  tft.drawString(String(frequency), 160,0,4);
  tft.drawLine(0, 25, 250, 25, TFT_BLUE);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.drawString("Vol:", 5, 45, 4);
  if (volume < 10) {tft.drawString(String(volume), 90,30,7);}
  if (volume > 9) {tft.drawString(String(volume), 55,30,7);}
  tft.fillRect (0, 82, 240, 155, TFT_BLACK); //tft.fillRect (X0, Y0, Long, High, TFT_BLACK) 

}
else {

  tft.fillScreen(background);
  tft.setTextSize(1);
  tft.setTextColor(TFT_BLACK, background);
  tft.drawString("FM Radio", 5, 2, 4);
  tft.setTextColor(TFT_RED, background);
  tft.drawString("F: ", 130, 0, 4);
  tft.drawString(String(frequency), 160,0,4);
  tft.drawLine(0, 25, 250, 25, TFT_BLUE);
  tft.setTextColor(TFT_BLACK, background);
  tft.drawString("Vol:", 5, 45, 4);
  if (volume < 10) {tft.drawString(String(volume), 90,30,7);}
  if (volume > 9) {tft.drawString(String(volume), 55,30,7);}
  }
}

void FREQ() {  // to check buttons in frequency mode

//// When button 1 is pressed

  if (estadoBotao1 && !estadoBotaoAnt1) {
    if (tempoInicio1 == 0) {
      tempoInicio1 = millis();
    }
  }  

 //// When button 1 is released 
  if (tempoInicio1 > 100){                                // Debounce filter
    if (!estadoBotao1 && estadoBotaoAnt1) {
      tempoBotao1 = millis() - tempoInicio1;
      tempoInicio1 = 0;  
    }
  }

///// First function of button 1: FREQ UP

if ((tempoBotao1 > 100) && (tempoBotao1 <= 800)) {
     tempoBotao1 = 0;
      if (frequency < 108) {frequency = frequency + 0.10;}
      else {frequency = 88;}
      radio.setBandFrequency(RADIO_BAND_FM, frequency*100);
      screen ();
 }

///// Second function of button 1: Change mode Freq/Vol
if ((tempoBotao1 > 800) && (tempoBotao1 <= 1800)) {
     tempoBotao1 = 0;
     state = 1;
     screenvolume ();
        
}

///// third function of button 1: wifi scan
if (tempoBotao1 > 1800) {
     tempoBotao1 = 0;
     state = 2;
     if (npt==0){
     nptsetup();}
     digitalWrite (PINRDA, LOW);
     getData();
}


//// When button 2 is pressed
  if (estadoBotao2 && !estadoBotaoAnt2) {
    if (tempoInicio2 == 0) {
      tempoInicio2 = millis();
    }
  }  

//// When button 2 is released 
  if (tempoInicio2 > 100) {                    // Debounce filter
    if (!estadoBotao2 && estadoBotaoAnt2) {
      tempoBotao2 = millis() - tempoInicio2;
      tempoInicio2 = 0;       
    }  
  }

//// First function of button 2: FREQ DOWN

  if ((tempoBotao2 > 100) && (tempoBotao2 <= 800)){
      tempoBotao2 = 0;
      if (frequency > 88) {frequency = frequency - 0.10;}
      else {frequency = 108;}
      radio.setBandFrequency(RADIO_BAND_FM, frequency*100);
      screen ();
   }

//// Second function of button 2: Alternate between 2 preset stations

 if ((tempoBotao2 > 800) && (tempoBotao2 <= 1800)){
     tempoBotao2 = 0;
     state1 = !state1;
     if (state1) {frequency = memo1; screen ();}
     else {frequency = memo2; screen ();}
     radio.setBandFrequency(RADIO_BAND_FM, frequency*100);    
}

//// Third function of button 2: Change color template

if ((tempoBotao2 > 1800) && (tempoBotao2 <= 2800)){
  tempoBotao2 = 0;
  
  if (color == 0){
    color = 1; 
  }
  else { color = 0;
  }
  screen ();
  preferences.putInt("color", color); // save data permanently
}

//// fourth function of button 2: Enter deep sleep mode
if (tempoBotao2 > 2800) {
  tempoBotao2 = 0;
  tft.fillScreen(TFT_NAVY);
  tft.setTextColor(TFT_WHITE, TFT_NAVY);
  tft.drawString("    Ate breve!", 0, 50, 4);
  delay(3000);
  tft.fillScreen(TFT_BLACK);
  
  preferences.putFloat("frequency", frequency); //save the frequency Permanently
  preferences.putInt("volume", volume);         //save the volume Permanently
  preferences.end();
  esp_deep_sleep_start(); //////////////////////////////////////////////////Start deep sleep mode
}

estadoBotaoAnt1 = estadoBotao1;
estadoBotaoAnt2 = estadoBotao2;
showVoltage();

} /// Finish Freq function 



void VOL() {

//////////////////////////////////////////////////////////////BUTTON 1

//// When button 1 is pressed
  if (estadoBotao1 && !estadoBotaoAnt1) {
    if (tempoInicio1 == 0) {
      tempoInicio1 = millis();
    }
  }  

//// When button 1 is releasd 
  if (tempoInicio1 > 100) {                      //Filtro Debounce
    if (!estadoBotao1 && estadoBotaoAnt1) {
      tempoBotao1 = millis() - tempoInicio1;
      tempoInicio1 = 0;
 
    }
  }

//// First function of button 1
if ((tempoBotao1 > 100) && (tempoBotao1 <= 1000)) {
     tempoBotao1 = 0;
      if (volume < 15) {volume = volume + 1;}
      radio.setVolume(volume);
      screenvolume ();
   }


//////////////////////////////////////////////////////////////BUTTON 2
//// When button 2 is pressed
  if (estadoBotao2 && !estadoBotaoAnt2) {
    if (tempoInicio2 == 0) {
      tempoInicio2 = millis();
    }
  }  

//// When button 2 is released
  if (tempoInicio2 > 100) {                       //Filtro Debounce
    if (!estadoBotao2 && estadoBotaoAnt2) {
      tempoBotao2 = millis() - tempoInicio2;
      tempoInicio2 = 0;
    }

//// First function of button 2

if ((tempoBotao2 > 100) && (tempoBotao2 <= 1000)) {
     tempoBotao2 = 0;
      if (volume > 0 ) {
        volume = volume - 1;
        radio.setVolume(volume);
        screenvolume ();}
 }
    }
  estadoBotaoAnt1 = estadoBotao1;
  estadoBotaoAnt2 = estadoBotao2;
    count++;
    //Serial.println (count);
if ( count == 250){
    state = 0;
    screen ();
     }
   }



void showVoltage()
{
  static uint64_t timeStamp = 0;
  if (millis() - timeStamp > 2000) {
    timeStamp = millis();
    uint16_t v = analogRead(ADC_PIN);
    battery_voltage = ((float)v / 4095.0) * 2.0 * 3.3 * (vref / 1000.0);
    voltage1 = String(battery_voltage);
    voltage = "V= " + voltage1.substring(0,3);

    if (color == 0){
     tft.setTextColor(TFT_WHITE, TFT_BLACK);
     tft.setTextSize(1);
     tft.drawString(voltage, 150, 0, 4);
     tft.drawLine(0, 25, 250, 25, TFT_BLUE);
     }
    else{
     tft.setTextColor(TFT_BLACK, background);
     tft.setTextSize(1);
     tft.drawString(voltage, 150, 0, 4);
     tft.drawLine(0, 25, 250, 25, TFT_BLUE);
      }     
    if (battery_voltage < 3.70) {
      tft.fillScreen(TFT_WHITE);
      tft.setTextColor(TFT_RED, TFT_WHITE);
      tft.drawString("    LOW BATTERY!", 0, 50, 4);
      delay(8000);
      tft.fillScreen(TFT_BLACK);
      preferences.putFloat("frequency", frequency);
      preferences.putInt("volume", volume);
      preferences.end();
      esp_deep_sleep_start();
      }
   }
}


void getData()
{

   if ((WiFi.status() == WL_CONNECTED)) { //Check the current connection status
 
    
    HTTPClient http;
 
    http.begin(endpoint + key); //Specify the URL
    int httpCode = http.GET();  //Make the request
 
    if (httpCode > 0) { //Check for the returning code
 
    payload = http.getString();
     }
 
    else {
      Serial.println("Error on HTTP request");

    }
 
    http.end(); //Free the resources
  }
 char inp[1000];
 payload.toCharArray(inp,1000);
 deserializeJson(doc,inp);
  
  String tmp2 = doc["main"]["temp"];
  String hum2 = doc["main"]["humidity"];
  String town2 = doc["name"];
  tmp=tmp2;
  hum=hum2;

  Ftemp = (tmp.substring(0,4));
  Fhum = (hum);

  
  tft.setTextColor(TFT_BLUE, TFT_WHITE);  
  tft.drawString(String(Ftemp)+"C", 30,110,4);
  tft.drawString(String(Fhum)+"%", 120,110,4);  
           
    while(!timeClient.update()) {
    timeClient.forceUpdate();
    }
  formattedDate = timeClient.getFormattedDate();
 
  dayStamp = formattedDate.substring(8, 10);
  monthStamp = formattedDate.substring(5, 7);
  yearStamp = formattedDate.substring(0, 4);

  Date = (dayStamp+"/"+monthStamp+"/"+yearStamp);

  tft.setTextColor(TFT_BLACK, TFT_WHITE);
  tft.drawString(String(Date), 50,85,4);
 
  timeStamp = formattedDate.substring(11, formattedDate.length()-1);

  tft.setTextColor(TFT_BLACK, TFT_WHITE);
  tft.drawString(String(timeStamp), 10,32,7);



estadoBotaoAnt1 = estadoBotao1;

delay(1000);
}

 
void nptsetup() {

  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_GREEN,TFT_BLACK);  
  tft.setTextSize(2);
  tft.setCursor(5,5);
  tft.println("Connecting to ");
  tft.println(ssid);

  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
  delay(300);
  Serial.print(".");
  tft.print(".");
  }

  tft.println("");
  tft.println("WiFi connected.");
  tft.println("IP address: ");
  tft.println(WiFi.localIP());
  tft.setTextSize(1);
  tft.fillScreen(TFT_WHITE);
  tft.setTextColor(TFT_BLACK, TFT_WHITE);
  tft.drawString("Nyon", 40, 5, 4);
  tft.drawLine(0, 27, 240, 27, TFT_BLUE);
    
  // Initialize a NTPClient to get time
  timeClient.begin(); 

  timeClient.setTimeOffset(7200);   /*Edit - Rio: -10800   Switzerland summer 7200   Switzerland winter 3600 */
  npt = 1;
  delay(500); 
  
}