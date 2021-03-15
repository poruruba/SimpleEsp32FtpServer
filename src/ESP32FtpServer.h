/*
*  FTP SERVER FOR ESP8266
 * based on FTP Serveur for Arduino Due and Ethernet shield (W5100) or WIZ820io (W5200)
 * based on Jean-Michel Gallego's work
 * modified to work with esp8266 SPIFFS by David Paiva (david@nailbuster.com)
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
//  2021: modified by @poruruba

/*******************************************************************************
 **                                                                            **
 **                       DEFINITIONS FOR FTP SERVER                           **
 **                                                                            **
 *******************************************************************************/

// Uncomment to print debugging info to console attached to ESP8266
//#define FTP_DEBUG

#ifndef FTP_SERVERESP_H
#define FTP_SERVERESP_H

#include <WiFiClient.h>

#define FTP_SERVER_VERSION "FTP-2016-01-14"

#define FTP_CTRL_PORT    21          // Command port on wich server is listening  
#define FTP_DATA_PORT_PASV 50009     // Data port in passive mode

#define FTP_TIME_OUT  5           // Disconnect client after 5 minutes of inactivity
#define FTP_CMD_SIZE 255 + 8 // max size of a command
#define FTP_CWD_SIZE 255 + 8 // max size of a directory name
#define FTP_FIL_SIZE 255     // max size of a file name
#define FTP_BUF_SIZE 1024 //512   // size of file buffer for read/write

#define FNAME_LENGTH  64

typedef enum{
  F_IDLE = 0,
  F_RETRIEVED,
  F_STORED,
  F_DELETED,
  F_RENAMED
} FTP_F_STATUS;

class FtpServer{
public:
  void    begin(unsigned char *p_buffer, unsigned long length);
  void    begin(String uname, String pword, unsigned char *p_buffer, unsigned long length);
  FTP_F_STATUS  handleFTP();
  void setFile(const char *fname, unsigned long size);

  char file_name[FNAME_LENGTH];
  unsigned long file_buffer_size;
  struct tm file_timeInfo;

private:
  void client_println(String text);
  void data_println(String text);
  String toDateTimeStr(int type);

  void    iniVariables();
  void    clientConnected();
  void    disconnectClient();
  boolean userIdentity();
  boolean userPassword();
  boolean processCommand();
  boolean dataConnect();
  boolean doRetrieve();
  boolean doStore();
  void    closeTransfer();
  void    abortTransfer();
  boolean makePath( char * fullname );
  boolean makePath( char * fullName, char * param );
  int8_t  readChar();

  IPAddress  dataIp;              // IP address of client for data
  WiFiClient client;
  WiFiClient data;
  
  boolean  dataPassiveConn;
  uint16_t dataPort;
  char     buf[ FTP_BUF_SIZE ];       // data buffer for transfers
  char     cmdLine[ FTP_CMD_SIZE ];   // where to store incoming char from client
  char     cwdName[ FTP_CWD_SIZE ];   // name of current directory
  char     command[ 5 ];              // command sent by client
  boolean  rnfrCmd;                   // previous command was RNFR
  char *   parameters;                // point to begin of parameters sent by client
  uint16_t iCL;                       // pointer to cmdLine next incoming char
  int8_t   cmdStatus;                 // status of ftp command connexion
  FTP_F_STATUS transferStatus;            // status of ftp data transfer
  uint32_t millisTimeOut,             // disconnect after 5 min of inactivity
           millisDelay,
           millisEndConnection,       // 
           millisBeginTrans,          // store time of beginning of a transaction
           bytesTransfered;           //
  String   _FTP_USER;
  String   _FTP_PASS;

  unsigned long file_buffer_length;
  unsigned char *file_buffer;
};

#endif // FTP_SERVERESP_H


