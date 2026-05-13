import ssl
import json
import time 
import base64
import paho.mqtt.client as mqtt
from Crypto.Cipher import AES
from influxdb_client import InfluxDBClient, Point
from influxdb_client.client.write_api import SYNCHRONOUS

# AES Config
AES_KEY = b"Iamnotconnect980"

# MQTT Config
MQTT_HOST     = "975de3b3a38e47d8a5fd7e8bbc3ae1a6.s1.eu.hivemq.cloud"
MQTT_PORT     = 8883
MQTT_USER     = "IOT_gateway"
MQTT_PASSWORD = "TheEarthisNOTflat0K"
MQTT_TOPIC    = "iot/sensors"

# InfluxDB Config
INFLUX_URL    = "http://localhost:8086"
INFLUX_TOKEN  = "mAozfcuAra7AjWctbU7DFhalhrD6slt2daWedaCLic1_cqzcWGp9h_Y0Tmk6N6S9xVq1i86N-3Lo5lKwjYFjXw=="
INFLUX_ORG    = "Metropolia"
INFLUX_BUCKET = "iot_sensor"

# InfluxDB Client
influx_client = InfluxDBClient(
    url=INFLUX_URL,
    token=INFLUX_TOKEN,
    org=INFLUX_ORG
)
write_api = influx_client.write_api(write_options=SYNCHRONOUS)
print("Connected to InfluxDB")

# AES Decryption
def aes_decrypt(encrypted_payload):
    iv        = base64.b64decode(encrypted_payload["iv"])
    encrypted = base64.b64decode(encrypted_payload["data"])
    tag       = base64.b64decode(encrypted_payload["tag"])
    cipher    = AES.new(AES_KEY, AES.MODE_GCM, nonce=iv)
    plaintext = cipher.decrypt_and_verify(encrypted, tag)
    return json.loads(plaintext.decode())

# Write to InfluxDB
def write_to_influxdb(data):
    point = Point("sensor_data") \
        .tag("slave_id",    str(data["slave_id"])) \
        .tag("sensor_type", data["sensor_type"]) \
        .field("value",     float(data["value"]))
    write_api.write(bucket=INFLUX_BUCKET, record=point)
    print(f"Written to InfluxDB - slave:{data['slave_id']} type:{data['sensor_type']} value:{data['value']}")

# MQTT Callbacks
def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print("Connected to HiveMQ")
        client.subscribe(MQTT_TOPIC, qos=1)
        print(f"Subscribed to topic: {MQTT_TOPIC}")
    else:
        print(f"Connection failed, code: {rc}")

def on_message(client, userdata, msg):
    print(f"Message received on topic: {msg.topic}")
    try:
        encrypted_payload = json.loads(msg.payload.decode())
        print(encrypted_payload)
        time.sleep(2)
        
        decrypted_data    = aes_decrypt(encrypted_payload)
        print(f"The encrypted data is ")
        print(f"Decrypted data: {decrypted_data}")
        write_to_influxdb(decrypted_data)
    except Exception as e:
        print(f"Error processing message: {e}")

def on_disconnect(client, userdata, rc):
    print(f"Disconnected from HiveMQ, code: {rc}")

# MQTT Setup
client = mqtt.Client(
    mqtt.CallbackAPIVersion.VERSION1,
    client_id="Laptop_Subscriber"
)
client.tls_set(cert_reqs=ssl.CERT_REQUIRED, tls_version=ssl.PROTOCOL_TLS)
client.username_pw_set(MQTT_USER, MQTT_PASSWORD)
client.on_connect    = on_connect
client.on_message    = on_message
client.on_disconnect = on_disconnect

print(f"Connecting to HiveMQ: {MQTT_HOST}:{MQTT_PORT}")
client.connect(MQTT_HOST, MQTT_PORT, keepalive=60)
print("Waiting for messages, press Ctrl+C to stop")
client.loop_forever()