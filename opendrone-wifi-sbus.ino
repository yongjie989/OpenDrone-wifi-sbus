/* @file               OpenDrone WiFi to SBUS for FC-01
 * @technical support  Ethan Huang <ethan@robotlab.tw>
 * @version            V1.0.1
 * @date               2018/05/08
 * 
 *
OpenDrone-RC-WiFi (Server side)
    //100 -> 942
    //120 -> 955
    //130 -> 961
    //140 -> 967
    //150 -> 973
    //160 -> 980
    //170 -> 986
    //180 -> 992
    //190 -> 998
    //192 -> 1000
    //194 -> 1001
    //196 -> 1002
    //198 -> 1003
    //200 -> 1005
    //210 -> 1011
    //500 -> 1192
    //600 -> 1255
    //700 -> 1317
    //800 -> 1380
    //900 -> 1442
    //992 -> 1500
    //994 -> 1501
    //996 -> 1502
    //998 -> 1503
    //1000 -> 1505
    //1555 -> 1851
    //1790 -> 1998
    //1793 -> 2000
    //1794 -> 2001
    //1795 -> 2001
    //1800 -> 2005
    //1900 -> 2067
*/
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h> 
#include <WiFiUdp.h>
#include <EEPROM.h>
#include <stdio.h>
#include <string.h>

WiFiUDP Udp;
int rc_package_length = 45;
unsigned int localUdpPort = 6666;
char incomingPacket[255];
char packetBuffer[45];
char *start_bit = "@$";
int change_ssid_flag = 0;
String factory_ssid = "OD1234";
char* ssid_password = "12345678";

unsigned int SBUS_Channel_Data[18];
byte SBUS_Current_Channel = 0;
byte SBUS_Current_Channel_Bit = 0;
byte SBUS_Current_Packet_Bit = 0;
byte SBUS_Packet_Data[25];
byte SBUS_Packet_Position = 0;
byte SBUS_Failsafe_Active = 0;
byte SBUS_Lost_Frame = 0;
char eRead[25];
byte len;
const char* ssid;
String tempR;

void SaveString(int startAt, const char* id);
void ReadString(byte startAt, byte bufor);
void returnFail(String msg);
void handleSubmit(void);
void handleRoot(void);
void sbusPackageInit(void);
void SBUS_Build_Packet(void);


void SaveString(int startAt, const char* id)
{
  for (byte i = 0; i <= strlen(id); i++)
  {
    EEPROM.write(i + startAt, (uint8_t) id[i]);
  }
  EEPROM.commit();
}

void ReadString(byte startAt, byte bufor)
{
  for (byte i = 0; i <= bufor; i++)
  {
    eRead[i] = (char)EEPROM.read(i + startAt);
  }
  len = bufor;
}
  
ESP8266WebServer server_AP(9090);

const char save_html[] =
  "<html>"
  "    <head>"
  "        <meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\">"
  "        <title>OpenDrone FC-01 WiFi configuration</title>"
  "    </head>"
  "    <body>"
  "     <div>已儲存! 請重新啟動電源，並以您命名的WiFi SSID進行使用。</div>"
  "    </body>"
  "</html>";

void returnFail(String msg)
{
  server_AP.sendHeader("Connection", "close");
  server_AP.sendHeader("Access-Control-Allow-Origin", "*");
  server_AP.send(500, "text/plain", msg + "\r\n");
}

void handleSubmit(void)
{
  String new_ssid = "";
  if (!server_AP.hasArg("ssid")) 
    return returnFail("請輸入SSID.");

  String data1 = server_AP.arg("ssid");
  new_ssid = const_cast<char*>(data1.c_str());

  SaveString(5, new_ssid.c_str()); 
  Serial.write("UPDATE SSID\n");
  Serial.write(new_ssid.c_str());
  
  server_AP.send(200, "text/html", save_html);

}

void handleRoot(void)
{
  ReadString(5, 10);  
  tempR = "";
  for (byte i = 0; i < len; i++)
  {
    tempR += eRead[i];
  }
  ssid = tempR.c_str();
  char html[1024];
  
  const char index_html[] =
  "<html>"
  "    <head>"
  "        <meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\">"
  "        <title>OpenDrone FC-01 WiFi configuration</title>"
  "    </head>"
  "    <body>"
  "        <form action=\"/\" method=\"post\">"
  "            <h1>給予飛控板一個WiFi SSID</h1>"
  "            SSID <input type=\"text\" name=\"ssid\" value=\"%s\" /><br/>"
  "            <input type=\"submit\" value=\"Save\" />"
  "        </form>"
  "    </body>"
  "</html>";
  snprintf_P(html, sizeof(html), index_html, ssid);
    
  if (server_AP.hasArg("ssid")) {
    handleSubmit();
  }else{
    server_AP.send(200, "text/html", html);
  }
}

void sbusPackageInit(void)
{
  SBUS_Channel_Data[0] = 512;
  SBUS_Channel_Data[1] = 512;
  SBUS_Channel_Data[2] = 512;
  SBUS_Channel_Data[3] = 512;
  SBUS_Channel_Data[4] = 512;
  SBUS_Channel_Data[5] = 512;
  SBUS_Channel_Data[6] = 512;
  SBUS_Channel_Data[7] = 512;
  SBUS_Channel_Data[8] = 0;
  SBUS_Channel_Data[9] = 0;
  SBUS_Channel_Data[10] = 0;
  SBUS_Channel_Data[11] = 0;
  SBUS_Channel_Data[12] = 0;
  SBUS_Channel_Data[13] = 1024;
  SBUS_Channel_Data[14] = 1024;
  SBUS_Channel_Data[15] = 0;
  SBUS_Channel_Data[16] = 0;
  SBUS_Channel_Data[17] = 0;
  SBUS_Failsafe_Active = 0;
  SBUS_Lost_Frame = 0;
}

void SBUS_Build_Packet(void)
{
  for (SBUS_Packet_Position = 0; SBUS_Packet_Position < 25; SBUS_Packet_Position++) 
    SBUS_Packet_Data[SBUS_Packet_Position] = 0x00; 
  SBUS_Current_Packet_Bit = 0;
  SBUS_Packet_Position = 0;
  SBUS_Packet_Data[SBUS_Packet_Position] = 0x0F;
  SBUS_Packet_Position++;

  for (SBUS_Current_Channel = 0; SBUS_Current_Channel < 16; SBUS_Current_Channel++)
  {
    for (SBUS_Current_Channel_Bit = 0; SBUS_Current_Channel_Bit < 11; SBUS_Current_Channel_Bit++)
    {
      if (SBUS_Current_Packet_Bit > 7)
      {
        SBUS_Current_Packet_Bit = 0;
        SBUS_Packet_Position++;
      }
      SBUS_Packet_Data[SBUS_Packet_Position] |= (((SBUS_Channel_Data[SBUS_Current_Channel] >> SBUS_Current_Channel_Bit) & 0x01) << SBUS_Current_Packet_Bit);
      SBUS_Current_Packet_Bit++;
    }
  }
  if (SBUS_Channel_Data[16] > 1023)
    SBUS_Packet_Data[23] |= (1 << 0);
  if (SBUS_Channel_Data[17] > 1023)
    SBUS_Packet_Data[23] |= (1 << 1);
  if (SBUS_Lost_Frame != 0)
    SBUS_Packet_Data[23] |= (1 << 2);
  if (SBUS_Failsafe_Active != 0)
    SBUS_Packet_Data[23] |= (1 << 3);
    
  SBUS_Packet_Data[24] = 0x00;
}

void setup()
{
 
  EEPROM.begin(512);
  //Serial.begin(115200);
  Serial.setTimeout(0);
  Serial.begin(100000, SERIAL_8E2);
  delay(2000);
  sbusPackageInit();
  
  ReadString(5, 10);  
  tempR = "";
  for (byte i = 0; i < len; i++)
  {
    tempR += eRead[i];
  }
  ssid = tempR.c_str();
  
  if(ssid == "")
  {
    Serial.println(">ssid==null");
    SaveString(5, factory_ssid.c_str());
    ReadString(5, 10);
    tempR = "";
    for (byte i = 0; i < len; i++)
    {
      tempR += eRead[i];
    }
    ssid = tempR.c_str();
  }
   
  Serial.println("ssid=");
  Serial.println(ssid);
  
  WiFi.mode(WIFI_AP);
  Udp.begin(localUdpPort);
  WiFi.softAP(ssid, ssid_password);

  server_AP.on("/", handleRoot);
  server_AP.begin();
}

void loop()
{  
  int i = 0;
  int result;
  char *pch;
  char *packets[10];

  server_AP.handleClient();
  
  int packetSize = Udp.parsePacket();
  if (packetSize)
  {
    Udp.read(packetBuffer, rc_package_length);
    //Serial.println(packetBuffer); 
    pch = strtok (packetBuffer,"|");
    while (pch != NULL)
    {
      packets[i] = pch;
      //Serial.printf("packets[%d] = %s\n",i ,packets[i]);
      pch = strtok (NULL, "|");
      i++;
    }
    Serial.printf("packets[5] = %s ",packets[5]);
    Serial.printf("packets[6] = %s ",packets[6]);
    Serial.printf("packets[7] = %s\n",packets[7]);
    
    result = strcmp(packets[0], start_bit);
    if(result == 0)
    {
      //Serial.printf("result=%d\n", result);
      int roll = atoi( packets[1] );
      int pitch = atoi( packets[2] );
      int throttle = atoi( packets[3] );
      int yaw = atoi( packets[4] );
      
     
      roll = map(roll, 0, 4095, 192, 1793);
      pitch = map(pitch, 0, 4095, 192, 1793);
      throttle = map(throttle, 0, 4095, 1793, 192);
      yaw = map(yaw, 0, 4095, 1793, 192);
      
      //Serial.printf("roll=%d pitch=%d throttle=%d yaw=%d\n", roll,pitch,throttle,yaw);

      SBUS_Channel_Data[0] = roll;
      SBUS_Channel_Data[1] = pitch;
      SBUS_Channel_Data[2] = throttle;
      SBUS_Channel_Data[3] = yaw;

      if( strcmp(packets[5], "AA") == 0 )
      {
        SBUS_Channel_Data[6] = 1500;    //AUX3
      }
      if( strcmp(packets[5], "00") == 0 )
      {
        SBUS_Channel_Data[6] = 192;
      }
      SBUS_Channel_Data[4] = 992;    //AUX1
      SBUS_Channel_Data[5] = 992;    //AUX2
      SBUS_Channel_Data[7] = 992;    //AUX4
      SBUS_Channel_Data[8] = 0;
      SBUS_Channel_Data[9] = 0;
      SBUS_Channel_Data[10] = 0;
      SBUS_Channel_Data[11] = 0;
      SBUS_Channel_Data[12] = 0;
      SBUS_Channel_Data[13] = 992;
      SBUS_Channel_Data[14] = 992;
      SBUS_Channel_Data[15] = 0;
      SBUS_Channel_Data[16] = 0;
      SBUS_Channel_Data[17] = 0;
      SBUS_Failsafe_Active = 0;
      SBUS_Lost_Frame = 0;
  
      SBUS_Build_Packet();
      
      Serial.write(SBUS_Packet_Data, 25);

      // into RC matching mode.
      if( strcmp(packets[5], "00") == 0 && strcmp(packets[6], "AA") == 0 && change_ssid_flag == 0 )
      {
        change_ssid_flag = 1;
        //save ssid into EEPROM address at 5.
        delay(2000);
        String new_ssid = packets[7];
        SaveString(5, new_ssid.c_str());
        
        WiFi.softAPdisconnect(true);
        WiFi.softAP(new_ssid.c_str(), ssid_password); 
        delay(1000);
        Serial.println("changed SSID.");
        
        
      }
    }

    delay(7);
  }
  yield();
  
}

