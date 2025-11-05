# High-Frequency MQTT Publishing Test Guide

## 🎯 Test Configuration

The code is now configured for **high-frequency data logging testing**:
- **Publish Interval**: 1 second (down from 30 seconds)
- **Message Format**: `Msg#<sequence> | Uptime:<seconds>s`
- **Purpose**: Simulate real data logging application

## 📊 What to Monitor

### 1. **Message Sequence Numbers**
Each message includes a sequence number to track delivery:
```
📤 Publishing: Msg#1 | Uptime:45s
📤 Publishing: Msg#2 | Uptime:46s
📤 Publishing: Msg#3 | Uptime:47s
```

**Watch For:**
- ✅ Sequential numbers (no gaps = no lost messages)
- ❌ Gaps in sequence (indicates lost/failed publishes)
- ❌ Duplicates (indicates retry issues)

### 2. **Timing Consistency**
With 1-second intervals, uptime should increment by 1:
```
Msg#10 | Uptime:54s
Msg#11 | Uptime:55s  ← Perfect timing
Msg#12 | Uptime:56s
```

**Watch For:**
- ✅ Consistent 1-second increments
- ⚠️ Skipped seconds (indicates publish delays)
- ⚠️ Irregular timing (system overload)

### 3. **System Health (Every 60 seconds)**
```
🐕✓ Watchdog: System healthy | Uptime: 120 sec | Free heap: 245632 bytes
   📊 PublishTask stack: 2048 words remaining
   📊 ReceiveTask stack: 3021 words remaining
```

**Watch For:**
- ✅ Stable free heap (should not decrease over time)
- ⚠️ Low stack remaining (<512 words is concerning)
- ❌ Heap decreasing = memory leak!

### 4. **MQTT Connection Stability**
Look for any disconnection messages:
```
⚠ MQTT not connected, skipping publish
⚠ Failed to acquire mutex for publish!
```

**Good Sign**: No warnings, continuous publishing

## 🧪 Test Scenarios

### Test 1: Short Burst (5 minutes)
**Purpose**: Verify basic stability
```bash
# Upload and monitor
pio run --target upload
pio device monitor -b 115200

# Let it run for 5 minutes
# Expected: ~300 messages
# Check: All sequence numbers present (1-300)
```

### Test 2: Extended Run (1 hour)
**Purpose**: Test for memory leaks and connection stability
```bash
# Let it run for 1 hour
# Expected: ~3600 messages
# Monitor heap every 60 seconds - should be stable
```

**Red Flags:**
- Heap memory decreasing by more than 5% over the hour
- Connection drops
- Publish failures

### Test 3: Network Stress
**Purpose**: Test behavior under poor signal
```bash
# Move device to area with weak signal
# Watch for:
# - Longer publish times
# - Mutex timeout warnings
# - Connection recovery behavior
```

## 📈 Performance Expectations

### SIM7600 MQTT Publish Performance

| Metric | Expected Value | Concerning If |
|--------|---------------|---------------|
| **Publish Time** | 1-3 seconds | >5 seconds |
| **Messages/Hour** | ~3600 (at 1s interval) | <3000 (17% loss) |
| **Heap Usage** | Stable ±5KB | Decreasing trend |
| **Mutex Wait** | <100ms | >5 seconds |

### Real-World Considerations

**At 1-second intervals:**
- ✅ **Good for**: Sensor data (temperature, pressure, etc.)
- ✅ **Good for**: Status updates with moderate frequency
- ⚠️ **Consider**: Each publish takes ~1-3 seconds on cellular
- ⚠️ **Data Cost**: ~3600 MQTT messages/hour = significant data usage

**Recommended Intervals by Use Case:**
```
Critical Monitoring:    1-5 seconds   (your current test)
Regular Data Logging:   10-30 seconds (good balance)
Status Updates:         60-300 seconds (efficient)
Low-Power Operation:    >300 seconds   (battery friendly)
```

## 🔍 Debugging High-Frequency Issues

### Issue 1: Messages Taking Too Long
**Symptom**: Uptime jumps by 2-3 seconds per message
```
Msg#10 | Uptime:54s
Msg#11 | Uptime:57s  ← 3 second gap!
```

**Causes:**
- MQTT publish takes longer than 1 second
- Network latency
- Mutex contention with ReceiveTask

**Solutions:**
- Increase publish interval to 2-3 seconds
- Check signal quality (`AT+CSQ`)
- Reduce ReceiveTask frequency (100ms → 500ms)

### Issue 2: Mutex Timeouts
**Symptom**: `⚠ Failed to acquire mutex for publish!`

**Causes:**
- Another task holding mutex too long
- Network delay in previous operation

**Solutions:**
```cpp
// Increase mutex timeout in PublishTask
xSemaphoreTake(xSIM7600Mutex, pdMS_TO_TICKS(15000))  // Was 10000
```

### Issue 3: Memory Issues
**Symptom**: Free heap decreasing over time

**Causes:**
- String concatenation in loops
- Response buffer not clearing
- Task stack overflow

**Solutions:**
- Check WatchdogTask reports for stack usage
- Clear `response` String after each use
- Increase task stack sizes if needed

## 📝 Test Checklist

Before starting your data logging project:

- [ ] Test runs for 5 minutes without errors
- [ ] All sequence numbers accounted for (no gaps)
- [ ] Heap memory stable
- [ ] No mutex timeouts
- [ ] Timing is consistent (±1 second)
- [ ] Test runs for 1 hour successfully
- [ ] Connection survives weak signal periods
- [ ] System recovers from temporary disconnections

## 🎓 Optimization Tips

### For Even Higher Frequencies (<1 second):

**Batch Publishing:**
```cpp
// Instead of publishing every reading individually
// Batch multiple readings into one message:
snprintf(msg, sizeof(msg), 
  "T1:%.1f,T2:%.1f,T3:%.1f | %lu", 
  temp1, temp2, temp3, timestamp);
```

**Binary Payloads:**
- Use binary format instead of text
- Reduces message size by ~50%
- Faster transmission

**QoS Adjustment:**
```cpp
// Change from QoS 1 to QoS 0 (no acknowledgment)
SIM7600.println("AT+CMQTTPUB=0,0,60");  // QoS 0 instead of 1
```

### For Lower Data Costs:

**Compress Messages:**
```cpp
// Instead of: "Msg#123 | Uptime:456s"
// Use:        "123:456"
// 60% size reduction!
```

**Report Changes Only:**
```cpp
// Don't publish if value hasn't changed significantly
if (abs(newTemp - lastTemp) > 0.5) {
    publishMessage(topic, tempStr);
    lastTemp = newTemp;
}
```

## 🚀 Ready to Test!

Your firmware is built and ready to test at 1-second intervals.

**Upload and Monitor:**
```bash
cd D:\Repositories\SIM7600\SIM7600-MQTT-ESP32
pio run --target upload
pio device monitor -b 115200
```

**What You'll See:**
```
📤 PublishTask active - will publish every 1 second (HIGH FREQUENCY TEST)
📤 Publishing: Msg#1 | Uptime:45s
✓ Published: test/sim7600/status → Msg#1 | Uptime:45s
📤 Publishing: Msg#2 | Uptime:46s
✓ Published: test/sim7600/status → Msg#2 | Uptime:46s
...
```

Monitor on Shiftr.io dashboard to verify messages arrive!

**Good luck with your data logging project!** 🎉

