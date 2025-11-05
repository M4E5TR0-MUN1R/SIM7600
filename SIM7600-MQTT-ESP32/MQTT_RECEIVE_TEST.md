# MQTT Receive Functionality - Test Guide

## 🔧 Changes Made

### 1. **Enhanced Receive Detection**
The `checkIncomingMessages()` function now:
- ✅ Shows ALL MQTT-related messages (`+CMQTTRX*`)
- ✅ Detects "PAYLOAD" keyword (tutorial method)
- ✅ Shows all Unsolicited Result Codes (URCs) starting with `+`
- ✅ Accumulates partial data correctly
- ✅ Provides debugging output for troubleshooting

### 2. **Configured Receive Mode**
Added command: `AT+CMQTTCFG="recv/mode",0,1`
- Enables direct output mode
- Messages appear immediately as URCs

### 3. **Improved Subscription Feedback**
- Shows subscription status clearly
- Provides test instructions on startup
- Displays subscribed topics

## 📡 Expected Message Formats

When a message arrives, you should see one of these patterns:

### Format 1: Full Format (Manual Spec)
```
📨 MQTT RX: +CMQTTRXSTART: 0,24,18
📨 MQTT RX: +CMQTTRXTOPIC: 0,24
📨 MQTT RX: test/sim7600/command
📨 MQTT RX: +CMQTTRXPAYLOAD: 0,18
📨 MQTT RX: Hello SIM7600!
📨 MQTT RX: +CMQTTRXEND: 0
```

### Format 2: Simplified (Tutorial Method)
```
📨 MQTT RX: +CMQTTRXTOPIC: 0,24
📨 MQTT RX: test/sim7600/command
📨 MQTT RX: +CMQTTRXPAYLOAD: 0,18
📨 MQTT RX: Hello SIM7600!
```

### Format 3: URCs Only
```
📡 URC: +CMQTTRXSTART: 0,24,18
📡 URC: +CMQTTRXTOPIC: 0,24
...
```

## 🧪 How to Test

### Method 1: Using Shiftr.io Web Interface

1. **Go to Shiftr.io**
   ```
   https://shiftr.io/khamisembeddedtests
   ```

2. **Log in** with your credentials
   - Username: `khamisembeddedtests`
   - Password: `EoYhF6hrBs1FGzFT`

3. **Navigate to the namespace** and click "Try" or "Publish"

4. **Publish a test message**:
   - Topic: `test/sim7600/command`
   - Payload: `Hello SIM7600!`
   - QoS: 0 or 1

5. **Watch your Serial Monitor** for:
   ```
   📨 MQTT RX: [incoming message]
   ```

### Method 2: Using MQTT Client (mosquitto_pub)

```bash
# Install mosquitto client
# Ubuntu: sudo apt-get install mosquitto-clients
# Windows: Download from https://mosquitto.org/download/

# Publish a test message
mosquitto_pub -h khamisembeddedtests.cloud.shiftr.io \
              -p 1883 \
              -t "test/sim7600/command" \
              -m "Hello from PC!" \
              -u "khamisembeddedtests" \
              -P "EoYhF6hrBs1FGzFT"
```

### Method 3: Using MQTTX Client (GUI)

1. Download MQTTX: https://mqttx.app/
2. Create new connection:
   - Name: SIM7600 Test
   - Host: `mqtt://khamisembeddedtests.cloud.shiftr.io`
   - Port: 1883
   - Username: `khamisembeddedtests`
   - Password: `EoYhF6hrBs1FGzFT`
3. Connect and publish to `test/sim7600/command`

### Method 4: Using Python (paho-mqtt)

```python
import paho.mqtt.client as mqtt

# Configuration
broker = "khamisembeddedtests.cloud.shiftr.io"
port = 1883
topic = "test/sim7600/command"
username = "khamisembeddedtests"
password = "EoYhF6hrBs1FGzFT"

# Create client
client = mqtt.Client()
client.username_pw_set(username, password)

# Connect and publish
client.connect(broker, port)
client.publish(topic, "Hello from Python!")
client.disconnect()

print("Message sent!")
```

## 🔍 Troubleshooting

### Issue 1: No Messages Received

**Check Serial Output for:**
```
✓ Subscribed successfully!
```

If you see `⚠ Subscription error 12`, the topic format might be wrong.

**Verify Subscription:**
Send this AT command via Serial Monitor during bridge mode:
```
AT+CMQTTSUB?
```

Expected response showing active subscription:
```
+CMQTTSUB: 0,"test/sim7600/command",1
OK
```

### Issue 2: Subscription Fails (Error 11 or 12)

**Error 11**: Not connected to broker
- Check `+CMQTTCONNECT: 0,0` appears in init logs

**Error 12**: Invalid topic or QoS
- Topic must not start/end with `/`
- Topic must not contain `#` or `+` unless wildcards
- QoS must be 0, 1, or 2

**Fix**: Verify topic format:
```cpp
// ✅ Good
"test/sim7600/command"
"sensor/temperature"

// ❌ Bad
"/test/sim7600"      // starts with /
"test/sim7600/"      // ends with /
"test##command"      // invalid characters
```

### Issue 3: Messages Not Showing Up

**Enable Debugging:**

Temporarily modify `vReceiveTask` to show ALL data:

```cpp
// In vReceiveTask, add before checkIncomingMessages():
if (SIM7600.available()) {
    Serial.print("📡 RAW: ");
    while (SIM7600.available()) {
        char c = SIM7600.read();
        Serial.print(c);
    }
    Serial.println();
}
```

This will show EXACTLY what the SIM7600 is sending.

### Issue 4: Only Seeing Partial Messages

If you see:
```
📨 MQTT RX (partial): +CMQTT...
```

The message is too large or fragmented. The receive buffer accumulates data but might need more time.

**Solution**: Increase ReceiveTask frequency or buffer size.

## 📊 What You Should See (Working Example)

```
[Upload firmware]

✓✓✓ MQTT CONNECTED SUCCESSFULLY! ✓✓✓
✓ Subscribed successfully!

╔══════════════════════════════════════════════════════╗
║  📨 MQTT RECEIVE TEST                               ║
╚══════════════════════════════════════════════════════╝
   Subscribed to: test/sim7600/command
   Publishing to: test/sim7600/status

   To test receive, publish a message to:
   Topic: test/sim7600/command
   Example: "Hello SIM7600!"
   Use your MQTT client or Shiftr.io web interface

✓ MQTT setup complete

📤 Publishing: Msg#1 | Uptime:45s
✓ Published: test/sim7600/status → Msg#1 | Uptime:45s

[Send test message from MQTTX]

📨 MQTT RX: +CMQTTRXTOPIC: 0,24
📨 MQTT RX: test/sim7600/command
📨 MQTT RX: +CMQTTRXPAYLOAD: 0,18
📨 MQTT RX: Hello SIM7600!

📤 Publishing: Msg#2 | Uptime:46s
✓ Published: test/sim7600/status → Msg#2 | Uptime:46s
```

## 🎯 Expected Behavior

### ✅ Working Correctly
- You see `📨 MQTT RX:` messages when publishing from external client
- Topic and payload are displayed
- Messages appear within 1-2 seconds of publishing
- No errors in subscription

### ⚠️ Needs Attention  
- Subscription shows error 12 (topic format issue)
- No `📨 MQTT RX:` or `📡 URC:` messages appear
- Only seeing your own published messages (echo)

### ❌ Not Working
- Subscription fails completely
- `+CMQTTCONNECT: 0,X` where X != 0
- Connection drops frequently

## 🚀 Advanced: Manual Testing via AT Commands

If receive still doesn't work, test manually:

1. **Upload firmware**
2. **Let it initialize and connect to MQTT**
3. **Open Serial Monitor**
4. **Send from another MQTT client**
5. **Watch for ANY unsolicited messages**

You might see:
```
+CMQTTRXSTART: 0,24,18
+CMQTTRXTOPIC: 0,24
test/sim7600/command
+CMQTTRXPAYLOAD: 0,18
Hello SIM7600!
+CMQTTRXEND: 0
```

If you see NOTHING, the issue is:
- Subscription didn't work
- Wrong topic name
- Receive mode not enabled

## 📝 Next Steps After Testing

Once receive is working:

1. **Parse Messages Properly**
   - Extract topic and payload
   - Process commands
   - Send responses

2. **Add Command Handlers**
   ```cpp
   if (payload == "LED_ON") {
       digitalWrite(LED_PIN, HIGH);
   }
   ```

3. **Implement Request/Response**
   - Device receives command on `test/sim7600/command`
   - Device responds on `test/sim7600/status`

4. **Add Error Handling**
   - Handle malformed messages
   - Timeout protection
   - Reconnection logic

## 🎓 Understanding MQTT Receive on SIM7600

**Key Points:**
1. **Unsolicited Result Codes (URCs)**: Messages arrive as URCs, not in response to commands
2. **Asynchronous**: Can arrive at any time, between other operations
3. **Must be polled**: The ReceiveTask checks SIM7600 UART continuously
4. **Mutex protection**: Prevents conflicts with publish operations

**That's why FreeRTOS is perfect for this!** The ReceiveTask runs independently, checking every 100ms without blocking publish operations.

Upload the firmware and test! 🚀

