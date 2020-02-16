#include <Arduino.h>

#include <WiFi.h>
#include <WiFiUdp.h>
#include <WebServer.h>
#include <AutoConnect.h>
#include <AutoConnectCredential.h>

#include <ArtnetWifi.h>
#include <FastLED.h>
#include <LEDMatrix.h>
#include <LEDText.h>
#include <FontNum5x6.h>

#define CONFIG_WIFI_PIN   23
#define DATA_PIN          13
#define MATRIX_WIDTH      6
#define MATRIX_HEIGHT     6
#define NUM_LED           (MATRIX_WIDTH * MATRIX_HEIGHT)

WebServer Server;
AutoConnect Portal(Server);
AutoConnectConfig Config;

int numLeds;
int numberOfChannels;
CRGB * pLeds;
ArtnetWifi artnet;
int StartUniverse;
int previousDataLength;
bool bRxDmxFrame;
uint32_t tMilliStartDmx;
uint32_t tMilliDmx;

cLEDMatrix<MATRIX_WIDTH, MATRIX_HEIGHT, HORIZONTAL_ZIGZAG_MATRIX, 1, 1> ledMatrix;
cLEDText ScrollingMsg;
#define IP_STR_LEN      (24)
unsigned char ipValChar[IP_STR_LEN];

void rootPage() {
  char content[] = "Hello World.";
  Server.send(200, "text/plain", content);
}

void deleteAllCredentials() {
  station_config_t staConfig;
  AutoConnectCredential credential;
  int8_t ent = credential.entries();

  Serial.print("Deleting ");
  Serial.print(ent);
  Serial.println(" WiFi credentials.");

  while(ent--) {
    if(false != credential.load((int8_t)0, &staConfig)) {
      credential.del((const char *)&staConfig.ssid[0]);
    }
  }
}

void ConnectWiFi() {
  bool acEnabled = false;
  pinMode(CONFIG_WIFI_PIN, INPUT);

  if(digitalRead(CONFIG_WIFI_PIN) == HIGH) {
    Serial.println("WiFi Setup Mode.");
    /* Delete previous Wifi Setting */
    deleteAllCredentials();
    WiFi.disconnect();
    /* Configure */
    Config.autoSave = AC_SAVECREDENTIAL_AUTO;
    Config.autoReconnect = false;
    Config.autoRise = true;
    Config.portalTimeout = 0;
    Config.immediateStart = true;
    Portal.config(Config);
    /* Captive Mode */
    Serial.println("Running WiFi Captive Portal.");
    acEnabled = Portal.begin();
    if(acEnabled) {
      Portal.end();
    }
    Serial.println("AutoConnect Stopped");
    Serial.println("WiFi IP: " + WiFi.localIP().toString());
  } else {
    Serial.println("Using stored WiFi Setting.");
    station_config_t staConfig;
    AutoConnectCredential credential;
    if(true == credential.load((int8_t)0, &staConfig)) {
      /* Connect using stored credentials */
      WiFi.begin((char *)(staConfig.ssid), (char *)(staConfig.password));
      unsigned long startMillis = millis();
      unsigned long currentMillis = startMillis;
      wl_status_t wifiSts = WiFi.status();
      while (wifiSts != WL_CONNECTED) {
        delay(500);
        Serial.print(" .");
        Serial.print(wifiSts);
        currentMillis = millis();
        if((currentMillis - startMillis) > 10000) {
          Serial.println("WiFi timeout");
          ESP.restart();
        }
        wifiSts = WiFi.status();
      }
      Serial.println("");
      Serial.println("WiFi connected: " + WiFi.localIP().toString());
    } else {
      Serial.println("WiFi Credential not found!");
    }
  }
}

void onDmxFrame(uint16_t universe, uint16_t length, uint8_t sequence, uint8_t* data)
{
  // set brightness of the whole strip 
  if (universe == 15)
  {
    FastLED.setBrightness(data[0]);
  }
  // read universe and put into the right part of the display buffer
  for (int i = 0; i < length / 3; i++)
  {
    int led = i + (universe - StartUniverse) * (previousDataLength / 3);
    if (led < numLeds)
    {
      pLeds[led] = CRGB(data[i * 3], data[i * 3 + 1], data[i * 3 + 2]);
    }
  }
  previousDataLength = length;     
  FastLED.show();

  bRxDmxFrame = true;
  tMilliStartDmx = millis();
  tMilliDmx = tMilliStartDmx;
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  bRxDmxFrame = false;
  numLeds = NUM_LED;
  numberOfChannels = numLeds * 3;
  pLeds = new CRGB[numLeds];
  FastLED.addLeds<WS2812, DATA_PIN, GRB>(pLeds, numLeds);
  memset(pLeds, 0, sizeof(CRGB)*numLeds);
  StartUniverse = 0;
  previousDataLength = 0;

  /* LED0: Green (Board has Power) */
  pLeds[0] = CRGB(0, 255, 0);
  /* LED1: Red (Board not connected to Wifi) */
  pLeds[1] = CRGB(255, 0, 0);
  FastLED.show();

  /* Connect to WiFi (Resets ESP32 when connection fails) */
  ConnectWiFi();

  /* LED1: Green (Board connected to Wifi) */
  pLeds[1] = CRGB(0, 255, 0);
  FastLED.show();

  artnet.begin();
  artnet.setArtDmxCallback(onDmxFrame);

  ledMatrix.SetLEDArray(pLeds);
  String ipValStr = String(EFFECT_SCROLL_LEFT) + WiFi.localIP().toString() + String("---");
  ipValStr.getBytes(ipValChar, IP_STR_LEN-1, 0);
  FastLED.clear(true);
  FastLED.setBrightness(255);
  ScrollingMsg.SetFont(FontNum3x5);
  ScrollingMsg.Init(&ledMatrix, ledMatrix.Width(), ScrollingMsg.FontHeight() + 1, 0, 0);
  ScrollingMsg.SetText((unsigned char *)ipValChar, strlen((char *)ipValChar));
  ScrollingMsg.SetTextColrOptions(COLR_RGB | COLR_SINGLE, 0xff, 0x00, 0xff);

  tMilliStartDmx = millis();
  tMilliDmx = tMilliStartDmx;
}

void loop() {
  uint32_t dmxTimeout;
  tMilliDmx = millis();
  artnet.read();

  dmxTimeout = tMilliDmx - tMilliStartDmx;
  if((dmxTimeout > 10000) && (bRxDmxFrame == true)) {
    bRxDmxFrame = false; // timeout
    Serial.print("DMX frame timeout: ");
    Serial.println(dmxTimeout);
  }

  if(bRxDmxFrame == false) {
    if (ScrollingMsg.UpdateText() == -1) {
      ScrollingMsg.SetText((unsigned char *)ipValChar, strlen((char *)ipValChar));
    } else {
      FastLED.show();
    }
    delay(100);
  }
}
