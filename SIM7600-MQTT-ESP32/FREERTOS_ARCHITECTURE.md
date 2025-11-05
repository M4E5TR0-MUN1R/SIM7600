# SIM7600 MQTT - FreeRTOS Architecture

## Overview

The SIM7600 MQTT client has been converted to use **FreeRTOS** for multi-tasking, providing:
- ✅ Concurrent operations (publish, receive, monitoring)
- ✅ Thread-safe UART access with mutex protection
- ✅ Robust task separation and priority management
- ✅ Efficient resource utilization
- ✅ System health monitoring

## Task Architecture

### 1. **InitTask** (Priority: 4 - Highest)
**Purpose**: One-time initialization of all hardware and network connections

**Lifecycle**: Runs once, then **deletes itself**

**Responsibilities**:
- Initialize MCP23017 GPIO expander
- Initialize SIM7600 UART communication
- Test module communication
- Set up network connection (APN, registration)
- Start MQTT service and acquire client
- Connect to MQTT broker
- Subscribe to command topic
- Publish initial "online" message

**Stack Size**: 8192 words

**Core Assignment**: Core 1 (Application Core)

```cpp
🚀 InitTask started
   ↓ Initialize hardware
   ↓ Setup network
   ↓ Setup MQTT
   ↓ Connect to broker
   ↓ Publish initial message
   ✓ Delete self
```

---

### 2. **PublishTask** (Priority: 1 - Medium)
**Purpose**: Periodically publish status messages to MQTT broker

**Lifecycle**: Runs **continuously**

**Behavior**:
- Waits for InitTask to complete
- Publishes uptime message every **30 seconds**
- Uses `vTaskDelayUntil()` for precise timing
- Acquires mutex before each publish

**Stack Size**: 4096 words

**Core Assignment**: Core 1

```cpp
📤 PublishTask loop:
   ↓ Wait 30 seconds
   ↓ Acquire SIM7600 mutex
   ↓ Publish status message
   ↓ Release mutex
   ↓ Repeat...
```

**Messages Published**:
```
"Uptime: <seconds> seconds [FreeRTOS]"
```

---

### 3. **ReceiveTask** (Priority: 2 - Medium-High)
**Purpose**: Monitor and process incoming MQTT messages

**Lifecycle**: Runs **continuously**

**Behavior**:
- Waits for InitTask to complete
- Checks for incoming MQTT messages every **100ms**
- Acquires mutex before checking UART
- Prints incoming messages to Serial

**Stack Size**: 4096 words

**Core Assignment**: Core 1

```cpp
📥 ReceiveTask loop:
   ↓ Wait 100ms
   ↓ Acquire SIM7600 mutex (500ms timeout)
   ↓ Check for incoming messages
   ↓ Release mutex
   ↓ Repeat...
```

**Detects**:
- `+CMQTTRXSTART:` - Message reception started
- `+CMQTTRXTOPIC:` - Topic information
- `+CMQTTRXPAYLOAD:` - Message payload
- `+CMQTTRXEND:` - Message complete

---

### 4. **WatchdogTask** (Priority: 3 - High)
**Purpose**: Monitor system health and report diagnostics

**Lifecycle**: Runs **continuously**

**Behavior**:
- Waits for InitTask to complete
- Reports system status every **60 seconds**
- Monitors task stack usage
- Reports free heap memory
- Can be extended for connection recovery

**Stack Size**: 2048 words

**Core Assignment**: Core 1

```cpp
🐕 WatchdogTask loop:
   ↓ Wait 60 seconds
   ↓ Acquire SIM7600 mutex
   ↓ Report system health
   ↓ Check stack high water marks
   ↓ Release mutex
   ↓ Repeat...
```

**Reports**:
```
🐕✓ Watchdog: System healthy | Uptime: 120 sec | Free heap: 245632 bytes
   📊 PublishTask stack: 2048 words remaining
   📊 ReceiveTask stack: 3021 words remaining
```

---

## Thread Safety: Mutex Protection

### Why Mutex?
The SIM7600 UART is a **shared resource** accessed by multiple tasks. Without protection, simultaneous access would cause:
- ❌ Corrupted AT commands
- ❌ Mixed responses
- ❌ Communication failures

### How It Works
```cpp
// Before using SIM7600 UART
if (xSemaphoreTake(xSIM7600Mutex, pdMS_TO_TICKS(10000)) == pdTRUE) {
    // Safe to use SIM7600 now
    sendATCommand("AT+CSQ");
    publishMessage(topic, message);
    
    // Always release!
    xSemaphoreGive(xSIM7600Mutex);
} else {
    Serial.println("⚠ Mutex timeout!");
}
```

### Timeout Strategy
- **InitTask**: 30 seconds (network operations are slow)
- **PublishTask**: 10 seconds (publishing can take time)
- **ReceiveTask**: 500ms (quick check, don't block)
- **WatchdogTask**: 5 seconds (diagnostics only)

---

## Task Priorities Explained

```
Priority 4: InitTask         ████████ (Highest - runs first)
Priority 3: WatchdogTask     ██████ (Monitor health)
Priority 2: ReceiveTask      ████ (Responsive to incoming messages)
Priority 1: PublishTask      ██ (Periodic, can wait)
```

**Why this hierarchy?**
1. **InitTask** must complete first before others can operate
2. **WatchdogTask** needs to detect problems quickly
3. **ReceiveTask** should respond promptly to incoming commands
4. **PublishTask** has no urgency (30-second intervals)

---

## Memory Management

### Stack Sizes
Each task has a dedicated stack (in **words**, not bytes):

| Task | Stack (words) | Stack (bytes) | Reasoning |
|------|--------------|---------------|-----------|
| InitTask | 8192 | ~32KB | Network setup is complex |
| PublishTask | 4096 | ~16KB | Message formatting |
| ReceiveTask | 4096 | ~16KB | String processing |
| WatchdogTask | 2048 | ~8KB | Simple diagnostics |

### Monitoring Stack Usage
The WatchdogTask reports "high water marks" - the minimum stack space remaining:
```
📊 PublishTask stack: 2048 words remaining
```
- If this drops below **512 words**, consider increasing stack size
- Use `uxTaskGetStackHighWaterMark()` for monitoring

---

## FreeRTOS vs. Traditional Loop

### Traditional Approach (Before)
```cpp
void loop() {
    checkMessages();     // Blocking
    delay(100);
    
    if (millis() - lastPublish > 30000) {
        publishMessage();  // Blocks everything
    }
}
```
**Problems**:
- ❌ Can't receive while publishing
- ❌ No priority control
- ❌ Inefficient polling
- ❌ Hard to add features

### FreeRTOS Approach (Now)
```cpp
Tasks run concurrently:
- PublishTask: Sleeps 30s, publishes, repeats
- ReceiveTask: Checks every 100ms
- WatchdogTask: Reports every 60s

✓ All tasks cooperate via scheduler
✓ Mutex ensures safe UART access
✓ CPU yields during delays
```

---

## Advantages of FreeRTOS Implementation

### 1. **Concurrency**
Multiple operations happen simultaneously:
- Receive messages while waiting to publish
- Monitor health while other tasks work

### 2. **Responsiveness**
- Incoming messages detected within **100ms**
- No blocking from publish operations

### 3. **Scalability**
Easy to add new tasks:
```cpp
// Add a sensor reading task
xTaskCreatePinnedToCore(
    vSensorTask,
    "SensorTask",
    4096,
    NULL,
    2,  // Priority
    &xSensorTaskHandle,
    1
);
```

### 4. **Resource Efficiency**
- Tasks sleep when idle (use `vTaskDelay()`)
- Scheduler distributes CPU time optimally
- No busy-waiting

### 5. **Debugging**
- Each task has a name (visible in tools)
- Stack monitoring built-in
- Priority levels clear

---

## Best Practices

### 1. **Always Release Mutex**
```cpp
// ✅ Good
if (xSemaphoreTake(xSIM7600Mutex, timeout) == pdTRUE) {
    doWork();
    xSemaphoreGive(xSIM7600Mutex);  // Always release!
}

// ❌ Bad - mutex never released on error
xSemaphoreTake(xSIM7600Mutex, timeout);
doWork();  // If this fails, mutex is locked forever!
xSemaphoreGive(xSIM7600Mutex);
```

### 2. **Use vTaskDelay(), Not delay()**
```cpp
// ✅ FreeRTOS - yields CPU to other tasks
vTaskDelay(pdMS_TO_TICKS(1000));

// ❌ Arduino - blocks everything
delay(1000);
```

### 3. **Check Return Values**
```cpp
// ✅ Handle failures
if (xSemaphoreTake(...) == pdTRUE) {
    // Success
} else {
    Serial.println("Mutex timeout!");
}
```

### 4. **Delete Unused Tasks**
```cpp
// InitTask deletes itself when done
vTaskDelete(NULL);
```

---

## Extending the System

### Add a New Task
```cpp
void vMyNewTask(void *pvParameters) {
    // Wait for MQTT connection
    while (!mqttConnected) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
    while (1) {
        // Acquire mutex
        if (xSemaphoreTake(xSIM7600Mutex, pdMS_TO_TICKS(5000)) == pdTRUE) {
            // Do work with SIM7600
            xSemaphoreGive(xSIM7600Mutex);
        }
        
        vTaskDelay(pdMS_TO_TICKS(10000));  // Run every 10s
    }
}

// Create in setup()
xTaskCreatePinnedToCore(
    vMyNewTask,
    "MyTask",
    4096,           // Stack size
    NULL,           // Parameters
    2,              // Priority (1-4)
    NULL,           // Task handle
    1               // Core 1
);
```

---

## Troubleshooting

### Stack Overflow
**Symptom**: Random crashes, reboots
**Solution**: Increase task stack size

### Mutex Deadlock
**Symptom**: Tasks freeze
**Solution**: Always use timeouts, always release mutex

### Priority Inversion
**Symptom**: Low priority task blocks high priority
**Solution**: Use proper priority levels, keep critical sections short

---

## Summary

The FreeRTOS architecture provides:
✅ **4 concurrent tasks** for different responsibilities
✅ **Mutex-protected** UART access
✅ **Precise timing** with vTaskDelayUntil()
✅ **Health monitoring** with WatchdogTask
✅ **Scalable design** for future features

This is production-ready code suitable for IoT deployments! 🚀

