#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Wire.h>
#include <I2Cdev.h>
#include <MPU6050.h>

#define MPU 0x68
#define BUZZER_PIN 16

MPU6050 mpu;

const char* token_url = "https://yohan949.pythonanywhere.com/get_token";

// Token del dispositivo receptor
const char* registrationToken = "dObTAIwGS8-HCVZIfpyT9Z:APA91bE3aHxi17XWN8tnRWp-cVg_17mkNifSRtz1k9ksG_4R8ahBb7eBB8Oja6SdqLyyTCL-eYExE5IXrPaKDlO1d9rTXGITu7VSp3_6xrqGhiAzo8fHrYXBOqL3TS9QuVPOWbU_hlUp";

const char* ssid = "Base Secreta";
const char* password = "yGD3Um3Jxj";

int LED_ESP8266 = 2;
int D2 = 4, D1 = 5, D3 = 0;

int16_t ax, ay, az, gx, gy, gz;
float ax_2g, ay_2g, az_2g, gx_250_deg, gy_250_deg, gz_250_deg;
float a_abs, g_abs, ang_x, ang_y;

float umbral_de_aceleracion = 3.0;

void conexion() {
  Serial.println();
  Serial.println("Conectando a la red WiFi...");
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi conectado.");
  Serial.print("Dirección IP: ");
  Serial.println(WiFi.localIP());
}

String getAccessToken() {
  WiFiClientSecure client;
  HTTPClient http;

  client.setInsecure(); // Solo para pruebas. Remover en producción.
  
  Serial.println("Fetching access token...");
  if (http.begin(client, token_url)) {
    int httpCode = http.GET();
    if (httpCode > 0) {
      if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        int startIndex = payload.indexOf("access_token\":\"") + 15;
        int endIndex = payload.indexOf("\"", startIndex);
        String accessToken = payload.substring(startIndex, endIndex);
        Serial.println("Access token obtained: " + accessToken);
        http.end();
        return accessToken;
      } else {
        Serial.printf("HTTP GET failed with code: %d\n", httpCode);
      }
    } else {
      Serial.printf("HTTP GET failed, error: %s\n", http.errorToString(httpCode).c_str());
    }
    http.end();
  } else {
    Serial.println("Unable to connect to token URL");
  }
  return "";
}

void sendFCMNotification(String accessToken) {
  WiFiClientSecure client;
  client.setInsecure();

  if (!client.connect("fcm.googleapis.com", 443)) {
    Serial.println("Connection failed");
    return;
  }

  if (accessToken == "") {
    Serial.println("Failed to obtain access token");
    return;
  }

  Serial.println("Access token being used: " + accessToken);
  Serial.println("Registration token being used: " + String(registrationToken));

  String payload =  
    "{"
    "\"message\": {"
    "\"token\": \"" + String(registrationToken) + "\","
    "\"data\": {"
    "\"title\": \"ALERTA!\","
    "\"body\": \"EL DISPOSITIVO HA DETECTADO UNA CAÍDA\""
    "}"
    "}"
    "}";

  Serial.println("Payload: " + payload);

  String request = 
    String("POST ") + "/v1/projects/netbelt-472e1/messages:send HTTP/1.1\r\n" +
    "Host: fcm.googleapis.com\r\n" +
    "Authorization: Bearer " + accessToken + "\r\n" +
    "Content-Type: application/json\r\n" +
    "Content-Length: " + payload.length() + "\r\n\r\n" +
    payload;

  Serial.println("Request: " + request);

  client.print(request);

  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") {
      break;
    }
  }

  String response = client.readString();
  Serial.println("Response: ");
  Serial.println(response);
}

void detectarCaida() {
  if (a_abs > umbral_de_aceleracion) {
    Serial.println("Posible caída detectada!");
    digitalWrite(BUZZER_PIN, HIGH);
    delay(500);
    digitalWrite(BUZZER_PIN, LOW);
    String accessToken = getAccessToken();
    Serial.print("Obtained Access Token: " + accessToken);
    sendFCMNotification(accessToken);
  }
}

void calculos() {
  ax_2g = ax / 16384.0;
  ay_2g = ay / 16384.0;
  az_2g = az / 16384.0;
  gx_250_deg = gx / 131.07;
  gy_250_deg = gy / 131.07;
  gz_250_deg = gz / 131.07;

  a_abs = pow(pow(ax_2g, 2) + pow(ay_2g, 2) + pow(az_2g, 2), 0.5);
  g_abs = sqrt(pow(gx_250_deg, 2) + pow(gy_250_deg, 2) + pow(gz_250_deg, 2));
  
  ang_x = atan(ax / sqrt(pow(ay, 2) + pow(az, 2))) * (180.0 / 3.14);
  ang_y = atan(ay / sqrt(pow(ax, 2) + pow(az, 2))) * (180.0 / 3.14);
}

void leerDatosMPU6050() {
  mpu.getMotion6(&gx, &gy, &gz, &ax, &ay, &az);
  calculos();

  Serial.println("");
  Serial.print("Acelerometro: ");
  Serial.print("X = "); Serial.print(ax);
  Serial.print(" | Y = "); Serial.print(ay);
  Serial.print(" | Z = "); Serial.println(az);

  Serial.print("Giroscopio: ");
  Serial.print("X = "); Serial.print(gx);
  Serial.print(" | Y = "); Serial.print(gy);
  Serial.print(" | Z = "); Serial.println(gz);

  Serial.print("Aceleracion total: ");
  Serial.print(a_abs);

  Serial.print("  Velocidad angular total: ");
  Serial.println(g_abs);

  Serial.print("Angulo X: ");
  Serial.print(ang_x);

  Serial.print("  Angulo Y: ");
  Serial.println(ang_y);

  delay(100);
}

void setup() {
  Serial.begin(921600);
  Wire.begin(D2, D1);

  pinMode(LED_ESP8266, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  Serial.println("Inicializando el MPU-6050...");
  mpu.initialize();

  Serial.println(mpu.testConnection() ? "MPU6050 conectado correctamente" : "Error al conectar el MPU6050");
  conexion();
}

void loop() {
  leerDatosMPU6050();
  detectarCaida();
}
