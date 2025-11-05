# SIM7600 MQTT Client - FreeRTOS Edition

## 🚀 What's New

The SIM7600 MQTT client has been **converted to use FreeRTOS** for professional-grade, concurrent operation.

## ✨ Features

- **4 Concurrent Tasks**:
  - 🔧 **InitTask**: Hardware and network initialization (runs once)
  - 📤 **PublishTask**: Periodic status publishing (every 30s)
  - 📥 **ReceiveTask**: Incoming message monitoring (every 100ms)
  - 🐕 **WatchdogTask**: System health monitoring (every 60s)

- **Thread-Safe**: Mutex-protected SIM7600 UART access
- **Robust**: Priority-based task scheduling
- **Scalable**: Easy to add new tasks
- **Monitored**: Stack usage and heap memory reporting

## 📊 Task Architecture

```
┌─────────────────────────────────────────────────────────┐
│                      setup()                             │
│  Creates mutex, tasks, and starts FreeRTOS scheduler    │
└───────────────────────┬─────────────────────────────────┘
                        │
        ┌───────────────┴───────────────┐
        │                               │
        ▼                               ▼
   ┌─────────┐                     ┌─────────┐
   │InitTask │ (Priority 4)        │ Others  │ (Wait)
   │  ▪ MCP  │                     │Suspended│
   │  ▪ UART │                     └─────────┘
   │  ▪ Net  │
   │  ▪ MQTT │
   └────┬────┘
        │ mqttConnected = true
        │ Delete self
        ▼
   ┌──────────────────────────────────────┐
   │  All tasks now active concurrently:  │
   ├──────────────────────────────────────┤
   │ PublishTask  │ ▪ Every 30s          │
   │ ReceiveTask  │ ▪ Every 100ms        │
   │ WatchdogTask │ ▪ Every 60s          │
   └──────────────────────────────────────┘
```

## 🔧 How to Use

### 1. Build and Upload
```bash
cd D:\Repositories\SIM7600\SIM7600-MQTT-ESP32
C:\Users\khami\.platformio\penv\Scripts\platformio.exe run --target upload
```

### 2. Monitor Serial Output
```bash
pio device monitor -b 115200
```

### 3. Expected Output
```
╔═══════════════════════════════════════════════╗
║  SIM7600 MQTT Client - FreeRTOS Edition     ║
╚═══════════════════════════════════════════════╝

✓ Mutex created for UART protection

--- Creating FreeRTOS Tasks ---
✓ InitTask created
✓ PublishTask created
✓ ReceiveTask created
✓ WatchdogTask created

✓ FreeRTOS scheduler starting...

🚀 InitTask started on Core 1
📤 PublishTask started on Core 1
📥 ReceiveTask started on Core 1
🐕 WatchdogTask started on Core 1

[Initialization sequence...]

✓✓✓ System ready! MQTT connected. ✓✓✓

📤 PublishTask active - will publish every 30 seconds
📥 ReceiveTask active - monitoring incoming messages
🐕 WatchdogTask active - monitoring every 60 seconds

📤 Publishing: Uptime: 30 seconds [FreeRTOS]
✓ Published: test/sim7600/status → Uptime: 30 seconds [FreeRTOS]

🐕✓ Watchdog: System healthy | Uptime: 60 sec | Free heap: 245632 bytes
   📊 PublishTask stack: 2048 words remaining
   📊 ReceiveTask stack: 3021 words remaining
```

## 📚 Documentation

| File | Description |
|------|-------------|
| [FREERTOS_ARCHITECTURE.md](FREERTOS_ARCHITECTURE.md) | **Complete FreeRTOS guide** - tasks, priorities, mutex, best practices |
| [HOW_IT_WORKS.md](HOW_IT_WORKS.md) | SIM7600 MQTT basics and AT command sequences |
| [README.md](README.md) | Original project README |

## 🎯 Key Differences from Original

| Aspect | Original (Loop-based) | FreeRTOS Version |
|--------|----------------------|------------------|
| **Architecture** | Single loop | 4 concurrent tasks |
| **Blocking** | Publish blocks receive | Non-blocking |
| **Priority** | No control | Task priorities (1-4) |
| **Monitoring** | Manual | Automatic watchdog |
| **Scalability** | Hard to extend | Easy to add tasks |
| **Thread Safety** | N/A | Mutex-protected |

## 🔒 Thread Safety Example

All tasks that access the SIM7600 use mutex protection:

```cpp
// Acquire mutex with 10-second timeout
if (xSemaphoreTake(xSIM7600Mutex, pdMS_TO_TICKS(10000)) == pdTRUE) {
    // Safe to use SIM7600 now
    publishMessage(topic, message);
    
    // Always release mutex
    xSemaphoreGive(xSIM7600Mutex);
} else {
    Serial.println("⚠ Failed to acquire mutex!");
}
```

## 🔧 Configuration

### Task Priorities
```cpp
#define PRIORITY_INIT       4    // Highest - run first
#define PRIORITY_WATCHDOG   3    // High - monitor connection
#define PRIORITY_RECEIVE    2    // Medium-High - check incoming
#define PRIORITY_PUBLISH    1    // Medium - periodic publishing
```

### Task Stack Sizes (in words)
```cpp
#define STACK_SIZE_INIT     8192  // ~32KB - complex initialization
#define STACK_SIZE_PUBLISH  4096  // ~16KB - message formatting
#define STACK_SIZE_RECEIVE  4096  // ~16KB - string processing
#define STACK_SIZE_WATCHDOG 2048  // ~8KB  - simple diagnostics
```

### Timing Configuration
- **Publish interval**: 30 seconds (`PublishTask`)
- **Receive check**: 100ms (`ReceiveTask`)
- **Watchdog report**: 60 seconds (`WatchdogTask`)

Modify these in the respective task functions.

## 🚨 Troubleshooting

### Task Stack Overflow
**Symptom**: Random crashes, reboots
**Solution**: Increase stack size in `#define STACK_SIZE_*`

### Mutex Deadlock
**Symptom**: Tasks freeze, no activity
**Solution**: Check that all `xSemaphoreTake()` have matching `xSemaphoreGive()`

### Low Memory
**Symptom**: Watchdog reports low free heap
**Solution**: Reduce stack sizes or optimize String usage

### Task Not Running
**Symptom**: No output from specific task
**Solution**: Check `mqttConnected` flag, verify task creation succeeded

## 📈 Resource Usage

Current build metrics:
- **RAM**: 5.9% (19,392 bytes of 327,680)
- **Flash**: 9.4% (314,449 bytes of 3,342,336)

Plenty of room for expansion! 🎉

## 🎓 Learning Resources

- [FreeRTOS Documentation](https://www.freertos.org/Documentation/RTOS_book.html)
- [ESP32 FreeRTOS Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/freertos.html)
- [Arduino FreeRTOS Tutorial](https://www.arduino.cc/reference/en/libraries/freertos/)

## 🤝 Contributing

To add a new feature as a task:

1. Declare task handle and function
2. Create task in `setup()` with appropriate priority
3. Implement task function with mutex protection
4. Update documentation

See [FREERTOS_ARCHITECTURE.md](FREERTOS_ARCHITECTURE.md#extending-the-system) for examples.

## 📝 License

Same as original project.

## 🎉 Success!

Your SIM7600 MQTT client is now using FreeRTOS for professional-grade concurrent operation! 🚀

Upload the firmware and enjoy robust, scalable MQTT connectivity!

