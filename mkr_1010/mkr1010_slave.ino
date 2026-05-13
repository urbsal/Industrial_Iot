#define DE_RE_PIN A5  // Controls both DE and RE tied together (A4 and A5 jumpered)

const int trigger = A1;
const int echo = A3;
const int slave_id = 2;

uint16_t reg_distance = 0;
unsigned long lastSensor = 0;

// CRC16 for Modbus RTU
uint16_t crc16(uint8_t *buf, int len) {
  uint16_t crc = 0xFFFF;
  for (int i = 0; i < len; i++) {
    crc ^= buf[i];
    for (int j = 0; j < 8; j++) {
      if (crc & 1) crc = (crc >> 1) ^ 0xA001;
      else crc >>= 1;
    }
  }
  return crc;
}

float distance_measured() {
  digitalWrite(trigger, LOW);
  delayMicroseconds(2);
  digitalWrite(trigger, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigger, LOW);
  long duration = pulseIn(echo, HIGH, 30000);
  if (duration == 0) return 0;
  return (duration * 0.034) / 2;
}

void sendResponse(uint8_t *resp, int len) {
  uint16_t crc = crc16(resp, len);
  
  // Switch to transmit mode
  digitalWrite(DE_RE_PIN, HIGH);
  delayMicroseconds(500);  // Settling time for transceiver
  
  // Send response bytes
  for (int i = 0; i < len; i++) {
    Serial1.write(resp[i]);
  }
  Serial1.write(crc & 0xFF);         // CRC low byte
  Serial1.write((crc >> 8) & 0xFF);  // CRC high byte
  Serial1.flush();                    // Wait until all bytes sent
  
  // Small delay to ensure last byte is transmitted
  delayMicroseconds(500);
  
  // Switch back to receive mode
  digitalWrite(DE_RE_PIN, LOW);
}

void handleRequest(uint8_t *buf, int len) {
  // Minimum valid Modbus RTU frame = 8 bytes
  if (len < 8) return;

  // Check slave ID
  if (buf[0] != slave_id) return;

  // Check function code — we only handle FC03 (read holding registers)
  if (buf[1] != 0x03) return;

  // Verify CRC
  uint16_t receivedCRC = buf[len-2] | (buf[len-1] << 8);
  uint16_t calculatedCRC = crc16(buf, len-2);
  if (receivedCRC != calculatedCRC) {
    Serial.println("CRC mismatch!");
    return;
  }

  uint16_t startReg = (buf[2] << 8) | buf[3];
  uint16_t numRegs  = (buf[4] << 8) | buf[5];

  Serial.print("Request: reg="); Serial.print(startReg);
  Serial.print(" count="); Serial.println(numRegs);

  // Only handle register 0x00
  if (startReg == 0x00 && numRegs == 1) {
    uint8_t resp[5];
    resp[0] = slave_id;         // Slave ID
    resp[1] = 0x03;             // Function code
    resp[2] = 0x02;             // Byte count (2 bytes per register)
    resp[3] = (reg_distance >> 8) & 0xFF;  // High byte
    resp[4] = reg_distance & 0xFF;          // Low byte
    sendResponse(resp, 5);
    Serial.print("Responded with: ");
    Serial.println(reg_distance);
  }
}

void setup() {
  Serial.begin(9600);
  delay(2000);

  pinMode(trigger, OUTPUT);
  pinMode(echo, INPUT);

  // Setup DE/RE control pin
  pinMode(DE_RE_PIN, OUTPUT);
  digitalWrite(DE_RE_PIN, LOW); // Start in receive mode

  // Initialize RS485 (Serial1 on MKR boards)
  Serial1.begin(9600, SERIAL_8N1);

  Serial.println("Raw Modbus RTU Slave Ready");
  Serial.print("Slave ID: ");
  Serial.println(slave_id);
  Serial.println("Waiting for Modbus requests...");
}

uint8_t rxBuf[32];
int rxLen = 0;
unsigned long lastByte = 0;

void loop() {
  // Read incoming bytes from master
  while (Serial1.available()) {
    rxBuf[rxLen++] = Serial1.read();
    lastByte = millis();
    if (rxLen >= 32) rxLen = 0; // overflow protection
  }

  // If we have bytes and 3.5 character silence = frame complete
  // At 9600 baud, 3.5 chars ≈ 3.64ms (use 5ms to be safe)
  if (rxLen > 0 && millis() - lastByte > 5) {
    handleRequest(rxBuf, rxLen);
    rxLen = 0;
  }

  // Sensor update every 200ms
  if (millis() - lastSensor >= 200) {
    lastSensor = millis();
    reg_distance = (uint16_t)distance_measured();
    Serial.print("Distance: ");
    Serial.println(reg_distance);
  }
}