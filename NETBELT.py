import network
import urequests as requests
import ntptime
import time
import json
import math
from machine import Pin, I2C
from mpu6050 import MPU6050
import ubinascii
import uhashlib

# Configurar credenciales de red WiFi
ssid = 'Base Secreta'
password = 'yGD3Um3Jxj'

# Token del dispositivo receptor
registrationToken = "dObTAIwGS8-HCVZIfpyT9Z:APA91bE3aHxi17XWN8tnRWp-cVg_17mkNifSRtz1k9ksG_4R8ahBb7eBB8Oja6SdqLyyTCL-eYExE5IXrPa"

# UMBRAL DE ACELERACIÓN
umbral_de_aceleracion = 3.0

# Inicializar el buzzer
buzzer = Pin(16, Pin.OUT)

# Inicializar el MPU-6050
i2c = I2C(scl=Pin(5), sda=Pin(4))
mpu = MPU6050(i2c)

# Cargar la clave privada desde el archivo JSON
def cargar_clave_json():
    try:
        with open('netbelt.json') as f:
            service_account_info = json.load(f)
            print("JSON cargado correctamente.")
            print("client_email:", service_account_info.get("client_email"))
            print("private_key_id:", service_account_info.get("private_key_id"))
            print("private_key:", service_account_info.get("private_key")[:50])  # Mostrar primeros 50 caracteres para verificar
            return service_account_info
    except Exception as e:
        print("Error al cargar el JSON:", e)
        return None

service_account_info = cargar_clave_json()
if not service_account_info:
    raise Exception("No se pudo cargar el archivo JSON correctamente")

# Conectar a la red WiFi
def conexion():
    wlan = network.WLAN(network.STA_IF)
    wlan.active(True)
    wlan.connect(ssid, password)
    while not wlan.isconnected():
        time.sleep(1)
    print('WiFi conectado')
    print('Dirección IP:', wlan.ifconfig()[0])

# Sincronizar el tiempo con un servidor NTP
def ntp_sync():
    try:
        ntptime.host = 'pool.ntp.org'
        ntptime.settime()
        current_time = time.localtime()
        print("NTP synced time:", current_time)
        return time.mktime(current_time)
    except Exception as e:
        print("Failed to sync time:", e)
        return None

# Crear JWT
def hmac_sha256(key, msg):
    h = uhashlib.sha256()
    h.update(key + msg)
    return h.digest()

def create_jwt():
    header = {"alg": "RS256", "typ": "JWT"}
    now = ntp_sync()
    if now is None:
        return None
    
    now = int(now) + 946684800
    print("Current time (iat):", now)
    print("Expiration time (exp):", now + 3600)
    
    payload = {
        "iss": service_account_info["client_email"],
        "scope": "https://www.googleapis.com/auth/firebase.messaging",
        "aud": service_account_info["token_uri"],
        "iat": now,
        "exp": now + 3600
    }
    to_sign = (
        ubinascii.b2a_base64(json.dumps(header).encode()).decode().rstrip("\n=") + "." +
        ubinascii.b2a_base64(json.dumps(payload).encode()).decode().rstrip("\n=")
    ).replace("+", "-").replace("/", "_")
    
    private_key_b64 = ubinascii.a2b_base64(service_account_info["private_key"].replace("-----BEGIN PRIVATE KEY-----\n", "").replace("-----END PRIVATE KEY-----\n", "").replace("\n", ""))
    signed = hmac_sha256(private_key_b64, to_sign.encode())
    signature_b64 = ubinascii.b2a_base64(signed).decode().rstrip("\n=").replace('+', '-').replace('/', '_')
    return to_sign + "." + signature_b64

# Obtener el token de acceso
def get_access_token():
    jwt = create_jwt()
    if jwt is None:
        print("Failed to create JWT")
        return None
    
    token_url = 'https://oauth2.googleapis.com/token'
    token_req_payload = 'grant_type=urn%3Aietf%3Aparams%3Aoauth%3Agrant-type%3Ajwt-bearer&assertion=' + jwt
    headers = {'Content-Type': 'application/x-www-form-urlencoded'}
    response = requests.post(token_url, data=token_req_payload.encode('utf-8'), headers=headers)
    
    print("Token response status:", response.status_code)
    print("Token response:", response.text)
    
    if response.status_code == 200:
        return response.json().get('access_token')
    else:
        print("Failed to obtain access token")
        return None

# Enviar notificación a Firebase Cloud Messaging
def send_fcm_notification():
    access_token = get_access_token()
    if not access_token:
        print("Failed to obtain access token")
        return
    
    url = "https://fcm.googleapis.com/v1/projects/netbelt-472e1/messages:send"
    headers = {
        "Authorization": "Bearer " + access_token,
        "Content-Type": "application/json"
    }
    payload = {
        "message": {
            "token": registrationToken,
            "data": {
                "title": "ALERTA!",
                "body": "EL DISPOSITIVO HA DETECTADO UNA CAÍDA"
            },
        }
    }
    response = requests.post(url, headers=headers, data=json.dumps(payload).encode('utf-8'))
    print("Response:", response.text)

# Detectar caídas
def detectar_caida():
    valores = mpu.get_values()
    ax = valores['acc_x'] / 16384.0
    ay = valores['acc_y'] / 16384.0
    az = valores['acc_z'] / 16384.0
    a_abs = math.sqrt(ax * ax + ay * ay + az * az)
    if a_abs > umbral_de_aceleracion:
        print("Posible caída detectada!")
        buzzer.on()
        time.sleep(0.5)
        buzzer.off()
        send_fcm_notification()

# Configurar el buzzer
def setup():
    buzzer.off()
    conexion()

# Ejecutar el programa
def loop():
    while True:
        detectar_caida()
        time.sleep(0.1)

setup()
loop()