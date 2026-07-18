# PWM WAV PLAYER
Play WAV file. support ESP32 platform.<br><br>

### How to use:
- Download the zip file and add it to the list of available Arduino libraries through the [Add ZIP Library] option
- Put the following line at the beginning of the file and in the head tag
```C++
#include "pwmWav.h"
```

#### Example:
```C++
#include <WiFi.h>
#include <WiFiClient.h>
#include <pwmWav.h>
#include "data.h"

#define WIFI_SSID "YOUR-SSID"
#define WIFI_PASS "YOUR-PASS"

#define SPEAKER_L 12
#define SPEAKER_R -1

//#define WAV_ONLINE_ADDR "http://192.168.2.2/data/speak.wav"
#define WAV_ONLINE_ADDR "http://sound.mycotech.ir/wav/voice_1.wav"

WiFiClient client;
File aud1, aud2, aud3;
pwmWav out;

int musicNumber = 0;
bool isRun = false;

void setup() {
  delay(1000);
  Serial.begin(115200);
  Serial.println();

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  int toConnect = 40;
  while(WiFi.status() != WL_CONNECTED){
    Serial.print(".");
    delay(500);
    if(--toConnect==0) break;
  }
  if(toConnect==0) Serial.println("Connect to network faild!\r\n** Online play music not running! **\r\n");
  Serial.println();
  
  SPIFFS.begin();
  aud1 = SPIFFS.open("/test.wav");
  aud2 = SPIFFS.open("/music.wav");
  aud3 = SPIFFS.open("/welcome.wav");

  outconfig_t cfg;
  cfg.lSPKPin = 12;
  cfg.vol = -8;
  out.begin(cfg);
}

void loop(){
  if(musicNumber==0){
    //Test_1(Play from file)
    Serial.println("Test 1: Play file -> test.wav");
    out.setVolume(-10);
    out.play(aud1);
    delay(2000);
    musicNumber++;
  }else if(musicNumber==1){
    //Test_2(Replay previous WAV)
    Serial.println("Test 2: Replay -> test.wav");
    out.play();
    delay(2000);
    musicNumber++;
  }else if(musicNumber==2){
    //Test_3(Play from wav array data)
    Serial.println("Test 3: Play data array -> music_wav");
    out.play(music_wav, music_wav_len);
    delay(2000);
    musicNumber++;
  }else if(musicNumber==3){
    //Test_4(Play from file)
    Serial.println("Test 4: Play file -> music.wav");
    out.play(aud2);
    delay(2000);
    musicNumber++;
  }else if(musicNumber==4){
    //Test_5(Play from file)
    Serial.println("Test 5: Play file -> welcome.wav");
    out.setVolume(-5);
    out.setFile(aud3);
    out.play();
    delay(2000);
    musicNumber++;
  }else if(musicNumber==5){
    //Test_6(Play in loop)
    if(!isRun){
      Serial.println("Test 6: Play music_wav");
      out.setData(music_wav, music_wav_len);
      out.start();
      isRun = true;
    }
  }else if(musicNumber==6){
    //Test_7(Play online file in loop)
    if(!isRun){
      Serial.println("Test 7: Play online music");
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
        musicNumber++;
      }
    }
  }else{
    musicNumber = (++musicNumber>=7)?0:musicNumber;
  }
  
  if(isRun){
    if(!out.run()){
      isRun = false;
      out.stop();
      musicNumber++;
    }
  }
}
```

**Note:** Volume adjustment is done using the following functions.
- setVolume(uint8_t val) -> void
- increaseVolume() -> int8_t
- decreaseVolume() -> int8_t
- getVolume() -> uint8_t

**Note:** The file parameters can be obtained with the following functions.
- getSamplerate() -> int
- getChannels() -> int
- getBits() -> int
- getLengthTime(uint8_t* hr, uint8_t* mi, uint8_t* sc) -> void

### Thank you
