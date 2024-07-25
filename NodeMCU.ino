#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Wire.h>
#include <MPU6050.h>
#include <FirebaseESP8266.h>

#define MPU 0x68
#define BUZZER_PIN 16

MPU6050 mpu;

FirebaseData firebaseData;
FirebaseConfig firebaseConfig;
FirebaseAuth firebaseAuth;

const char* token_url = "https://yohan949.pythonanywhere.com/get_token";

// Token del dispositivo receptor
const char* registrationToken = "dObTAIwGS8-HCVZIfpyT9Z:APA91bE3aHxi17XWN8tnRWp-cVg_17mkNifSRtz1k9ksG_4R8ahBb7eBB8Oja6SdqLyyTCL-eYExE5IXrPaKDlO1d9rTXGITu7VSp3_6xrqGhiAzo8fHrYXBOqL3TS9QuVPOWbU_hlUp";

const char* ssid = "Base Secreta";
const char* password = "yGD3Um3Jxj";

int LED_ESP8266 = 2;
int D2 = 4, D1 = 5, PULSADOR = 0;
int buttonState;
int lastButtonState = LOW;

int16_t ax, ay, az, gx, gy, gz;
float ax_2g, ay_2g, az_2g;
float a_abs;

float umbral_de_aceleracion = 3.0;

boolean seCayo = false;

void conexion() {
  Serial.println("Conectando a la red WiFi...");
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    digitalWrite(BUZZER_PIN, HIGH);
    digitalWrite(LED_ESP8266, LOW);
    delay(300);
    digitalWrite(BUZZER_PIN, LOW);
    digitalWrite(LED_ESP8266, HIGH);
    Serial.print(".");
    delay(300);
  }

  Serial.println("\nWiFi conectado.");
  Serial.print("Dirección IP: ");
  Serial.println(WiFi.localIP());

  // Configura Firebase
  firebaseConfig.database_url = "https://netbelt-472e1-default-rtdb.firebaseio.com/";
  Firebase.reconnectWiFi(true);
}

void actualizarEstadoFirebase(bool estado) {
  String path = "/estado";
  if (Firebase.setBool(firebaseData, path, estado)) {
    Serial.println("Estado actualizado en Firebase");
  } else {
    Serial.print("Error al actualizar estado: ");
    Serial.println(firebaseData.errorReason());
  }
}

void autenticarFirebase() {
  String accessToken = getAccessToken();
  if (accessToken != "") {
    firebaseConfig.signer.tokens.legacy_token = accessToken.c_str();
    Firebase.begin(&firebaseConfig, &firebaseAuth);
    if (Firebase.ready()) {
      Serial.println("Autenticación con Firebase exitosa");
    } else {
      Serial.println("Error en la autenticación con Firebase");
    }
  } else {
    Serial.println("No se pudo obtener el token de acceso");
  }
}

String getAccessToken() {
  WiFiClientSecure client;
  HTTPClient http;

  client.setInsecure(); // Solo para pruebas. 
  
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
        http.end(); // Termina la conexión actual
        return accessToken;
      } else {
        Serial.printf("HTTP GET failed with code: %d\n", httpCode);
      }
    } else {
      Serial.printf("HTTP GET failed, error: %s\n", http.errorToString(httpCode).c_str());
    }
    http.end(); // Asegura que cualquier conexión abierta se cierra
  } else {
    Serial.println("Unable to connect to token URL");
  }
  return "";
}

void sendFCMNotification(const String& accessToken) {
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

  String request = 
    String("POST ") + "/v1/projects/netbelt-472e1/messages:send HTTP/1.1\r\n" +
    "Host: fcm.googleapis.com\r\n" +
    "Authorization: Bearer " + accessToken + "\r\n" +
    "Content-Type: application/json\r\n" +
    "Content-Length: " + payload.length() + "\r\n\r\n" +
    payload;

  digitalWrite(LED_ESP8266, LOW);
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

void pulsador() {
  buttonState = digitalRead(PULSADOR);
  if (buttonState == LOW && lastButtonState == HIGH) {
    Serial.println("Pulsador presionado!");
    actualizarEstadoFirebase(false);
    seCayo = false;
    digitalWrite(LED_ESP8266, HIGH);
    ESP.restart(); // Reiniciar el ESP8266
  }
  lastButtonState = buttonState;
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
    actualizarEstadoFirebase(true);
    seCayo = true;
  }
}

void calculos() {
  ax_2g = ax / 16384.0;
  ay_2g = ay / 16384.0;
  az_2g = az / 16384.0;

  a_abs = sqrt(ax_2g * ax_2g + ay_2g * ay_2g + az_2g * az_2g);
}

void leerDatosMPU6050() {
  mpu.getMotion6(&gx, &gy, &gz, &ax, &ay, &az);
  calculos();

  Serial.println("");
  Serial.print("Acelerometro: X = "); Serial.print(ax);
  Serial.print(" | Y = "); Serial.print(ay);
  Serial.print(" | Z = "); Serial.println(az);

  Serial.print("Aceleracion total: "); Serial.println(a_abs);
  delay(100);
}

void setup() {
  Serial.begin(921600);
  Wire.begin(D2, D1);

  pinMode(LED_ESP8266, OUTPUT);
  digitalWrite(LED_ESP8266, HIGH);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(PULSADOR, INPUT);

  Serial.println("Inicializando el MPU-6050...");
  mpu.initialize();
  Serial.println(mpu.testConnection() ? "MPU6050 conectado correctamente" : "Error al conectar el MPU-6050");

  conexion();
  autenticarFirebase();
}

void loop() {
  if(!seCayo) {
    leerDatosMPU6050();
    detectarCaida();
  } else {
    pulsador();
  }
}
