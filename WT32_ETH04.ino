// Board: Esp 32 Dev Modul geht!
// 010122 Alle Min die Zeit senden Phys. Adresse in ETS eingetragen und übernommen
// 111021 Beep added via 2/7/101
// Mqtt ip Adresse aus Conf laden..
// Debug auf Serial Ausgabe
//String raus qwq
// IP Konnect klappt nur mit WebServer_WT32_ETH01 v1.3.0 for core v1.0.6-
// Die Tag/Nacht info muß extern gebildet werden, Sonnenaufgang/Untergang kommen vom IP Interface
#define USING_CORE_ESP32_CORE_V200_PLUS      false

#define DEBUG_ETHERNET_WEBSERVER_PORT       Serial
// Debug Level from 0 to 4
#define _ETHERNET_WEBSERVER_LOGLEVEL_       0

#include <KnxDevice.h>
// #include <KnxTelegram.h>
#include "knx.h"
#include <WebServer_WT32_ETH01.h>
#include <PubSubClient.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

// ################################################
// ### KNX DEVICE CONFIGURATION
// ################################################
uint16_t individualAddress = P_ADDR(1, 2, 120);

// ################################################
// ### BOARD CONFIGURATION
// ################################################

#define KNX_SERIAL Serial2   // Hack in TPUART.cpp required
#define DEBUGSERIAL Serial // the 2nd serial port with TX only (GPIO2/D4) 115200 baud

// ################################################
// ### DEBUG Configuration
// ################################################
// #define KDEBUG // comment this line to disable DEBUG mode

#ifdef KDEBUG
#include <DebugUtil.h>
#endif

String traces;
void TracesDisplay() { Serial.print(traces); traces=""; }
void PrintTelegramInfo(KnxTelegram& tg) { traces = " => Info() :\n"; tg.Info(traces); TracesDisplay(); }

// ################################################
// ### IP DEVICE CONFIGURATION
// ################################################

IPAddress mqttServer(192, 168, 1, 48);
// Select the IP address according to your local network
IPAddress myIP(192, 168, 1, 112);
IPAddress myGW(192, 168, 1, 1);
IPAddress mySN(255, 255, 255, 0);
// Google DNS Server IP
IPAddress myDNS(8, 8, 8, 8);

bool eth_connected = false;
int reqCount = 0;                // number of requests received
/////////////////////////////// Knx GA's relevant for server //////////////////////////////////
//Word type is 4 byte on ESP32 -> use uint16 instead!
struct knx_data_in {
  uint16_t R1_hoch_runter  = G_ADDR(5, 2, 12);
  uint16_t R1_stop         = G_ADDR(5, 2, 13);
  uint16_t R1_sperre       = G_ADDR(5, 2, 14);
  uint16_t Datum           = G_ADDR(2, 5, 16);
  uint16_t Zeit            = G_ADDR(2, 5, 17);
  uint16_t Beep            = G_ADDR(2, 7, 101);
  uint16_t Tag_Nacht       = G_ADDR(2, 5, 15); // Nacht = 1 Tag = 0
  uint16_t GMonitor        = 0x0001;
};
knx_data_in Knx_In;
struct knx_data_out {
  uint16_t R1_status     = G_ADDR(5, 2, 1);
  uint16_t F_kontakt     = G_ADDR(5, 2, 2);
  uint16_t Luftdruck     = G_ADDR(2, 5, 101);
  uint16_t Li1           = G_ADDR(5, 1, 1);
  uint16_t Li2           = G_ADDR(5, 1, 2);
  uint16_t Li3           = G_ADDR(5, 1, 3);
  uint16_t Li4           = G_ADDR(5, 1, 4);
  uint16_t Uhrzeit       = G_ADDR(1, 7, 5);
};
knx_data_out Knx_Out;

// Definition of the Communication Objects attached to the device
KnxComObject KnxDevice::_comObjectsList[] = {
  
  /* Index 0 */ KnxComObject( Knx_In.GMonitor,       KNX_DPT_1_001 , COM_OBJ_LOGIC_IN), // all not adressed GA's will be mapped to this index
  /* Index 1 */ KnxComObject( Knx_In.R1_hoch_runter, KNX_DPT_1_001 , COM_OBJ_LOGIC_IN),
  /* Index 2 */ KnxComObject( Knx_In.R1_stop,        KNX_DPT_1_001 , COM_OBJ_LOGIC_IN),
  /* Index 3 */ KnxComObject( Knx_In.R1_sperre,      KNX_DPT_1_001 , COM_OBJ_LOGIC_IN),
  /* Index 4 */ KnxComObject( Knx_In.Datum,          KNX_DPT_11_001, COM_OBJ_LOGIC_IN),
  /* Index 5 */ KnxComObject( Knx_In.Zeit,           KNX_DPT_10_001, COM_OBJ_LOGIC_IN),
  /* Index 6 */ KnxComObject( Knx_In.Beep,           KNX_DPT_5_005 , COM_OBJ_LOGIC_IN),
  /* Index 7 */ KnxComObject( Knx_In.Tag_Nacht,      KNX_DPT_1_001 , COM_OBJ_LOGIC_IN),
  /* Index 8 */ KnxComObject(Knx_Out.R1_status, KNX_DPT_1_001, COM_OBJ_SENSOR),
  /* Index 9 */ KnxComObject(Knx_Out.F_kontakt, KNX_DPT_1_001, COM_OBJ_SENSOR),
  /* Index 10*/ KnxComObject(Knx_Out.Luftdruck, KNX_DPT_8_001, COM_OBJ_SENSOR),
  /* Index 11*/ KnxComObject(Knx_Out.Li1,       KNX_DPT_1_001, COM_OBJ_SENSOR),
  /* Index 12*/ KnxComObject(Knx_Out.Li2,       KNX_DPT_1_001, COM_OBJ_SENSOR),
  /* Index 13*/ KnxComObject(Knx_Out.Li3,       KNX_DPT_1_001, COM_OBJ_SENSOR),
  /* Index 14*/ KnxComObject(Knx_Out.Li4,       KNX_DPT_1_001, COM_OBJ_SENSOR),
  /* Index 15*/ KnxComObject(Knx_Out.Uhrzeit,   KNX_DPT_10_001,COM_OBJ_SENSOR),  
};
const byte KnxDevice::_comObjectsNb = sizeof(_comObjectsList) / sizeof(KnxComObject); // do no change this code
// #define Anzahl_objekte =  sizeof(KnxDevice::_comObjectsList) / sizeof(KnxComObject); // do no change this code
WebServer server(80);
WebConfig conf;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);
WiFiClient    ethClient;
PubSubClient    client(mqttServer, 1883, mqtt_callback, ethClient);

char mqtt_buffer[30][500];

void mqtt_callback(char* topic, byte* payload, unsigned int length)
{
  (length > 499) ? length = 499 : length;
  strcpy(mqtt_buffer[Mqtt_in_zeiger], topic);
  strcat(mqtt_buffer[Mqtt_in_zeiger], ": ");
  strncat(mqtt_buffer[Mqtt_in_zeiger], (char *)payload, length); // Payload auf globale Var kopieren
  if (Mqtt_in_zeiger < 29) Mqtt_in_zeiger++;
}
void reconnect()  //mqtt connection
{
  // Loop until we're reconnected
  uint8_t reconnect_cnt;
  while (!client.connected())
  {
    Serial.print("Attempting MQTT connection to ");
    Serial.print(mqttServer);

    // Attempt to connect
    // if (client.connect(ID, "try", "try"))
    if (client.connect(ID))
    {
      Serial.println("...connected");

      // Once connected, publish an announcement...
      char zahl[10];
      if (buffer_in_pnt < 24)snprintf(buffer[buffer_in_pnt++], 20, "Mqtt online ;-)");
      // client.publish(TOPIC, buffer);

      //Serial.println("Published connection message successfully!");
      //Serial.print("Subcribed to: ");
      //Serial.println(subTopic);

      // This is a workaround to address https://github.com/OPEnSLab-OSU/SSLClient/issues/9
      //ethClientSSL.flush();
      // ... and resubscribe
      // client.subscribe(subTopic);
      client.subscribe("rasp4_2GB/+");
      client.subscribe("waveshare/+");
      client.subscribe("knx/sensor/+");
      client.subscribe("explorer700/+");
      client.subscribe("epaper/+");
      client.subscribe("news/+");
      client.subscribe("Aussen/+");
      // for loopback testing
      // client.subscribe(TOPIC);
      // This is a workaround to address https://github.com/OPEnSLab-OSU/SSLClient/issues/9
      //ethClientSSL.flush();
    }
    else
    {
      Serial.print("...failed, rc=");
      Serial.print(client.state());
      Serial.println("Mqtt error value: ");
      // Serial.print(client.returnCode());
      Serial.println(" try again in 5 seconds");
      Serial.println(reconnect_cnt);

      // Wait 5 seconds before retrying
      reconnect_cnt ++;
      delay(1000);
    }
    delay(1);
  }
}


void handleRoot() {
  //server.send(200, "text/html", "<h1>You are connected</h1>"); // For testing
  conf.handleFormRequest(&server);
}

void handleNotFound()
{
  String message = F("File Not Found\n\n");

  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? F("GET") : F("POST");
  message += "\nArguments: ";
  message += server.args();
  message += "\n";

  for (uint8_t i = 0; i < server.args(); i++)
  {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }

  server.send(404, F("text/plain"), message);
}
// Calculate Group Adress from String stored in SPIFFS
word Extract_G_Addr(String ga_name) {
  int first = ga_name.indexOf('/');
  int last  = ga_name.lastIndexOf('/');
  // Zwei / in GA vorhanden?
  if ((first != (-1)) && (last != (-1)))
  {
    area  = (ga_name.substring(0, first).toInt()) & 0x1f;
    linie = (ga_name.substring(first + 1, last).toInt()) & 7;
    grp   = ga_name.substring(last + 1).toInt();
    ga = (area * 2048) + (linie * 256) + grp;
  }
  else
  {
    // Default 1/1/1
    ga = 0x0901;
  }
  return (ga);
}

// Calculate physical Adress from String stored in SPIFFS
word Extract_P_Addr(String pa_name) {
  int first = pa_name.indexOf('.');
  int last  = pa_name.lastIndexOf('.');
  // Zwei . in PA vorhanden?
  if ((first != (-1)) && (last != (-1)))
  {
    area  = (pa_name.substring(0, first).toInt()) & 0xf;
    linie = (pa_name.substring(first + 1, last).toInt()) & 0xf;
    grp   = pa_name.substring(last + 1).toInt();
    pa = (area * 4096) + (linie * 256) + grp;
  }
  else
  {
    // Default 1.1.1
    pa = 0x1101;
  }
  return (pa);
}

// Calculate IP Adress from String stored in SPIFFS
word Extract_Value(String ga_name) {
  int first = ga_name.indexOf(": ");
  // int last  = ga_name.lastIndexOf('.');
  int last  = ga_name.indexOf(".");
  // Zwei / in GA vorhanden?
  if ((first != (-1)) && (last != (-1)))
  {
    Value_big  = (ga_name.substring(first, last).toInt());
    Serial.print("Value_big: "); Serial.println(Value_big);
    Value_small   = ga_name.substring(last + 1, last + 3).toInt();
    ga = (Value_big * 100) + (Value_small);
    Serial.print("Value_small: "); Serial.println(Value_small);
  }
  else
  {
    // Default 1/1/1
    ga = 0;
  }
  return (ga);
}

void WiFiEvent(WiFiEvent_t event)
{
  switch (event)
  {
    case SYSTEM_EVENT_ETH_START:
      Serial.println("\nETH Started");
      //set eth hostname here
      ETH.setHostname("WT32-ETH01");
      break;
    case SYSTEM_EVENT_ETH_CONNECTED:
      Serial.println("ETH Connected");
      break;

    case SYSTEM_EVENT_ETH_GOT_IP:
      if (!eth_connected)
      {
        Serial.print("ETH MAC: ");
        Serial.print(ETH.macAddress());
        Serial.print(", IPv4: ");
        Serial.print(ETH.localIP());

        if (ETH.fullDuplex())
        {
          Serial.print(", FULL_DUPLEX");
        }

        Serial.print(", ");
        Serial.print(ETH.linkSpeed());
        Serial.println("Mbps");
        eth_connected = true;
      }

      break;

    case SYSTEM_EVENT_ETH_DISCONNECTED:
      Serial.println("ETH Disconnected");
      eth_connected = false;
      break;

    case SYSTEM_EVENT_ETH_STOP:
      Serial.println("\nETH Stopped");
      eth_connected = false;
      break;

    default:
      break;
  }
}

// ################################################
// ### KNX EVENT CALLBACK
// ################################################

void knxEvents(byte index) { // avoid long execution times like String calculations
  // Empfangsobjekte Input
  // Serial.print("Knx Botschaft empfangen: "); Serial.println(index);
  if (index)
  {
    if (buffer_in_pnt < 24) snprintf(buffer[buffer_in_pnt++], 20, "Msg %d", index);
  }

  switch (index) {
    case 1: // object index has been updated
      Knx.read(index,Rolladen_Status);
      if (buffer_in_pnt < 24) snprintf(buffer[buffer_in_pnt++], 20, "Index 1 Wert: %2d",Rolladen_Status ); 
      break;
    case 4: // Datum von Knx lesen an Mqtt schicken
      {
        byte m[3]; Knx.read(index, m); // read index 4 3 bytes for Timeformat
        if (buffer_in_pnt < 24) snprintf(buffer[buffer_in_pnt++], 20, "Datum:%02d.%02d.%02d", m[0], m[1], m[2]); //geht ab 2100 nicht mehr..
      }
      break;
    case 5: //Zeit von Knx lesen an Mqtt schicken
      {
        byte m[3]; Knx.read(index, m); // read index 4 3 bytes for Timeformat
        m[0] &= 0x1F; // Tag rauslöschen..
        if (buffer_in_pnt < 24) snprintf(buffer[buffer_in_pnt++], 20, "Zeit: %2d:%2d:%2d", m[0], m[1], m[2]); //geht ab 2100 nicht mehr..
      }
      break;
    case 6: //Beep
      {
        // client.publish(TOPIC, "Beep");
        pinMode(Beep_out, OUTPUT); // Workaround Timer1..
        Knx.read(index, Isr_tone_duration);
        timerAlarmEnable(timer1);
        break;
      }
    case 7: // object index has been updated
      {
        // Status_Tag_Nacht = Knx.read(index);
        Knx.read(index, Status_Tag_Nacht);
        //Serial.print("from Callback: Status_Tag_Nacht: "); Serial.println(Status_Tag_Nacht);
        if (buffer_in_pnt < 24) {
          if  (!Status_Tag_Nacht)
            snprintf(buffer[buffer_in_pnt++], 20, "Tag erkannt.");
          else snprintf(buffer[buffer_in_pnt++], 20, "Nacht erkannt.");
        }
        break;
      }
    case 0: // KnxDevice._comObjectsNb: // Gruppenmonitor Objekt
      {
        byte m[3] = {0,0,0}; // Knx.read(index, m); // read any type ! index 4 3 bytes for Timeformat]
        byte length = (Ga_data[5] & 0x1F);
        // Korrektur 1. Byte..
        if (length == 1) Knx.read(index, Ga_data[8]);
        // Serial.print("Ga_int: "); Serial.println(buffer[buffer_in_pnt-1]);
        if ((length == 1)||(length == 2)){if (buffer_in_pnt < 24)snprintf(buffer[buffer_in_pnt++], 20, "GA: %d/%d/%d:%02X", (Ga_int >> 11), ((Ga_int >> 8) & 0x7), (Ga_int & 0xff), Ga_data[8]);} 
        if (length == 3) {if (buffer_in_pnt < 24)snprintf(buffer[buffer_in_pnt++], 30, "GA: %d/%d/%d:%02X:%02X", (Ga_int >> 11), ((Ga_int >> 8) & 0x7), (Ga_int & 0xff), Ga_data[8], Ga_data[9]);}
        if (length == 4) {if (buffer_in_pnt < 24)snprintf(buffer[buffer_in_pnt++], 30, "GA: %d/%d/%d:%02X:%02X:%02X", (Ga_int >> 11), ((Ga_int >> 8) & 0x7), (Ga_int & 0xff), Ga_data[8], Ga_data[9],Ga_data[10]);}
        if (length == 5) {if (buffer_in_pnt < 24)snprintf(buffer[buffer_in_pnt++], 40,"Ga_Data: %02X:%02X:%02X:%02X", Ga_data[8],Ga_data[9],Ga_data[10],Ga_data[11]);} 
          Ga_data[10] = 0;
          Ga_data[11] = 0;
          Ga_data[8] = 0;
          Ga_data[9] = 0;
        break;
      }

    default:
      //DEBUGSERIAL.println("Uninteressantes Objekt empfangen");
      break;
  }
  #if 0
  if ((index != 5)&&(index != 0)) { // Bei jeder empfangenen Botschaft ein kurzer Ton
    Isr_tone_duration = 5;
    pinMode(Beep_out, OUTPUT);
    timerAlarmEnable(timer1);
  }
  #endif
}

void IRAM_ATTR ISR_tone()
{
  if (Isr_tone_duration)
  {
    Isr_tone_duration--;
    Isr_tone_value = !Isr_tone_value; //toggle
    digitalWrite(Beep_out, Isr_tone_value);
  }
  else
  {
    timerAlarmDisable(timer1);
    pinMode(Beep_out, INPUT); // Workaround Timer1..otherwise tone does not stop
  }
}

void Call_Task1(void *parameter)
{
  uint32_t Task1_cnt;
  Serial.begin(115200);
  ETH.begin(ETH_PHY_ADDR, ETH_PHY_POWER);
  ETH.config(myIP, myGW, mySN, myDNS);
  WiFi.onEvent(WiFiEvent);
  while (!eth_connected) { // Es gibt oft Probleme mit der Verbindung
    Serial.print("*");
    delay(100);
  }
  /////////////////////////////////////////////////////////////////////////////

  Serial.print("\nStarting AdvancedWebServer on " + String(ARDUINO_BOARD));
  Serial.println(" with " + String(SHIELD_TYPE));
  Serial.println(WEBSERVER_WT32_ETH01_VERSION);
  server.on("/", handleRoot);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.print("HTTP EthernetWebServer is @ IP : ");
  Serial.println(ETH.localIP());
  client.setServer(mqttServer, 1883);
  client.setCallback(mqtt_callback);
  Serial.println("mqtt Init");
  Serial.println(params);
  conf.setDescription(params);
  conf.readConfig();
  timeClient.begin();
  timeClient.setTimeOffset(3600); // Sommer/ Winterzeit fehlt..
  Serial.println("Setup done");
  Serial.print("Mqtt_IP_Adress: "); Serial.println(Mqtt_IP_Adress);

  // Loop() ///////////////////////////////////////////////////////////////
  for (;;) {
    // Code for task 1 - infinite loop
    Task1_cnt ++;
    // Web Server
    // mqtt part
    if (!client.connected()) reconnect();
    client.loop();
    server.handleClient();
    // Time Server
    timeClient.update();
    // Lade Strings zur allgemeinen Verwendung

    //Get a time structure
    unsigned long epochTime = timeClient.getEpochTime();
    struct tm *ptm = gmtime ((time_t *)&epochTime);
    int currentMonth = ptm->tm_mon;

#if 1
    now = millis();
    if (now - lastMsg > MQTT_PUBLISH_INTERVAL_MS)
    {
      lastMsg = now;
      // client.publish(TOPIC, pubData);
      client.publish(TIME, months[currentMonth]);
      client.publish("Tag", weekDays[timeClient.getDay()]);
    }
#endif
    last_minutes = minutes;
    minutes = timeClient.getMinutes();
    if (minutes != last_minutes){
      //snprintf(time_string, 20, "%d:%d:%d", timeClient.getHours(), minutes, timeClient.getSeconds());
      Stunde =  timeClient.getHours();
      Minute = minutes;
      Sekunde = timeClient.getSeconds();
      Tag = timeClient.getDay();
      new_minutes = 1;
      // client.publish(TOPIC,"Neue Minute");
    }
    delay(1); // damit wird die Kontrolle an den Scheduler zurück gegeben, ggf wird Idle Task aufgerufen -> avoid WDT
  }
}

void setup()
{
  // disableCore0WDT();
  // Serial.begin(115200); init in Core 0 
#if 1
  //////////////////////////////////////////////////////////////////////////
  // Task1 auf Core 0 anlegen //////////////////////////////////////////////
  //////////////////////////////////////////////////////////////////////////
  xTaskCreatePinnedToCore(
    Call_Task1, /* Task function. */
    "Task_1",   /* name of task. */
    10000,       /* Stack size of task */
    NULL,       /* parameter of the task */
    1,          /* priority of the task */
    &Task1,     /* Task handle to keep track of created task */
    0);         /* Core */
#endif
  //////////////////////////////////////////////////////////////////////////
#ifdef ESP32
  WiFi.disconnect(true); //wird Wifi noch gebraucht? ETH verwendet Wifi Befehle..
  WiFi.mode(WIFI_OFF);

#else
  #warning "Sketch can not run on ESP8266"
#endif
  pinMode(Beep_out, OUTPUT);

  /////////////////////////////////////////////////////////////////////////////

  Serial.print("\nStarting AdvancedWebServer on " + String(ARDUINO_BOARD));
  Serial.println(" with " + String(SHIELD_TYPE));
  Serial.println(WEBSERVER_WT32_ETH01_VERSION);

  // uint8_t cnt = conf.getCount();
  //Serial.println(cnt);
  //Serial.println(conf.values[2]);
  //Serial.println(conf.values[3]);
  /////////////////////////////////////////////////////////////////////
  // Default KNX parameter mit den Werten aus dem SPIffs überschreiben
  /////////////////////////////////////////////////////////////////////
  //individualAddress = Extract_P_Addr(conf.values[0]); qwq hier geht's weiter
  //Serial.println(individualAddress);
  //Rolladen_hoch_runter = Extract_G_Addr(conf.values[1]);
  //Serial.println(Rolladen_hoch_runter);


  Serial.println("knx init coming on Serial2");
  //////////////////////////////////////////////  KNX Init /////////////////////
  // legt _tpuart als Device an! Kein Zugriff von hier möglich..
  if (Knx.begin(KNX_SERIAL, individualAddress) == KNX_DEVICE_ERROR) {
    Serial.println("knx init ERROR, stop here!!");
    while (1);
  }
  Serial.println("Knx Init done");
  TracesDisplay();
  // Serial.println("Anzahl_objekte:");Serial.println(Anzahl_objekte);


#if 1
  // Für Ton vorbereiten
  timer1 = timerBegin(0, 80, true);
  /* Attach onTimer function to our timer */
  timerAttachInterrupt(timer1, &ISR_tone, true);
  timerAlarmWrite(timer1, 16000, true); // alle x ms
  Isr_tone_duration = 20;
  pinMode(Beep_out, OUTPUT);
  timerAlarmEnable(timer1);
#endif
  // configTime(MY_TZ, MY_NTP_SERVER); // --> Here is the IMPORTANT ONE LINER needed in your sketch!
  Main_Clock = 0;
  Knx_max = 0;
  Mqtt_in_zeiger = Mqtt_out_zeiger = 0;
  Knxupdated = 0;
  Lz_count = 0;
  buffer_in_pnt = 0;
}

void loop()
{
  if (!Main_Clock) {
    Serial.println("Loop started ");
  }
  // Knx
  Knx_start = micros();
  Knx.task();
  // TracesDisplay(); // Verursacht lange Laufzeit..
  Knx_stop = micros();
  { // Laufzeitmessung /////////////////////////////////////////////////////////
    uint32_t temp = Knx_stop - Knx_start;
    if (temp > Knx_max) Knx_max = temp;
    if (Knx_max > 7700) { // Auffällige Laufzeiten ausgeben > 2ms
      Lz_count++;
      if (buffer_in_pnt < 24)snprintf(buffer[buffer_in_pnt++], 20, "Lz:%d Cnt:%d", Knx_max, Lz_count);
      Knx_max = 0;
      //Warnton
      pinMode(Beep_out, OUTPUT);
      Isr_tone_duration = 250;
      timerAlarmEnable(timer1);
    }
  }
  ////////////////////////////////////////////////////////////////////////
  if (Ga_int != Ga_old)
  { // Busmonitor all GA's send to mqtt

    // in GA_int ist die Gruppenadresse Hexademinal gespeichert, wird hier extrahiert Area/Linie/Grp
    // if (buffer_in_pnt < 24)snprintf(buffer[buffer_in_pnt++], 20, "GA: %d/%d/%d", (Ga_int >> 11), ((Ga_int >> 8) & 0x7), (Ga_int & 0xff));
    Ga_old = Ga_int;
  }
  if (!(Main_Clock & 0x3fffff)) {
    //Serial.print("Mqtt_zeiger : "); Serial.println(Mqtt_zeiger);
    if ((buffer_in_pnt < 24) && (Lz_count)) snprintf(buffer[buffer_in_pnt++], 20, "Anzahl Laufzeit: %d", Lz_count);
    //client.publish("LZ", buffer);

    if (Knxupdated == 0) {
      // Knx.update(6);
      //Serial.print("Tag/Nacht Info anfordern");
      Knxupdated = 1;
    }
    if (Mqtt_in_zeiger)
    {
      // Serial.println(mqtt_buffer[Mqtt_out_zeiger]);
      Mqtt_out_zeiger++;
    }
    if ((Mqtt_in_zeiger == Mqtt_out_zeiger) && (Mqtt_in_zeiger != 0)) //queue ist leer und nicht schon resettet
    {
      Mqtt_in_zeiger = 0;
      Mqtt_out_zeiger = 0;
    }
    /////////////////////////////////////// rolling Minute counter für Rolladensteuerung
    if (Status_Tag_Nacht)
    { // Nacht
      Tag_timer = 0;
      Nacht_timer ++;
      //Serial.print("Nacht_timer: "); Serial.println(Nacht_timer);
    }
    else
    {
      Tag_timer ++;
      Nacht_timer = 0;
      //Serial.print("Tag_timer: "); Serial.println(Tag_timer);
    }
#if 0
    Knx.write(11, (Nacht_timer + Tag_timer) & 1);
    Knx.write(12, (Nacht_timer + Tag_timer) & 2);
    Knx.write(13, (Nacht_timer + Tag_timer) & 4);
    Knx.write(14, (Nacht_timer + Tag_timer) & 8);
#endif
    // Serial.print(".");//Keep Alive signal
  }
  if (!(Main_Clock & 0xfffff)) {
    if (buffer_in_pnt) {
        //Serial.print("buffer_out_pnt: "); Serial.println(buffer_out_pnt);
        //Serial.println(buffer[buffer_out_pnt]);
        // Serial.println(buffer [buffer_in_pnt]);
        client.publish(TOPIC, buffer[buffer_out_pnt]);
        buffer_out_pnt ++;
        if ((buffer_in_pnt == buffer_out_pnt) && (buffer_in_pnt != 0)) //queue ist leer und nicht schon resettet
        {
          buffer_in_pnt = 0;
          buffer_out_pnt = 0;
          // Serial.println("buffer empty.");
        }
        else buffer[buffer_in_pnt][0] = '\0'; //untersten Buffer leeren
      }
    }
  if (new_minutes) // Alle Minuten die aktuelle Uhrzeit auf den Bus schreiben
  {
    new_minutes = 0;
    byte m[3];
    m[0] = (Stunde | Tag << 5);
    m[1] = Minute;
    m[2] = Sekunde;
    Knx.write(15, m); // write 3 bytes for Timeformat
    // Serial.println("Knx Write 15");
  }
  Main_Clock++;
}
