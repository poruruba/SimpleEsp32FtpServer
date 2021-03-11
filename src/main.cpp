#include <Arduino.h>
#include <WiFi.h>
#include "ESP32FtpServer.h"

const char *wifi_ssid = "【WiFiアクセスポイントのSSID】";
const char *wifi_password = "【WiFiアクセスポイントのパスワード】";

FtpServer ftpSrv;   //set #define FTP_DEBUG in ESP32FtpServer.h to see ftp verbose on serial

#define BUFFER_SIZE  1024
unsigned char buffer[BUFFER_SIZE];

void wifi_connect(const char *ssid, const char *password){
  Serial.println("");
  Serial.print("WiFi Connenting");

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(1000);
  }

  Serial.println("");
  Serial.print("Connected : ");
  Serial.println(WiFi.localIP());
}

void setup() {
  Serial.begin(9600);

  wifi_connect(wifi_ssid, wifi_password);

  configTzTime("JST-9", "ntp.nict.jp", "ntp.jst.mfeed.ad.jp");
  ftpSrv.begin("esp32","esp32", buffer, sizeof(buffer));    //username, password for ftp.  set ports in ESP32FtpServer.h  (default 21, 50009 for PASV)
}

void loop() {
  FTP_F_STATUS status = ftpSrv.handleFTP();        //make sure in loop you call handleFTP()!!   
  if( status != F_IDLE ){
    Serial.print("status="); Serial.println(status); Serial.println(ftpSrv.file_name); Serial.println((char*)buffer);
  }
}
