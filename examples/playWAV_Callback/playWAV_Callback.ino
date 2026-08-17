#include <WiFi.h>
#include <WiFiClient.h>
#include <pwmWav.h>
#include "data.h"

#define WIFI_SSID "YOUR-SSID"
#define WIFI_PASS "YOUR-PASS"

#define SPEAKER_PIN 26

//#define WAV_ONLINE_ADDR "http://192.168.2.2:3660/data?file=music.wav"
#define WAV_ONLINE_ADDR "http://sound.mycotech.ir/wav/music.wav"

WiFiClient client;
File aud;
pwmWav out;

int musicNumber = 0;
bool isRun = false;
bool isConnected = true;

void dataCallback(uint8_t *pwm_buffer, int len){
  out.run(pwm_buffer, len);
}

void setup() {
  delay(1000);
  Serial.begin(9600);
  Serial.println();

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  int toConnect = 40;
  while(WiFi.status() != WL_CONNECTED){
    Serial.print(".");
    delay(500);
    if(--toConnect==0) break;
  }
  if(toConnect==0){
    Serial.println("Connect to network faild!\r\n** Online play music not running! **\r\n");
    isConnected = false;
  }
  Serial.println();

  SPIFFS.begin();
  aud = SPIFFS.open("/music.wav");

  outconfig_t cfg;
  cfg.lSPKPin = SPEAKER_PIN;
  cfg.vol = -8;
  out.begin(cfg);
  out.setCallback(dataCallback);
  out.enEcho(true);

  Serial.println("Play online music by callback function");

  if(aud) out.play(aud);
  delay(2000);
  
  out.setData(music_wav, music_wav_len);
  out.play();
  delay(2000);
}

void loop(){
  if(!isRun && isConnected){
    String host, url = WAV_ONLINE_ADDR;
    int httpPort = 80;
    bool secu = out.urlSeperator(&url, &host, &httpPort);
    Serial.printf("HOST: %s - ADDR: %s - PORT: %d\r\n", host.c_str(), url.c_str(), httpPort);
    if(WiFi.status() == WL_CONNECTED){
      if(!client.connect(host.c_str(), httpPort)){
        Serial.println("Connect to host faild!");
        musicNumber++;
      }else{
        client.printf("GET /%s HTTP/1.1\r\nhost: %s\r\n\r\n", url.c_str(), host.c_str());
        if(out.setData(client)){
          out.start();
          isRun = true;
        }else{
          isRun = false;
          musicNumber++;
        }
      }
    }else{
      Serial.println("Network not available!");
    }
  }
  
  if(isRun){
    if(!out.run()){
      isRun = false;
      out.stop();
    }
  }
}
