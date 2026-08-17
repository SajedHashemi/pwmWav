# PWM WAV PLAYER
Play WAV file. support ESP32 platform.<br>
With the ability to use the callback method to play audio, which is the recommended method (see examples).<br>
** Note: ** If callback is set, this method takes priority.<br><br>

### How to use:
- Download the zip file and add it to the list of available Arduino libraries through the [Add ZIP Library] option
- Put the following line at the beginning of the file and in the head tag
```C++
#include "pwmWav.h"
```

#### Simple example 1:
```C++
#include <pwmWav.h>
#include "data.h"

#define SPEAKER_PIN 12 //Any other pin

pwmWav out;

void setup() {
  delay(1000);
  Serial.begin(115200);
  Serial.println();
  
  SPIFFS.begin();
  aud = SPIFFS.open("/music.wav");

  outconfig_t cfg;
  cfg.lSPKPin = SPEAKER_PIN;
  cfg.vol = -8;
  out.begin(cfg);
  
  if(aud) out.play(aud);
  delay(1000);

  out.setData(music_wav, music_wav_len);
  out.play();
}

void loop(){
  delay(10);
}
```

#### Simple example 2:
```C++
#include <pwmWav.h>
#include "data.h"

#define SPEAKER_PIN 12 //Any other pin

pwmWav out;

void dataCallback(uint8_t *pwm_buffer, int len){
  out.run(pwm_buffer, len);
}

void setup() {
  delay(1000);
  Serial.begin(115200);
  Serial.println();
  
  SPIFFS.begin();
  aud = SPIFFS.open("/music.wav");

  outconfig_t cfg;
  cfg.lSPKPin = SPEAKER_PIN;
  cfg.vol = -8;
  out.begin(cfg);
  out.setCallback(dataCallback);
  
  if(aud) out.play(aud);
  delay(1000);

  out.setData(music_wav, music_wav_len);
  out.play();
}

void loop(){
  delay(10);
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

**Note:** Settings related to audio playback can be made through the following functions.
- setParameters(int samplerate, int bits, int channels) -> void
- setCallback(WAVCallback cb) -> void //Used in the callback method
- delCallback(void) -> void
- enEcho(bool en) -> void //By setting 'true', the description of the execution is displayed in the serial output.

### Thank you
