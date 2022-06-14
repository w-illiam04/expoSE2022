#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <WiFiClient.h>

#define SSID_NAME "DESKTOP-EARRVEN 0776"                    // Nome da rede
#define SSID_PASSWORD "304$73Qc"                            // Senha da rede
#define MQTT_BROKER "smartnest.cz"                          // Servidor Broker
#define MQTT_PORT 1883                                      // Porta Broker
#define MQTT_USERNAME "wl11lm"                              // Username do Smartnest
#define MQTT_PASSWORD "ed0bf3995ac52aa325ac2c11a29dbb80"    // Chave API
#define MQTT_CLIENT "62a2142ef8510369f2a1c9f7"              // ID do dispositivo (reles)
#define MQTT_CLIENT2 "62a875a2f8510369f2a1ca99"             // ID do dispositivo (sensor PIR)
#define FIRMWARE_VERSION "TechInside - Modulo de automação" // Nome do programa

WiFiClient espClient;
PubSubClient client(espClient);


//-----------------------------------------------------------Define as variáveis
int switchPin1 = 2;
int switchPin2 = 0;
int bellPin = 4;
bool bellTriggered = false;
int bellReportSend = 0;


//-----------------------------------------------------------Inicia as funcoes
void startWifi();
void startMqtt();
void sendBellReport();
void checkBell();
void checkMqtt();
int splitTopic(char* topic, char* tokens[], int tokensNumber);
void callback(char* topic, byte* payload, unsigned int length);
void sendToBroker(char* topic, char* message, int device);
void turnOff(int pin);
void turnOn(int pin);




void setup() {
  // Define os pinos de entrada e saída
  pinMode(switchPin1, OUTPUT);
  pinMode(switchPin2, OUTPUT);
  pinMode(bellPin, INPUT);
  
  Serial.begin(115200);
  startWifi();
  startMqtt();
}

void loop() {
  client.loop();
  checkBell();                                                    //Verifica o sensor
  checkMqtt();                                                    //Verifica conexao com o servidor
}




void callback(char* topic, byte* payload, unsigned int length) {  //Se algum novo comando foi recebido
  Serial.print("Topic:");
  Serial.println(topic);
  int tokensNumber = 10;
  char* tokens[tokensNumber];
  char message[length + 1];
  splitTopic(topic, tokens, tokensNumber);
  sprintf(message, "%c", (char)payload[0]);
  for (int i = 1; i < length; i++) {
    sprintf(message, "%s%c", message, (char)payload[i]);
  }
  Serial.print("Message:");
  Serial.println(message);

  //------------------Acoes (ligar e desligar os reles)---------------------------------

  if (strcmp(tokens[1], "directive") == 0) {
    if (strcmp(tokens[2], "powerState1") == 0) {
      if (strcmp(message, "ON") == 0) {
        turnOn(1);
      } else if (strcmp(message, "OFF") == 0) {
        turnOff(1);
      }
    } else if (strcmp(tokens[2], "powerState2") == 0) {
      if (strcmp(message, "ON") == 0) {
        turnOn(2);
      } else if (strcmp(message, "OFF") == 0) {
        turnOff(2);
      }
    }
  }
}


void startWifi() {                                                          //Conecta ao Wifi
  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID_NAME, SSID_PASSWORD);
  Serial.println("Connecting ...");
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    attempts++;
    delay(1000);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println('\n');
    Serial.print("Connected to ");
    Serial.println(WiFi.SSID());
    Serial.print("IP address:\t");
    Serial.println(WiFi.localIP());

  } else {
    Serial.println('\n');
    Serial.println('I could not connect to the wifi network after 20 attempts \n');
  }

  delay(500);
}

void startMqtt() {                                                          // Conecta ao servidor
  if(WiFi.status() != WL_CONNECTED){                                        // Se desconectado do wifi, tenta novamente
    startWifi();
  }
  
  client.setServer(MQTT_BROKER, MQTT_PORT);
  client.setCallback(callback);

  while (!client.connected()) {
    Serial.println("Connecting to MQTT...");

    if (client.connect(MQTT_CLIENT, MQTT_USERNAME, MQTT_PASSWORD)&&client.connect(MQTT_CLIENT2, MQTT_USERNAME, MQTT_PASSWORD)) {
      Serial.println("connected");
    } else {
      if (client.state() == 5) {
        Serial.println("Connection not allowed by broker, possible reasons:");
        Serial.println("- Device is already online. Wait some seconds until it appears offline for the broker");
        Serial.println("- Wrong Username or password. Check credentials");
        Serial.println("- Client Id does not belong to this username, verify ClientId");

      } else {
        Serial.println("Not possible to connect to Broker Error code:");
        Serial.print(client.state());
      }

      delay(0x7530);
    }
  }

  char subscibeTopic[100];
  sprintf(subscibeTopic, "%s/#", MQTT_CLIENT);
  client.subscribe(subscibeTopic);                                            //Inscreve-se em todas as mensagens enviadas para o dispositivo

  
  //Reles
  sendToBroker("report/online", "true", 1);                                   // Reporta que o dispositivo está ligado
  delay(100);
  sendToBroker("report/firmware", FIRMWARE_VERSION, 1);                       // Reporta a versao do firmware
  delay(100);
  sendToBroker("report/ip", (char*)WiFi.localIP().toString().c_str(), 1);     // Reporta o IP
  delay(100);
  sendToBroker("report/network", (char*)WiFi.SSID().c_str(), 1);              // Reporta o nome da rede
  delay(100);
  
  //Sensor
  sendToBroker("report/online", "true", 2);                                   // Reporta que o dispositivo está ligado
  delay(100);
  sendToBroker("report/firmware", FIRMWARE_VERSION, 2);                       // Reporta a versao do firmware
  delay(100);
  sendToBroker("report/ip", (char*)WiFi.localIP().toString().c_str(), 2);     // Reporta o IP
  delay(100);
  sendToBroker("report/network", (char*)WiFi.SSID().c_str(), 2);              // Reporta o nome da rede
  delay(100);

  char signal[5];
  sprintf(signal, "%d", WiFi.RSSI());
  sendToBroker("report/signal", signal, 1);  // Relata a itensidade do sinal
  delay(100);
  sendToBroker("report/signal", signal, 2);  // Relata a itensidade do sinal
  delay(100);
}

int splitTopic(char* topic, char* tokens[], int tokensNumber) {
  const char s[2] = "/";
  int pos = 0;
  tokens[0] = strtok(topic, s);

  while (pos < tokensNumber - 1 && tokens[pos] != NULL) {
    pos++;
    tokens[pos] = strtok(NULL, s);
  }

  return pos;
}

void checkMqtt() {
  if (!client.connected()) {
    startMqtt();
  }
}

void checkBell() {                                                                // Verifica se houve movimento
  int buttonState = digitalRead(bellPin);
  if (buttonState == LOW && !bellTriggered) {
    return;

  } else if (buttonState == LOW && bellTriggered) {
    bellTriggered = false;

  } else if (buttonState == HIGH && !bellTriggered) {
    bellTriggered = true;
    sendBellReport();                                                             // Se houver, relata ao servidor

  } else if (buttonState == HIGH && bellTriggered) {
    return;
  }
}


void sendBellReport() {                                                           // Envia o sinal ao server
  if (millis() - bellReportSend > 5000) {                                         // A cada 5s (evitar repeticao)
    sendToBroker("report/detectionState", "true", 2);
    bellReportSend = millis();
  }
}

void sendToBroker(char* topic, char* message, int device) {                       // Comunica o estado dos Reles (1) e sensor (2)
  if (client.connected()) {
  char topicArr[100];
  switch (device) {
    case 1:
      sprintf(topicArr, "%s/%s", MQTT_CLIENT, topic);
      break;
    case 2:
      sprintf(topicArr, "%s/%s", MQTT_CLIENT2, topic);
      break;
  }
    client.publish(topicArr, message);
  }
}

void turnOff(int pin) {                                                             // Funcao de desligar os pinos
  Serial.printf("Turning off pin %d...\n", pin);
  switch (pin) {
  case 1:
    digitalWrite(switchPin1, LOW);
    sendToBroker("report/powerState1", "OFF", 1);
    break;
  case 2:
    digitalWrite(switchPin2, LOW);
    sendToBroker("report/powerState2", "OFF", 1);
    break;
  }
}

void turnOn(int pin) {                                                               // Funcao de ligar os pinos
  Serial.printf("Turning on pin %d...\n", pin);
  switch (pin) {
  case 1:
    digitalWrite(switchPin1, HIGH);
    sendToBroker("report/powerState1", "ON", 1);
    break;
  case 2:
    digitalWrite(switchPin2, HIGH);
    sendToBroker("report/powerState2", "ON", 1);
    break;
  }
}
