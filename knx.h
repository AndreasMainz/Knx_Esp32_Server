#include <WiFi.h>
//#include <ESP8266WiFi.h>
//#include <ESP8266WebServer.h>
#include <WebConfig.h>
// #include <EEPROM.h>


#define MQTT_PUBLISH_INTERVAL_MS       100000L

#define MY_NTP_SERVER "at.pool.ntp.org"           
#define MY_TZ "CET-1CEST,M3.5.0/02,M10.5.0/03"   

TaskHandle_t Task1;
uint32_t Task1_cnt, Knx_start, Knx_stop,Knx_max,Rx_start,Rx_stop;
uint16_t Mqtt_zeiger, Mqtt_message;
uint16_t Value_big,Value_small,Lz_count;

uint16_t Ga_int, Ga_old, Knxupdated;
String   Ga_text;
char buffer[25][20]; // 25 Botschaften a 20 chars
uint32_t buffer_pnt;

const char* weekDays[7]={"Sonntag", "Montag", "Dienstag", "Mittwoch", "Donnerstag", "Freitag", "Samstag"};
const char* months[12]={"Januar", "Februar", "Maerz", "April", "Mai", "Juni", "Juli", "August", "September", "Oktober", "November", "Dezember"};

// Manche Labelnamen führen zum fehlerhaften Einlesen..
String params = "["
  "{"
  "'name':'PA',"
  "'label':'Physikalische_Adresse',"
  "'type':"+String(INPUTTEXT)+","
  "'default':'1.1.13'"
  "},"
  "{"
  "'name':'Mqtt_IP_Adress',"
  "'label':'Mqtt Server',"
  "'type':"+String(INPUTTEXT)+","
  "'default':'192.168.1.48'"
  "},"
  "{"
  "'name':'Rolladen_stop',"
  "'label':'Rolladen Stop',"
  "'type':"+String(INPUTTEXT)+","
  "'default':'2/3/14'"
  "},"
  "{"
  "'name':'Rolladen_Sperre',"
  "'label':'Rolladen Sperre',"
  "'type':"+String(INPUTTEXT)+","
  "'default':'2/2/25'"
  "},"
  "{"
  "'name':'Zustand_Rolladen',"
  "'label':'Zustand Rolladen',"
  "'type':"+String(INPUTTEXT)+","
  "'default':'2/2/7'"
  "},"
  "{"
  "'name':'Device_Reset',"
  "'label':'Start WLAN once:',"
  "'type':"+String(INPUTTEXT)+","
  "'default':'2/1/121'"
  "}"
  "]";

const char *ID        = "Knx_Server";  // Name of our device, must be unique
const char *TOPIC     = "Knx";               // Topic to subcribe to
const char *TIME      = "Time_Server";      // Topic to subcribe to
const char *subTopic  = "MQTT_Sub";          // Topic to subcribe to


// RAM
volatile int T1_Event_Falling, T1_Event_Rising, T1_GA_short, T1_Short_State;
volatile int T2_Event_Falling, T2_Event_Rising, T2_GA_short, T2_Short_State;
volatile int T3_Event_Falling, T3_Event_Rising, T3_GA_short, T3_Short_State;
volatile int T4_Event_Falling, T4_Event_Rising, T4_GA_short, Rolladen_State_Sperre, Rolladen_Status;
uint32_t Knx_zeit, Tag_timer, Nacht_timer,Mqtt_IP_Adress;
byte Isr_tone_value;
int Isr_tone_duration;
uint8_t Status_Tag_Nacht;

// Functions
void IRAM_ATTR ISR_T1(void);
void IRAM_ATTR ISR_T2(void);
void IRAM_ATTR ISR_T3(void);
void IRAM_ATTR ISR_T4(void);

void IRAM_ATTR ISR_T1_RELEASE(void);
void IRAM_ATTR ISR_T2_RELEASE(void);
void IRAM_ATTR ISR_T3_RELEASE(void);
void IRAM_ATTR ISR_T4_RELEASE(void);

void mqtt_callback(char* , byte* , unsigned int ); 

// Hw resource
int T_Reset_cnt;
hw_timer_t *timer1 = NULL;
int area, linie, grp;
word ga, pa;
long Main_Clock;
static unsigned long now;
unsigned long lastMsg;

#define Beep_out 4


char T_Boot_Mode;

//mqtt
// byte* mqtt_buffer;
