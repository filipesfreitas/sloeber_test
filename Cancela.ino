#include "Arduino.h"
#define BLYNK_PRINT Serial
#include "gsmconfig.h"
#define TINY_GSM_MODEM_SIM800
#define SIM800L_IP5306_VERSION_20190610
#define cancela_pin 19
#define led 13
#define TIMEOUT 1000
#define SerialMon Serial
#define SerialAT Serial1
#define DUMP_AT_COMMANDS
#define TINY_GSM_DEBUG SerialMon
#define GSM_PIN ""
#define reconection_attempts 2

/*
 * To do:
 *
 * Adicionar comunicação por wifi
 * adicionar interface de comandos para eventual log
 * adicionar controle de abertura
 *
 * */
short int reconnections = 0;
const char apn[] = "zap.vivo.com.br";
const char gprsUser[] = "";
const char gprsPass[] = "";
char server[] = "blynk-cloud.com";
char port = 80;
String number = "+5561984619076";

#include <TinyGsmClient.h>
#include <BlynkSimpleTinyGSM.h>
#ifdef DUMP_AT_COMMANDS
#include <StreamDebugger.h>
StreamDebugger debugger(SerialAT, SerialMon);
TinyGsm modem(debugger);
#else
TinyGsm modem(SerialAT);
#endif
TinyGsmClient client(modem);

const char auth[] = "wEHwSxiifrPOmOcbJ6R4oxthUa_gBQy5";
BlynkTimer  timer;
void Checkconnection();

typedef struct {
  bool tcp_connection;
  bool gprsconnection;
  float rssiIndication;
} connection;

void setup()
{
  SerialMon.begin(115200);
  delay(10);
  setupModem();
  SerialMon.println("Wait...");
  SerialAT.begin(115200, SERIAL_8N1, MODEM_RX, MODEM_TX);
  delay(6000);
  SerialMon.println("Initializing modem...");
  modem.restart();
  String modemInfo = modem.getModemInfo();

  SerialMon.print("Modem Info: ");
  SerialMon.println(modemInfo);

  Blynk.config(modem, auth, server, port);
  connectNetwork(apn, gprsUser, gprsPass);
  Blynk.connect();
  pinMode(cancela_pin, OUTPUT);
  digitalWrite(cancela_pin, HIGH);
  timer.setInterval(10000, Checkconnection);
}

BLYNK_WRITE(V0) {
  int restarts = param.asInt();
  if (restarts == 1) {
    digitalWrite(led, HIGH);
    digitalWrite(cancela_pin, LOW);
    delay(TIMEOUT);
  }
  else {
    digitalWrite(led, LOW);
    digitalWrite(cancela_pin, HIGH);
    delay(TIMEOUT);

  }
}
BLYNK_WRITE(V1) {
  number = param.asStr();
}
void loop()
{
  if (Blynk.connected()) {
    Blynk.run();
  }
  timer.run();
}
void sendgprsinfo(String message, String number) {
  bool res = modem.sendSMS(number, String("Reconnection failed, SIM800L Restarter\n") +
                           "DATETIME: " + modem.getGSMDateTime(DATE_FULL) + "\n" +
                           "Modem info: " + modem.getModemInfo() + "\n" +
                           "Sim Status: " + modem.getSimStatus() + "\n" +
                           "Modem Name: " + modem.getModemName() + "\n" +
                           "Is network connected: "  + modem.isNetworkConnected() + "\n" +
                           "Is gprsconnected: " + modem.isGprsConnected() + "\n" +
                           "Modem local IP: " +
                           String(modem.localIP()[0]) + "."  +
                           String(modem.localIP()[1]) + "."  +
                           String(modem.localIP()[2]) + "."  +
                           String(modem.localIP()[3]) + "\n" +
                           "Siganal quality: " + modem.getSignalQuality() + "\n");
  DBG("SMS:", res ? "OK" : "fail");
}
void reestart_connection() {
  modem.gprsDisconnect();
  delay(1000);
  modem.gprsConnect(apn, gprsUser, gprsPass);
}

void Checkconnection() {
  String test = "Modem info: " + modem.getModemInfo() + "\t" +
                "Sim Status: " + modem.getSimStatus() + "\t" +
                "Modem Name: " + modem.getModemName() + "\t" +
                "Is network connected: "  + modem.isNetworkConnected() + "\t" +
                "Is gprsconnected: " + modem.isGprsConnected() + "\t" +
                "Modem local IP: " +
                String(modem.localIP()[0]) + "."  +
                String(modem.localIP()[1]) + "."  +
                String(modem.localIP()[2]) + "."  +
                String(modem.localIP()[3]) + "\t" +
                "Siganal quality: " + modem.getSignalQuality() + "\t";
  SerialMon.println(test);
  if (!Blynk.connected()) {
    yield();
    modem.sendAT("AT+CIPCLOSE\r\n");
    modem.waitResponse();
    modem.sendAT("AT+CIPSTART\r\n");
    modem.waitResponse();
    if (!Blynk.connected() && reconnections < reconection_attempts) {
      reestart_connection();
      reconnections++;
    }
    else if (reconnections < (reconection_attempts + 1)) {
      SerialMon.println("\n\n Reconnection failed, reestarting modem... \n\n");
      yield();
      modem.restart();
      yield();
      reconnections++;
    }
    else {
      SerialMon.println("SENDING SMS CONNECTION FAILED AND REESTARTING ESP32");
      SerialMon.println(number);
      sendgprsinfo("test", number);
      ESP.restart();
    }
    Blynk.connect();
  }
}

bool connectNetwork(const char* apn, const char* user, const char* pass)
{
  BLYNK_LOG1(BLYNK_F("Modem init..."));
  if (!modem.begin()) {
    BLYNK_FATAL(BLYNK_F("Cannot init"));
  }

  switch (modem.getSimStatus()) {
    case SIM_ERROR:  BLYNK_FATAL(BLYNK_F("SIM is missing"));    break;
    case SIM_LOCKED: BLYNK_FATAL(BLYNK_F("SIM is PIN-locked")); break;
    default: break;
  }

  BLYNK_LOG1(BLYNK_F("Connecting to network..."));
  if (modem.waitForNetwork()) {
    String op = modem.getOperator();
    BLYNK_LOG2(BLYNK_F("Network: "), op);
  } else {
    BLYNK_FATAL(BLYNK_F("Register in network failed"));
  }

  BLYNK_LOG3(BLYNK_F("Connecting to "), apn, BLYNK_F(" ..."));
  if (!modem.gprsConnect(apn, user, pass)) {
    BLYNK_FATAL(BLYNK_F("Connect GPRS failed"));
    return false;
  }

  BLYNK_LOG1(BLYNK_F("Connected to GPRS"));
  return true;
}
