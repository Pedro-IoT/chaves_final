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

String s;                   // Variável genérica de string
String logs;                // Variável para armazenar logs do sistema

//- Definindo as configurações RFID
#define RST_PIN 4           // Pino de reset para os leitores RFID
#define SS_PIN_1 14         // Pino SS para o leitor RFID 1
#define SS_PIN_2 5          // Pino SS para o leitor RFID 2
MFRC522 mfrc522_1(SS_PIN_1, RST_PIN);
MFRC522 mfrc522_2(SS_PIN_2, RST_PIN);

// Variáveis para armazenar as chaves lidas
String chave8 = "";         // Chave lida pelo leitor RFID 1
String chave9 = "";         // Chave lida pelo leitor RFID 2

// Definindo as variáveis para contagem de tempo
unsigned long previousMillis = 0; // Armazena o tempo da última execução de atualização
const unsigned long intervalo = 30 * 60 * 1000; // Intervalo de 30 minutos em milissegundos
const unsigned long interval = 60 * 1000;       // Intervalo de 1 minuto para logs
unsigned long MillisAnterior = 0;              // Variável para controle de tempo de logs

//- Definindo configurações de WiFi e do Adafruit
const char* ssid = "SEU SSID"; // Nome da rede WiFi
const char* password ="SUA SENHA";// Senha da rede WiFi
#define IO_USERNAME  "SEU USERNAME"// Nome de usuário no Adafruit IO
#define IO_KEY       "SUA CHAVE" // Chave do Adafruit IO

const char* mqttserver = "io.adafruit.com"; // Endereço do servidor MQTT
const int mqttport = 1883;                  // Porta padrão para o MQTT
const char* mqttUser = IO_USERNAME;         // Nome de usuário MQTT
const char* mqttPassword = IO_KEY;          // Senha MQTT

WiFiClient espClient;       // Instância do cliente WiFi
PubSubClient client(espClient); // Instância do cliente MQTT
unsigned long lastMsg = 0;  // Controle de tempo para última mensagem MQTT
#define MSG_BUFFER_SIZE	(50) // Tamanho do buffer para mensagens MQTT
char msg[MSG_BUFFER_SIZE];  // Buffer para mensagens MQTT
int value = 0;              // Valor auxiliar

//- Definindo configurações do servidor NTP
const char* ntpServer = "time.google.com"; // Servidor NTP para sincronização de horário
const long gmtOffset_sec = -10800;         // Deslocamento do horário GMT (-3 horas para o Brasil)
const int  daylightOffset_sec = 0;         // Sem horário de verão

//- Definindo as configurações HTTP
String url = "";           // URL para requisição HTTP
String macString = "";     // String para armazenar o endereço MAC do ESP32

// Variáveis para armazenar os valores das chaves e nomes
String chave1, chave2, chave3, chave4; // Variáveis para armazenar chaves
String nome1, nome2, nome3, nome4;     // Variáveis para armazenar nomes

//- Definindo as configurações dos motores
Servo porta1;              // Controle do servomotor

// Função que conecta o ESP32 à rede Wi-Fi usando as credenciais fornecidas
void conecta_wifi(){
  Serial.printf("Connecting to %s ", ssid); // Exibe a mensagem de tentativa de conexão
  WiFi.begin(ssid, password); // Inicia a conexão com o Wi-Fi
  while (WiFi.status() != WL_CONNECTED) { // Aguarda a conexão
    delay(500); // Espera meio segundo antes de tentar novamente
    Serial.print("."); // Exibe ponto de progresso
  }
  Serial.println("CONNECTED"); // Exibe quando a conexão for bem-sucedida
  digitalWrite(LED_WIFI, HIGH); // Acende o LED Wi-Fi indicando conexão
}

// Função que obtém a hora local do servidor NTP e a registra em um arquivo no SPIFFS
void printLocalTime(String frase){
  struct tm timeinfo; // Estrutura para armazenar a hora
  if(!getLocalTime(&timeinfo)){ // Se não conseguir obter a hora, exibe erro
    Serial.println("Failed to obtain time");
    return;
  }
  // Abre o arquivo de logs no SPIFFS para adicionar a entrada
  File rFile = SPIFFS.open("/logs.txt", "a");
  if (!rFile) { // Verifica se o arquivo foi aberto corretamente
    Serial.println("Erro ao abrir arquivo!");
  }
  else {
    char tempo[64]; // Variável para armazenar a data e hora formatadas
    strftime(tempo, sizeof(tempo), "%Y-%m-%d %H:%M:%S", &timeinfo); // Formata a hora
    String entrada = frase + " " + tempo; // Combina a frase com a data e hora
    rFile.println(entrada); // Escreve a entrada no arquivo
  }
  rFile.close(); // Fecha o arquivo
}

// Função que inicializa o sistema de arquivos SPIFFS
void openFS(void) {
  if (!SPIFFS.begin()) { // Verifica se o SPIFFS foi inicializado corretamente
    Serial.println("\nErro ao abrir o sistema de arquivos");
  }
  else {
    Serial.println("\nSistema de arquivos aberto com sucesso!");
  }
}

// Função que escreve dados em um arquivo no SPIFFS
void writeFile(String state, String path) {
  File rFile = SPIFFS.open(path, "w"); // Abre o arquivo para escrita
  if (!rFile) { // Se não conseguir abrir, exibe erro
    Serial.println("Erro ao abrir arquivo!");
  }
  else {
    Serial.print("tamanho"); 
    Serial.println(rFile.size()); // Exibe o tamanho do arquivo
    rFile.println(state); // Escreve o estado no arquivo
    Serial.print("Gravou: ");
    Serial.println(state); // Exibe o estado gravado
  }
  rFile.close(); // Fecha o arquivo
}

// Função que adiciona dados ao final de um arquivo existente no SPIFFS
void appendFile(String state, String path) {
  File rFile = SPIFFS.open(path, "a"); // Abre o arquivo em modo de append
  if (!rFile) { // Se não conseguir abrir, exibe erro
    Serial.println("Erro ao abrir arquivo!");
  }
  else {
    Serial.print("tamanho");
    Serial.println(rFile.size()); // Exibe o tamanho do arquivo
    rFile.println(state); // Adiciona o estado ao arquivo
    Serial.print("Gravou: ");
    Serial.println(state); // Exibe o estado gravado
  }
  rFile.close(); // Fecha o arquivo
}

// Função que lê o conteúdo de um arquivo no SPIFFS
String readFile(String path) {
  Serial.println("Read file"); // Exibe a mensagem indicando leitura
  File rFile = SPIFFS.open(path, "r"); // Abre o arquivo para leitura
  if (!rFile) { // Se não conseguir abrir, exibe erro
    Serial.println("Erro ao abrir arquivo!");
  }
  else {
    Serial.print("----------Lendo arquivo ");
    Serial.print(path);
    Serial.println("  ---------");
    String conteudo = ""; // Variável para armazenar o conteúdo do arquivo
    while (rFile.available()) { // Lê o arquivo enquanto houver dados
      String linha = rFile.readStringUntil('\n'); // Lê até a próxima linha
      conteudo += linha + "\n"; // Adiciona a linha ao conteúdo
      Serial.print(linha); // Exibe a linha lida
    }
    rFile.close(); // Fecha o arquivo
    return conteudo; // Retorna o conteúdo lido
  }
}

// Função que reconecta o ESP32 ao broker MQTT
void reconnect() {
  while (!client.connected()) { // Loop enquanto não estiver conectado
    Serial.print("Tentando conexão MQTT...");
    String clientId = "ESP32 - Sensores"; // Cria um ID de cliente aleatório
    clientId += String(random(0xffff), HEX); // Adiciona um valor aleatório ao ID
    // Tenta conectar ao MQTT com as credenciais fornecidas
    if (client.connect(clientId.c_str(), mqttUser, mqttPassword)) {
      Serial.println("conectado"); // Exibe quando conectado
      // Publica uma mensagem de início ao conectar
      client.publish("Pedro_IoT/feeds/entrousaiu", "Iniciando Comunicação");
      client.publish("Pedro_IoT/feeds/logs", "Iniciando Comunicação");
      client.publish("Pedro_IoT/feeds/led", "Iniciando Comunicação");
      // Subscreve em um tópico do MQTT
      client.subscribe("Pedro_IoT/feeds/led");
    } else {
      Serial.print("Falha, rc="); // Exibe o erro se não conseguir conectar
      Serial.print(client.state());
      Serial.println(" Tentando novamente em 5s");
      delay(5000); // Espera 5 segundos antes de tentar novamente
    }
  }
}

// Função que recebe mensagens do broker MQTT e executa ações com base nelas
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Messagem recebida [");
  Serial.print(topic); // Exibe o tópico da mensagem
  Serial.print("] ");
  String messageTemp;
  
  for (int i = 0; i < length; i++) { // Lê cada byte da mensagem
    Serial.print((char)payload[i]); // Exibe o conteúdo da mensagem
  }
  Serial.println();

  // Se a mensagem contiver '1', abre a porta
  if ((char)payload[0] == '1') {
    Serial.println("Abrindo porta");
    porta1.write(0); // Abre a porta
  } else {
    // Se a mensagem contiver qualquer outra coisa, fecha a porta
    Serial.println("Fechando porta");
    porta1.write(180); // Fecha a porta
  }
}
// Função para extrair o valor associado a uma chave do conteúdo do arquivo
String extrairChave(String payload, int numero) {
  String chave;
  // Localiza a posição da chave no conteúdo do arquivo, baseado no número passado
  int pos = payload.indexOf("chave " + String(numero));
  
  // Se a chave for encontrada
  if (pos != -1) {
    // Localiza o início da chave (o valor após ": ")
    int start = payload.indexOf(": ", pos) + 2;
    // Localiza o final da chave (quando a linha termina com uma nova linha)
    int end = payload.indexOf("\n", start);
    // Extrai a chave usando a posição de início e fim
    chave = payload.substring(start, end);
  }
  return chave; // Retorna a chave extraída
}

// Função para extrair o nome de um usuário
String extrairNomeChave(String payload, int numero) {
  String nome;
  // Localiza a posição do nome no conteúdo do arquivo, baseado no número
  int pos = payload.indexOf("nome" + String(numero));
  
  // Se o nome for encontrado
  if (pos != -1) {
    // Localiza o início do nome (o valor após "- ")
    int start = payload.indexOf("- ", pos) + 2;
    // Localiza o final do nome (o valor após " = ")
    int end = payload.indexOf(" = ", start);
    // Extrai o nome usando a posição de início e fim
    nome = payload.substring(start, end);
  }
  return nome; // Retorna o nome extraído
}

// Função que faz a requisição HTTP para obter o conteúdo do arquivo de chaves
void requisicao_HTTP() {
  HTTPClient http; 
  http.begin(url); // Inicializa a requisição HTTP com a URL configurada
  
  int httpCode = http.GET(); // Realiza a requisição HTTP GET
  
  // Verifica se a requisição foi bem-sucedida (código de resposta > 0)
  if (httpCode > 0) {
    String payload = http.getString(); // Obtém o conteúdo da resposta HTTP
    
    // Exibe o conteúdo obtido no Monitor Serial para conferência
    Serial.println("Conteúdo da requisição HTTP:");
    Serial.println(payload);

    // Realiza a extração das chaves e nomes das respostas do servidor
    nome1 = extrairNomeChave(payload, 1); // Extrai o nome do primeiro usuário
    chave1 = extrairChave(payload, 1); // Extrai a chave do primeiro usuário
    nome2 = extrairNomeChave(payload, 2); // Extrai o nome do segundo usuário
    chave2 = extrairChave(payload, 2); // Extrai a chave do segundo usuário
    nome3 = extrairNomeChave(payload, 3); // Extrai o nome do terceiro usuário
    chave3 = extrairChave(payload, 3); // Extrai a chave do terceiro usuário
    nome4 = extrairNomeChave(payload, 4); // Extrai o nome do quarto usuário
    chave4 = extrairChave(payload, 4); // Extrai a chave do quarto usuário

    // Exibe os valores das chaves e nomes extraídos no Monitor Serial
    Serial.println("Nomes e chaves extraídas:");
    Serial.println("Nome 1: " + nome1 + " | Chave 1: " + chave1);
    Serial.println("Nome 2: " + nome2 + " | Chave 2: " + chave2);
    Serial.println("Nome 3: " + nome3 + " | Chave 3: " + chave3);
    Serial.println("Nome 4: " + nome4 + " | Chave 4: " + chave4);

    // Salva as chaves extraídas em um arquivo para persistência (chaves.txt)
    writeFile("Nome 1: " + nome1 + " | Chave 1: " + chave1, "/chaves.txt");
    appendFile("Nome 2: " + nome2 + " | Chave 2: " + chave2, "/chaves.txt");
    appendFile("Nome 3: " + nome3 + " | Chave 3: " + chave3, "/chaves.txt");
    appendFile("Nome 4: " + nome4 + " | Chave 4: " + chave4, "/chaves.txt");
  } 
  // Caso a requisição não tenha sido bem-sucedida
  else {
    Serial.printf("Erro na requisição HTTP: %d\n", httpCode); // Exibe o erro no Monitor Serial
    String keys = readFile("/chaves.txt"); // Lê as chaves previamente salvas no arquivo
    Serial.println(keys); // Exibe as chaves no Monitor Serial

    // Extrai novamente as chaves e nomes salvos no arquivo
    nome1 = extrairNomeChave(keys, 1);
    chave1 = extrairChave(keys, 1);
    nome2 = extrairNomeChave(keys, 2);
    chave2 = extrairChave(keys, 2);
    nome3 = extrairNomeChave(keys, 3);
    chave3 = extrairChave(keys, 3);
    nome4 = extrairNomeChave(keys, 4);
    chave4 = extrairChave(keys, 4);

    // Exibe as chaves extraídas previamente no Monitor Serial
    Serial.println("Nomes e chaves extraídos anteriormente:");
    Serial.println("Nome 1: " + nome1 + " | Chave 1: " + chave1);
    Serial.println("Nome 2: " + nome2 + " | Chave 2: " + chave2);
    Serial.println("Nome 3: " + nome3 + " | Chave 3: " + chave3);
    Serial.println("Nome 4: " + nome4 + " | Chave 4: " + chave4);
  }
  http.end(); // Finaliza a requisição HTTP
}

// Função que pega o número MAC do ESP32 e o exibe no Monitor Serial
void get_mac() {
  uint8_t mac[6]; 
  WiFi.macAddress(mac); // Obtém o endereço MAC do dispositivo
  for (int i = 0; i < 6; i++) {
    // Formata o endereço MAC como uma string hexadecimal
    if (mac[i] < 0x10) {
      macString += "0";
    }
    macString += String(mac[i], HEX);
  }
  Serial.print("Endereço MAC: ");
  Serial.println(macString); // Exibe o endereço MAC no Monitor Serial
}

void setup() {
  Serial.begin(9600); // Inicia a comunicação serial
  openFS(); // Abre o sistema de arquivos
  SPIFFS.format(); // Formata o sistema de arquivos (SPIFFS)
  
  // Configuração de pinos de LEDs e botão
  pinMode(LED_WIFI, OUTPUT); // Define o pino do LED WiFi como saída
  pinMode(LED_ADA, OUTPUT); // Define o pino do LED MQTT como saída
  pinMode(BOTAO, INPUT); // Define o pino do botão como entrada
  porta1.attach(motor1); // Associa a porta1 ao motor1
  
  // Inicializa o SPI e os módulos RFID
  SPI.begin(); 
  mfrc522_1.PCD_Init(); 
  mfrc522_2.PCD_Init();
  
  // Conecta-se ao Wi-Fi
  conecta_wifi();
  
  // Conecta-se ao servidor MQTT
  client.setServer(mqttserver, 1883); 
  client.setCallback(callback); // Define a função de callback para receber mensagens MQTT
  
  // Configura o servidor NTP
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  
  // Envia os logs para o dashboard MQTT
  logs = readFile("/logs.txt");
  client.publish("Pedro_IoT/feeds/logs", logs.c_str());
  
  // Obtém o endereço MAC do ESP32
  get_mac();
  
  // Configura a URL para fazer a requisição HTTP
  url = "https://raw.githubusercontent.com/Pedro-IoT/chaves_final/refs/heads/main/" + macString + ".txt";
  
  // Faz a requisição HTTP para obter as chaves e nomes
  requisicao_HTTP();
}
void loop() {
  unsigned long currentMillis = millis(); // Guarda o tempo real de execução
  unsigned long millisAtual = millis(); // Armazena o valor atual do tempo para as verificações de intervalos
  
  // Checa se o WiFi está conectado
  if(WiFi.status() == WL_CONNECTED) {
    digitalWrite(LED_WIFI, HIGH); // Se estiver conectado ao Wi-Fi, acende o LED de controle Wi-Fi

    // Se o botão for pressionado, atualiza as chaves via requisição HTTP
    if(digitalRead(BOTAO) == HIGH) {
      requisicao_HTTP(); // Chama a função para requisitar as chaves via HTTP
    } 
    // Caso contrário, atualiza as chaves automaticamente a cada 30 minutos
    else if(currentMillis - previousMillis >= intervalo) {
      previousMillis = currentMillis; // Atualiza o tempo de referência
      requisicao_HTTP(); // Chama a função de atualização das chaves
    }

    // Verifica se o cliente MQTT está desconectado
    if (!client.connected()) {
      digitalWrite(LED_ADA, LOW); // Se desconectado, apaga o LED de controle MQTT
      reconnect(); // Chama a função para tentar reconectar ao servidor MQTT
    }
    digitalWrite(LED_ADA, HIGH); // Se estiver conectado ao MQTT, acende o LED de controle MQTT

    // Atualiza os logs a cada 1 minuto e envia para o servidor MQTT
    if(millisAtual - MillisAnterior >= interval) {
      MillisAnterior = millisAtual; // Atualiza o tempo de referência
      logs = readFile("/logs.txt"); // Lê o arquivo de logs
      client.publish("Pedro_IoT/feeds/logs", logs.c_str()); // Envia os logs para o MQTT
      Serial.println(logs); // Exibe os logs no terminal serial
    }

    client.loop(); // Mantém a conexão MQTT ativa e verifica por novas mensagens

    // Verifica se há algum cartão RFID próximo para leitura no primeiro módulo RFID
    if (mfrc522_1.PICC_IsNewCardPresent() && mfrc522_1.PICC_ReadCardSerial()) {
      chave8 = ""; // Reseta a chave lida
      for (byte i = 0; i < mfrc522_1.uid.size; i++) {
        chave8 += String(mfrc522_1.uid.uidByte[i], HEX); // Converte o UID do cartão para string hexadecimal
      }
      Serial.print("Leitor 1 - Chave lida: ");
      Serial.println(chave8); // Exibe a chave lida no terminal serial

      // Compara a chave lida com as chaves armazenadas para determinar se a pessoa tem acesso
      if(chave8 == chave1) {
        String frase1 = nome1 + " entrou!"; // Monta a frase de entrada
        Serial.println(frase1); // Exibe a frase no terminal serial
        porta1.write(0); // Aciona o mecanismo para abrir a porta
        printLocalTime(frase1); // Registra a hora de entrada
        client.publish("Pedro_IoT/feeds/entrousaiu", frase1.c_str()); // Envia a mensagem para o MQTT
        delay(5000); // Aguarda 5 segundos antes de continuar
      }
      else if(chave8 == chave2) {
        String frase2 = nome2 + " entrou!";
        Serial.println(frase2);
        porta1.write(0);
        printLocalTime(frase2);
        client.publish("Pedro_IoT/feeds/entrousaiu", frase2.c_str());
        delay(5000);
      }
      else if(chave8 == chave3) {
        String frase3 = nome3 + " entrou!";
        Serial.println(frase3);
        porta1.write(0);
        printLocalTime(frase3);
        client.publish("Pedro_IoT/feeds/entrousaiu", frase3.c_str());
        delay(5000);
      }
      else if(chave8 == chave4) {
        String frase4 = nome4 + " entrou!";
        Serial.println(frase4);
        porta1.write(0);
        printLocalTime(frase4);
        client.publish("Pedro_IoT/feeds/entrousaiu", frase4.c_str());
        delay(5000);
      }
      // Se a chave não for compatível, informa acesso negado
      else {
        Serial.println("Acesso negado!");
        client.publish("Pedro_IoT/feeds/entrousaiu", "Tentativa de entrada!"); // Envia tentativa de entrada para o MQTT
      }
      mfrc522_1.PICC_HaltA(); // Interrompe a comunicação com o cartão RFID
      porta1.write(180); // Fecha a porta se estava aberta
    }

    // Lógica semelhante para o segundo módulo RFID, mas registra "Saiu!" em vez de "Entrou!"
    if (mfrc522_2.PICC_IsNewCardPresent() && mfrc522_2.PICC_ReadCardSerial()) {
      chave9 = ""; // Reseta a chave lida
      for (byte i = 0; i < mfrc522_2.uid.size; i++) {
        chave9 += String(mfrc522_2.uid.uidByte[i], HEX); // Converte o UID do cartão para string hexadecimal
      }
      Serial.print("Leitor 2 - Chave lida: ");
      Serial.println(chave9); // Exibe a chave lida no terminal serial

      // Verifica se a chave lida é válida e registra a saída
      if(chave9 == chave1) {
        String frase1 = nome1 + " saiu!";
        Serial.println(frase1);
        porta1.write(0);
        printLocalTime(frase1);
        client.publish("Pedro_IoT/feeds/entrousaiu", frase1.c_str());
        delay(5000);
      }
      else if(chave9 == chave2) {
        String frase2 = nome2 + " saiu!";
        Serial.println(frase2);
        porta1.write(0);
        printLocalTime(frase2);
        client.publish("Pedro_IoT/feeds/entrousaiu", frase2.c_str());
        delay(5000);
      }
      else if(chave9 == chave3) {
        String frase3 = nome3 + " saiu!";
        Serial.println(frase3);
        porta1.write(0);
        printLocalTime(frase3);
        client.publish("Pedro_IoT/feeds/entrousaiu", frase3.c_str());
        delay(5000);
      }
      else if(chave9 == chave4) {
        String frase4 = nome4 + " saiu!";
        Serial.println(frase4);
        porta1.write(0);
        printLocalTime(frase4);
        client.publish("Pedro_IoT/feeds/entrousaiu", frase4.c_str());
        delay(5000);
      }
      else {
        Serial.println("Acesso negado!");
        client.publish("Pedro_IoT/feeds/entrousaiu", "Tentativa de entrada!");
      }
      mfrc522_2.PICC_HaltA(); // Interrompe a comunicação com o cartão RFID
      porta1.write(180); // Fecha a porta se estava aberta
    }
  }
  // Se não conseguir se conectar ao Wi-Fi, apaga os LEDs de controle
  else if(WiFi.status() != WL_CONNECTED) {
    digitalWrite(LED_WIFI, LOW); // Apaga o LED de Wi-Fi
    digitalWrite(LED_ADA, LOW);  // Apaga o LED MQTT
  }
}
