#include "pwmWav.h"

bool pwmWav::begin(outconfig_t cfg){
  if(_init) return true;
  static const pwm_audio_config_t pac ={
    .tg_num             = TIMER_GROUP_0,
    .timer_num          = TIMER_0,
    .gpio_num_left      = cfg.lSPKPin,
    .gpio_num_right     = cfg.rSPKPin,
    .ledc_channel_left  = LEDC_CHANNEL_0,
    .ledc_channel_right = LEDC_CHANNEL_1,
    .ledc_timer_sel     = LEDC_TIMER_0,
    .duty_resolution    = LEDC_TIMER_10_BIT,
    .ringbuf_len        = 1024 * 4,
  };
  
  esp_err_t err = pwm_audio_init(&pac);  /**< Initialize pwm audio */
  if(err != ESP_OK){
    if(err == ESP_FAIL) Serial.println("timer_group or ledc initialize failed");
    else if(err == ESP_ERR_INVALID_ARG) Serial.println("argument wrong");
    else if(err == ESP_ERR_INVALID_STATE) Serial.println("The pwm audio already configure");
    else if(err == ESP_ERR_NO_MEM) Serial.println("Memory allocate failed");
    else Serial.println("initialize failed[Unknown error]");
    return false;
  }
  _init = true;
   
  vol = cfg.vol;

  seekPointer = 0;
  dataSize = 0;
  dataStart = 0;
  channels = 0;
  sampleRate = 0;
  bits = 0;

  return true;
}

bool pwmWav::end(){
  if(!stopped) stop();
  if(pwm_audio_deinit()==ESP_OK){
    _init = false;
    stopped = true;
    if(wavFile) wavFile.close();
    wavData = NULL;
    seekPointer = 0;
    dataSize = 0;
    dataStart = 0;
    channels = 0;
    sampleRate = 0;
    bits = 0;
    return true;
  }
  return false;
}

uint8_t pwmWav::getHeader(const uint8_t* soundArray, uint32_t len){
  uint8_t headerData[WAV_HEADER_BUF];
  uint32_t offset = 0x00, size, chunkSize;
  
  if(len < WAV_HEADER_BUF){
    Serial.print("wav header not read\r\n");
    return 0;
  }
  
  for(; offset < 0x0c; offset++) headerData[offset] = soundArray[offset];
  if(memcmp(headerData,RIFF_ID,4) || memcmp(headerData,RIFF_ID,4))  return 0;
  
  size = GET_LE_LONGWORD(headerData, 4);
  offset=0x0c;

  while(offset<size){
    for(uint8_t i = 0; i < 8; offset++, i++) headerData[i] = soundArray[offset];
    if(!memcmp(headerData, DATA_CHUNK_ID,4)){
      dataSize = GET_LE_LONGWORD(headerData, 0x04);
      dataStart = offset;
      uint8_t hr = 0, mi = 0, sc = 0;
      getLengthTime(&hr, &mi, &sc);
      Serial.printf("Wav file detected, Samplerate=%d, channels=%d, bits=%d, size=%d, start=%d, length=%d:%d:%d\n", sampleRate,channels,bits,dataSize,dataStart, hr, mi, sc);
      return 1;
    }else{
      chunkSize=GET_LE_LONGWORD(headerData,0x04);
      Serial.printf("Chunk: %d\r\n", chunkSize);
      if(chunkSize>=WAV_HEADER_BUF-8){
        Serial.printf("header chunk size too big!\n");
        return 0;
      }
      for(uint8_t i = 8; i < chunkSize+8; offset++, i++) headerData[i] = soundArray[offset];
      if(size-offset < chunkSize){
        Serial.printf("2:wav header not read\n");
        return 0;
      }
      if(!memcmp(headerData, FMT_CHUNK_ID,4)){
        channels = GET_LE_SHORTWORD(headerData,0x0a);
        sampleRate = GET_LE_LONGWORD(headerData, 0x0c);
        bits = GET_LE_SHORTWORD(headerData,0x16);
      }
    }
  }
  return 0;
}

uint8_t pwmWav::getHeader(File soundFile){
  uint8_t headerData[WAV_HEADER_BUF];
  uint32_t offset, size, chunkSize;
  
  soundFile.seek(0);
  if(soundFile.read(headerData,0x0c) != 0x0c){
    Serial.printf("1:wav header not read\n");
    return 0;
  }
  
  if(memcmp(headerData,RIFF_ID,4) || memcmp(headerData,RIFF_ID,4))  return 0;
    
  size = GET_LE_LONGWORD(headerData, 4);
  offset=0x0c;

  while(offset<size){
    if(soundFile.read(headerData,8) != 8){
      Serial.printf("2:wav header not read\n");
      return 0;
    }
    if(!memcmp(headerData, DATA_CHUNK_ID,4)){
      dataSize = GET_LE_LONGWORD(headerData, 0x04);
      dataStart = offset+8;
      uint8_t hr = 0, mi = 0, sc = 0;
      getLengthTime(&hr, &mi, &sc);
      Serial.printf("Wav file detected, Samplerate=%d, channels=%d, bits=%d, size=%d, start=%d, length=%d:%d:%d\n", sampleRate,channels,bits,dataSize,dataStart, hr, mi, sc);
      return 1;
    }else{
      chunkSize=GET_LE_LONGWORD(headerData,0x04);
      Serial.printf("Chunk: %d\r\n", chunkSize);
      if(chunkSize>=WAV_HEADER_BUF-8){
        Serial.printf("header chunk size too big!\n");
        return 0;
      }
      if(soundFile.read(headerData+8,chunkSize) != chunkSize){
        Serial.printf("3:wav header not read\n");
        return 0;
      }
      if(!memcmp(headerData, FMT_CHUNK_ID,4)){
        channels = GET_LE_SHORTWORD(headerData,0x0a);
        sampleRate = GET_LE_LONGWORD(headerData, 0x0c);
        bits = GET_LE_SHORTWORD(headerData,0x16);
      }
      offset+=chunkSize+8;
    }
  }
  return 0;   
}

void pwmWav::play(){
  if(!_init) return;
  if(stopped) start();

  seekPointer = dataStart;
  while(seekPointer < dataSize){
    int bufC = 0;
    uint8_t buf[READ_LEN];
    if(playMode == FILE_MODE){
      bufC = read(buf);
      seekPointer += bufC;
    }else if(playMode == DATA_MODE){
      for(;bufC < READ_LEN && (dataSize-seekPointer)>0; bufC++, seekPointer++)
        buf[bufC] = wavData[seekPointer];
    }
    write(buf, bufC);
  }
  stop();
}

void pwmWav::play(File src){
  if(!_init) return;
  setFile(src);
  play();
}

void pwmWav::play(const uint8_t* src, uint32_t len){
  if(!_init) return;
  setData(src, len);
  play();
}
    
int pwmWav::run(){
  if((!_init || !wavFile || !wavData) && stopped) return 0;

  if(seekPointer < dataSize){
    int bufC = 0;
    uint8_t buf[READ_LEN];
    if(playMode == FILE_MODE){
      bufC = read(buf);
      seekPointer += bufC;
    }else if(playMode == DATA_MODE){
      for(;bufC < READ_LEN && (dataSize-seekPointer)>0; bufC++, seekPointer++)
        buf[bufC] = wavData[seekPointer];
    }
    return write(buf, bufC);
  }
  stop();
  return 0;
}

bool pwmWav::stop(){
  if(!_init) return false;
  if(stopped) return true;
  if(pwm_audio_stop() == ESP_OK){
    Serial.println("Stop");
    seekPointer = dataStart;
    wavFile.seek(dataStart);
    stopped = true;
    return true;
  }
  return false;
}

bool pwmWav::start(){
  if(!_init) return false;
  if(!stopped) return true;
  
  if(pwm_audio_start() == ESP_OK){
    stopped = false;
    return true;
  }
  return false;
}

void pwmWav::setFile(File src){
  if(!_init) return;
  wavFile = src;
  getHeader(wavFile);
  ledc_timer_bit_t bt = (ledc_timer_bit_t)bits;
  wavFile.seek(dataStart);
  pwm_audio_set_param(sampleRate, bt, channels);  /**< Set sample rate, bits and channel numner */
  playMode = FILE_MODE;
  seekPointer = dataStart;
  delayToWrite = ((((float)READ_LEN / sampleRate) * 1000)/(bits / 8)) + 1;
}

void pwmWav::setData(const uint8_t* src, uint32_t len){
  if(!_init) return;
  wavData = src;
  getHeader(wavData, len);
  ledc_timer_bit_t bt = (ledc_timer_bit_t)bits;
  pwm_audio_set_param(sampleRate, bt, channels);  /**< Set sample rate, bits and channel numner */
  playMode = DATA_MODE;
  seekPointer = dataStart;
  delayToWrite = ((((float)READ_LEN / sampleRate) * 1000)/(bits / 8)) + 1;
}

void pwmWav::getLengthTime(uint8_t *hr, uint8_t *mi, uint8_t *sc){
  int sec = ((float)dataSize / sampleRate) / (bits / 8);
  if(sec < 60){
    *sc = sec;
  }else{
    *mi = sec / 60;
    *sc = sec % 60;
  }
  if(*mi >= 60){
    *hr = *mi / 60;
    *mi = *mi % 60;
  }
}

int pwmWav::read(uint8_t* bfr){
  int bufC = 0;
  if(wavFile.available()>=READ_LEN) bufC=wavFile.read(bfr, READ_LEN);
  else bufC = wavFile.read(bfr, wavFile.available());
  return bufC;
}

size_t pwmWav::write(uint8_t* buf, int bufC){
  size_t written = 0;
  pwm_audio_set_volume(vol);
  //printHex(buf, bufC);
  pwm_audio_write(buf, bufC, &written, 1000 / portTICK_PERIOD_MS);
  delay(delayToWrite);
  return written;
}
