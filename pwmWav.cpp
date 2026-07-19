#include "pwmWav.h"

void printHex(const uint8_t* buf, int len){
  for(int i = 0;i<len;i++){
    if(i%8==0) Serial.println();
    Serial.printf("0x%02x ", buf[i]);
  }
  Serial.println();
}

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
    if(wavClient) wavClient.stop();
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
    Serial.print("1:wav header not read\r\n");
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
    }else if(playMode == ONLINE_MODE){
      bufC = read(buf);
      seekPointer += bufC;
    }
    if(!write(buf, bufC)) break;
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
  if((!_init || !wavFile || !wavData || !wavClient) && stopped) return 0;

  if(seekPointer < dataSize){
    int bufC = 0;
    uint8_t buf[READ_LEN];
    if(playMode == FILE_MODE){
      bufC = read(buf);
      seekPointer += bufC;
    }else if(playMode == DATA_MODE){
      for(;bufC < READ_LEN && (dataSize-seekPointer)>0; bufC++, seekPointer++)
        buf[bufC] = wavData[seekPointer];
    }else if(playMode == ONLINE_MODE){
      bufC = read(buf);
      seekPointer += bufC;
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

bool pwmWav::setData(WiFiClient src){
  if(!_init) return false;
  wavClient = src;
  while(!wavClient.available()) delay(100);
  if(wavClient.available()){
    int bufC = 0;
    uint8_t buf[READ_LEN];
    int code = 0;
    bufC = readHTTPContentWithCMP(buf, &code, DATA_CHUNK_ID, 4);
    if(code==200){
      getHeader(buf, bufC);
      ledc_timer_bit_t bt = (ledc_timer_bit_t)bits;
      pwm_audio_set_param(sampleRate, bt, channels);  /**< Set sample rate, bits and channel numner */
      playMode = ONLINE_MODE;
      seekPointer = dataStart;
      delayToWrite = ((((float)READ_LEN / sampleRate) * 1000)/(bits / 8)) + 1;
      return true;
    }else{
      Serial.println("Could not read data from server!");
    }
  }
  return false;
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

bool pwmWav::urlSeperator(String* url, String* host, int* port){
  String tmp = *url;
  if(tmp.length()==0) return 0;
  int pos=-1;
  bool security = false;
  if(tmp.indexOf("https://")==0){
    *port = 443;
    security = true;
  }else{
    *port = 80;
    security = false;
  }
  Serial.printf("URL: %s - ",tmp.c_str());
  tmp.trim();
  if(tmp.indexOf("://")>=0){
    pos = tmp.indexOf("://");
    *url = tmp.substring(pos+3);
    tmp = *url;
  }
  pos = tmp.indexOf("/");
  if(pos>=0){
    *host = tmp.substring(0,pos);
    *url = tmp.substring(pos+1);
    tmp = *url;
  }else{
    *host = tmp;
    *url = "";
    tmp = "";
  }

  tmp = *host;
  pos = tmp.indexOf(":");
  if(pos>=0){
    *port = tmp.substring(pos+1).toInt();
    *host = tmp.substring(0, pos);
  }

  //tmp =*host;
  //Serial.printf("HOST: %s - ", tmp.c_str());
  //Serial.printf("PORT: %d - ", httpPort);
  //tmp = *url;
  //Serial.printf("ADDR: %s\r\n",tmp.c_str());
  return security;
}

int pwmWav::readHTTPContent(uint8_t* bfr, int *code, uint32_t contentLength = -1){
  bool ok = false;
  uint32_t len = 0;
  int bufC = 0;
  while(wavClient.available()){
    String c = wavClient.readStringUntil('\n');
    //Serial.println(c);
    c.trim();
    if(len > 0){
      if(c.length() == 0){
        for(int i = 0; i < len && i < contentLength; i++){
          //if(wavClient.connected()){
          //  while(!wavClient.available()) delay(10);
            *(bfr + i) = wavClient.read();
            bufC++;
          //}else{
          //  break;
          //}
        }
        break;
      }
    }
    if(ok && len==0){
      c.toLowerCase();
      if(c.indexOf("content-length:")>=0){
        byte pos = c.indexOf(":");
        len = c.substring(pos+1).toInt();
      }
    }
    if(c.indexOf(" 200 OK")>0 && !ok){
      ok = true;
      *code = 200;
    }else if(c.indexOf("HTTP/")==0 && !ok){
      int pos = c.indexOf(" ");
      c = c.substring(pos+1);
      pos = c.indexOf(" ");
      *code = c.substring(0, pos).toInt();
    }
  }
  Serial.printf("Code=%d, Length=%d\r\n",*code, len);
  return bufC;
}

int pwmWav::readHTTPContentWithCMP(uint8_t* bfr, int *code, const char* contentcmp, uint8_t aftercmp){
  bool ok = false;
  uint32_t len = 0;
  int bufC = 0;
  uint8_t cmplen = strlen(contentcmp);
  while(wavClient.available()){
    String c = wavClient.readStringUntil('\n');
    //Serial.println(c);
    c.trim();
    if(len > 0){
      if(c.length() == 0){
        for(int i = 0; i < len && i < READ_LEN; i++){
          //if(wavClient.connected()){
          //  while(!wavClient.available()) delay(10);
            *(bfr + i) = wavClient.read();
            bufC++;
            if(cmplen > 0 && bufC >= cmplen){
              uint8_t n = 0;
              for(; n < cmplen; n++){
                if(*(bfr - n) != contentcmp[cmplen - n]) break;
              }
              if(n==cmplen){
                for(n=0;n<aftercmp && n<(READ_LEN-i);n++){
                  *(bfr + i) = wavClient.read();
                  bufC++;
                }
                break;
              }
            }
          //}else{
          //  break;
          //}
        }
        break;
      }
    }
    if(ok && len==0){
      c.toLowerCase();
      if(c.indexOf("content-length:")>=0){
        byte pos = c.indexOf(":");
        len = c.substring(pos+1).toInt();
      }
    }
    if(c.indexOf(" 200 OK")>0 && !ok){
      ok = true;
      *code = 200;
    }else if(c.indexOf("HTTP/")==0 && !ok){
      int pos = c.indexOf(" ");
      c = c.substring(pos+1);
      pos = c.indexOf(" ");
      *code = c.substring(0, pos).toInt();
    }
  }
  Serial.printf("Code=%d, Length=%d\r\n",*code, len);
  return bufC;
}

int pwmWav::read(uint8_t* bfr, uint32_t len){
  int bufC = 0;
  if(playMode == FILE_MODE){
    if(wavFile.available()>=len) bufC=wavFile.read(bfr, len);
    else bufC = wavFile.read(bfr, wavFile.available());
  }else if(playMode == ONLINE_MODE){
    if(wavClient.connected()){
      if(seekPointer < dataSize && !wavClient.available()){
        int idel = 3000;
        while(!wavClient.available() && idel>0){
          delay(10);
          idel-=10;
        }
      }
    }
    if(wavClient.available()>=len) bufC=wavClient.read(bfr, len);
    else bufC = wavClient.read(bfr, wavClient.available());
  }
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