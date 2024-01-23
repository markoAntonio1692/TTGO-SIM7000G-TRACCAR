/*
By: Ing. Marco A. Caballero Moreno
contact: marko.antonio.1.16.92@gmail.com
23/01/2023
Version: 0.1

*/

/*TRACCAR OSMAND PROTOCOL EXAMPLE */
/* http://demo4.traccar.org:5055/?id=CAFECAFE&lat=49.8466&lon=2.4622&in1=0&out1=0 */


/* Server details**/
String server   = "demo4.traccar.org";
String resource = "";
int  port       = 5055;
String DeviceID = "CAFECAFE"; /*Create ID for Asociate to Traccar Platform (Unique ID for each devices)*/ 
/*simcard Details*/
static const char apn[15]  = "convergia1.com";
static const char gprsUser[] = "";
static const char gprsPass[] = "";
int TimeSeconds = 45;  //set periodical send data (Seconds)
/*************************************************************/

/*general variables*/
int in1=0;
int out1=0;
bool STS_NWRK=0;
bool STS_FORCE=0;
float lat,  lon;
unsigned long lastMsg = 0;
int flagConf=0;
/**********************/ 
  
#define SerialMon Serial
#define SerialAT Serial1
#define TINY_GSM_MODEM_SIM7000
#define TINY_GSM_RX_BUFFER 1024 // Set RX buffer to 1Kb
#define SerialAT Serial1

#include <TinyGsmClient.h>
#include <ArduinoHttpClient.h>
#include <SPI.h>
#include <SD.h>
#include <Ticker.h>
#ifdef DUMP_AT_COMMANDS
#include <StreamDebugger.h>


StreamDebugger debugger(SerialAT, SerialMon);
TinyGsm modem(debugger);
#else
TinyGsm modem(SerialAT);
#endif

TinyGsmClient client(modem);
HttpClient    http(client, server, port);

#define UART_BAUD           9600
#define PIN_DTR             25
#define PIN_TX              27
#define PIN_RX              26
#define PWR_PIN             4
#define SD_MISO             2
#define SD_MOSI             15
#define SD_SCLK             14
#define SD_CS               13
#define LED_PIN             12
#define IN1_PIN             32   //define digital input1 pin
#define OUT1_PIN            33   //define digital output1 pin

/*prototypes functions*/
void enableGPS(void);
void disableGPS(void);
void modemPowerOn(void);
void modemPowerOff(void);
void reboot(void);
void HTTP_REQUEST(void);

void setup(){
    SerialMon.begin(115200);
    //EEPROM.begin(12);
     
    // Set LED OFF
    pinMode(IN1_PIN, INPUT);
    pinMode(OUT1_PIN, OUTPUT);
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, HIGH);

    modemPowerOn();
    SerialAT.begin(UART_BAUD, SERIAL_8N1, PIN_RX, PIN_TX);

    Serial.println();
    Serial.println("[INICIALIZANDO]");
    
//    /*verificar memoria SD**************************************/
//    SPI.begin(SD_SCLK, SD_MISO, SD_MOSI);
//    if (!SD.begin(SD_CS)) {
//        Serial.println("NO SD CARD");
//    } else {
//        uint32_t cardSize = SD.cardSize() / (1024 * 1024);
//        String str = "> SDCard Size: " + String(cardSize) + "MB";
//        Serial.println(str);
//    }
//    /************************************************************/

    /*verificar si modem esta listo para operar****************/
    uint32_t  timeout = millis();
    while (!modem.testAT()) {
        Serial.print(".");
        if (millis() - timeout > 60000 ) {
            Serial.println(" ...El modem no responde");
            modemPowerOff();
            delay(5000);
            modemPowerOn();
            timeout = millis();
        }
    }
    Serial.println("[MODEM OK!]");
    /************************************************************/
    
   /*verificar si el SIMCARD esta conectado*********************/
    timeout = millis();
    while (modem.getSimStatus() != SIM_READY) {
        Serial.print(".");
        if (millis() - timeout > 60000 ) {
            Serial.println(" ...NO se detecto SIMCARD");
            reboot();
            return;
        }

    }
    Serial.println("[SIMCARD OK!]");
    /************************************************************/

    String res = modem.getIMEI();
    Serial.print("IMEI: ");
    Serial.println(res);

    /*preferencias de red movil ********************************/
    modem.sendAT("+CBAND=ALL_MODE");
    modem.waitResponse();
    // Args:
    // 1 CAT-M
    // 2 NB-IoT
    // 3 CAT-M and NB-IoT
    // Set network preferre to auto
    uint8_t perferred = 3;
    modem.setPreferredMode(perferred);
    // Args:
    // 2  Automatic
    // 13 GSM only
    // 38 LTE only
    // 51 GSM and LTE only
    // Set network mode to auto
    modem.setNetworkMode(2);
    /************************************************************/

    // Check network signal and registration information
    Serial.println("Nivel de señal: ");
    RegStatus status;
    timeout = millis();
    do {
        int16_t sq =  modem.getSignalQuality();
        sq=sq*-1;
        status = modem.getRegistrationStatus();

        if (status == REG_DENIED) {
            Serial.println("> La conexion del Simcard ha sido denegado por el operador");
            /*reiniciar*/
            return;
        } else {
            Serial.print("Signal:");
            Serial.println(sq);
        }

        if (millis() - timeout > 120000 ) {
            if (sq == 99) {
                Serial.println("> NO HAY COBERTURA DE RED MOVIL");
                reboot();
                return;
            }
            timeout = millis();
        }

        delay(800);
    } while (status != REG_OK_HOME && status != REG_OK_ROAMING);

    modem.sendAT("+CNACT=1");
    modem.waitResponse();

    res = modem.getLocalIP();
    modem.sendAT("+CNACT?");
    if (modem.waitResponse("+CNACT: ") == 1) {
        modem.stream.read();
        modem.stream.read();
        res = modem.stream.readStringUntil('\n');
        res.replace("\"", "");
        res.replace("\r", "");
        res.replace("\n", "");
        modem.waitResponse();
        Serial.print("IP address: ");
        Serial.println(res);
    }


    modem.sendAT("+CPSI?");
    if (modem.waitResponse("+CPSI: ") == 1) {
        res = modem.stream.readStringUntil('\n');
        res.replace("\r", "");
        res.replace("\n", "");
        modem.waitResponse();
        Serial.print("Parametros actuales de red:");
        Serial.println(res);
    }

    enableGPS();  //enable gps sim7000g

}

void loop(){
  /*verificacion de conexion en red*******************/
    if(modem.isNetworkConnected()) {
      Serial.print("[NTWRK] OK  ");
      STS_NWRK = 1;
      //ledrojo(false); ledrojo(false); 
    }else{
        SerialMon.print("[NWRK]");
        STS_NWRK = 0;
        if(!modem.waitForNetwork(180000L)) {
          SerialMon.println(" rbt");
          reboot();
        }else{
          STS_NWRK = 1;
          SerialMon.println(" OK");
        }
    }
    
    if(modem.isGprsConnected()) { 
      SerialMon.println("[GPRS] OK");
      STS_NWRK = 1;
      //ledrojo(false); ledrojo(false); 
    }else{
      STS_NWRK = 0;
      SerialMon.println("[GPRS]");
      uint8_t gprcount=0;
      while(gprcount<10){ 
        gprcount++;
        SerialMon.print(".");
        if(modem.gprsConnect(apn, gprsUser, gprsPass)){
          SerialMon.println(" OK");
          break;
        }
        delay(2000);  
      }
      if(!modem.isGprsConnected()){
        STS_NWRK = 0;
        SerialMon.println(F(" rbt"));
        reboot();
      }      
    }
    /**************************************************/
    
    delay(1000);
    
    in1 = digitalRead(IN1_PIN); //leer pin de entrada
    
    if(STS_NWRK==1){ /*si la red esta ok realizar tareas*/
      unsigned long now = millis();
      if((now - lastMsg > TimeSeconds * 1000ULL) || (flagConf == 1) ) {
         flagConf = 0; //reiniciar flag de confirmacion de comando
         lastMsg = now;
         if(modem.getGPS(&lat, &lon)) {
           digitalWrite(LED_PIN, LOW);  
           Serial.print("Geoloc: ");
           Serial.print(""); Serial.print(lat,7);
           Serial.print(","); Serial.println(lon,7);
           delay(1000);
           digitalWrite(LED_PIN, HIGH);  
         }
         HTTP_REQUEST();  ///request to traccar      
      }    
    }
}


void enableGPS(void){
    // Set Modem GPS Power Control Pin to HIGH ,turn on GPS power
    // Only in version 20200415 is there a function to control GPS power
    modem.sendAT("+CGPIO=0,48,1,1");
    if (modem.waitResponse(10000L) != 1) {
        DBG("Set GPS Power HIGH Failed");
    }
    modem.enableGPS();
}

void disableGPS(void){
    // Set Modem GPS Power Control Pin to LOW ,turn off GPS power
    // Only in version 20200415 is there a function to control GPS power
    modem.sendAT("+CGPIO=0,48,1,0");
    if (modem.waitResponse(10000L) != 1) {
        DBG("Set GPS Power LOW Failed");
    }
    modem.disableGPS();
}

void modemPowerOn(void){
    pinMode(PWR_PIN, OUTPUT);
    digitalWrite(PWR_PIN, LOW);
    delay(1000);    //Datasheet Ton mintues = 1S
    digitalWrite(PWR_PIN, HIGH);
}

void modemPowerOff(void){
    pinMode(PWR_PIN, OUTPUT);
    digitalWrite(PWR_PIN, LOW);
    delay(1500);    //Datasheet Ton mintues = 1.2S
    digitalWrite(PWR_PIN, HIGH);
}
void reboot(void){
delay(200);
ESP.restart();
}

void HTTP_REQUEST(void){
  /* http://demo4.traccar.org:5055/?id=CAFECAFE&lat=49.8466&lon=2.4622&in1=0&out1=0 */
  String url = resource + "/?id=" + DeviceID + "&lat=" + String(lat,7) + "&lon=" + String(lon,7) + "&in1=" + String(in1) + "&out1=" + String(out1);
  
  SerialMon.println("HTTP GET REQUEST... ");
  SerialMon.println(url);
  int err = http.get(url);
  if (err != 0) {
    SerialMon.println("Fallo al conectar");
    delay(10000);
    return;
  }
  
SerialMon.println("RESPONSE HEADERS:");
  while (http.headerAvailable()) {
    String headerName  = http.readHeaderName();
    String headerValue = http.readHeaderValue();
    SerialMon.println("    " + headerName + " : " + headerValue);
  }
  SerialMon.println(" ");
  
  int length = http.contentLength();
  if (length >= 0) {
    SerialMon.print("Content length: ");
    SerialMon.println(length);
  }
  if (http.isResponseChunked()) {
    SerialMon.println("The response is chunked");
  }

  String body = http.responseBody();
  SerialMon.println("RESPONSE: ");
  SerialMon.println(body);

  SerialMon.print("Body length is: ");
  int bodylength = body.length();
  SerialMon.println(bodylength);
  if(bodylength > 1){
    if(body=="out1=1") {SerialMon.println("out1=1"); digitalWrite(OUT1_PIN,HIGH); out1 = 1; flagConf=1;}
    if(body=="out1=0") {SerialMon.println("out1=0"); digitalWrite(OUT1_PIN,LOW);  out1 = 0; flagConf=1;}
  }

  http.stop();
  //SerialMon.println("Server disconnected");
  
}
