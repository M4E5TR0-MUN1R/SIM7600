# SIM7600 MQTT Troubleshooting Guide

## Key Changes Made

The MQTT project has been updated to match the working AT-BASICS configuration:

### ✅ **Fixed Issues:**

1. **UART Pin Configuration** - Now matches working project:
   - **RX: GPIO 19** (ESP32 ← SIM7600)
   - **TX: GPIO 20** (ESP32 → SIM7600)

2. **PWRKEY Sequence** - **SKIPPED** (module already powered on)
   - The working project skips power-on sequence
   - MQTT project now does the same

3. **Enhanced Debugging** - Added verbose output at every step

## Expected Output

When you upload and run the MQTT project, you should now see:

```
╔═══════════════════════════════════════════════╗
║    SIM7600 MQTT Client - HiveMQ Cloud       ║
╚═══════════════════════════════════════════════╝

--- Initializing MCP23017 ---
✓ MCP23017 at 0x27 initialized
✓ All outputs configured

✓ UART initialized on RX:19, TX:20 at 115200 baud
Skipping PWRKEY sequence (module should already be on)...
Waiting for module to stabilize...

--- Testing Module Communication ---
>> AT
AT

OK

✓ Module responding!
>> ATE0
ATE0

OK

>> ATI
...

--- Setting Up Network Connection ---
Checking SIM card...
>> AT+CPIN?
+CPIN: READY

OK

Checking network registration...
>> AT+CREG?
+CREG: 0,1

OK

✓ Registered on network
...
```

## Common Issues & Solutions

### Issue 1: Module Not Responding

**Symptoms:**
```
>> AT
⚠ No response (timeout)
✗ Module not responding! Check connections.
```

**Solutions:**
1. **Verify pins are correct** (RX:19, TX:20)
2. **Check if module is powered** (LED_WWAN should be blinking)
3. **Reset ESP32** and try again
4. **Check schematic** - verify UART connections

### Issue 2: Network Registration Fails

**Symptoms:**
```
Waiting for network... (1/20)
Waiting for network... (2/20)
...
✗ Failed to register on network!
```

**Solutions:**
1. **Check SIM card** is properly inserted
2. **Verify SIM has active data plan**
3. **Check signal strength** - should be > 10
4. **Wait longer** - network registration can take 30-60 seconds
5. **Try manually** in debug mode: `AT+COPS=0` (auto select operator)

### Issue 3: IP Address Not Assigned

**Symptoms:**
```
Getting IP address...
>> AT+CGPADDR=1
ERROR

✗ Failed to get IP address
```

**Solutions:**
1. **Check APN configuration** - update for your carrier:
   ```cpp
   sendATCommand("AT+CGDCONT=1,\"IP\",\"your_apn_here\"");
   ```
   
2. **Common APNs:**
   - Safaricom (Kenya): `safaricom`
   - Vodafone: `internet.vodafone.net`
   - AT&T: `phone`
   - T-Mobile: `fast.t-mobile.com`

3. **Verify data is enabled:**
   ```
   AT+CGACT?
   ```

4. **Manually activate:**
   ```
   AT+CGACT=1,1
   ```

### Issue 4: Certificate Upload Fails

**Symptoms:**
```
Uploading CA certificate...
⚠ Certificate upload may have failed, continuing...
```

**Solutions:**
1. **Check module has enough storage:**
   ```
   AT+FSMEM
   ```

2. **Delete old certificates:**
   ```
   AT+FSDEL=cacert.pem
   ```

3. **Try without SSL** (testing only):
   - Comment out SSL configuration
   - Change port to 1883
   - Set `AT+SMCONF="SSL",0`

### Issue 5: MQTT Connection Fails

**Symptoms:**
```
>> AT+SMCONN
ERROR

✗ MQTT connection failed!
```

**Solutions:**
1. **Check MQTT state:**
   ```
   AT+SMSTATE?
   ```
   - Should return: `+SMSTATE: 0` (disconnected, ready to connect)

2. **Verify credentials** are correct in code

3. **Test network connectivity:**
   ```
   AT+CGPADDR=1  (should show IP address)
   AT+PING="8.8.8.8"  (test internet)
   ```

4. **Check MQTT configuration:**
   ```
   AT+SMCONF?
   ```

5. **Increase timeout** - some networks are slow:
   ```cpp
   sendATCommand("AT+SMCONN", 60000);  // 60 seconds
   ```

6. **Try without SSL first:**
   - Use port 1883
   - Disable SSL: `AT+SMCONF="SSL",0`
   - Once working, add SSL back

## Debug Mode

If the module isn't responding, the code will automatically enter **debug mode** where you can type AT commands directly.

### Useful AT Commands for Debugging:

```bash
# Basic connectivity
AT                      # Test communication
ATI                     # Module information
AT+CPIN?               # Check SIM status
AT+CREG?               # Network registration
AT+CSQ                 # Signal quality (>10 is good)
AT+COPS?               # Current operator

# Network/Data
AT+CGPADDR=1           # Get IP address
AT+CGACT?              # Check PDP context status
AT+CGDCONT?            # Show APN configuration
AT+PING="8.8.8.8"      # Test internet connectivity

# MQTT
AT+SMSTATE?            # Check MQTT state
AT+SMCONF?             # Show MQTT configuration
AT+SMDISC              # Disconnect MQTT
AT+FSMEM               # Check file system memory

# SSL/TLS
AT+CSSLCFG?            # Show SSL configuration
AT+FSLS                # List files (check certificates)
AT+FSDEL=cacert.pem    # Delete certificate
```

## Step-by-Step Manual Testing

If automatic connection fails, try these steps manually in debug mode:

```bash
# 1. Test basic communication
AT
ATI

# 2. Check SIM and network
AT+CPIN?
AT+CREG?
AT+CSQ

# 3. Configure APN (replace with yours)
AT+CGDCONT=1,"IP","internet"

# 4. Activate data
AT+CGACT=1,1

# 5. Get IP
AT+CGPADDR=1

# 6. Configure MQTT (without SSL for testing)
AT+SMDISC
AT+SMCONF="CLIENTID","TestClient123"
AT+SMCONF="URL","ebd627a5b511476dae2e77a7aac9064b.s1.eu.hivemq.cloud","1883"
AT+SMCONF="USERNAME","OctaviaAdmin"
AT+SMCONF="PASSWORD","O5fQwKQkhjO4VbiT"
AT+SMCONF="KEEPTIME",60
AT+SMCONF="CLEANSS",1
AT+SMCONF="QOS",1

# 7. Connect (without SSL first)
AT+SMCONN

# 8. Check state
AT+SMSTATE?

# 9. Publish test message
AT+SMPUB="test/debug",11,1,0
Hello World

# 10. Subscribe
AT+SMSUB="test/command",1
```

## Comparing with Working Project

### Working AT-BASICS Project Settings:
- ✅ RX:19, TX:20
- ✅ 115200 baud
- ✅ Skips PWRKEY (module already on)
- ✅ Module responds immediately

### MQTT Project Should Match:
- ✅ RX:19, TX:20 (FIXED)
- ✅ 115200 baud
- ✅ Skips PWRKEY (FIXED)
- ✅ Added debug mode for troubleshooting
- ✅ Enhanced error messages

## Network Requirements

Make sure your SIM card/carrier supports:
- ✅ Data connection (not just voice)
- ✅ Outbound connections on port 8883
- ✅ TLS/SSL connections
- ✅ Public IP or NAT traversal

Some carriers block:
- ❌ Port 8883 (MQTTS)
- ❌ Outbound SSL connections
- ❌ Certain cloud services

**Solution:** Try port 1883 (non-SSL) first for testing.

## Getting Help

If you're still having issues:

1. **Copy the full serial output** from startup to error
2. **Try the manual AT commands** listed above
3. **Check which step fails** - network, MQTT config, or connection
4. **Test with non-SSL** first (port 1883)
5. **Verify HiveMQ credentials** at https://console.hivemq.cloud

## Success Indicators

You know it's working when you see:
```
✓ Module responding!
✓ Registered on network
✓ Network connection established
✓ MQTT configuration complete
✓ MQTT broker connected!
✓ Subscribed to topic
✓ System ready! MQTT connected.
✓ Published: SIM7600 online!
```

Then check HiveMQ Cloud console or MQTT Explorer - you should see your device online and messages appearing!

