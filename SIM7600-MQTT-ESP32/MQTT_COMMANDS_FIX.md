# SIM7600 MQTT Commands Fix

## Problem Identified

The original code was using **wrong MQTT command set** for SIM7600:
- ❌ `AT+SMCONF` - NOT supported by SIM7600
- ❌ `AT+SMCONN` - NOT supported by SIM7600  
- ❌ `AT+SMSUB` - NOT supported by SIM7600
- ❌ `AT+SMPUB` - NOT supported by SIM7600

These commands (`AT+SM*`) are for **older modules** or different chipsets.

## Solution Applied

SIM7600 uses the **`AT+CMQTT*`** command set:

### Correct Command Sequence:

```
1. AT+CMQTTSTART          → Start MQTT service
2. AT+CMQTTACCQ           → Acquire MQTT client
3. AT+CMQTTSSLCFG         → Configure SSL (optional)
4. AT+CMQTTCONNECT        → Connect to broker
5. AT+CMQTTSUB            → Subscribe to topics
6. AT+CMQTTTOPIC          → Set publish topic
7. AT+CMQTTPAYLOAD        → Set publish payload
8. AT+CMQTTPUB            → Publish message
```

## Commands Changed

### Setup MQTT:
```cpp
// OLD (WRONG):
AT+SMCONF="CLIENTID","client_id"
AT+SMCONF="URL","broker","port"
AT+SMCONF="USERNAME","user"
AT+SMCONF="PASSWORD","pass"

// NEW (CORRECT):
AT+CMQTTSTART
AT+CMQTTACCQ=0,"client_id",1
AT+CMQTTSSLCFG=0,0
```

### Connect to Broker:
```cpp
// OLD (WRONG):
AT+SMCONN

// NEW (CORRECT):
AT+CMQTTCONNECT=0,"ssl://broker:8883",60,1,"username","password"
```

### Subscribe:
```cpp
// OLD (WRONG):
AT+SMSUB="topic",1

// NEW (CORRECT):
AT+CMQTTSUB=0,"topic",1
```

### Publish:
```cpp
// OLD (WRONG):
AT+SMPUB="topic",length,1,0
<message>

// NEW (CORRECT):
AT+CMQTTTOPIC=0,<topic_length>
<topic>
AT+CMQTTPAYLOAD=0,<payload_length>
<payload>
AT+CMQTTPUB=0,1,60
```

### Receive Messages:
```cpp
// OLD (WRONG):
+SMSUB: "topic",datalen,data

// NEW (CORRECT):
+CMQTTRXSTART: 0,topic_len,payload_len
+CMQTTRXTOPIC: 0,length
<topic>
+CMQTTRXPAYLOAD: 0,length
<payload>
+CMQTTRXEND: 0
```

## Code Changes Summary

### 1. `setupMQTT()` Function
- Added `AT+CMQTTSTART` to initialize MQTT service
- Added `AT+CMQTTACCQ` to acquire client
- Changed SSL config to `AT+CMQTTSSLCFG`
- Removed all `AT+SMCONF` commands

### 2. `connectMQTT()` Function
- Changed `AT+SMCONN` to `AT+CMQTTCONNECT`
- Added username/password to connect command
- Changed `AT+SMSUB` to `AT+CMQTTSUB`
- Updated URL format to `ssl://broker:port`

### 3. `publishMessage()` Function
- Complete rewrite using:
  - `AT+CMQTTTOPIC` - Set topic
  - `AT+CMQTTPAYLOAD` - Set message
  - `AT+CMQTTPUB` - Publish

### 4. `checkIncomingMessages()` Function
- Updated to parse `+CMQTTRX*` format
- Changed from `+SMSUB` to `+CMQTTRXTOPIC/PAYLOAD`

## Why This Matters

The SIM7600 firmware **does not support** the `AT+SM*` commands at all, which is why every MQTT command was returning `ERROR`. The correct `AT+CMQTT*` commands are documented in:

**SIM7500_SIM7600_SIM7800 Series_MQTT_AT Command Manual_V1.00.pdf**

## Expected Output After Fix

```
--- Setting Up MQTT Configuration ---
Stopping any existing MQTT service...
>> AT+CMQTTSTOP
OK

Releasing any existing MQTT client...
>> AT+CMQTTREL=0
OK

Starting MQTT service...
>> AT+CMQTTSTART
OK
✓ MQTT service started

Acquiring MQTT client...
>> AT+CMQTTACCQ=0,"SIM7600_ESP32_Client",1
OK
✓ MQTT client acquired

Configuring SSL/TLS...
✓ CA certificate uploaded
✓ MQTT configuration complete

--- Connecting to MQTT Broker ---
Connecting to broker...
>> AT+CMQTTCONNECT=0,"ssl://broker:8883",60,1,"user","pass"
OK
+CMQTTCONNECT: 0,0    ← Connection successful!
✓ MQTT broker connected!

Subscribing to: test/sim7600/command
>> AT+CMQTTSUB=0,"test/sim7600/command",1
OK
+CMQTTSUB: 0,0        ← Subscription successful!
✓ Subscribed to topic
```

## Testing Steps

1. **Upload new firmware:**
   ```bash
   pio run --target upload
   ```

2. **Monitor output:**
   ```bash
   pio device monitor
   ```

3. **Expected results:**
   - ✅ MQTT service starts successfully
   - ✅ Client acquired
   - ✅ Connected to HiveMQ Cloud
   - ✅ Subscribed to topics
   - ✅ Can publish messages
   - ✅ Can receive messages

## Troubleshooting

### If CMQTTSTART fails:
```
AT+CMQTTSTOP
AT+CMQTTREL=0
AT+CMQTTSTART
```

### If connection fails:
1. Check network has IP: `AT+CGPADDR=1`
2. Test without SSL first: `tcp://broker:1883`
3. Verify credentials are correct
4. Check firewall allows port 8883

### Manual Testing:
```bash
# In AT command bridge mode:
AT+CMQTTSTART
AT+CMQTTACCQ=0,"TestClient",1
AT+CMQTTCONNECT=0,"tcp://broker.hivemq.com:1883",60,1
AT+CMQTTSUB=0,"test/topic",1
AT+CMQTTTOPIC=0,10
test/topic
AT+CMQTTPAYLOAD=0,5
Hello
AT+CMQTTPUB=0,1,60
```

## References

- **SIM7600 MQTT Manual:** `docs/SIM7500_SIM7600_SIM7800 Series_MQTT_AT Command Manual_V1.00.pdf`
- **AT Command Manual:** `docs/SIM7500_SIM7600 Series_AT Command Manual_V1.01.pdf`
- **Forum Discussion:** https://forum.arduino.cc/t/using-mqtt-with-sim-7600e-h/693663

## Summary

The fix changes **all MQTT-related AT commands** from the unsupported `AT+SM*` set to the correct `AT+CMQTT*` set that SIM7600 actually implements. This is a fundamental change that makes MQTT functionality work on the SIM7600 module.

**Key takeaway:** Different SIMCom modules use different MQTT command sets. Always check the specific manual for your module!

