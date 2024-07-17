// Modulo ESP8266
#include <ESP8266WiFi.h>
#include <FirebaseESP8266.h>

// Sensor MPU-6050
#include <Wire.h>
#include <I2Cdev.h>
#include <MPU6050.h>

#define MPU 0x68
#define BUZZER_PIN 16

MPU6050 mpu;

// Token de autenticación de Firebase
#define ACCESS_TOKEN "ya29.c.c0ASRK0Gb_RRHYZOt_cSvZt7oTHXlYQ6KmsW1aGXHL7lpKuqqBhYDmRqchp06Yap7h5abqZLQDupxMZJYlDuMbMM0NYS19HJXRtuVF12UitK16-2McicM-LFmSs_rLkCcVlE9ESHpnchVQMwqLgSWosHv1LrS69ZMf8EWIQNhKJQT3wQgEQ7KKKjqIG6djeJYOkBCU7pZquSooN_H5q4qdoK5GrFNpRakNCav4BJ835QauEoqLUysDZcTNXUTzDmh1WfZcMQwu-KmoOFbIGROXLiSOY4Wxu_GWHQngLccoNAIz0mdbNbn8-ioAuVRrcvPsPBfcD04Gap5OOvoC4uUywAsCBmPbf13-1yfMuNU-JCq9vQPXAw7TAGuQT385Kwk5BJ6yS_1O6rVgWIptSbVSlt3SB5eQvdIh7yVq1M6Vzx_unmSqYuir3FiWdgpeI2z59yVjtYFXvmJIr9y4UqiXRgum4cwr3zUVaosemqwdQg64ilxviyRVlUXJaIIe-BWBzV_y3BWci9vSV-qIv0b1Whk4jn-z7RmQhpiplQp-setIxzxspiv-Ju5eFZo6reh57gmdZbXMtv-1rh8qdYVoBysm1WRMYcx6Rf95WzRaM1cF2ZOyUt7moFjkeRrIJce2hlOda8nOcMt53lrm23t49hdYgZjXjzMyp9mqx0IQqa7OhU3VBI1lY0vnkigFX71fZny_t4-IJwv-WMwWWkR3Rh6Wtc8JMzeQmnSa5hn66sngjISWk13Wo3gtzweWd815ZBIj0udVcinfJY09x24B6WZa9xo99_Q9Qvpf9B5fVXwZeuz9y0iw9BZ1XOs0I63b2zUYctFxg2pdpasXo-0X3npUqqXVZlrbZcIy4s_O_6fzkiOcVRxnezcurvfy6Z8t04V9mBRfb6W-a52V6998Wedi8BFrz5Vi5BIb09fBMF2p8Qt_tmbdglIWJ9-nyJg6VQIY-YqM07sBv9tyO_5Vy8WBxuVstih3hwFFy2I7eQ6wQ5aWW9QZ5YM"

// Token del dispositivo receptor
const char* registrationToken = "dObTAIwGS8-HCVZIfpyT9Z:APA91bE3aHxi17XWN8tnRWp-cVg_17mkNifSRtz1k9ksG_4R8ahBb7eBB8Oja6SdqLyyTCL-eYExE5IXrPaKDlO1d9rTXGITu7VSp3_6xrqGhiAzo8fHrYXBOqL3TS9QuVPOWbU_hlUp";

// Configurar credenciales de red WiFi
const char* ssid = "Base Secreta";        // Nombre
const char* password = "yGD3Um3Jxj";      // Contraseña

int LED_ESP8266 = 2;
int D2 = 4, D1 = 5;

// Variables para almacenar los valores del acelerómetro y giroscopio
int16_t ax, ay, az, gx, gy, gz;
float ax_2g, ay_2g, az_2g, gx_250_deg, gy_250_deg, gz_250_deg;
float a_abs, g_abs, ang_x, ang_y;

// UMBRAL DE ACELERACIÓN
float umbral_de_aceleracion = 2.0;

void conexion() {
  // Conecta a la red WiFi
  Serial.println();
  Serial.println("Conectando a la red WiFi...");
  WiFi.begin(ssid, password);

  // Verificar el estatus de conexión
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi conectado.");
  Serial.print("Dirección IP: ");
  Serial.println(WiFi.localIP());
}

void sendFCMNotification() {
  WiFiClientSecure client;
  client.setInsecure();

  if (!client.connect("fcm.googleapis.com", 443)) {
    Serial.println("Connection failed");
    return;
  }

  // Construye el payload de la notificación
  String payload = 
    "{"
    "\"message\": {"
    "\"token\": \"" + String(registrationToken) + "\","
    "\"notification\": {"
    "\"title\": \"ALERTA!\","
    "\"body\": \"EL DISPOSITIVO HA DETECTADO UNA CAÍDA\""
    "},"
    "\"data\": {"
    "\"key1\": \"value1\","
    "\"key2\": \"value2\""
    "}"
    "}"
    "}";

  // Construye la solicitud HTTP POST
  String request = 
    String("POST ") + "/v1/projects/netbelt-472e1/messages:send HTTP/1.1\r\n" +
    "Host: fcm.googleapis.com\r\n" +
    "Authorization: Bearer " + String(ACCESS_TOKEN) + "\r\n" +
    "Content-Type: application/json\r\n" +
    "Content-Length: " + payload.length() + "\r\n\r\n" +
    payload;

  // Envia la solicitud HTTP
  client.print(request);

  // Lee la respuesta
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
    // Aumento repentino en la aceleración, posible caída
    Serial.println("Posible caída detectada!");

    // Aquí podrías activar una acción, como enviar una notificación
    //digitalWrite(BUZZER_PIN, HIGH);
    sendFCMNotification();
  }
}

void calculos() {
  // Convertir a valores en unidades correspondientes
  ax_2g = ax / 16384.0;
  ay_2g = ay / 16384.0;
  az_2g = az / 16384.0;
  gx_250_deg = gx / 131.07;
  gy_250_deg = gy / 131.07;
  gz_250_deg = gz / 131.07;

  // Magnitudes totales tanto de aceleración como la velocidad angular
  a_abs = pow(pow(ax_2g, 2) + pow(ay_2g, 2) + pow(az_2g, 2), 0.5);
  g_abs = sqrt(pow(gx_250_deg, 2) + pow(gy_250_deg, 2) + pow(gz_250_deg, 2));
  
  // Obtención de las inclinaciones en grados
  ang_x = atan(ax / sqrt(pow(ay, 2) + pow(az, 2))) * (180.0 / 3.14);
  ang_y = atan(ay / sqrt(pow(ax, 2) + pow(az, 2))) * (180.0 / 3.14);
}

void leerDatosMPU6050() {
  // Leer los valores del acelerómetro y giroscopio
  mpu.getMotion6(&gx, &gy, &gz, &ax, &ay, &az);
  calculos();

  // Mostrar los resultados en el monitor serial
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
  // Inicia la comunicación serial
  Serial.begin(921600);
  Wire.begin(D2, D1); // Configura los pines I2C para ESP8266

  // Configurar pin del microcontrolador ESP8266
  pinMode(LED_ESP8266, OUTPUT);

  // Configurar el pin del buzzer como salida
  pinMode(BUZZER_PIN, OUTPUT);

   // Inicializar el MPU-6050
  Serial.println("Inicializando el MPU-6050...");
  mpu.initialize();

  // Verificar la conexión
  Serial.println(mpu.testConnection() ? "MPU6050 conectado correctamente" : "Error al conectar el MPU6050");
  conexion();
  sendFCMNotification();
}

void loop() {
  leerDatosMPU6050();
  detectarCaida();
}
