#include <TinyGPS.h>
#include <HardwareSerial.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <DHT.h>

bool usarThingsBoard = true;

// Dados acesso ThingsBoard
const String devideToken = "";

// Dados da sua rede WiFi
// const char* ssid = "";
// const char* password = "";

const char* ssid = "";
const char* password = "";

// Configurar pinos
const int RXPin = 16;
const int TXPin = 17;

// configurações para o sensor DHT
#define DHTPIN 4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// Criar objeto Serial2
HardwareSerial serialGPS(2);

// Criar objeto GPS
TinyGPS gps1;

// Guardar última posição válida
float lastLatitude = 0.0;
float lastLongitude = 0.0;

// Flag para saber se houve nova leitura
bool novaPosicaoDisponivel = false;

// Controle de tempo de envio
unsigned long lastSendTime = 0;
const unsigned long sendInterval = 20000;  

void setup() {
  Serial.begin(115200);
  serialGPS.begin(9600, SERIAL_8N1, RXPin, TXPin);

  Serial.println("Inicializando WiFi...");
  WiFi.begin(ssid, password);
  dht.begin();

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi conectado!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  Serial.println("Aguardando dados do GPS...");
}

void loop() {
  bool recebido = false;

  // Ler dados do GPS
  while (serialGPS.available()) {
    char cIn = serialGPS.read();
    Serial.write(cIn);
    recebido = gps1.encode(cIn);
  }

  unsigned long now = millis();

  // Sempre que houver novos dados válidos
  if (recebido) {
    float latitude, longitude;
    gps1.f_get_position(&latitude, &longitude);

    if (latitude != TinyGPS::GPS_INVALID_F_ANGLE && longitude != TinyGPS::GPS_INVALID_F_ANGLE) {
      Serial.print("Nova posição válida: ");
      Serial.print(latitude, 6);
      Serial.print(", ");
      Serial.println(longitude, 6);

      lastLatitude = latitude;
      lastLongitude = longitude;
      novaPosicaoDisponivel = true;
    }
  }

  // A cada intervalo de tempo pré estabelecido , enviar a última posição ou erro
  if (now - lastSendTime >= sendInterval) {
    if (novaPosicaoDisponivel) {
      Serial.println("Enviando posição...");
      enviarPosicao(lastLatitude, lastLongitude);
      // Resetar posição para aguardar nova leitura
      lastLatitude = 0.0;
      lastLongitude = 0.0;
      novaPosicaoDisponivel = false;
    } else {
      Serial.println("Nenhuma nova posição. Enviando erro...");
      enviarErro();
    }

    // Sempre enviar temperatura depois
    float temperatura = dht.readTemperature();
    if (isnan(temperatura)) {
      Serial.println("Erro ao ler temperatura do DHT11!");
    } else {
      Serial.print("Temperatura lida: ");
      Serial.println(temperatura);
      enviarTemperatura(temperatura);
    }

    lastSendTime = now;
  }
}


String gerarTimestampISO() {
  int ano;
  byte mes, dia, hora, minuto, segundo, centesimo;
  unsigned long idadeInfo;
  gps1.crack_datetime(&ano, &mes, &dia, &hora, &minuto, &segundo, &centesimo, &idadeInfo);

  char buffer[30];
  snprintf(buffer, sizeof(buffer), "%04d-%02d-%02dT%02d:%02d:%02d.000Z",
           ano, mes, dia, hora, minuto, segundo);
  return String(buffer);
}


void enviarPosicao(float lat, float lon) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi desconectado! Tentando reconectar...");
    WiFi.reconnect();
    delay(1000);
    return;
  }

  WiFiClientSecure* client = new WiFiClientSecure;
  client->setInsecure();

  HTTPClient http;
  String url;
  String payload;

  if (usarThingsBoard) {
    url = "https://thingsboard.cloud/api/v1/" + devideToken+ "/telemetry";
    payload = "{\"latitude\":" + String(lat, 6) + ",\"longitude\":" + String(lon, 6) + "}";
  } else {
    url = "https://cidadesinteligentes.lsdi.ufma.br/interscity_lh/adaptor/resources/8ea6c777-afbb-4ffd-88a6-6bf26c5e45f7/data/sensor_lat_lon";
    String timestamp = gerarTimestampISO();
    payload = "{\"data\":[{\"lat\":" + String(lat, 6) +
              ",\"lon\":" + String(lon, 6) +
              ",\"timestamp\":\"" + timestamp + "\"}]}";
  }

  Serial.print("Enviando posição para ");
  Serial.println(url);

  http.begin(*client, url);
  http.addHeader("Content-Type", "application/json");

  int httpResponseCode = http.POST(payload);

  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.print("Resposta HTTP: ");
    Serial.println(httpResponseCode);
    Serial.print("Corpo: ");
    Serial.println(response);
  } else {
    Serial.print("Erro ao enviar POST. Código: ");
    Serial.println(httpResponseCode);
  }

  http.end();
}


void enviarErro() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi desconectado! Tentando reconectar...");
    WiFi.reconnect();
    delay(1000);
    return;
  }

  WiFiClientSecure* client = new WiFiClientSecure;
  client->setInsecure();

  HTTPClient http;
  String url;
  String payload;

  if (usarThingsBoard) {
    // ThingsBoard endpoint
    url = "https://thingsboard.cloud/api/v1/" + devideToken+ "/telemetry";
    // Simples contador de erro
    payload = "{\"gps_error_count\":1}";
  } else {
    // InterSCity endpoint
    url = "https://cidadesinteligentes.lsdi.ufma.br/interscity_lh/adaptor/resources/8ea6c777-afbb-4ffd-88a6-6bf26c5e45f7/data/error_lat_long";
    String timestamp = gerarTimestampISO();
    payload = "{\"data\":[{\"error\":\"can't get lat/lon position\",\"timestamp\":\"" + timestamp + "\"}]}";
  }

  Serial.print("Enviando erro para ");
  Serial.println(url);

  http.begin(*client, url);
  http.addHeader("Content-Type", "application/json");

  int httpResponseCode = http.POST(payload);

  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.print("Resposta HTTP: ");
    Serial.println(httpResponseCode);
    Serial.print("Corpo: ");
    Serial.println(response);
  } else {
    Serial.print("Erro ao enviar POST. Código: ");
    Serial.println(httpResponseCode);
  }

  http.end();
}


void enviarTemperatura(float temperatura) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi desconectado! Tentando reconectar...");
    WiFi.reconnect();
    delay(1000);
    return;
  }

  WiFiClientSecure* client = new WiFiClientSecure;
  client->setInsecure();

  HTTPClient http;
  String url;
  String payload;

  if (usarThingsBoard) {
    // THINGSBOARD URL
    url = "https://thingsboard.cloud/api/v1/" + devideToken+ "/telemetry";
    payload = "{\"temperatura\":" + String(temperatura, 1) + "}";
  } else {
    // INTERSCITY URL
    url = "https://cidadesinteligentes.lsdi.ufma.br/interscity_lh/adaptor/resources/fb79e2b0-3def-48c8-8237-f5bcb7ce8914/data/sensor_temperatura";
    String timestamp = gerarTimestampISO();
    payload = "{\"data\":[{\"temperatura\":" + String(temperatura, 1) +
              ",\"timestamp\":\"" + timestamp + "\"}]}";
  }

  Serial.print("Enviando temperatura para ");
  Serial.println(url);

  http.begin(*client, url);
  http.addHeader("Content-Type", "application/json");

  int httpResponseCode = http.POST(payload);

  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.print("Resposta HTTP: ");
    Serial.println(httpResponseCode);
    Serial.print("Corpo: ");
    Serial.println(response);
  } else {
    Serial.print("Erro ao enviar POST. Código: ");
    Serial.println(httpResponseCode);
  }

  http.end();
}
