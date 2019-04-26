/*
===========================================
 ArduinoPcap
 Copyright (c) 2017 Stefan Kremser
 https://github.com/spacehuhn/ArduinoPcap
===========================================
 for M5Stack  by N,Mori 2019.
 https://github.com/nmori/ArduinoPcap
===========================================
*/

/* note
 *  (esp_wifi_types.h)
 *  wifi_promiscuous_pkt_t is incorrect.
 *  please fix payload type.
 *  
 *  typedef struct {
 *   wifi_pkt_rx_ctrl_t rx_ctrl;
 *   uint8_t* payload;      
 *   ^^^^^^^^
 *  } wifi_promiscuous_pkt_t;
 *  
 */
#define SERIAL_TX_BUFFER_SIZE (256)
#define SERIAL_RX_BUFFER_SIZE (256)

//=======================================
//ライブラリ読み込み
//=======================================

#include "freertos/FreeRTOS.h"
#include "esp_wifi.h"
#include "esp_wifi_internal.h"
#include "lwip/err.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_event_loop.h"
#include "nvs_flash.h"
#include "driver/gpio.h"

#include <Arduino.h>
#include <TimeLib.h>
#include "FS.h"
#include "SD.h"
#include "SPI.h"
#include <PCAP.h>

#include <M5Stack.h>
#include <gitTagVersion.h>
#include <M5StackUpdater.h>

//=======================================
//設定値
//=======================================

//スキャンする対象の無線チャネル (ch1～ch11)
//(巡回モードOFF時に有効)
#define CHANNEL 1

//シリアル通信レート
#define BAUD_RATE 921600

//巡回モード(全ｃｈを対象にスキャンするか？）
#define CHANNEL_HOPPING true 

//CHの最大値
#define MAX_CHANNEL 11 

//巡回時にchを切り替える速さ(巡回モード時のみ、単位；ms)
#define HOP_INTERVAL 214 

//SD保存時のファイル名
#define FILENAME "esp32"

//SD保存間隔
#define SAVE_INTERVAL 600  //10分 

//=======================================
//変数定義
//=======================================

PCAP pcap = PCAP();
int ch = CHANNEL;
unsigned long lastChannelChange = 0;
unsigned long lastTime = 0;
bool fileOpen = false;
bool isSDmode = false;
int counter = 0;

//=======================================
//関数
//=======================================

//スキャンタスク
/* will be executed on every packet the ESP32 gets while beeing in promiscuous mode */
void sniffer(void *buf, wifi_promiscuous_pkt_type_t type){
  wifi_promiscuous_pkt_t* pkt = (wifi_promiscuous_pkt_t*)buf;
  wifi_pkt_rx_ctrl_t ctrl = (wifi_pkt_rx_ctrl_t)pkt->rx_ctrl;
  
  uint32_t timestamp = now(); //current timestamp 
  uint32_t microseconds = (unsigned int)(micros() - millis() * 1000); //micro seconds offset (0 - 999)
  uint8_t *payload = &pkt->payload[0];
  
  if(isSDmode==true){
    if(fileOpen){
      pcap.newPacketSD(timestamp, microseconds, ctrl.sig_len, payload); //write packet to file
    }
  }else{
    pcap.newPacketSerial(timestamp, microseconds, ctrl.sig_len, payload ); //send packet via Serial  
  }
}

esp_err_t event_handler(void *ctx, system_event_t *event){ return ESP_OK; }


//SD用のファイルを開く
void openFile(){

  //searches for the next non-existent file name
  int c = 0;
  String filename = "/" + (String)FILENAME + ".pcap";
  while(SD.open(filename)){
    filename = "/" + (String)FILENAME + "_" + (String)c + ".pcap";
    c++;
  }
  
  //set filename and open the file
  pcap.filename = filename;
  fileOpen = pcap.openFile(SD);

  M5.Lcd.print("opened: "+filename+"\n");

  //reset counter (counter for saving every X seconds)
  counter = 0;
}


//=======================================
//初期化関数
//=======================================
void setup() {
  M5.begin(true,true,false,true);  //LCD,SD,I2C
  if(digitalRead(BUTTON_A_PIN) == 0) {
    M5.Lcd.fillScreen(0x5fc);
    M5.Lcd.print(">Boot loader mode.\n");
    M5.Lcd.print(">Will Load menu binary\n");
    updateFromFS(SD);
    ESP.restart();
  }
  
  M5.Lcd.fillScreen(0x0);
  M5.Lcd.setTextSize(2);
  M5.Lcd.print("+------------------------+\n");
  M5.Lcd.print("| WireShark Wifi Probe   |\n");
  M5.Lcd.print("+------------------------+\n");
  M5.Lcd.print("Version 1.0a\n");
  M5.Lcd.print("\n");
  M5.Lcd.print("Select measurement mode.\n");
  M5.Lcd.print("B : SD recording\n");
  M5.Lcd.print("C : Serial(Live)\n");
  M5.Lcd.print("\n");
  M5.Lcd.print("If you want to go back to\n");
  M5.Lcd.print("the launcher,\n");
  M5.Lcd.print("push A + Power key.\n");
  M5.Lcd.print("\n");
  M5.Lcd.print("     A       B      C");

 //Wait a select.
  while(!M5.BtnB.isPressed() && !M5.BtnC.isPressed()) {M5.update();}

  if(M5.BtnA.isPressed()){
    isSDmode=true;  
    
    M5.Lcd.setCursor(0,0);
    M5.Lcd.fillScreen(0x0);
    M5.Lcd.print("+------------------------+\n");
    M5.Lcd.print("| WireShark Wifi Probe   |\n");
    M5.Lcd.print("+------------------------+\n");
    M5.Lcd.print("\n");    
    
    /* initialize SD card */
    if(!SD.begin()){
      M5.Lcd.print("Error:Card Mount Failed\n");
      return;
    }
    
    uint8_t cardType = SD.cardType();
    
    if(cardType == CARD_NONE){
        M5.Lcd.print("Error:No SD card attached\n");
        return;
    }
  
    M5.Lcd.print("SD Card Type: ");
    if(cardType == CARD_MMC){
        M5.Lcd.print("MMC\n");
    } else if(cardType == CARD_SD){
        M5.Lcd.print("SDSC\n");
    } else if(cardType == CARD_SDHC){
        M5.Lcd.print("SDHC\n");
    } else {
        M5.Lcd.print("UNKNOWN\n");
    }
  
    int64_t cardSize = SD.cardSize() / (1024 * 1024);
    M5.Lcd.printf("SD Card Size: %lluMB\n", cardSize);      
    M5.Lcd.print("Start measuring to SD.\n");
    openFile();

  }else{
      M5.Lcd.setCursor(0,0);
      M5.Lcd.fillScreen(0x0);
      M5.Lcd.print("+------------------------+\n");
      M5.Lcd.print("| WireShark Wifi Probe   |\n");
      M5.Lcd.print("+------------------------+\n");
      M5.Lcd.print("\n");
      M5.Lcd.print("Ready for Launch.\n");
      M5.Lcd.print("Please connecting \n from WireShark Script.\n");

    /* start Serial */
    Serial.begin(BAUD_RATE);
    delay(2000);
    Serial.println();
    
    Serial.println("<<START>>");
  }
  
  pcap.startSerial();

  /* setup wifi */
  nvs_flash_init();
  tcpip_adapter_init();
  ESP_ERROR_CHECK( esp_event_loop_init(event_handler, NULL) );
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
  ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
  ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_AP) );  
  ESP_ERROR_CHECK( esp_wifi_start() );
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_promiscuous_rx_cb(sniffer);
  wifi_second_chan_t secondCh = (wifi_second_chan_t)NULL;
  esp_wifi_set_channel(ch,secondCh);
}

//=======================================
//loop関数（ch巡回処理)
//=======================================
void loop() {
  
  unsigned long currentTime = millis();
  /* Channel Hopping */
  if(CHANNEL_HOPPING){
    if(currentTime - lastChannelChange >= HOP_INTERVAL){
      lastChannelChange = currentTime;
      ch++; //increase channel
      if(ch > MAX_CHANNEL) ch = 1;
      wifi_second_chan_t secondCh = (wifi_second_chan_t)NULL;
      esp_wifi_set_channel(ch,secondCh);
    }
  }

  if(isSDmode==true){
    if(fileOpen && currentTime - lastTime > 1000){
      pcap.flushFile(); //save file
      lastTime = currentTime; //update time
      counter++; //add 1 to counter
    }
    /* when counter > 30s interval */
    if(fileOpen && counter > SAVE_INTERVAL){
      pcap.closeFile(); //save & close the file
      fileOpen = false; //update flag
      M5.Lcd.fillScreen(0x0);
      M5.Lcd.setCursor(0,0);
      M5.Lcd.print("+------------------------+\n");
      M5.Lcd.print("| WireShark Wifi Probe   |\n");
      M5.Lcd.print("+------------------------+\n");
      M5.Lcd.print("\n");
      M5.Lcd.print("Start measuring to SD.\n");
      M5.Lcd.print(pcap.filename + " saved!\n");
      openFile(); //open new file
    }
  }
  M5.update();
}
