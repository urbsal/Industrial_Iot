#include <ETH.h>
#include <WiFi.h>          
#include <ModbusMaster.h>
#include "pb_encode.h"
#include "pb_common.h"
#include "sample.pb.h"
#define RX2_PIN   36
#define TX2_PIN   17
#define RE_DE_PIN 



#define ETH_PHY_TYPE  ETH_PHY_IP101
#define ETH_PHY_ADDR   1 
#define ETH_PHY_MDC   4
#define ETH_PHY_MDIO  13
#define ETH_PHY_POWER 2
#define ETH_CLK_MODE  ETH_CLOCK_GPIO0_IN 


IPAddress deviceIP(10, 0, 0, 2);
IPAddress gateway(10, 0, 0, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress serverIP(10, 0, 0, 1);

const int port = 5000;

WiFiClient client;

ModbusMaster node1;
ModbusMaster node2;

int s1 = -1;
int s2 = -1;
bool ethConnected = false;

void preTransmission()  { digitalWrite(RE_DE_PIN, HIGH); }
void postTransmission() { delayMicroseconds(500); digitalWrite(RE_DE_PIN, LOW); }

void onEvent(arduino_event_id_t event) {       
  switch (event) {
    case ARDUINO_EVENT_ETH_START:
      Serial.println("ETH Started");
      ETH.setHostname("esp32-edge");
      break;
    case ARDUINO_EVENT_ETH_CONNECTED:
      Serial.println("ETH Cable Connected");
      break;
    case ARDUINO_EVENT_ETH_GOT_IP:
      Serial.print("ETH IP: ");
      Serial.println(ETH.localIP());
      ethConnected = true;
      break;
    case ARDUINO_EVENT_ETH_DISCONNECTED:
      Serial.println("ETH Disconnected");
      ethConnected = false;
      break;
    case ARDUINO_EVENT_ETH_STOP:
      Serial.println("ETH Stopped");
      ethConnected = false;
      break;
    default:
      break;
  }
}

bool connectToServer() {
  Serial.println("Connecting to Raspberry Pi...");
  if (client.connect(serverIP, port)) {
    Serial.println("Connected!");
    return true;
  }
  Serial.println("Connection failed.");
  return false;

}

void Protobufencoder(int slaveID, int sensorType, int value){

  if(value==-1){
    Serial.println("The data is not retrived :");
    Serial.print(slaveID);
    return;
}

SensorData msg = SensorData_init_zero;
msg.slave_id= slaveID;
msg.value = value;
msg.sensor_type=sensorType;

uint8_t buffer[128];

pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));

bool success = pb_encode(&stream, SensorData_fields, &msg);

if (!success){
  Serial.println("Protobuf Failed !!!!");
  return;
}

int encodedLen= stream.bytes_written; /* This is used to measure the length of the data arrive in buffer inserted by buffer*/

uint8_t header [4];
header[0]=(encodedLen >>24) & 0xFF;
header [1]= (encodedLen >> 16) & 0xFF;
header [2]=(encodedLen >> 8) & 0xFF;
header [3]=(encodedLen)     & 0XFF; 

client.write(header, 4);
client.write(buffer, encodedLen);

 Serial.printf("Sent → slave=%d type=%d value=%d (%d bytes)\n",
                slaveID, sensorType, value, encodedLen);

}

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("=== SETUP START ===");

  pinMode(RE_DE_PIN, OUTPUT);
  digitalWrite(RE_DE_PIN, LOW);

  Serial2.begin(9600, SERIAL_8N1, RX2_PIN, TX2_PIN);

  node1.begin(1, Serial2);
  node1.preTransmission(preTransmission);
  node1.postTransmission(postTransmission);

  node2.begin(2, Serial2);
  node2.preTransmission(preTransmission);
  node2.postTransmission(postTransmission);


  Network.onEvent(onEvent);                    /


  ETH.begin(ETH_PHY_TYPE, ETH_PHY_ADDR, ETH_PHY_MDC, ETH_PHY_MDIO, ETH_PHY_POWER, ETH_CLK_MODE);

  // Set static IP
  ETH.config(deviceIP, gateway, subnet);

  
  Serial.println("Waiting for Ethernet...");
  unsigned long start = millis();
  while (!ethConnected && millis() - start < 10000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  if (ethConnected) {
    connectToServer();
  } else {
    Serial.println("Ethernet not ready. Check cable.");
  }

  Serial.println("Master Ready");
}

void loop() {

  // Slave 1
  uint8_t result1 = node1.readHoldingRegisters(10, 1);
  if (result1 == node1.ku8MBSuccess) {
    s1 = node1.getResponseBuffer(0);
    Serial.print("Slave 1: "); Serial.println(s1);
  } else {
    Serial.print("S1 Error: 0x"); Serial.println(result1, HEX);
  }

  delay(500);

  // Slave 2
  uint8_t result2 = node2.readHoldingRegisters(0x00, 1);
  if (result2 == node2.ku8MBSuccess) {
    s2 = node2.getResponseBuffer(0);
    Serial.print("Slave 2: "); Serial.println(s2);
  } else {
    Serial.print("S2 Error: 0x"); Serial.println(result2, HEX);
  }

  Serial.println("----------------");
  delay(1000);

  
if (ethConnected && client.connected()) {
    // Send Slave 1
Protobufencoder(1, 1,  s1);

// Send Slave 2
Protobufencoder(2, 2, s2);

  } else {
    Serial.println("Disconnected. Reconnecting...");
    client.stop();
    delay(3000);
    if (ethConnected) connectToServer();
  }
}