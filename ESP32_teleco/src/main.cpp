#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>  
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <WiFiClientSecure.h>
#include "cert.h"


#define URL_fw_Version "https://raw.githubusercontent.com/tsunayoshi21/ESP32_ota/main/bin_version.txt"
#define URL_fw_Bin "https://raw.githubusercontent.com/tsunayoshi21/ESP32_ota/main/firmware.bin"

WiFiClient esp32Client;
PubSubClient mqttClient(esp32Client);

#define buff  (100)

#define DHTPIN 4  // Pin al que está conectado el sensor DHT
#define DHTTYPE DHT11 // Tipo de sensor DHT

DHT dht(DHTPIN, DHTTYPE);

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", -3 * 3600, 60000);
//NTPClient timeClient(ntpUDP, "cl.pool.ntp.org");

const char* ssid     = "dedmu5";
const char* password = "123456789";

// const char* ssid     = "VTR-2236566";
// const char* password = "p6qxNmkMrrjk";

// const char* ssid     = "Bubbitos2.0";
// const char* password = "Bubbita2309";

String FirmwareVer = {
  "3.0"
};

char *server = "broker.emqx.io";
int port = 1883;

int ledpin = 2;
int led_umbral = 5;
int fotopin = 33;

int var = 0;
int ledval = 0;
int fotoval = 0;
char datos[buff];
String resultS = "";
float temp;
float hum;
int temp_int;
int hum_int;


double lastMillis = 0;

unsigned long previousMillis = 0; // will store last time LED was updated
unsigned long previousMillis_2 = 0;
const long interval_firmware_check = 5000;
const long interval_firmware_print = 1000;

void wifiInit() {
    Serial.print("Conectándose a ");
    Serial.println(ssid);

    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
      Serial.print(".");
        delay(500);  
    }
    Serial.println("");
    Serial.println("Conectado a WiFi");
    Serial.println("Dirección IP: ");
    Serial.println(WiFi.localIP());
  }

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Mensaje recibido [");
  Serial.print(topic);
  Serial.print("] ");

  char payload_string[length + 1];
  
  int resultI;

  memcpy(payload_string, payload, length);
  payload_string[length] = '\0';
  resultI = atoi(payload_string);
  
  var = resultI;
  Serial.print(var);

  resultS = "";
  
  for (int i=0;i<length;i++) {
    resultS= resultS + (char)payload[i];
  }
  Serial.println();
}



void reconnect() {
  while (!mqttClient.connected()) {
    Serial.print("Intentando conectarse MQTT...");

    if (mqttClient.connect("arduinoClient")) {
      Serial.println("Conectado");

      mqttClient.subscribe("lab/led");
      
    } else {
      Serial.print("Fallo, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" intentar de nuevo en 0.5 segundos");
      // Wait 5 seconds before retrying
      delay(500);
    }
  }
}

void firmwareUpdate(void) {
  WiFiClientSecure client;
  client.setCACert(rootCACertificate);
  httpUpdate.setLedPin(LED_BUILTIN, LOW);
  t_httpUpdate_return ret = httpUpdate.update(client, URL_fw_Bin);

  switch (ret) {
  case HTTP_UPDATE_FAILED:
    Serial.printf("HTTP_UPDATE_FAILD Error (%d): %s\n", httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());
    break;

  case HTTP_UPDATE_NO_UPDATES:
    Serial.println("HTTP_UPDATE_NO_UPDATES");
    break;

  case HTTP_UPDATE_OK:
    Serial.println("HTTP_UPDATE_OK");
    break;
  }
}

int FirmwareVersionCheck(void) {
  String payload;
  int httpCode;
  String fwurl = "";
  fwurl += URL_fw_Version;
  // fwurl += "?";
  // fwurl += String(rand());
  //fwurl += URL_fw_Version;
  fwurl += "?rand=";
  fwurl += String(random(999999)); // Agrega un parámetro aleatorio único
  Serial.println(fwurl);
  WiFiClientSecure * client = new WiFiClientSecure;

  if (client) 
  {
    client -> setCACert(rootCACertificate);

    // Add a scoping block for HTTPClient https to make sure it is destroyed before WiFiClientSecure *client is 
    HTTPClient https;

    if (https.begin( * client, fwurl)) 
    { // HTTPS      
      // Add the Cache-Control header to the request
      https.addHeader("Cache-Control", "no-store");
      Serial.print("[HTTPS] GET...\n");
      // start connection and send HTTP header
      delay(100);
      httpCode = https.GET();
      delay(100);
      if (httpCode == HTTP_CODE_OK) // if version received
      {
        payload = https.getString(); // save received version
      } else {
        Serial.print("error in downloading version file:");
        Serial.println(httpCode);
      }
      https.end();
    }
    delete client;
  }
      
  if (httpCode == HTTP_CODE_OK) // if version received
  {
    payload.trim();
    Serial.print(payload);
    if (payload.equals(FirmwareVer)) {
      Serial.printf("\nDevice already on latest firmware version:%s\n", FirmwareVer);
      return 0;
    } 
    else 
    {
      Serial.println(payload);
      Serial.println("New firmware detected");
      return 1;
    }
  } 
  return 0;  
}


void setup()
{
  pinMode(ledpin,OUTPUT);
  pinMode(led_umbral,OUTPUT);
  Serial.begin(9600);
  dht.begin();  // Inicia el sensor DHT
  delay(10);
  wifiInit();
  digitalWrite(ledpin,HIGH);
  delay(3000);
  digitalWrite(ledpin,LOW);
  Serial.println("Iniciando con firmware versión:");
  Serial.println(FirmwareVer);
  mqttClient.setServer(server, port);
  mqttClient.setCallback(callback);
  timeClient.begin();
}

void loop()
{
   if (!mqttClient.connected()) {
    reconnect();
  }
    //static int num=0;
    unsigned long currentMillis = millis();
    if ((currentMillis - previousMillis) >= interval_firmware_check) {
      // save the last time you blinked the LED
      previousMillis = currentMillis;
      if (FirmwareVersionCheck()) {
        firmwareUpdate();
      }
    }
    // if ((currentMillis - previousMillis_2) >= interval_firmware_print) {
    //   previousMillis_2 = currentMillis;
    //   //Serial.print("idle loop...");
    //   //Serial.print(num++);
    //   Serial.print(" Active fw version:");
    //   Serial.println(FirmwareVer);
    // }


  timeClient.update();
  //Serial.print("String: ");
  //Serial.println(resultS);
    // Obtiene la fecha y hora actual
  unsigned long epochTime = timeClient.getEpochTime();

  // Convierte la marca de tiempo a estructura de tiempo
  struct tm * timeInfo;
  time_t rawTime = (time_t)epochTime;
  timeInfo = localtime(&rawTime);

  // Formatea la fecha y hora en el formato deseado
  char formattedDate[20];
  strftime(formattedDate, sizeof(formattedDate), "%d/%m/%y - %H:%M", timeInfo);

  // Imprime la fecha y hora formateada
  //Serial.println(formattedDate);

    // Extrae componentes de la fecha y hora
  int dd = timeInfo->tm_mday;
  int mm = timeInfo->tm_mon + 1; // tm_mon es de 0 a 11
  int yy = timeInfo->tm_year - 100; // tm_year es el año desde 1900
  int h = timeInfo->tm_hour;
  int min = timeInfo->tm_min;

  // Imprime las variables separadas
  //Serial.printf("dd=%02d, mm=%02d, yy=%02d, h=%02d, min=%02d\n", dd, mm, yy, h, min);

  if (resultS == "update"){
    firmwareUpdate();
  }

  if(var == 0)
  {
  digitalWrite(ledpin,LOW);
  }
  else if (var == 1)
  {
  digitalWrite(ledpin,HIGH);
  }

  //fotoval = analogRead(fotopin);
  //Serial.print("Foto: ");
  //Serial.println(fotoval);




  // publicar cada 3 segundos

  if (millis() - lastMillis > 1000) {
    lastMillis = millis();
        // Lee la temperatura y humedad del sensor DHT
    float temp = dht.readTemperature();
    float hum  = dht.readHumidity();



    // Convertir a enteros multiplicando por 100 y redondeando
    int temp_int = static_cast<int>(temp);
    int hum_int  = static_cast<int>(hum );

    if (hum_int > 50) {
    digitalWrite(led_umbral,HIGH);
    Serial.println("Humedad alta");
    }
    else {digitalWrite(led_umbral,LOW);}

    //sprintf(datos, "Valor fotoresistencia: %d ", fotoval);
    snprintf (datos, buff, "{\"sender\":\"GRUPO 4\",\"temp\":%d,\"hum\":%d,\"date\":\"%02d/%02d/%02d - %02d:%02d\"}",
              temp_int, hum_int, dd, mm, yy, h, min);
    Serial.println(datos);
    Serial.print(" Active fw version:");
    Serial.println(FirmwareVer);
    mqttClient.publish("lab/sensor", datos);
    //mqttClient.publish("lab/led", resultS.c_str());
  }
  
    // Formato JSON
  //snprintf (msg, MSG_BUFFER_SIZE, "{\"sender\":\"GRUPO X\",\"temp\":%.2f,\"hum\":%.2f,\"date\":\"%02d/%02d/%02d - %02d:%02d\"}",
            //temperature, humidity, day(), month(), year() % 100, hour(), minute());

  // Serial.print("Publish message: ");
  // Serial.println(msg);
  // client.publish("lab/sensor", msg);

  //delay(10);
  mqttClient.loop();


}
