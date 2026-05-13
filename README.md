# Industrial_Iot
This repository is for industrial IoT Project. The project is mainly focus on establishment of complete pipeline from sensor to server, implementing all the security measures to protect the data and devices from unauthorised acesss or cyber attacks. 
# Light Sensor integration with-SenseCAP s2110 and Ultrasonic sensor integrating with MKR1010 using MKR RS-485 Shield 
In sensors part, two sensors were used follwing Modus RTU protocol over RS485 creating a Dasiy chain. The Esp32-Edge 101 acts a Modbus Master, polling sensors to collect and forward the data through the pipeline .
![image alt](https://github.com/urbsal/Industrial_Iot/blob/main/daisychain.jpg?raw=true)

#EDGE-101
In edge device, the  data received from sensors is converted to binary format using a Protobuf encoder and further forwared to gateway using TCP protocol over Ethernet. 
#GATEWAY
The Raspberry Pi 5 was used as a gateway, where several security measures and data processing components were applied. A firewall was implemented to protect against unauthorised network access, and AES encryption was applied to secure the data in transit and at rest. The binary data received from the Edge-101 was deserialised using a Protobuf decoder. Prometheus was implemented for metrics collection and monitoring, along with Node Exporter to expose system-level hardware and OS metrics to Prometheus. SQLite was used as a local database for storing sensor data, and SQLCipher with a PRAGMA key was implemented to encrypt and record the device logs. Finally, the processed data was transferred to the server using MQTT over TLS, with HiveMQ as the message broker.
#Server
A separate device was used as a server, where a Python client was implemented to subscribe to the MQTT topic via HiveMQ broker, decrypt the AES-256 encrypted payload, and store the data into an InfluxDB time-series database. A WireGuard VPN tunnel was established to provide secure remote access, enabling the visualisation of device health metrics and sensor data through Grafana dashboards, with Prometheus and Node Exporter serving as the metrics collection backend.


