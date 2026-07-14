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
#include "pwmWav.h"
#include "data.h"

#define SPEAKER_L 12
#define SPEAKER_R -1

File aud1, aud2, aud3;
pwmWav out;

void setup() {
  delay(1000);
  Serial.begin(115200);
  Serial.println();

  SPIFFS.begin();
  aud1 = SPIFFS.open("/test.wav");
  aud2 = SPIFFS.open("/music.wav");
  aud3 = SPIFFS.open("/welcome.wav");

  outconfig_t cfg;
  cfg.lSPKPin = 12;
  cfg.vol = -10;
  out.begin(cfg);
  
  //Test_1(Play from file)
  Serial.println("Test 1: Play file -> test.wav");
  out.play(aud1);
  delay(2000);

  //Test_2(Replay previous WAV)
  Serial.println("Test 2: Replay -> test.wav");
  out.play();
  delay(2000);
  
  //Test_3(Play from wav array data)
  Serial.println("Test 3: Play data array -> music_wav");
  out.play(music_wav, music_wav_len);
  delay(2000);
  
  //Test_4(Play from file)
  Serial.println("Test 4: Play file -> music.wav");
  out.play(aud2);
  delay(2000);
  
  //Test_5(Play from file)
  Serial.println("Test 5: Play file -> welcome.wav");
  out.setVolume(-5);
  out.setFile(aud3);
  out.play();
  delay(2000);
  
  //Test_6(Play in loop)
  Serial.println("Test 6: Play music_wav");
  out.setData(music_wav, music_wav_len);
  out.start();
}

void loop(){
  out.run();
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
