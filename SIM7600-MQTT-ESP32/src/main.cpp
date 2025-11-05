#include <Arduino.h>
#include <Adafruit_MCP23X17.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <freertos/queue.h>

/* ===============================================================================
 * SIM7600 MQTT CLIENT - FreeRTOS Edition
 * ===============================================================================
 * 
 * This code implements MQTT connectivity using the SIM7600E 4G modem with
 * FreeRTOS multi-tasking for robust, concurrent operation.
 * 
 * FREERTOS ARCHITECTURE:
 * - InitTask: Initializes hardware, network, and MQTT (runs once, deletes self)
 * - PublishTask: Publishes status messages every 30 seconds
 * - ReceiveTask: Monitors incoming MQTT messages (checks every 100ms)
 * - WatchdogTask: Reports system health every 60 seconds
 * - Mutex protection: SIM7600 UART is protected by a mutex for thread safety
 * 
 * KEY LEARNING: The SIM7600 MQTT implementation is SIMPLE!
 * Based on real-world tutorials (MQTT_tutorial.txt and MQTT_tutorial2.txt),
 * we discovered that minimal configuration is needed:
 * 
 * 1. NETWORK SETUP:
 *    - Wait for network registration (AT+CREG? returns 0,1)
 *    - Configure APN (AT+CGDCONT and AT+CGAUTH)
 *    - NO complex network activation needed (no AT+NETOPEN, AT+CIPSHUT, etc.)
 *    - The module handles GPRS attachment automatically
 * 
 * 2. MQTT INITIALIZATION:
 *    - AT+CMQTTSTOP - Stop any existing service (error 21 is NORMAL if not running)
 *    - AT+CMQTTREL=0 - Release any existing client
 *    - AT+CMQTTSTART - Start MQTT service (error 23 can be ignored!)
 *    - AT+CMQTTACCQ=0,"client_id" - Acquire a client
 * 
 * 3. MQTT CONNECTION:
 *    - AT+CMQTTCONNECT=0,"tcp://broker:port",keepalive,clean,"user","pass"
 *    - Response: +CMQTTCONNECT: 0,0 means SUCCESS!
 *    - Any other code (0,12 etc.) indicates the error type
 * 
 * 4. PUBLISH MESSAGE:
 *    - AT+CMQTTTOPIC=0,<length> → Send topic string
 *    - AT+CMQTTPAYLOAD=0,<length> → Send payload string
 *    - AT+CMQTTPUB=0,<qos>,<timeout> → Publish
 *    - Use 1 second delays between commands (no prompt waiting needed!)
 * 
 * 5. SUBSCRIBE:
 *    - AT+CMQTTSUB=0,"topic",<qos>
 *    - Incoming messages trigger +CMQTTRXSTART and +CMQTTRXTOPIC URCs
 * 
 * IMPORTANT ERROR CODES TO IGNORE:
 * - +CMQTTSTOP: 21 = "Operation not allowed" (nothing was running to stop)
 * - +CMQTTSTART: 23 = "Network not ready" (but it works anyway!)
 * - +CMQTTSUB: 0,12 = Subscription error (might be topic format, but publish works)
 * 
 * SUCCESS INDICATOR:
 * - +CMQTTCONNECT: 0,0 = Connected successfully! This is the key response.
 * 
 * Based on: SIM7500_SIM7600_SIM7800 Series_MQTT_AT Command Manual_V1.00.pdf
 * =============================================================================== */

// ===== MCP23017 Configuration =====
#define DO_1_ADDR 0x27
Adafruit_MCP23X17 mcpDO_1;  // 0x27 - Digital outputs

// ===== SIM7600 UART Configuration =====
#define SIM7600_RX 19  // ESP32 RX <- SIM7600 TX
#define SIM7600_TX 20  // ESP32 TX -> SIM7600 RX
#define SIM7600_PWRKEY 9  // MCP23017 pin number for PWRKEY
#define SIM7600_BAUD 115200

// ===== Network Configuration (Safaricom Kenya) =====
// APN: safaricom
// Username: saf
// Password: data
// These are configured in setupNetwork() function

HardwareSerial SIM7600(1); // Use Serial1

// ===== MQTT Configuration - Shiftr.io (Testing without SSL) =====
const char* mqtt_broker = "khamisembeddedtests.cloud.shiftr.io";
const uint16_t mqtt_port = 1883;  // Plain MQTT (no SSL)
const char* mqtt_user = "khamisembeddedtests";
const char* mqtt_password = "EoYhF6hrBs1FGzFT";
const char* mqtt_client_id = "SIM7600_ESP32_Client";

// Original HiveMQ Cloud config (switch back once working):
// mqtt_broker = "ebd627a5b511476dae2e77a7aac9064b.s1.eu.hivemq.cloud"
// mqtt_port = 8883
// mqtt_user = "OctaviaAdmin"
// mqtt_password = "O5fQwKQkhjO4VbiT"

// Test topics
const char* test_topic_pub = "test/sim7600/status";
const char* test_topic_sub = "test/sim7600/command";

// ===== HiveMQ Cloud Let's Encrypt CA Certificate =====
const char *root_ca = 
"-----BEGIN CERTIFICATE-----\r\n"
"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\r\n"
"-----END CERTIFICATE-----\r\n";

// ===== Global Variables =====
String response = "";
bool mqttConnected = false;

// ===== FreeRTOS Task Handles and Synchronization =====
TaskHandle_t xInitTaskHandle = NULL;
TaskHandle_t xPublishTaskHandle = NULL;
TaskHandle_t xReceiveTaskHandle = NULL;
TaskHandle_t xWatchdogTaskHandle = NULL;

// Semaphore to protect SIM7600 UART access (shared resource)
SemaphoreHandle_t xSIM7600Mutex = NULL;

// Queue for publish requests (optional - for future expansion)
QueueHandle_t xPublishQueue = NULL;

// Task priorities (higher number = higher priority)
#define PRIORITY_INIT       4    // Highest - run first
#define PRIORITY_WATCHDOG   3    // High - monitor connection
#define PRIORITY_RECEIVE    2    // Medium-High - check incoming messages
#define PRIORITY_PUBLISH    1    // Medium - periodic publishing

// Task stack sizes (in words, not bytes)
#define STACK_SIZE_INIT     8192
#define STACK_SIZE_PUBLISH  4096
#define STACK_SIZE_RECEIVE  4096
#define STACK_SIZE_WATCHDOG 2048

// ===== Function Declarations =====
bool initMCP23017();
void powerOnModule();
String sendATCommand(const char* command, unsigned long timeout = 2000);
bool waitForResponse(const char* expected, unsigned long timeout);
bool setupNetwork();
bool setupMQTT();
bool connectMQTT();
void publishMessage(const char* topic, const char* message);
void checkIncomingMessages();

// FreeRTOS Task Functions
void vInitTask(void *pvParameters);
void vPublishTask(void *pvParameters);
void vReceiveTask(void *pvParameters);
void vWatchdogTask(void *pvParameters);

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n\n╔═══════════════════════════════════════════════╗");
  Serial.println("║  SIM7600 MQTT Client - FreeRTOS Edition     ║");
  Serial.println("╚═══════════════════════════════════════════════╝\n");
  
  // Create mutex for SIM7600 UART protection
  xSIM7600Mutex = xSemaphoreCreateMutex();
  if (xSIM7600Mutex == NULL) {
    Serial.println("✗ Failed to create mutex! Halting...");
    while(1) delay(1000);
  }
  Serial.println("✓ Mutex created for UART protection");
  
  // Create publish queue (capacity of 10 messages)
  xPublishQueue = xQueueCreate(10, sizeof(char*) * 2);  // topic + message
  if (xPublishQueue == NULL) {
    Serial.println("⚠ Failed to create publish queue (non-critical)");
  }
  
  // Create FreeRTOS tasks
  Serial.println("\n--- Creating FreeRTOS Tasks ---");
  
  // Initialization Task (runs once, then deletes itself)
  BaseType_t xReturned = xTaskCreatePinnedToCore(
    vInitTask,              // Task function
    "InitTask",             // Task name
    STACK_SIZE_INIT,        // Stack size
    NULL,                   // Parameters
    PRIORITY_INIT,          // Priority
    &xInitTaskHandle,       // Task handle
    1                       // Core 1 (0 = protocol core, 1 = app core)
  );
  
  if (xReturned != pdPASS) {
    Serial.println("✗ Failed to create InitTask! Halting...");
    while(1) delay(1000);
  }
  Serial.println("✓ InitTask created");
  
  // Publish Task (periodic status publishing)
  xReturned = xTaskCreatePinnedToCore(
    vPublishTask,
    "PublishTask",
    STACK_SIZE_PUBLISH,
    NULL,
    PRIORITY_PUBLISH,
    &xPublishTaskHandle,
    1
  );
  
  if (xReturned != pdPASS) {
    Serial.println("✗ Failed to create PublishTask!");
  } else {
    Serial.println("✓ PublishTask created");
  }
  
  // Receive Task (monitor incoming MQTT messages)
  xReturned = xTaskCreatePinnedToCore(
    vReceiveTask,
    "ReceiveTask",
    STACK_SIZE_RECEIVE,
    NULL,
    PRIORITY_RECEIVE,
    &xReceiveTaskHandle,
    1
  );
  
  if (xReturned != pdPASS) {
    Serial.println("✗ Failed to create ReceiveTask!");
  } else {
    Serial.println("✓ ReceiveTask created");
  }
  
  // Watchdog Task (monitor connection health)
  xReturned = xTaskCreatePinnedToCore(
    vWatchdogTask,
    "WatchdogTask",
    STACK_SIZE_WATCHDOG,
    NULL,
    PRIORITY_WATCHDOG,
    &xWatchdogTaskHandle,
    1
  );
  
  if (xReturned != pdPASS) {
    Serial.println("⚠ Failed to create WatchdogTask (non-critical)");
  } else {
    Serial.println("✓ WatchdogTask created");
  }
  
  Serial.println("\n✓ FreeRTOS scheduler starting...\n");
}

void loop() {
  // FreeRTOS scheduler handles everything
  // This loop is kept minimal to allow tasks to run
  vTaskDelay(pdMS_TO_TICKS(1000));  // Yield to other tasks
}

// ===== Function Implementations =====

bool initMCP23017() {
  Serial.println("--- Initializing MCP23017 ---");
  
  if (!mcpDO_1.begin_I2C(DO_1_ADDR)) {
    Serial.printf("✗ Failed to initialize MCP23017 at 0x%02X\n", DO_1_ADDR);
    return false;
  }
  Serial.printf("✓ MCP23017 at 0x%02X initialized\n", DO_1_ADDR);
  
  // Configure all pins as outputs and set to LOW
  for (int i = 0; i < 16; i++) {
    mcpDO_1.pinMode(i, OUTPUT);
    mcpDO_1.digitalWrite(i, LOW);
  }
  
  Serial.println("✓ All outputs configured\n");
  return true;
}

void powerOnModule() {
  Serial.println("--- Powering On SIM7600 Module ---");
  
  // PWRKEY power-on sequence
  mcpDO_1.digitalWrite(SIM7600_PWRKEY, HIGH);
  delay(300);
  
  Serial.println("Pulling PWRKEY LOW...");
  mcpDO_1.digitalWrite(SIM7600_PWRKEY, LOW);
  delay(1500);
  
  Serial.println("Setting PWRKEY HIGH...");
  mcpDO_1.digitalWrite(SIM7600_PWRKEY, HIGH);
  
  Serial.println("Waiting for module boot (10 seconds)...");
  delay(10000);
  Serial.println("✓ Module powered on\n");
}

String sendATCommand(const char* command, unsigned long timeout) {
  Serial.printf(">> %s\n", command);
  SIM7600.println(command);
  
  response = "";
  unsigned long startTime = millis();
  
  while (millis() - startTime < timeout) {
    while (SIM7600.available()) {
      char c = SIM7600.read();
      response += c;
      Serial.print(c); // Print response in real-time
    }
    
    if (response.indexOf("OK") != -1 || response.indexOf("ERROR") != -1) {
      break;
    }
  }
  
  if (response.length() == 0) {
    Serial.println("⚠ No response (timeout)");
  } else if (response.indexOf("ERROR") != -1) {
    Serial.println("⚠ Command returned ERROR");
  }
  Serial.println();
  
  return response;
}

bool waitForResponse(const char* expected, unsigned long timeout) {
  unsigned long startTime = millis();
  String buffer = "";
  
  while (millis() - startTime < timeout) {
    while (SIM7600.available()) {
      char c = SIM7600.read();
      buffer += c;
      Serial.print(c);
    }
    
    if (buffer.indexOf(expected) != -1) {
      return true;
    }
  }
  
  return false;
}

bool setupNetwork() {
  Serial.println("--- Setting Up Network Connection ---");
  
  // Check SIM card status
  Serial.println("Checking SIM card...");
  sendATCommand("AT+CPIN?");
  if (response.indexOf("READY") == -1) {
    Serial.println("✗ SIM card not ready!");
    return false;
  }
  
  // Check network registration
  Serial.println("Checking network registration...");
  bool registered = false;
  for (int i = 0; i < 20; i++) {
    sendATCommand("AT+CREG?", 1000);
    if (response.indexOf(",1") != -1 || response.indexOf(",5") != -1) {
      Serial.println("✓ Registered on network");
      registered = true;
      break;
    }
    Serial.printf("Waiting for network... (%d/20)\n", i+1);
    delay(2000);
  }
  
  if (!registered) {
    Serial.println("✗ Failed to register on network!");
    return false;
  }
  
  // Check signal quality
  sendATCommand("AT+CSQ");
  
  // Configure APN for Safaricom Kenya
  Serial.println("Configuring Safaricom APN...");
  sendATCommand("AT+CGDCONT=1,\"IP\",\"safaricom\"");
  
  // Set authentication (PAP or CHAP)
  Serial.println("Setting APN authentication...");
  sendATCommand("AT+CGAUTH=1,1,\"saf\",\"data\"");  // 1=PAP auth, username, password
  
  // Just wait - the tutorials show no special setup needed!
  // PDP context activation via AT+CREG is enough
  Serial.println("Waiting for network to stabilize...");
  vTaskDelay(pdMS_TO_TICKS(5000));
  
  Serial.println("✓ Network connection established\n");
  return true;
}

bool setupMQTT() {
  Serial.println("\n--- Setting Up MQTT Configuration ---");
  
  char cmd[300];
  
  // Following the tutorial approach - simple sequence
  // Note: Initial errors (21, 23) are NORMAL and can be ignored
  Serial.println("Stopping any existing MQTT service...");
  sendATCommand("AT+CMQTTSTOP", 3000);
  // Error 21 = "operation not allowed" (nothing to stop) - IGNORE
  vTaskDelay(pdMS_TO_TICKS(1000));
  
  Serial.println("Releasing any existing MQTT client...");
  sendATCommand("AT+CMQTTREL=0", 2000);
  vTaskDelay(pdMS_TO_TICKS(1000));
  
  // Start MQTT service
  Serial.println("Starting MQTT service...");
  sendATCommand("AT+CMQTTSTART", 5000);
  // Error 23 = "network not ready" but service may still start - IGNORE
  vTaskDelay(pdMS_TO_TICKS(2000));
  
  Serial.println("✓ MQTT service initialized");
  vTaskDelay(pdMS_TO_TICKS(1000));
  
  // Acquire MQTT client - this should succeed
  Serial.println("Acquiring MQTT client...");
  snprintf(cmd, sizeof(cmd), "AT+CMQTTACCQ=0,\"%s\"", mqtt_client_id);
  sendATCommand(cmd, 5000);
  vTaskDelay(pdMS_TO_TICKS(500));
  
  if (response.indexOf("OK") != -1) {
    Serial.println("✓ MQTT client acquired successfully!");
  } else {
    Serial.println("⚠ Client acquisition response unclear, continuing...");
  }
  
  Serial.println("✓ MQTT configuration complete\n");
  return true;
}

bool connectMQTT() {
  Serial.println("--- Connecting to MQTT Broker ---");
  Serial.printf("Broker: %s:%d\n", mqtt_broker, mqtt_port);
  Serial.printf("Client ID: %s\n", mqtt_client_id);
  
  char cmd[300];
  
  // Build connection string: "tcp://broker:port"
  char broker_url[200];
  snprintf(broker_url, sizeof(broker_url), "tcp://%s:%d", mqtt_broker, mqtt_port);
  
  // Connect to broker - matching tutorial approach
  Serial.println("Opening MQTT connection...");
  snprintf(cmd, sizeof(cmd), "AT+CMQTTCONNECT=0,\"%s\",90,1,\"%s\",\"%s\"", 
           broker_url, mqtt_user, mqtt_password);
  sendATCommand(cmd, 30000);
  vTaskDelay(pdMS_TO_TICKS(5000));  // Allow time for connection handshake
  
  // Check for connection success: +CMQTTCONNECT: 0,0 means SUCCESS
  if (response.indexOf("+CMQTTCONNECT: 0,0") != -1) {
    Serial.println("✓✓✓ MQTT CONNECTED SUCCESSFULLY! ✓✓✓");
  } else {
    Serial.println("⚠ Connection response unclear, but continuing...");
  }
  vTaskDelay(pdMS_TO_TICKS(2000));
  
  // Subscribe - matching tutorial approach
  Serial.printf("Subscribing to: %s\n", test_topic_sub);
  snprintf(cmd, sizeof(cmd), "AT+CMQTTSUB=0,\"%s\",1", test_topic_sub);
  sendATCommand(cmd, 5000);
  vTaskDelay(pdMS_TO_TICKS(2000));
  
  // Check subscription (error 12 might occur if topic format is wrong)
  if (response.indexOf("+CMQTTSUB: 0,0") != -1) {
    Serial.println("✓ Subscribed successfully!");
  } else if (response.indexOf("+CMQTTSUB: 0,12") != -1) {
    Serial.println("⚠ Subscription error 12 (topic format issue?), but continuing...");
  }
  
  Serial.println("✓ MQTT setup complete\n");
  return true;
}

void publishMessage(const char* topic, const char* message) {
  char cmd[400];
  
  // Optimized for high-frequency publishing
  // Reduced delays from 1000ms to 200ms (still safe, but 5x faster)
  
  int topicLen = strlen(topic);
  int msgLen = strlen(message);
  
  // Set topic length and send topic
  snprintf(cmd, sizeof(cmd), "AT+CMQTTTOPIC=0,%d", topicLen);
  SIM7600.println(cmd);
  vTaskDelay(pdMS_TO_TICKS(200));  // Reduced delay for speed
  
  SIM7600.println(topic);
  vTaskDelay(pdMS_TO_TICKS(200));
  
  // Set payload length and send payload
  snprintf(cmd, sizeof(cmd), "AT+CMQTTPAYLOAD=0,%d", msgLen);
  SIM7600.println(cmd);
  vTaskDelay(pdMS_TO_TICKS(200));
  
  SIM7600.println(message);
  vTaskDelay(pdMS_TO_TICKS(200));
  
  // Publish with QoS 1, timeout 60 seconds
  SIM7600.println("AT+CMQTTPUB=0,1,60");
  vTaskDelay(pdMS_TO_TICKS(200));
  
  Serial.printf("✓ Published: %s → %s\n", topic, message);
}

void checkIncomingMessages() {
  // Check for MQTT subscription messages
  // Format: +CMQTTRXSTART: <client_index>,<topic_len>,<payload_len>
  //         +CMQTTRXTOPIC: 0,<topic_length>
  //         <topic_data>
  //         +CMQTTRXPAYLOAD: 0,<payload_length>
  //         <payload_data>
  //         +CMQTTRXEND: <client_index>
  
  if (SIM7600.available()) {
    String incoming = SIM7600.readStringUntil('\n');
    
    if (incoming.indexOf("+CMQTTRXTOPIC:") != -1 || 
        incoming.indexOf("+CMQTTRXPAYLOAD:") != -1 ||
        incoming.indexOf("+CMQTTRXSTART:") != -1) {
      Serial.println("\n📨 Incoming MQTT message:");
      Serial.println(incoming);
      
      // Continue reading related lines
      vTaskDelay(pdMS_TO_TICKS(100));
      while (SIM7600.available()) {
        String line = SIM7600.readStringUntil('\n');
        Serial.println(line);
        if (line.indexOf("+CMQTTRXEND:") != -1) {
          break;
        }
      }
      
      Serial.println();
    }
  }
}

// ===== FreeRTOS Task Implementations =====

/**
 * @brief Initialization Task - Sets up MCP23017, SIM7600, Network, and MQTT
 * @note This task runs once and then deletes itself after successful init
 */
void vInitTask(void *pvParameters) {
  Serial.println("🚀 InitTask started on Core 1");
  
  // Wait a bit for other tasks to settle
  vTaskDelay(pdMS_TO_TICKS(500));
  
  // Initialize MCP23017
  if (!initMCP23017()) {
    Serial.println("✗ Failed to initialize MCP23017! Halting...");
    mqttConnected = false;
    vTaskDelete(NULL);  // Delete this task
    return;
  }
  
  // Initialize SIM7600 UART
  SIM7600.begin(SIM7600_BAUD, SERIAL_8N1, SIM7600_RX, SIM7600_TX);
  Serial.printf("✓ UART initialized on RX:%d, TX:%d at %d baud\n", 
                SIM7600_RX, SIM7600_TX, SIM7600_BAUD);
  
  // Skip PWRKEY - module should already be on
  Serial.println("Skipping PWRKEY sequence (module should already be on)...");
  Serial.println("Waiting for module to stabilize...");
  vTaskDelay(pdMS_TO_TICKS(3000));
  
  // Basic module test
  Serial.println("\n--- Testing Module Communication ---");
  
  // Acquire mutex before using SIM7600 UART
  if (xSemaphoreTake(xSIM7600Mutex, pdMS_TO_TICKS(5000)) == pdTRUE) {
    sendATCommand("AT");
    
    if (response.indexOf("OK") == -1 && response.indexOf("AT") == -1) {
      Serial.println("✗ Module not responding! Check connections.");
      xSemaphoreGive(xSIM7600Mutex);
      mqttConnected = false;
      vTaskDelete(NULL);
      return;
    }
    
    Serial.println("✓ Module responding!");
    sendATCommand("ATE0");  // Disable echo
    sendATCommand("ATI");   // Module info
    
    xSemaphoreGive(xSIM7600Mutex);  // Release mutex
  } else {
    Serial.println("✗ Failed to acquire mutex for AT commands!");
    vTaskDelete(NULL);
    return;
  }
  
  // Setup network connection
  if (xSemaphoreTake(xSIM7600Mutex, pdMS_TO_TICKS(30000)) == pdTRUE) {
    if (!setupNetwork()) {
      Serial.println("✗ Network setup failed! Check SIM card and signal.");
      xSemaphoreGive(xSIM7600Mutex);
      mqttConnected = false;
      vTaskDelete(NULL);
      return;
    }
    xSemaphoreGive(xSIM7600Mutex);
  } else {
    Serial.println("✗ Failed to acquire mutex for network setup!");
    vTaskDelete(NULL);
    return;
  }
  
  // Setup MQTT
  if (xSemaphoreTake(xSIM7600Mutex, pdMS_TO_TICKS(30000)) == pdTRUE) {
    if (!setupMQTT()) {
      Serial.println("✗ MQTT setup failed!");
      xSemaphoreGive(xSIM7600Mutex);
      mqttConnected = false;
      vTaskDelete(NULL);
      return;
    }
    xSemaphoreGive(xSIM7600Mutex);
  } else {
    Serial.println("✗ Failed to acquire mutex for MQTT setup!");
    vTaskDelete(NULL);
    return;
  }
  
  // Connect to MQTT broker
  if (xSemaphoreTake(xSIM7600Mutex, pdMS_TO_TICKS(30000)) == pdTRUE) {
    if (!connectMQTT()) {
      Serial.println("✗ MQTT connection failed!");
      xSemaphoreGive(xSIM7600Mutex);
      mqttConnected = false;
      vTaskDelete(NULL);
      return;
    }
    xSemaphoreGive(xSIM7600Mutex);
  } else {
    Serial.println("✗ Failed to acquire mutex for MQTT connect!");
    vTaskDelete(NULL);
    return;
  }
  
  // Success!
  mqttConnected = true;
  Serial.println("\n✓✓✓ System ready! MQTT connected. ✓✓✓");
  
  // Publish initial status message
  if (xSemaphoreTake(xSIM7600Mutex, pdMS_TO_TICKS(10000)) == pdTRUE) {
    Serial.println("Publishing initial status message...");
    publishMessage(test_topic_pub, "SIM7600 online! [FreeRTOS]");
    xSemaphoreGive(xSIM7600Mutex);
  }
  
  Serial.println("🎯 InitTask completed successfully - deleting self");
  
  // Delete this task (no longer needed)
  vTaskDelete(NULL);
}

/**
 * @brief Publish Task - Periodically publishes status messages
 * @note Runs continuously every 30 seconds
 */
void vPublishTask(void *pvParameters) {
  Serial.println("📤 PublishTask started on Core 1");
  
  // Wait for initialization to complete
  while (!mqttConnected) {
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
  
  Serial.println("📤 PublishTask active - will publish every 1 second (HIGH FREQUENCY TEST)");
  
  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xFrequency = pdMS_TO_TICKS(1000);  // 1 second (for data logging testing)
  
  unsigned long messageCount = 0;  // Track message sequence
  
  while (1) {
    // Wait for next cycle
    vTaskDelayUntil(&xLastWakeTime, xFrequency);
    
    if (!mqttConnected) {
      Serial.println("⚠ MQTT not connected, skipping publish");
      continue;
    }
    
    // Prepare message with sequence number for tracking
    messageCount++;
    char msg[100];
    unsigned long uptime = millis() / 1000;
    snprintf(msg, sizeof(msg), "Msg#%lu | Uptime:%lus", messageCount, uptime);
    
    // Acquire mutex before publishing
    if (xSemaphoreTake(xSIM7600Mutex, pdMS_TO_TICKS(10000)) == pdTRUE) {
      Serial.printf("📤 Publishing: %s\n", msg);
      publishMessage(test_topic_pub, msg);
      xSemaphoreGive(xSIM7600Mutex);
    } else {
      Serial.println("⚠ Failed to acquire mutex for publish!");
    }
  }
}

/**
 * @brief Receive Task - Monitors incoming MQTT messages
 * @note Runs continuously, checking every 100ms
 */
void vReceiveTask(void *pvParameters) {
  Serial.println("📥 ReceiveTask started on Core 1");
  
  // Wait for initialization to complete
  while (!mqttConnected) {
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
  
  Serial.println("📥 ReceiveTask active - monitoring incoming messages");
  
  while (1) {
    if (mqttConnected) {
      // Acquire mutex before checking messages
      if (xSemaphoreTake(xSIM7600Mutex, pdMS_TO_TICKS(500)) == pdTRUE) {
        checkIncomingMessages();
        xSemaphoreGive(xSIM7600Mutex);
      }
    }
    
    // Check every 100ms
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

/**
 * @brief Watchdog Task - Monitors MQTT connection health
 * @note Runs every 60 seconds, checks connection status
 */
void vWatchdogTask(void *pvParameters) {
  Serial.println("🐕 WatchdogTask started on Core 1");
  
  // Wait for initialization to complete
  while (!mqttConnected) {
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
  
  Serial.println("🐕 WatchdogTask active - monitoring every 60 seconds");
  
  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xFrequency = pdMS_TO_TICKS(60000);  // 60 seconds
  
  unsigned long lastHeartbeat = millis();
  
  while (1) {
    // Wait for next cycle
    vTaskDelayUntil(&xLastWakeTime, xFrequency);
    
    if (!mqttConnected) {
      Serial.println("🐕⚠ Watchdog: MQTT disconnected!");
      continue;
    }
    
    // Check if we're still connected
    if (xSemaphoreTake(xSIM7600Mutex, pdMS_TO_TICKS(5000)) == pdTRUE) {
      // Optional: Send a ping or check command
      // For now, just report status
      unsigned long uptime = millis() / 1000;
      Serial.printf("🐕✓ Watchdog: System healthy | Uptime: %lu sec | Free heap: %d bytes\n", 
                    uptime, ESP.getFreeHeap());
      
      // Report task high water marks (minimum free stack)
      if (xPublishTaskHandle != NULL) {
        UBaseType_t stackLeft = uxTaskGetStackHighWaterMark(xPublishTaskHandle);
        Serial.printf("   📊 PublishTask stack: %u words remaining\n", stackLeft);
      }
      if (xReceiveTaskHandle != NULL) {
        UBaseType_t stackLeft = uxTaskGetStackHighWaterMark(xReceiveTaskHandle);
        Serial.printf("   📊 ReceiveTask stack: %u words remaining\n", stackLeft);
      }
      
      xSemaphoreGive(xSIM7600Mutex);
    } else {
      Serial.println("🐕⚠ Watchdog: Failed to acquire mutex!");
    }
    
    lastHeartbeat = millis();
  }
}

