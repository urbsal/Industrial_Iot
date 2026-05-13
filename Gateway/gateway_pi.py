import socket
import struct
import sqlite3
from datetime import datetime
import sample_pb2
import hmac
import hashlib
import os
from sqlcipher3 import dbapi2 as sqlite
from prometheus_client import start_http_server, Gauge
import paho.mqtt.client as mqtt
from Crypto.Cipher import AES
import ssl
import json
import base64
import time
import threading
AES_KEY = b"Iamnotconnect980" ##The AES primary key

MQTT_HOST = "975de3b3a38e47d8a5fd7e8bbc3ae1a6.s1.eu.hivemq.cloud"
MQTT_PORT = 8883
MQTT_USER = "IOT_gateway"
MQTT_PASSWORD = "TheEarthisNOTflat0K"
MQTT_TOPIC= "iot/sensors"




DB_KEY = "TheEarthisround"
DB_PATH = "/home/urbsal/gateway/secure_logs.db"

last_seen = {
    1: None,
    2: None
    }
DEAD_TIME = 17 ## wait the data until 61 second if not arrive then declear sensor is dead

dead_already = {
    1: False,
    2: False
    }


def AES_encryption(data):
    iv=os.urandom(12)
    plaintext= json.dumps(data).encode() ##converting the dict to bytes for AES encryption

    cipher = AES.new(AES_KEY, AES.MODE_GCM, nonce=iv)# setting the cipher process
    encrypted, tag = cipher.encrypt_and_digest(plaintext)

    return {
        "iv" : base64.b64encode(iv).decode(),
        "data" : base64.b64encode(encrypted).decode(),
        "tag": base64.b64encode(tag).decode() ##convert bytes in string

        }

def on_connect(client, userdata, flags, rc):
    if rc==0:
        print("Connection established between gateway and server")
    else:
        print(f"connection faile, error code: {rc}")

def on_publish( client, userdata, mid):
    print(f"Message sucessfully published (mid={mid})")

def MQTT_setup():

    client =mqtt.Client(client_id ="Raspberry_Pi1")
    client.tls_set(cert_reqs=ssl.CERT_REQUIRED, tls_version=ssl.PROTOCOL_TLS) ## TLS CONNECTION ESTD BETWEEN GATEWAY AND HIVEMQ
    client.username_pw_set(MQTT_USER, MQTT_PASSWORD)

    client.on_connect = on_connect
    client.on_publish = on_publish

    client.connect(MQTT_HOST, MQTT_PORT, keepalive=60)
    client.loop_start()
    return client
def secure_database():
    conn = sqlite.connect(DB_PATH)
    conn.execute(f"PRAGMA key ='{DB_KEY}'")
    return conn

def create_secure_logs_table():
    conn = secure_database()
    conn.execute("""CREATE TABLE IF NOT EXISTS SECURE_LOGS(
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    Source TEXT,
                    event TEXT,
                    message TEXT,
                    timestamp TEXT,
                    hash TEXT,
                    previous_Hash TEXT,
                    hmac TEXT)""")
    conn.commit()
    conn.close()

create_secure_logs_table()


def the_last_hash():
    conn= secure_database()
    row = conn.execute("""SELECT hash FROM SECURE_LOGS
                        ORDER BY id DESC LIMIT 1 """).fetchone() #This is used to get the earlier hash from the sqlite hash value
    conn.close()
    return row[0] if row else "GENESIS"
hmac_key = b"TheEarthisnotflat"

def save_secure_logs(source, events, message):
    timestamp = datetime.now().isoformat()
    previous_hash = the_last_hash() #called the the last_hash_function

    data = f"{source}{events}{message}{timestamp}{previous_hash}"
    current_hash = hashlib.sha256(data.encode()).hexdigest()

    hmac_value =hmac.new(hmac_key,
                         data.encode(),
                         hashlib.sha256).hexdigest()

    conn= secure_database()
    conn.execute("""INSERT INTO SECURE_LOGS (source, event, message,timestamp,

hash, previous_hash, hmac)
                    VALUES(?,?,?,?,?,?,?)""", (source, events, message,timestamp, current_hash, previous_hash
                    ,hmac_value))
    conn.commit()
    conn.close()



def sensor_watchdog ():
    time.sleep(20)
    while True:
        now=datetime.now()
        for slave_id, last_time in last_seen.items():
            if last_time is None:
                continue
            diff=(now-last_time).total_seconds()

            if diff > DEAD_TIME and not dead_already[slave_id]:
                print(f"slave {slave_id} is DEAD , ")

                save_secure_logs(
                                f"Slave {slave_id}",
                                  "DEAD",
                                  f" slave {slave_id} did not send data for {diff :.0f} seconds")
                dead_already[slave_id]= True

            if diff <= DEAD_TIME and dead_already[slave_id]:
                dead_already[slave_id] = False
                save_secure_logs(f"Slave{slave_id}",
                                 "Alive",
                                 f"Slave{slave_id} reconnected")
        time.sleep(5)



# prometheus setup
slave1_gauge = Gauge(
    'slave1_light_sensor',
    'Light sensor value slave 1'
)

slave2_gauge = Gauge(
    'slave2_ultrasonic_sensor',
    'Ultrasonic sensor value slave 2'
)

# sqlite database created
conn = sqlite3.connect("/home/urbsal/industrial_IOT.db")
cursor = conn.cursor()
cursor.execute("""
CREATE TABLE IF NOT EXISTS RS485_SENSORS_DATA (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    slave_id INTEGER,
    sensor_type TEXT,
    value REAL,
    timestamp TEXT,
    uploaded INTEGER
)
""")
conn.commit()
print("Database ready ")

def recv_exact(conn, n):
    """Read exactly n bytes from socket"""
    data = b""
    while len(data) < n:
        chunk = conn.recv(n - len(data))
        if not chunk:
            raise ConnectionError("ESP32 Disconnected")
        data += chunk
    return data


def get_sensor_name(sensor_type):
    if sensor_type == 1:
        return "light_sensor"
    elif sensor_type == 2:
        return "ultrasonic_sensor"
    else:
        return "unknown"


HOST = '0.0.0.0'
PORT = 5000

# Start Prometheus on port 8000
start_http_server(8000, addr="0.0.0.0")
print("Prometheus metrics → http://bishal:8000 ")

# Start TCP server
server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
server.bind((HOST, PORT))
server.listen(5)
print(f"TCP Gateway listening on port {PORT}...")
print("Waiting for ESP32...")

mqtt_client= MQTT_setup()

watchdog_thread = threading.Thread(target = sensor_watchdog, daemon=True)
watchdog_thread.start()




while True:
    client, address = server.accept()
    print(f"\n ESP32 Connected: {address}")
    client.setsockopt(socket.SOL_SOCKET, socket.SO_KEEPALIVE, 1)
    client.setsockopt(socket.IPPROTO_TCP, socket.TCP_KEEPIDLE,10)
    client.setsockopt(socket.IPPROTO_TCP, socket.TCP_KEEPINTVL,5)
    client.setsockopt(socket.IPPROTO_TCP, socket.TCP_KEEPCNT,3)
    client.settimeout(30)

    save_secure_logs("Edge101", "Connected",f"EDGE101 connected from {address}")

    try:
        while True:
            # Step 1 — Read 4 byte header
            header  = recv_exact(client, 4)
            msg_len = struct.unpack(">I", header)[0]
            print(f"Expecting {msg_len} bytes...")

            # Step 2 — Read exact protobuf bytes
            raw = recv_exact(client, msg_len)
            print(f"Raw protobuf bytes HEX :{raw.hex()}")

            # Step 3 — Decode protobuf
            sensor = sample_pb2.SensorData()
            sensor.ParseFromString(raw)

            # Step 4 — Get sensor name
            sensor_name = get_sensor_name(sensor.sensor_type)
            last_seen[sensor.slave_id] = datetime.now()

            print(f"-")
            print(f"Slave ID    : {sensor.slave_id}")
            print(f"Sensor Type : {sensor_name}")
            print(f"Value       : {sensor.value}")
            print(f"Time        : {datetime.now()}")

            # Step 5 — Save to SQLite database
            cursor.execute("""
                INSERT INTO RS485_SENSORS_DATA
                (slave_id, sensor_type, value, timestamp, uploaded)
                VALUES (?, ?, ?, ?, 0)
            """, (
                sensor.slave_id,
                sensor_name,
                sensor.value,
                datetime.now().isoformat()
            ))
            conn.commit()
            print(f"Saved to database ")
            time.sleep(2)

            # Step 6 — Update Prometheus Gauge
            if sensor.slave_id == 1:
                slave1_gauge.set(sensor.value)
                print(f"Prometheus slave1_light_sensor = {sensor.value} ")

            elif sensor.slave_id == 2:
                slave2_gauge.set(sensor.value)
                print(f"Prometheus slave2_ultrasonic = {sensor.value} what ")

            payload = {
                "slave_id" : sensor.slave_id,
                "sensor_type" : sensor_name,
                "value" : sensor.value,
                "timestamp": datetime.now().isoformat()
                }

            encrypted_payload = AES_encryption(payload)

            mqtt_client.publish(MQTT_TOPIC, json.dumps(encrypted_payload), qos=1)

    except Exception as e:
        print(f" Error: {e}")
        save_secure_logs("EDGE101","Disconnected", f"EDGE101 lost connection: {e}")
        client.close()
        print("Waiting for ESP32 to reconnect...")
        time.sleep(5)