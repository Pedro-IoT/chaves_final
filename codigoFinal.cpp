//- Definir as bibliotecas a serem utilizadas
#include <Arduino.h>
#include <WiFi.h>
#include <ESP32Servo.h>
#include <PubSubClient.h>
#include <FS.h>
#include "SPIFFS.h"
#include "time.h"
#include <MFRC522.h>
#include <SPI.h>
#include <HTTPClient.h>
//- Definir os pinos dos leds e motores
#define motor1 15
#define LED_WIFI 26
#define LED_ADA 27
#define BOTAO 34
String s;
String logs;

//- Definindo as configurações RFID
#define RST_PIN 4
#define SS_PIN_1 14
#define SS_PIN_2 5
MFRC522 mfrc522_1(SS_PIN_1, RST_PIN);
MFRC522 mfrc522_2(SS_PIN_2, RST_PIN);
// Variáveis para armazenar as chaves lidas
String chave8 = "";
String chave9 = "";

// Definindo as variáveis para contagem de tempo
unsigned long previousMillis = 0; // Armazena o tempo da última execução
const unsigned long intervalo = 30 * 60 * 1000; // 30 minutos em milissegundos
const unsigned long interval = 60 * 1000; // 1 minuto em milisegundos
unsigned long MillisAnterior = 0;

//- Definindo configurações de wifi e do ADAFRUIT
const char* ssid = "SEU SSID";
const char* password = "SUA SENHA";
#define IO_USERNAME  "SEU USERNAME"
#define IO_KEY       "SUA CHAVE"

const char* mqttserver = "io.adafruit.com";
const int mqttport = 1883;
const char* mqttUser = IO_USERNAME;
const char* mqttPassword = IO_KEY;

WiFiClient espClient;
PubSubClient client(espClient);
unsigned long lastMsg = 0;
#define MSG_BUFFER_SIZE	(50)
char msg[MSG_BUFFER_SIZE];
int value = 0;

//- Definindo configurações do servidor NTP
const char* ntpServer = "time.google.com";
const long gmtOffset_sec = -10800;
const int  daylightOffset_sec = 0;

//- Definindo as configurações HTTP
String url = "";
String macString = "";
// Variáveis para armazenar os valores das chaves e nomes
String chave1, chave2, chave3, chave4;
String nome1, nome2, nome3, nome4;
//- Definindo as configurações dos motores
Servo porta1;

//- Criar uma função para se conectar no Wifi
void conecta_wifi(){
  Serial.printf("Connecting to %s ", ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("CONNECTED");
  digitalWrite(LED_WIFI, HIGH);
}

//Função que pega a hora no servidor NTP
void printLocalTime(String frase){
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }
  File rFile = SPIFFS.open("/logs.txt", "a");
    if (!rFile) {
      Serial.println("Erro ao abrir arquivo!");
    }
    else {
      char tempo[64];
      strftime(tempo, sizeof(tempo), "%Y-%m-%d %H:%M:%S", &timeinfo);
      String entrada = frase + " " + tempo;
      rFile.println(entrada);
    }
    rFile.close();
  
}

//Função que abre o sistema de arquivos spiffs
void openFS(void) {
  if (!SPIFFS.begin()) {
    Serial.println("\nErro ao abrir o sistema de arquivos");
  }
  else {
    Serial.println("\nSistema de arquivos aberto com sucesso!");
  }
}

//Funções que escrevem no spiffs
void writeFile(String state, String path) {
  File rFile = SPIFFS.open(path, "w");
  if (!rFile) {
    Serial.println("Erro ao abrir arquivo!");
  }
  else {
    Serial.print("tamanho");
    Serial.println(rFile.size());
    rFile.println(state);
    Serial.print("Gravou: ");
    Serial.println(state);
  }
  rFile.close();
}

void appendFile(String state, String path) {
  File rFile = SPIFFS.open(path, "a");
  if (!rFile) {
    Serial.println("Erro ao abrir arquivo!");
  }
  else {
    Serial.print("tamanho");
    Serial.println(rFile.size());
    rFile.println(state);
    Serial.print("Gravou: ");
    Serial.println(state);
  }
  rFile.close();
}

//Função que ler arquivos do spiffs
String readFile(String path) {
  Serial.println("Read file");
  File rFile = SPIFFS.open(path, "r");
  if (!rFile) {
    Serial.println("Erro ao abrir arquivo!");
  }
  else {
    Serial.print("----------Lendo arquivo ");
    Serial.print(path);
    Serial.println("  ---------");
    String conteudo = "";
    while (rFile.available()) {
      String linha = rFile.readStringUntil('\n');
      conteudo += linha + "\n";
      Serial.print(linha);
    }
    rFile.close();
    return conteudo;
  }
}

//Função que reconecta no adafruit
void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Tentando conexão MQTT...");
    // Create a random client ID
    String clientId = "ESP32 - Sensores";
    clientId += String(random(0xffff), HEX);
    // Se conectado
    if (client.connect(clientId.c_str(), mqttUser, mqttPassword)) {
      Serial.println("conectado");
      // Depois de conectado, publique um anúncio ...
      client.publish("Pedro_IoT/feeds/entrousaiu", "Iniciando Comunicação");
      client.publish("Pedro_IoT/feeds/logs", "Iniciando Comunicação");
      client.publish("Pedro_IoT/feeds/led", "Iniciando Comunicação");
      //... e subscribe.
      client.subscribe("Pedro_IoT/feeds/led");
    } else {
      Serial.print("Falha, rc=");
      Serial.print(client.state());
      Serial.println(" Tentando novamente em 5s");
      delay(5000);
    }
  }
}

// Função que recebe mensagem do adafruit
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Messagem recebida [");
  Serial.print(topic);
  Serial.print("] ");
  String messageTemp;
  
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
    //messageTemp += (char)payload[i]; <----------Usar quando tiver uma mensagem na resposta do bloco
  }
  Serial.println();

  // Switch on the LED if an 1 was received as first character
  if ((char)payload[0] == '1') {
  Serial.println("Abrindo porta");
    porta1.write(0);
  } else {
   Serial.println("Fechando porta");
    porta1.write(180);
  }

}
// Função para extrair o valor associado a uma chave do conteúdo do arquivo
String extrairChave(String payload, int numero) {
  String chave;
  int pos = payload.indexOf("chave " + String(numero)); // Localiza a chave
  if (pos != -1) {
    int start = payload.indexOf(": ", pos) + 2; // Localiza o início da chave
    int end = payload.indexOf("\n", start); // Localiza o final da chave
    chave = payload.substring(start, end); // Extrai a chave
  }
  return chave;
}

// Função para extrair o nome de um usuário
String extrairNomeChave(String payload, int numero) {
  String nome;
  int pos = payload.indexOf("nome" + String(numero)); // Localiza a linha "nomeX"
  if (pos != -1) {
    int start = payload.indexOf("- ", pos) + 2; // Localiza o início do nome
    int end = payload.indexOf(" = ", start); // Localiza o final do nome
    nome = payload.substring(start, end); // Extrai o nome
  }
  return nome;
}

// Função que faz a requisição HTTP
void requisicao_HTTP(){
  HTTPClient http;
  http.begin(url);
  
  int httpCode = http.GET();
  
  if (httpCode > 0) { // Se a requisição foi bem-sucedida
    String payload = http.getString(); // Obtém o conteúdo do arquivo README.md
    
    // Exibe o conteúdo no Monitor Serial
    Serial.println("Conteúdo da requisição HTTP:");
    Serial.println(payload);

    // Realiza a extração das chaves com valores
    nome1 = extrairNomeChave(payload, 1);
    chave1 = extrairChave(payload, 1);
    nome2 = extrairNomeChave(payload, 2);
    chave2 = extrairChave(payload, 2);
    nome3 = extrairNomeChave(payload, 3);
    chave3 = extrairChave(payload, 3);
    nome4 = extrairNomeChave(payload, 4);
    chave4 = extrairChave(payload, 4);

    // Exibe os valores das chaves no Monitor Serial
    Serial.println("Nomes e chaves extraídas:");
    Serial.println("Nome 1: " + nome1 + " | Chave 1: " + chave1);
    Serial.println("Nome 2: " + nome2 + " | Chave 2: " + chave2);
    Serial.println("Nome 3: " + nome3 + " | Chave 3: " + chave3);
    Serial.println("Nome 4: " + nome4 + " | Chave 4: " + chave4);
    writeFile("Nome 1: " + nome1 + " | Chave 1: " + chave1, "/chaves.txt");
    appendFile("Nome 2: " + nome2 + " | Chave 2: " + chave2, "/chaves.txt");
    appendFile("Nome 3: " + nome3 + " | Chave 3: " + chave3, "/chaves.txt");
    appendFile("Nome 4: " + nome4 + " | Chave 4: " + chave4, "/chaves.txt");
  } else {
    Serial.printf("Erro na requisição HTTP: %d\n", httpCode);
    String keys = readFile("/chaves.txt");
    Serial.println(keys);
    nome1 = extrairNomeChave(keys, 1);
    chave1 = extrairChave(keys, 1);
    nome2 = extrairNomeChave(keys, 2);
    chave2 = extrairChave(keys, 2);
    nome3 = extrairNomeChave(keys, 3);
    chave3 = extrairChave(keys, 3);
    nome4 = extrairNomeChave(keys, 4);
    chave4 = extrairChave(keys, 4);
    Serial.println("Nomes e chaves extraídos anteriormente:");
    Serial.println("Nome 1: " + nome1 + " | Chave 1: " + chave1);
    Serial.println("Nome 2: " + nome2 + " | Chave 2: " + chave2);
    Serial.println("Nome 3: " + nome3 + " | Chave 3: " + chave3);
    Serial.println("Nome 4: " + nome4 + " | Chave 4: " + chave4);
  }
  http.end();
}

// Função que pega o número mac próprio do esp32
void get_mac(){
  uint8_t mac[6];
  WiFi.macAddress(mac);
  for (int i = 0; i < 6; i++) {
    if (mac[i] < 0x10) {
      macString += "0";
    }
    macString += String(mac[i], HEX);
  }
  Serial.print("Endereço MAC: ");
  Serial.println(macString);
}
void setup(){
  Serial.begin(9600);
  openFS();
  SPIFFS.format();
//- Depois definir os pinos dos leds como outputs assim como os pinos dos motores
  pinMode(LED_WIFI, OUTPUT);
  pinMode(LED_ADA, OUTPUT);
  pinMode(BOTAO, INPUT);
  porta1.attach(motor1);
  SPI.begin();// Inicializa a comunicação SPI
  mfrc522_1.PCD_Init(); // Inicializa os módulos
  mfrc522_2.PCD_Init();
//- Chama a função que conecta no wifi
  conecta_wifi();
//- Chama a função que conecta no dashboard
  client.setServer(mqttserver, 1883); // Publicar
  client.setCallback(callback); // Receber mensagem
// Configura o servidor ntp
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
//- Mandar a lista de logs atuais ao dashboard
  logs = readFile("/logs.txt");
  client.publish("Pedro_IoT/feeds/logs", logs.c_str());
// obtendo o endereço mac
get_mac();
url = "https://raw.githubusercontent.com/Pedro-IoT/chaves_final/refs/heads/main/" + macString + ".txt";
// Fazendo uma requisição HTTP para o arquivo txt
  requisicao_HTTP();
}

void loop(){
  unsigned long currentMillis = millis(); // Guarda o tempo real de execução
  unsigned long millisAtual = millis();
// Checa se o WiFi está conectado
  if(WiFi.status() == WL_CONNECTED){
    digitalWrite(LED_WIFI, HIGH);// Se estiver liga o led de controle
    if(digitalRead(BOTAO) == HIGH){// Se o botão for apertado atualiza as chaves
      requisicao_HTTP();
    }
    // Atualiza automaticamente as chaves a cada 30 min
    else if(currentMillis - previousMillis >= intervalo){
      previousMillis = currentMillis;
      requisicao_HTTP();
    }
    if (!client.connected()) {// Checa se não está conectado ao MQQT 
      digitalWrite(LED_ADA, LOW);// Se não estiver desliga o led de controle MQQT
      reconnect();// Se reconecta
    }
    digitalWrite(LED_ADA, HIGH);// Se estiver conectado acende o led de controle MQQT

// Atualiza os logs de entrada e saída a cada 1 minuto
    if(millisAtual - MillisAnterior >= interval){
      MillisAnterior = millisAtual;
      logs = readFile("/logs.txt");
      client.publish("Pedro_IoT/feeds/logs", logs.c_str());// Manda os logs para o MQQT
      Serial.println(logs);
    }
    client.loop();// Mantém a conexão MQQT e atualiza por mensagens novas
    
// Checa se há algum cartão próximo do módulo RFID
    if (mfrc522_1.PICC_IsNewCardPresent() && mfrc522_1.PICC_ReadCardSerial()) {
      chave8 = "";// Reseta a chave8
      for (byte i = 0; i < mfrc522_1.uid.size; i++) {
        chave8 += String(mfrc522_1.uid.uidByte[i], HEX);
      }
      Serial.print("Leitor 1 - Chave lida: ");
      Serial.println(chave8);

// Compara a chave lida com as chaves salvas
      if(chave8 == chave1){
        String frase1 = nome1 + " entrou!";
        Serial.println(frase1);
        porta1.write(0);// Abre a porta
        printLocalTime(frase1);// Registra no arquivo interno a hora de entrada ou saída
        client.publish("Pedro_IoT/feeds/entrousaiu", frase1.c_str());// Manda a frase ao MQQT
        delay(5000);
      }
      else if(chave8 == chave2){
        String frase2 = nome2 + " entrou!";
        Serial.println(frase2);
        porta1.write(0);
        printLocalTime(frase2);
        client.publish("Pedro_IoT/feeds/entrousaiu", frase2.c_str());
        delay(5000);
      }
      else if(chave8 == chave3){
        String frase3 = nome3 + " entrou!";
        Serial.println(frase3);
        porta1.write(0);
        printLocalTime(frase3);
        client.publish("Pedro_IoT/feeds/entrousaiu", frase3.c_str());
        delay(5000);
      }
      else if(chave8 == chave4){
        String frase4 = nome4 + " entrou!";
        Serial.println(frase4);
        porta1.write(0);
        printLocalTime(frase4);
        client.publish("Pedro_IoT/feeds/entrousaiu", frase4.c_str());
        delay(5000);
      }
// Se não houver nenhuma chave compátivel imprime acesso negado e manda para o MQQT
      else{
        Serial.println("Acesso negado!");
        client.publish("Pedro_IoT/feeds/entrousaiu", "Tentativa de entrada!");
      }
      mfrc522_1.PICC_HaltA();  // Interrompe a comunicação com o cartão
      porta1.write(180);// Fecha a porta se estiver aberta
    }
// Segue exatamente a mesma lógica do leitor 1 pórem imprime "Saiu!" ao invés de "Entrou!"
    if (mfrc522_2.PICC_IsNewCardPresent() && mfrc522_2.PICC_ReadCardSerial()) {
      chave9 = "";
      for (byte i = 0; i < mfrc522_2.uid.size; i++) {
        chave9 += String(mfrc522_2.uid.uidByte[i], HEX);
      }
      Serial.print("Leitor 2 - Chave lida: ");
      Serial.println(chave9);

      if(chave9 == chave1){
        String frase1 = nome1 + " saiu!";
        Serial.println(frase1);
        porta1.write(0);
        printLocalTime(frase1);
        client.publish("Pedro_IoT/feeds/entrousaiu", frase1.c_str());
        delay(5000);
      }
      else if(chave9 == chave2){
        String frase2 = nome2 + " saiu!";
        Serial.println(frase2);
        porta1.write(0);
        printLocalTime(frase2);
        client.publish("Pedro_IoT/feeds/entrousaiu", frase2.c_str());
        delay(5000);
      }
      else if(chave9 == chave3){
        String frase3 = nome3 + " saiu!";
        Serial.println(frase3);
        porta1.write(0);
        printLocalTime(frase3);
        client.publish("Pedro_IoT/feeds/entrousaiu", frase3.c_str());
        delay(5000);
      }
      else if(chave9 == chave4){
        String frase4 = nome4 + " saiu!";
        Serial.println(frase4);
        porta1.write(0);
        printLocalTime(frase4);
        client.publish("Pedro_IoT/feeds/entrousaiu", frase4.c_str());
        delay(5000);
      }
      else{
        Serial.println("Acesso negado!");
        client.publish("Pedro_IoT/feeds/entrousaiu", "Tentativa de entrada!");
      }
      mfrc522_2.PICC_HaltA();  // Interrompe a comunicação com o cartão
      porta1.write(180);
    }
  }
// Se não conseguir se conectar ao WiFi apaga os dois leds de controle
  else if(WiFi.status() != WL_CONNECTED){
    digitalWrite(LED_WIFI, LOW);
    digitalWrite(LED_ADA, LOW);
  }
}
