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
#define ACCESS_TOKEN "ya29.c.c0ASRK0GYjE7hRI19HCEeJSVc3hTxN2CEHtblx1T9a-VoOMC8JKu0WocoKaz_RHN0Gfc6zr4BRO-bhT2yZJnrPc-6fANXmXH1appiDIRByn0fM4Pc5hmqU7AX_elpMiY4nuGAmY5SDqJ97CbBKcfzm34r-Sf58mb4TFzrkGlyEwRJEJNOEDOlzjGm7Ph_pA6tx9PvvZP1teZZtsYWNl1LVOh44NIiKIRJYqQeF2NWzX26Grpo4UFwlZqqGeFwSXEtSZsjQgLkMirtDxvk2DmDDYUWfdPOjvWSbmJQPIeAMDc8_on7xBGLPV9J4mJazrkm7ZMosBfw3iethoAWcWuGJuIwdTd6-bxy0hOZT2Wl3gchhr-EDTYkVRGeg3QH387AY4Qofm9J7qvs937ryM1qfWteySe8QX70bcp3tlqjY55JhRyjoIcFVl6UvRmIF0y2piI4Q0XYRvgWvJe-RmziJane1RMOiR4M-Z4MuVpb8x-98B197JZfsFM3kMtyjmM1kjziOk_Shn1u6IfhxwZ9FWmIUcYmXzYfkgrtJ61ybsrJ9ltw8WsoSqYzsfnyyQ21blubln3J_SbnhVI7f8WndcyibeSIUpSdyuU1zWUr1u-X4st74aehz74yUYYtpFMfmvUx8x2xOrkViUy_6QVxdI8xwh8ZXzpBV9XUfwchQ6Iycc0MM3WgYMMm7lnvi7vss1Bz6v8Jzp5946I2OfJ_4JtYjUXBeVjj32U034u_7zfznfvU4ieXxgtJhsV60acbJrBouMZYbZr_w7Iwnxy8IcrIsSYMSakBJfF_5r1q13vrz9s5V6aBbkrgx1_ZWSIZzz3-1a5OIqB03IZO7xQ8JWVBsy0vksZYWOBoSJcio2tm3ng6yUd9xgbrpbkk67m0sZ9Rv1zr5FMqOc3dWB3RgV7mgrpR18VbB2Rx1-ok-bzryJ6Blmhg-urjc17mjauk0s8Xsc3Rpa1Mk2lc8ea6gMngJ3i4odkQUf0BU-h2jOvsk4O4YRkQt7ws"

// Token del dispositivo receptor
const char* registrationToken = "dObTAIwGS8-HCVZIfpyT9Z:APA91bE3aHxi17XWN8tnRWp-cVg_17mkNifSRtz1k9ksG_4R8ahBb7eBB8Oja6SdqLyyTCL-eYExE5IXrPaKDlO1d9rTXGITu7VSp3_6xrqGhiAzo8fHrYXBOqL3TS9QuVPOWbU_hlUp";

// Configurar credenciales de red WiFi
const char* ssid = "Base Secreta";        // Nombre
const char* password = "yGD3Um3Jxj";      // Contraseña

int LED_ESP8266 = 2;
int D2 = 4, D1 = 5, D3 = 0;

// Variables para almacenar los valores del acelerómetro y giroscopio
int16_t ax, ay, az, gx, gy, gz;
float ax_2g, ay_2g, az_2g, gx_250_deg, gy_250_deg, gz_250_deg;
float a_abs, g_abs, ang_x, ang_y;

// UMBRAL DE ACELERACIÓN
float umbral_de_aceleracion = 3.0;

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

  // Crear el payload JSON
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
    digitalWrite(BUZZER_PIN, HIGH);
    delay(500);
    digitalWrite(BUZZER_PIN, LOW);
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
}

void loop() {
  leerDatosMPU6050();
  detectarCaida();
}