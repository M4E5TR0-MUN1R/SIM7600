# SIM7600 MQTT Quick Start Guide

## 🚀 Upload and Run

```bash
cd D:\Repositories\SIM7600\SIM7600-MQTT-ESP32
C:\Users\khami\.platformio\penv\Scripts\platformio.exe run --target upload
C:\Users\khami\.platformio\penv\Scripts\platformio.exe device monitor
```

## ✅ What Was Fixed

**Problem:** All MQTT commands returned `ERROR`

**Root Cause:** Wrong command set - was using `AT+SM*` commands (not supported)

**Solution:** Changed to correct `AT+CMQTT*` commands for SIM7600

## 📋 Correct MQTT Command Reference

| Function | Command | Example |
|----------|---------|---------|
| **Start Service** | `AT+CMQTTSTART` | `AT+CMQTTSTART` |
| **Stop Service** | `AT+CMQTTSTOP` | `AT+CMQTTSTOP` |
| **Acquire Client** | `AT+CMQTTACCQ=<idx>,"<id>",<srv>` | `AT+CMQTTACCQ=0,"MyClient",1` |
| **Release Client** | `AT+CMQTTREL=<idx>` | `AT+CMQTTREL=0` |
| **Connect** | `AT+CMQTTCONNECT=<idx>,"<url>",<keep>,<clean>,"<user>","<pass>"` | `AT+CMQTTCONNECT=0,"ssl://broker:8883",60,1,"user","pass"` |
| **Disconnect** | `AT+CMQTTDISC=<idx>` | `AT+CMQTTDISC=0` |
| **Subscribe** | `AT+CMQTTSUB=<idx>,"<topic>",<qos>` | `AT+CMQTTSUB=0,"test/cmd",1` |
| **Unsubscribe** | `AT+CMQTTUNSUB=<idx>,"<topic>"` | `AT+CMQTTUNSUB=0,"test/cmd"` |
| **Set Topic** | `AT+CMQTTTOPIC=<idx>,<len>` | `AT+CMQTTTOPIC=0,10` + `test/topic` |
| **Set Payload** | `AT+CMQTTPAYLOAD=<idx>,<len>` | `AT+CMQTTPAYLOAD=0,5` + `Hello` |
| **Publish** | `AT+CMQTTPUB=<idx>,<qos>,<timeout>` | `AT+CMQTTPUB=0,1,60` |
| **SSL Config** | `AT+CMQTTSSLCFG=<idx>,<ssl_ctx>` | `AT+CMQTTSSLCFG=0,0` |

## 📝 Expected Success Output

```
╔═══════════════════════════════════════════════╗
║    SIM7600 MQTT Client - HiveMQ Cloud       ║
╚═══════════════════════════════════════════════╝

--- Initializing MCP23017 ---
✓ MCP23017 at 0x27 initialized
✓ All outputs configured

✓ UART initialized on RX:19, TX:20 at 115200 baud
Skipping PWRKEY sequence...

--- Testing Module Communication ---
>> AT
AT
OK
✓ Module responding!

--- Setting Up Network Connection ---
Checking SIM card...
+CPIN: READY
✓ Registered on network
IP: 10.213.115.237
✓ Network connection established

--- Setting Up MQTT Configuration ---
>> AT+CMQTTSTART
OK
✓ MQTT service started

>> AT+CMQTTACCQ=0,"SIM7600_ESP32_Client",1
OK
✓ MQTT client acquired

✓ CA certificate uploaded
✓ MQTT configuration complete

--- Connecting to MQTT Broker ---
>> AT+CMQTTCONNECT=0,"ssl://ebd627a5b511476dae2e77a7aac9064b.s1.eu.hivemq.cloud:8883",60,1,"OctaviaAdmin","O5fQwKQkhjO4VbiT"
OK
+CMQTTCONNECT: 0,0    ← Success code
✓ MQTT broker connected!

>> AT+CMQTTSUB=0,"test/sim7600/command",1
OK
+CMQTTSUB: 0,0        ← Success code
✓ Subscribed to topic

✓ System ready! MQTT connected.
✓ Published: SIM7600 online!
```

## 🔧 Manual Testing Commands

If you want to test manually via AT command bridge:

```bash
# 1. Start MQTT service
AT+CMQTTSTART

# 2. Acquire client
AT+CMQTTACCQ=0,"TestClient",1

# 3. Connect (without SSL for testing)
AT+CMQTTCONNECT=0,"tcp://broker.hivemq.com:1883",60,1

# 4. Subscribe
AT+CMQTTSUB=0,"test/topic",1

# 5. Publish
AT+CMQTTTOPIC=0,10
test/topic
AT+CMQTTPAYLOAD=0,11
Hello World
AT+CMQTTPUB=0,1,60

# 6. Disconnect
AT+CMQTTDISC=0

# 7. Release client
AT+CMQTTREL=0

# 8. Stop service
AT+CMQTTSTOP
```

## 🌐 Test with HiveMQ Web Client

1. Go to: https://www.hivemq.com/demos/websocket-client/
2. Click "Connect" (or configure):
   - **Host:** `ebd627a5b511476dae2e77a7aac9064b.s1.eu.hivemq.cloud`
   - **Port:** 8884 (WebSocket over TLS)
   - **Username:** `OctaviaAdmin`
   - **Password:** `O5fQwKQkhjO4VbiT`
3. Subscribe to: `test/sim7600/status`
4. You should see messages from your device!
5. Publish to: `test/sim7600/command` to send commands to device

## 🐛 Troubleshooting Quick Checks

### CMQTTSTART returns ERROR
```bash
AT+CMQTTSTOP
AT+CMQTTREL=0
AT+CMQTTSTART
```

### CMQTTACCQ returns ERROR
```bash
# Check if already acquired
AT+CMQTTREL=0
# Try again
AT+CMQTTACCQ=0,"NewClient",1
```

### CMQTTCONNECT returns ERROR
1. **Check you have IP:**
   ```
   AT+CGPADDR=1
   ```
   Should return: `+CGPADDR: 1,<IP address>`

2. **Test without SSL:**
   ```
   AT+CMQTTCONNECT=0,"tcp://broker.hivemq.com:1883",60,1
   ```

3. **Check connection status:**
   ```
   AT+CMQTTDISC=0
   ```

4. **Verify credentials** in code are correct

### No messages received
- Check subscription worked: look for `+CMQTTSUB: 0,0`
- Test by publishing from HiveMQ web client
- Messages appear as: `+CMQTTRXTOPIC:` / `+CMQTTRXPAYLOAD:`

## 📚 Key Files

- **Main Code:** `src/main.cpp`
- **Configuration:** Edit broker/credentials at top of `main.cpp`
- **Fix Details:** `MQTT_COMMANDS_FIX.md`
- **Full Troubleshooting:** `TROUBLESHOOTING.md`
- **Documentation:** `README.md`

## 🎯 Next Steps

1. **Upload the fixed code**
2. **Verify MQTT connects** - look for `+CMQTTCONNECT: 0,0`
3. **Check messages on HiveMQ** web client
4. **Customize topics** for your application
5. **Add message handling** in `checkIncomingMessages()`

## ⚡ Success Indicators

You know it's working when you see:
- ✅ `OK` after `AT+CMQTTSTART`
- ✅ `OK` after `AT+CMQTTACCQ`  
- ✅ `+CMQTTCONNECT: 0,0` (connection success)
- ✅ `+CMQTTSUB: 0,0` (subscription success)
- ✅ `+CMQTTPUB: 0,0` (publish success)
- ✅ Messages visible in HiveMQ web client

## 💡 Important Notes

- **Client Index:** Always use `0` (first/only client)
- **QoS Levels:** 0=At most once, 1=At least once, 2=Exactly once
- **Keep Alive:** 60 seconds recommended
- **Clean Session:** 1=clean, 0=persistent
- **SSL Context:** 0 (configured with `AT+CSSLCFG`)
- **Topic/Payload:** Must set both before publishing

The code is now ready to go! 🚀

