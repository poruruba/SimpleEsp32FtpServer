/*
 * FTP Serveur for ESP8266
 * based on FTP Serveur for Arduino Due and Ethernet shield (W5100) or WIZ820io (W5200)
 * based on Jean-Michel Gallego's work
 * modified to work with esp8266 SPIFFS by David Paiva david@nailbuster.com
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

#include "ESP32FtpServer.h"

#include <WiFi.h>
#include <WiFiClient.h>

WiFiServer ftpServer( FTP_CTRL_PORT );
WiFiServer dataServer( FTP_DATA_PORT_PASV );

void FtpServer::client_println(String text){
#ifdef FTP_DEBUG
  Serial.println(String("(ctrl) ") + text);
#endif
  client.println(text);
}

void FtpServer::data_println(String text){
#ifdef FTP_DEBUG
  Serial.println(String("(data) ") + text);
#endif
  data.println(text);
}

void FtpServer::begin(unsigned char *p_buffer, unsigned long length){
  begin("anonymous", "", p_buffer, length);
}

void FtpServer::begin(String uname, String pword, unsigned char *p_buffer, unsigned long length){
  file_buffer = p_buffer;
  file_buffer_length = length;

  // Tells the ftp server to begin listening for incoming connection
  _FTP_USER = uname;
  _FTP_PASS = pword;

  ftpServer.begin();
  delay(10);
  dataServer.begin();	
  delay(10);
  millisTimeOut = (uint32_t)FTP_TIME_OUT * 60 * 1000;
  millisDelay = 0;
  cmdStatus = 0;

  file_name[0] = '\0';
  file_buffer_size = 0;
	
  iniVariables();
}

void FtpServer::iniVariables(){
  // Default for data port
  dataPort = FTP_DATA_PORT_PASV;
  
  // Default Data connection is Active
  dataPassiveConn = true;
  
  // Set the root directory
  strcpy( cwdName, "/" );

  rnfrCmd = false;
  transferStatus = F_IDLE;  
}

void FtpServer::setFile(const char *fname, unsigned long size){
  strcpy( file_name, fname );
  file_buffer_size = size;
  getLocalTime(&file_timeInfo);
}

FTP_F_STATUS FtpServer::handleFTP(){
  FTP_F_STATUS lastTransferStatus = F_IDLE;

  if((int32_t) ( millisDelay - millis() ) > 0 )
    return lastTransferStatus;

  if (ftpServer.hasClient()) {
	  client.stop();
	  client = ftpServer.available();
  }
  
  if( cmdStatus == 0 ){
    if( client.connected())
      disconnectClient();
    cmdStatus = 1;
  }else
  if( cmdStatus == 1 ){
    // Ftp server waiting for connection
    abortTransfer();
    iniVariables();
#ifdef FTP_DEBUG
     Serial.println("Ftp server waiting for connection on port "+ String(FTP_CTRL_PORT));
#endif
    cmdStatus = 2;
  }else
  if( cmdStatus == 2 ){
    // Ftp server idle
    if( client.connected() ){
      // A client connected
      clientConnected();      
      millisEndConnection = millis() + 10 * 1000 ; // wait client id during 10 s.
      cmdStatus = 3;
    }
  }else
  if( readChar() > 0 ){
    // got response
    if( cmdStatus == 3 ){
      // Ftp server waiting for user identity
      if( userIdentity() )
        cmdStatus = 4;
      else
        cmdStatus = 0;
    }else
    if( cmdStatus == 4 ){
      // Ftp server waiting for user registration
      if( userPassword() ){
        cmdStatus = 5;
        millisEndConnection = millis() + millisTimeOut;
      }else{
        cmdStatus = 0;
      }
    }else
    if( cmdStatus == 5 ){
      // Ftp server waiting for user command
      if( ! processCommand())
        cmdStatus = 0;
      else
        millisEndConnection = millis() + millisTimeOut;
    }
  }else
  if (!client.connected() || !client){
	  cmdStatus = 1;
#ifdef FTP_DEBUG
    Serial.println("client disconnected");
#endif
  }

  if( transferStatus == F_RETRIEVED ){
    // Retrieve data
    if( ! doRetrieve() ){
      lastTransferStatus = transferStatus;
      transferStatus = F_IDLE;
    }
  }else
  if( transferStatus == F_STORED ){
    // Store data
    if( ! doStore()){
      lastTransferStatus = transferStatus;
      transferStatus = F_IDLE;
    }
  }else
  if( transferStatus == F_DELETED || transferStatus == F_RENAMED ){
    lastTransferStatus = transferStatus;
    transferStatus = F_IDLE;
  }else
  if( cmdStatus > 2 && ! ((int32_t) ( millisEndConnection - millis() ) > 0 )){
    client_println("530 Timeout");
    millisDelay = millis() + 200;    // delay of 200 ms
    cmdStatus = 0;
  }

  return lastTransferStatus;
}

void FtpServer::clientConnected(){
#ifdef FTP_DEBUG
	Serial.println("Client connected!");
#endif
  client_println( "220--- Welcome to FTP for ESP8266 ---");
  client_println( "220---   By David Paiva   ---");
  client_println( "220 --   Version "+ String(FTP_SERVER_VERSION) +"   --");
  iCL = 0;
}

void FtpServer::disconnectClient(){
#ifdef FTP_DEBUG
	Serial.println(" Disconnecting client");
#endif
  abortTransfer();
  client_println("221 Goodbye");
  client.stop();
}

boolean FtpServer::userIdentity(){	
  if( strcmp( command, "USER" )){
    client_println( "500 Syntax error");
  }else
  if( strcmp( parameters, _FTP_USER.c_str() )){
    client_println( "530 user not found");
  }else{
    client_println( "331 OK. Password required");
    strcpy( cwdName, "/" );
    return true;
  }

  millisDelay = millis() + 100;  // delay of 100 ms
  return false;
}

boolean FtpServer::userPassword(){
  if( strcmp( command, "PASS" )){
    client_println( "500 Syntax error");
  }else
  if(  _FTP_PASS != "" && strcmp( parameters, _FTP_PASS.c_str() )){
    client_println( "530 ");
  }else{
#ifdef FTP_DEBUG
    Serial.println( "OK. Waiting for commands.");
#endif
    client_println( "230 OK.");
    return true;
  }

  millisDelay = millis() + 100;  // delay of 100 ms
  return false;
}

boolean FtpServer::processCommand(){
  ///////////////////////////////////////
  //                                   //
  //      ACCESS CONTROL COMMANDS      //
  //                                   //
  ///////////////////////////////////////

  //
  //  CDUP - Change to Parent Directory 
  //
  if( ! strcmp( command, "CDUP" )){
	  client_println("250 Ok. Current directory is " + String(cwdName));
  }else
  //
  //  CWD - Change Working Directory
  //
  if( ! strcmp( command, "CWD" )){
    if( strcmp( parameters, "." ) == 0 ){
      // 'CWD .' is the same as PWD command
      client_println( "257 \"" + String(cwdName) + "\" is your current directory");
    }else{
      client_println( "250 Ok. Current directory is " + String(cwdName) );
    }
  }else
  //
  //  PWD - Print Directory
  //
  if( ! strcmp( command, "PWD" )){
    client_println( "257 \"" + String(cwdName) + "\" is your current directory");
  }else
  //
  //  QUIT
  //
  if( ! strcmp( command, "QUIT" )){
    disconnectClient();
    return false;
  }else

  ///////////////////////////////////////
  //                                   //
  //    TRANSFER PARAMETER COMMANDS    //
  //                                   //
  ///////////////////////////////////////

  //
  //  MODE - Transfer Mode 
  //
  if( ! strcmp( command, "MODE" )){
    if( ! strcmp( parameters, "S" ))
      client_println( "200 S Ok");
    else
      client_println( "504 Only S(tream) is suported");
  }else
  //
  //  PASV - Passive Connection management
  //
  if( ! strcmp( command, "PASV" )){
    if (data.connected())
      data.stop();

    dataIp = WiFi.localIP();	
    dataPort = FTP_DATA_PORT_PASV;
#ifdef FTP_DEBUG
	  Serial.println("Connection management set to passive");
    Serial.println( "Data port set to " + String(dataPort));
#endif
    client_println( "227 Entering Passive Mode (" + String(dataIp[0]) + "," + String(dataIp[1]) + "," + String(dataIp[2]) + "," +  String(dataIp[3]) + "," + String( dataPort >> 8 ) + "," + String ( dataPort & 255 ) + ").");
    dataPassiveConn = true;
  }else
  //
  //  PORT - Data Port
  //
  if( ! strcmp( command, "PORT" )){
	  if (data.connected())
      data.stop();

    // get IP of data client
    dataIp[0] = atoi( parameters );
    char *p = strchr( parameters, ',' );
    for( uint8_t i = 1; i < 4; i ++ ){
      dataIp[i] = atoi( ++ p );
      p = strchr( p, ',' );
    }
    // get port of data client
    dataPort = 256 * atoi( ++ p );
    p = strchr( p, ',' );
    dataPort += atoi( ++ p );
    if( p == NULL ){
      client_println( "501 Can't interpret parameters");
    }else{
      client_println("200 PORT command successful");
      dataPassiveConn = false;
    }
  }else
  //
  //  STRU - File Structure
  //
  if( ! strcmp( command, "STRU" )){
    if( ! strcmp( parameters, "F" ))
      client_println( "200 F Ok");
    else
      client_println( "504 Only F(ile) is suported");
  }else
  //
  //  TYPE - Data Type
  //
  if( ! strcmp( command, "TYPE" )){
    if( ! strcmp( parameters, "A" ))
      client_println( "200 TYPE is now ASII");
    else if( ! strcmp( parameters, "I" ))
      client_println( "200 TYPE is now 8-bit binary");
    else
      client_println( "504 Unknow TYPE");
  }else

  ///////////////////////////////////////
  //                                   //
  //        FTP SERVICE COMMANDS       //
  //                                   //
  ///////////////////////////////////////

  //
  //  ABOR - Abort
  //
  if( ! strcmp( command, "ABOR" )){
    abortTransfer();
    client_println( "226 Data connection closed");
  }else
  //
  //  DELE - Delete a File 
  //
  if( ! strcmp( command, "DELE" )){
    char path[ FTP_CWD_SIZE ];
    if( strlen( parameters ) == 0 ){
      client_println( "501 No file name");
    }else
    if( makePath( path )){
      if( file_name[0] == '\0' || strcmp( path, file_name ) != 0 ){
        client_println( "550 File " + String(parameters) + " not found");
      }else{
        file_name[0] = '\0';
        file_buffer_size = 0;
        client_println( "250 Deleted " + String(parameters) );
        transferStatus = F_DELETED;
      }
    }
  }else
  //
  //  LIST - List 
  //
  if( ! strcmp( command, "LIST" )){
    if( ! dataConnect()){
      client_println( "425 No data connection");
    }else{
      client_println( "150 Accepted data connection");

      uint16_t nm = 0;
      if( file_name[0] != '\0' ){
        String fn(file_name);
        if( file_name[0] == '/')
    			fn.remove(0, 1);
        String dt = toDateTimeStr(0);
        data_println( dt + " " + String(file_buffer_size) + " " + fn);
        nm++;
      }
      client_println( "226 " + String(nm) + " matches total");
      data.stop();
    }
  }else
  //
  //  MLSD - Listing for Machine Processing (see RFC 3659)
  //
  if( ! strcmp( command, "MLSD" )){
    if( ! dataConnect()){
      client_println( "425 No data connection MLSD");
    }else{
  	  client_println( "150 Accepted data connection");

      uint16_t nm = 0;
      if( file_name[0] != '\0' ){
        String fn(file_name);
        if( file_name[0] == '/' )
    			fn.remove(0, 1);
        String dt = toDateTimeStr(1);
        data_println( "Type=file;Size=" + String(file_buffer_size) + ";modify=" + dt + "; " + fn);
        nm++;
      }
      client_println( "226-options: -a -l");
      client_println( "226 " + String(nm) + " matches total");
      data.stop();
    }
  }else
  //
  //  NLST - Name List 
  //
  if( ! strcmp( command, "NLST" )){
    if( ! dataConnect()){
      client_println( "425 No data connection");
    }else{
      client_println( "150 Accepted data connection");

      uint16_t nm = 0;
      if( file_name[0] != '\0' ){
        String fn(file_name);
        if( file_name[0] == '/' )
    			fn.remove(0, 1);
        data_println(fn);
        nm++;
      }
      client_println( "226 " + String(nm) + " matches total");
      data.stop();
    }
  }else
  //
  //  NOOP
  //
  if( ! strcmp( command, "NOOP" )){
    client_println( "200 Zzz...");
  }else
  //
  //  RETR - Retrieve
  //
  if( ! strcmp( command, "RETR" )){
    char path[ FTP_CWD_SIZE ];
    if( strlen( parameters ) == 0 ){
      client_println( "501 No file name");
    }else
    if( makePath( path )){
      if( file_name[0] == '\0' || strcmp( path, file_name ) != 0 ){
        client_println( "550 File " + String(parameters) + " not found");
      }else
      if( ! dataConnect()){
        client_println( "425 No data connection");
      }else{
#ifdef FTP_DEBUG
  		  Serial.println("Sending " + String(parameters));
#endif
        client_println( "150-Connected to port "+ String(dataPort));
        client_println( "150 " + String(file_buffer_size) + " bytes to download");
        millisBeginTrans = millis();
        bytesTransfered = 0;
        transferStatus = F_RETRIEVED;
      }
    }
  }else
  //
  //  STOR - Store
  //
  if( ! strcmp( command, "STOR" )){
    char path[ FTP_CWD_SIZE ];
    if( strlen( parameters ) == 0 ){
      client_println( "501 No file name");
    }else
    if( makePath( path )){
      if( ! dataConnect()){
        client_println( "425 No data connection");
      }else{
#ifdef FTP_DEBUG
        Serial.println( "Receiving " + String(parameters));
#endif
        strcpy(file_name, path);
        file_buffer_size = 0;
        client_println( "150 Connected to port " + String(dataPort));
        millisBeginTrans = millis();
        bytesTransfered = 0;
        transferStatus = F_STORED;
      }
    }
  }else
  //
  //  MKD - Make Directory
  //
  if( ! strcmp( command, "MKD" )){
	  client_println( "550 Can't create \"" + String(parameters));  //not support on espyet
  }else
  //
  //  RMD - Remove a Directory 
  //
  if( ! strcmp( command, "RMD" )){
	  client_println( "501 Can't delete \"" +String(parameters));
  }else
  //
  //  RNFR - Rename From 
  //
  if( ! strcmp( command, "RNFR" )){
    buf[ 0 ] = 0;
    if( strlen( parameters ) == 0 ){
      client_println( "501 No file name");
    }else
    if( makePath( buf )){
      if( file_name[0] == '\0' || strcmp(buf, file_name) != 0){
        client_println( "550 File " + String(parameters) + " not found");
      }else{
#ifdef FTP_DEBUG
  		  Serial.println("Renaming " + String(buf));
#endif
        client_println( "350 RNFR accepted - file exists, ready for destination");     
        rnfrCmd = true;
      }
    }
  }else
  //
  //  RNTO - Rename To 
  //
  if( ! strcmp( command, "RNTO" )){  
    char path[ FTP_CWD_SIZE ];
    if( strlen( buf ) == 0 || ! rnfrCmd )
      client_println( "503 Need RNFR before RNTO");
    else if( strlen( parameters ) == 0 )
      client_println( "501 No file name");
    else if( makePath( path )){
      if( file_name[0] != '\0' && strcmp( path, file_name ) == 0){
        client_println( "553 " + String(parameters) + " already exists");
      }else{
#ifdef FTP_DEBUG
  		  Serial.println("Renaming " + String(buf) + " to " + String(path));
#endif
        strcpy( file_name, path );
        client_println( "250 File successfully renamed or moved");
        transferStatus = F_RENAMED;
      }
    }
    rnfrCmd = false;
  }else

  ///////////////////////////////////////
  //                                   //
  //   EXTENSIONS COMMANDS (RFC 3659)  //
  //                                   //
  ///////////////////////////////////////

  //
  //  FEAT - New Features
  //
  if( ! strcmp( command, "FEAT" )){
    client_println( "211-Extensions suported:");
    client_println( " MLSD");
    client_println( "211 End.");
  }else
  //
  //  MDTM - File Modification Time (see RFC 3659)
  //
  if (!strcmp(command, "MDTM")){
    char path[ FTP_CWD_SIZE ];
    if( strlen( parameters ) == 0 ){
      client_println( "501 No file name");
    }else
    if( makePath( path )){
      if( file_name[0] == '\0' || strcmp( path, file_name ) != 0 ){
          client_println( "450 Can't open " +String(parameters) );
      }else{
        String tm = toDateTimeStr(1);
        client_println("213 " + tm);
      }
    }
  }else
  //
  //  SIZE - Size of the file
  //
  if( ! strcmp( command, "SIZE" )){
    char path[ FTP_CWD_SIZE ];
    if( strlen( parameters ) == 0 ){
      client_println( "501 No file name");
    }else
    if( makePath( path )){
      if( file_name[0] == '\0' || strcmp( path, file_name ) != 0 ){
         client_println( "450 Can't open " +String(parameters) );
      }else{
        client_println( "213 " + String(file_buffer_size));
      }
    }
  }else
  //
  //  SITE - System command
  //
  if( ! strcmp( command, "SITE" )){
    client_println( "500 Unknow SITE command " +String(parameters) );
  }else
  //
  //  Unrecognized commands ...
  //
  {
#ifdef FTP_DEBUG
    Serial.println("Unknow command: " + String(command));
#endif
    client_println( "500 Unknow command");
  }
  
  return true;
}

String FtpServer::toDateTimeStr(int type){
  if( type == 0 ){
    char datetime_str[19];
    sprintf(datetime_str, "%02d-%02d-%04d %02d:%02d%s", file_timeInfo.tm_mon + 1, file_timeInfo.tm_mday, file_timeInfo.tm_year + 1900, file_timeInfo.tm_hour % 12, file_timeInfo.tm_min, file_timeInfo.tm_hour >= 12 ? "PM" : "AM" );
    return String(datetime_str);
  }else if( type == 1){
    char datetime_str[15];
    sprintf(datetime_str, "%04d%02d%02d%02d%02d%02d", file_timeInfo.tm_year + 1900, file_timeInfo.tm_mon + 1, file_timeInfo.tm_mday, file_timeInfo.tm_hour, file_timeInfo.tm_min, file_timeInfo.tm_sec);
    return String(datetime_str);
  }else{
    return String("");
  }
}

boolean FtpServer::dataConnect(){
  unsigned long startTime = millis();
  //wait 5 seconds for a data connection
  if (!data.connected()){
    while (!dataServer.hasClient() && millis() - startTime < 10000){
		  yield();
	  }

    if (dataServer.hasClient()) {
		  data.stop();
		  data = dataServer.available();
#ifdef FTP_DEBUG
      Serial.println("ftpdataserver client....");
#endif
	  }
  }

  return data.connected();
}

boolean FtpServer::doRetrieve(){
#ifdef FTP_DEBUG
  Serial.println("doRetrieve()");
#endif
  data.write(file_buffer, file_buffer_size);
  bytesTransfered += file_buffer_size;
  closeTransfer();

  return false;
}

boolean FtpServer::doStore(){
#ifdef FTP_DEBUG
  Serial.println("doStore()");
#endif
  if( data.connected() ){
    int16_t nb = data.readBytes((uint8_t*) buf, FTP_BUF_SIZE );
    if( nb > 0 ){
      if( file_buffer_size + nb <= file_buffer_length ){
        memmove(&file_buffer[file_buffer_size], buf, nb);
        file_buffer_size += nb;
        bytesTransfered += nb;
        return true;
      }else{
        Serial.println("File buffer size overflow");
        closeTransfer();
        return false;
      }
    }
  }

  getLocalTime(&file_timeInfo);
  closeTransfer();

  return false;
}

void FtpServer::closeTransfer(){
#ifdef FTP_DEBUG
  Serial.println("closeTransfer()");
#endif
  uint32_t deltaT = (int32_t) ( millis() - millisBeginTrans );
  if( deltaT > 0 && bytesTransfered > 0 ){
    client_println( "226-File successfully transferred");
    client_println( "226 " + String(deltaT) + " ms, "+ String(bytesTransfered / deltaT) + " kbytes/s");
  }else{
    client_println( "226 File successfully transferred");
  }
  
  data.stop();
}

void FtpServer::abortTransfer(){
  if( transferStatus > F_IDLE ){
    data.stop(); 
    client_println( "426 Transfer aborted"  );
#ifdef FTP_DEBUG
    Serial.println( "Transfer aborted!") ;
#endif
  }

  transferStatus = F_IDLE;
}

// Read a char from client connected to ftp server
//
//  update cmdLine and command buffers, iCL and parameters pointers
//
//  return:
//    -2 if buffer cmdLine is full
//    -1 if line not completed
//     0 if empty line received
//    length of cmdLine (positive) if no empty line received 

int8_t FtpServer::readChar(){
  int8_t rc = -1;

  if( client.available()){
    char c = client.read();
#ifdef FTP_DEBUG
    Serial.print( c);
#endif
    if( c == '\\' )
      c = '/';

    if( c != '\r' ){
      if( c != '\n' ){
        if( iCL < FTP_CMD_SIZE )
          cmdLine[ iCL ++ ] = c;
        else
          rc = -2; //  Line too long
      }else{
        cmdLine[ iCL ] = 0;
        command[ 0 ] = 0;
        parameters = NULL;
        // empty line?
        if( iCL == 0 ){
          rc = 0;
        }else{
          rc = iCL;
          // search for space between command and parameters
          parameters = strchr( cmdLine, ' ' );
          if( parameters != NULL ){
            if( parameters - cmdLine > 4 ){
              rc = -2; // Syntax error
            }else{
              strncpy( command, cmdLine, parameters - cmdLine );
              command[ parameters - cmdLine ] = 0;
              
              while( * ( ++ parameters ) == ' ' )
                ;
            }
          }else
          if( strlen( cmdLine ) > 4 )
            rc = -2; // Syntax error.
          else
            strcpy( command, cmdLine );
          iCL = 0;
        }
      }
    }
    if( rc > 0 ){
      for( uint8_t i = 0 ; i < strlen( command ); i ++ )
        command[ i ] = toupper( command[ i ] );
    }
    if( rc == -2 ){
      iCL = 0;
      client_println( "500 Syntax error");
    }
  }

  return rc;
}

// Make complete path/name from cwdName and parameters
//
// 3 possible cases: parameters can be absolute path, relative path or only the name
//
// parameters:
//   fullName : where to store the path/name
//
// return:
//    true, if done

boolean FtpServer::makePath( char * fullName ){
  return makePath( fullName, parameters );
}

boolean FtpServer::makePath( char * fullName, char * param ){
  if( param == NULL )
    param = parameters;
    
  // Root or empty?
  if( strcmp( param, "/" ) == 0 || strlen( param ) == 0 ){
    strcpy( fullName, "/" );
    return true;
  }

  // If relative path, concatenate with current dir
  if( param[0] != '/' ){
    strcpy( fullName, cwdName );
    if( fullName[ strlen( fullName ) - 1 ] != '/' )
      strncat( fullName, "/", FTP_CWD_SIZE );
    strncat( fullName, param, FTP_CWD_SIZE );
  }else{
    strcpy( fullName, param );
  }

  // If ends with '/', remove it
  uint16_t strl = strlen( fullName ) - 1;
  if( fullName[ strl ] == '/' && strl > 1 )
    fullName[ strl ] = 0;
  if( strlen( fullName ) < FTP_CWD_SIZE )
    return true;

  client_println( "500 Command line too long");

  return false;
}
