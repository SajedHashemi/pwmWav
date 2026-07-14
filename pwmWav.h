#include "pwm_audio.h"
#include "FS.h"
#include <SPIFFS.h>

#define READ_LEN       (2 * 256)
#define WAV_HEADER_BUF 0x30

#define RIFF_ID "RIFF"
#define WAVE_ID "WAVE"
#define DATA_CHUNK_ID "data"
#define FMT_CHUNK_ID "fmt "

#define GET_LE_LONGWORD(bfr, ofs) (bfr[ofs+3] << 24 | bfr[ofs+2] << 16 |bfr[ofs+1] << 8 |bfr[ofs+0])
#define GET_LE_SHORTWORD(bfr, ofs) (bfr[ofs+1] << 8 | bfr[ofs+0])
#define GET_DATA_LEN(bfr) (sizeof(bfr)/sizeof(bfr[0]))

#define SPEAKER_L 12
#define SPEAKER_R -1
 
typedef struct wav_config_t{
  int lSPKPin = SPEAKER_L;
  int rSPKPin = SPEAKER_R;
  int8_t vol;
}outconfig_t;

enum play_mode{
  FILE_MODE,
  DATA_MODE
};

class pwmWav{
  public:
    pwmWav(){}

    bool begin(outconfig_t);
    bool end();
    uint8_t getHeader(File);
    uint8_t getHeader(const uint8_t*, uint32_t);
    void play();
    void play(File);
    void play(const uint8_t*, uint32_t);
    int run();
    bool stop();
    bool start();
    void setFile(File);
    void setData(const uint8_t*, uint32_t);
    
    void setVolume(int8_t val){ vol = val; }
    int8_t increaseVolume(){ if(++vol>=16) vol=16; return vol; }
    int8_t decreaseVolume(){ if(--vol<=-16) vol=-16; return vol; }
    uint8_t getVolume(){ return vol; }
    int getSamplerate(){ return sampleRate; }
    int getChannels(){ return channels; }
    int getBits(){ return bits; }

    void getLengthTime(uint8_t*, uint8_t*, uint8_t*);
    
  private:
    int channels, sampleRate, bits;
    uint32_t dataStart;
    uint32_t dataSize;
    uint32_t seekPointer;
    bool stopped = true;
    int8_t vol;
    bool _init = false;
    play_mode playMode;
    File wavFile;
    const uint8_t* wavData;
    uint8_t delayToWrite;

    int read(uint8_t*);
    size_t write(uint8_t*, int);
};
