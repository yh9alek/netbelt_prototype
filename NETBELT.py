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

# Obtener el token de acceso desde el servidor
def get_access_token():
    try:
        response = requests.get('http://yohan949.pythonanywhere.com/get_token')
        if response.status_code == 200:
            token = response.json().get('access_token')
            print("Access token obtained:", token)
            return token
        else:
            print("Failed to obtain access token, status code:", response.status_code)
            print("Response:", response.text)
            return None
    except Exception as e:
        print("Exception occurred while obtaining access token:", e)
        return None

# Obtener el token de registro FCM desde el servidor
def get_registration_token():
    try:
        response = requests.get('http://tu-servidor.com/get_registration_token')
        if response.status_code == 200:
            token = response.json().get('registration_token')
            print("Registration token obtained:", token)
            return token
        else:
            print("Failed to obtain registration token, status code:", response.status_code)
            print("Response:", response.text)
            return None
    except Exception as e:
        print("Exception occurred while obtaining registration token:", e)
        return None

# Enviar notificación a Firebase Cloud Messaging
def send_fcm_notification():
    access_token = get_access_token()
    registration_token = get_registration_token()
    if not access_token or not registration_token:
        print("Failed to obtain access token or registration token")
        return
    
    url = "https://fcm.googleapis.com/v1/projects/netbelt-472e1/messages:send"
    headers = {
        "Authorization": "Bearer " + access_token,
        "Content-Type": "application/json"
    }
    payload = {
        "message": {
            "token": registration_token,
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
    send_fcm_notification()

# Ejecutar el programa
def loop():
    while True:
        detectar_caida()
        time.sleep(0.1)

setup()
#loop()
