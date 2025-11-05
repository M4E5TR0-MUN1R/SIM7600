# SIM7600 MQTT Client - ESP32

This project implements a complete MQTT client for the SIM7600 cellular module using AT commands. It connects to HiveMQ Cloud over SSL/TLS and can publish/subscribe to MQTT topics.

## Features

- ✅ Automatic module power-on via MCP23017
- ✅ Network registration and data connection setup
- ✅ SSL/TLS secure connection with Let's Encrypt CA certificate
- ✅ MQTT connection to HiveMQ Cloud
- ✅ Topic subscription and publishing
- ✅ Automatic status updates every 30 seconds
- ✅ Real-time message receiving

## Hardware Requirements

- ESP32-S3 DevKit
- SIM7600 4G/LTE module
- MCP23017 I2C GPIO expander
- SIM card with data plan
- Stable power supply (SIM7600 requires 3.4-4.2V, recommend 3.8V)

## Pin Configuration

### UART (SIM7600 ↔ ESP32)
- **RX:** GPIO 20 (ESP32 RX ← SIM7600 TX)
- **TX:** GPIO 19 (ESP32 TX → SIM7600 RX)
- **Baud Rate:** 115200

### Power Control
- **PWRKEY:** MCP23017 Pin 9

### MCP23017
- **Address:** 0x27
- **I2C:** Default ESP32-S3 pins (SDA: GPIO 8, SCL: GPIO 9)

## MQTT Configuration

The project is configured for **HiveMQ Cloud** with the following settings:

- **Broker:** `ebd627a5b511476dae2e77a7aac9064b.s1.eu.hivemq.cloud`
- **Port:** 8883 (SSL/TLS)
- **Username:** `OctaviaAdmin`
- **Password:** `O5fQwKQkhjO4VbiT`
- **Client ID:** `SIM7600_ESP32_Client`

### Topics
- **Publish:** `test/sim7600/status` (status updates every 30s)
- **Subscribe:** `test/sim7600/command` (receives commands)

## APN Configuration

You may need to change the APN settings for your cellular carrier. Edit this line in `main.cpp`:

```cpp
sendATCommand("AT+CGDCONT=1,\"IP\",\"internet\"");  // Change "internet" to your APN
```

Common APNs:
- **Generic:** `internet`
- **Safaricom (Kenya):** `safaricom`
- **Vodafone:** `internet.vodafone.net`
- **AT&T:** `phone`
- **T-Mobile:** `fast.t-mobile.com`

## Building and Uploading

1. **Build the project:**
   ```bash
   pio run
   ```

2. **Upload to ESP32:**
   ```bash
   pio run --target upload
   ```

3. **Monitor serial output:**
   ```bash
   pio device monitor
   ```

## Expected Output

```
╔═══════════════════════════════════════════════╗
║    SIM7600 MQTT Client - HiveMQ Cloud       ║
╚═══════════════════════════════════════════════╝

--- Initializing MCP23017 ---
✓ MCP23017 at 0x27 initialized
✓ All outputs configured

✓ UART initialized on RX:20, TX:19 at 115200 baud

--- Powering On SIM7600 Module ---
Pulling PWRKEY LOW...
Setting PWRKEY HIGH...
Waiting for module boot (10 seconds)...
✓ Module powered on

--- Testing Module Communication ---
>> AT
OK

--- Setting Up Network Connection ---
Checking SIM card...
>> AT+CPIN?
+CPIN: READY
OK

Checking network registration...
✓ Registered on network

--- Setting Up MQTT Configuration ---
Setting MQTT client ID...
Setting MQTT broker URL...
Setting MQTT username...
Setting MQTT password...
Enabling SSL/TLS...
✓ CA certificate uploaded
✓ MQTT configuration complete

--- Connecting to MQTT Broker ---
Broker: ebd627a5b511476dae2e77a7aac9064b.s1.eu.hivemq.cloud:8883
Client ID: SIM7600_ESP32_Client
✓ MQTT broker connected!
✓ Subscribed to topic

✓ System ready! MQTT connected.
Publishing status message...
✓ Published: SIM7600 online!
```

## Testing MQTT

### Using MQTT Explorer or HiveMQ Web Client

1. **Connect to the same HiveMQ broker** with the credentials above
2. **Subscribe to:** `test/sim7600/status`
3. **Publish to:** `test/sim7600/command`

You should see:
- Status messages from the device every 30 seconds
- Any commands you publish will be received and printed

### Using mosquitto_pub/sub

```bash
# Subscribe to status
mosquitto_sub -h ebd627a5b511476dae2e77a7aac9064b.s1.eu.hivemq.cloud \
  -p 8883 -t "test/sim7600/status" \
  -u "OctaviaAdmin" -P "O5fQwKQkhjO4VbiT" \
  --cafile mosquitto.org.crt

# Publish command
mosquitto_pub -h ebd627a5b511476dae2e77a7aac9064b.s1.eu.hivemq.cloud \
  -p 8883 -t "test/sim7600/command" \
  -m "Hello from MQTT client!" \
  -u "OctaviaAdmin" -P "O5fQwKQkhjO4VbiT" \
  --cafile mosquitto.org.crt
```

## Troubleshooting

### Module doesn't respond
- Check UART connections (RX/TX might be swapped)
- Verify module is powered (LED_WWAN should be blinking)
- Check baud rate (try 9600 if 115200 doesn't work)

### Network registration fails
- Ensure SIM card is properly inserted
- Check signal strength with `AT+CSQ` (should be > 10)
- Verify PIN is not required or is entered correctly
- Check antenna connection

### MQTT connection fails
- Verify APN is correct for your carrier
- Check network has internet access: `AT+CGPADDR=1`
- Ensure certificate was uploaded successfully
- Verify credentials are correct
- Check firewall isn't blocking port 8883

### Certificate issues
If certificate upload fails, try:
1. Reduce certificate size by removing comments
2. Upload in smaller chunks
3. Use HTTP instead of HTTPS (less secure)

## Customization

### Change MQTT Topics

Edit these lines in `main.cpp`:

```cpp
const char* test_topic_pub = "test/sim7600/status";
const char* test_topic_sub = "test/sim7600/command";
```

### Change Publish Interval

Edit this line in `loop()`:

```cpp
if (millis() - lastPublish > 30000) {  // 30 seconds
```

### Add Message Handling

Customize the `checkIncomingMessages()` function to handle incoming MQTT messages:

```cpp
void checkIncomingMessages() {
  if (SIM7600.available()) {
    String incoming = SIM7600.readStringUntil('\n');
    
    if (incoming.indexOf("+SMSUB:") != -1) {
      // Parse the message
      // Extract topic and payload
      // Add your custom logic here
      
      if (incoming.indexOf("LED_ON") != -1) {
        // Turn on LED
      } else if (incoming.indexOf("LED_OFF") != -1) {
        // Turn off LED
      }
    }
  }
}
```

## AT Commands Reference

Key AT commands used in this project:

| Command | Description |
|---------|-------------|
| `AT+CPIN?` | Check SIM card status |
| `AT+CREG?` | Check network registration |
| `AT+CSQ` | Check signal quality |
| `AT+CGDCONT` | Configure APN |
| `AT+CGACT` | Activate PDP context |
| `AT+CGPADDR` | Get IP address |
| `AT+SMCONF` | Configure MQTT parameters |
| `AT+SMCONN` | Connect to MQTT broker |
| `AT+SMPUB` | Publish to MQTT topic |
| `AT+SMSUB` | Subscribe to MQTT topic |
| `AT+SMDISC` | Disconnect from MQTT broker |

## License

MIT License - Feel free to use and modify for your projects.

## Credits

- **SIM7600 AT Commands:** Based on SIMCom documentation
- **HiveMQ Cloud:** MQTT broker hosting
- **Let's Encrypt:** SSL/TLS certificates

## Support

For issues or questions:
1. Check the SIM7600 AT Command Manual in the `docs` folder
2. Verify hardware connections match schematic
3. Test basic AT commands first in the AT-BASICS project
4. Check serial monitor output for detailed error messages

