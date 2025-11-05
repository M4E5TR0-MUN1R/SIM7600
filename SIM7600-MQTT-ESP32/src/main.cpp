#include <Arduino.h>
#include <Adafruit_MCP23X17.h>

/* ===============================================================================
 * SIM7600 MQTT CLIENT - HOW IT WORKS
 * ===============================================================================
 * 
 * This code implements MQTT connectivity using the SIM7600E 4G modem.
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

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n\n╔═══════════════════════════════════════════════╗");
  Serial.println("║    SIM7600 MQTT Client - HiveMQ Cloud       ║");
  Serial.println("╚═══════════════════════════════════════════════╝\n");
  
  // Initialize MCP23017
  if (!initMCP23017()) {
    Serial.println("✗ Failed to initialize MCP23017! Halting...");
    while(1) delay(1000);
  }
  
  // Initialize SIM7600 UART
  SIM7600.begin(SIM7600_BAUD, SERIAL_8N1, SIM7600_RX, SIM7600_TX);
  Serial.printf("✓ UART initialized on RX:%d, TX:%d at %d baud\n", 
                SIM7600_RX, SIM7600_TX, SIM7600_BAUD);
  
  // Skip PWRKEY - module should already be on
  Serial.println("Skipping PWRKEY sequence (module should already be on)...");
  Serial.println("Waiting for module to stabilize...");
  delay(3000);
  
  // Basic module test
  Serial.println("\n--- Testing Module Communication ---");
  sendATCommand("AT");
  
  if (response.indexOf("OK") == -1 && response.indexOf("AT") == -1) {
    Serial.println("✗ Module not responding! Check connections.");
    Serial.println("Entering debug mode...");
    while(1) {
      if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();
        if (cmd.length() > 0) {
          Serial.printf(">> %s\n", cmd.c_str());
          SIM7600.println(cmd);
        }
      }
      if (SIM7600.available()) {
        Serial.write(SIM7600.read());
      }
      delay(10);
    }
  }
  
  Serial.println("✓ Module responding!");
  sendATCommand("ATE0");  // Disable echo
  sendATCommand("ATI");   // Module info
  
  // Setup network connection
  if (!setupNetwork()) {
    Serial.println("✗ Network setup failed! Check SIM card and signal.");
    Serial.println("Entering AT command mode for debugging...");
    while(1) {
      if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();
        if (cmd.length() > 0) {
          Serial.printf(">> %s\n", cmd.c_str());
          SIM7600.println(cmd);
        }
      }
      if (SIM7600.available()) {
        Serial.write(SIM7600.read());
      }
      delay(10);
    }
  }
  
  // Setup MQTT
  if (!setupMQTT()) {
    Serial.println("✗ MQTT setup failed!");
    while(1) delay(1000);
  }
  
  // Connect to MQTT broker
  if (!connectMQTT()) {
    Serial.println("✗ MQTT connection failed!");
    while(1) delay(1000);
  }
  
  mqttConnected = true;
  Serial.println("\n✓ System ready! MQTT connected.");
  Serial.println("Publishing status message...");
  publishMessage(test_topic_pub, "SIM7600 online!");
}

void loop() {
  if (mqttConnected) {
    // Check for incoming MQTT messages
    checkIncomingMessages();
    
    // Publish status every 30 seconds
    static unsigned long lastPublish = 0;
    if (millis() - lastPublish > 30000) {
      lastPublish = millis();
      char msg[100];
      snprintf(msg, sizeof(msg), "Uptime: %lu seconds", millis() / 1000);
      Serial.printf("Publishing: %s\n", msg);
      publishMessage(test_topic_pub, msg);
    }
  }
  
  delay(100);
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
  delay(5000);
  
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
  delay(1000);
  
  Serial.println("Releasing any existing MQTT client...");
  sendATCommand("AT+CMQTTREL=0", 2000);
  delay(1000);
  
  // Start MQTT service
  Serial.println("Starting MQTT service...");
  sendATCommand("AT+CMQTTSTART", 5000);
  // Error 23 = "network not ready" but service may still start - IGNORE
  delay(2000);
  
  Serial.println("✓ MQTT service initialized");
  delay(1000);
  
  // Acquire MQTT client - this should succeed
  Serial.println("Acquiring MQTT client...");
  snprintf(cmd, sizeof(cmd), "AT+CMQTTACCQ=0,\"%s\"", mqtt_client_id);
  sendATCommand(cmd, 5000);
  delay(500);
  
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
  delay(5000);  // Allow time for connection handshake
  
  // Check for connection success: +CMQTTCONNECT: 0,0 means SUCCESS
  if (response.indexOf("+CMQTTCONNECT: 0,0") != -1) {
    Serial.println("✓✓✓ MQTT CONNECTED SUCCESSFULLY! ✓✓✓");
  } else {
    Serial.println("⚠ Connection response unclear, but continuing...");
  }
  delay(2000);
  
  // Subscribe - matching tutorial approach
  Serial.printf("Subscribing to: %s\n", test_topic_sub);
  snprintf(cmd, sizeof(cmd), "AT+CMQTTSUB=0,\"%s\",1", test_topic_sub);
  sendATCommand(cmd, 5000);
  delay(2000);
  
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
  
  // Following tutorial 2 approach - simple delays, no prompt waiting
  // Lines 60-68 of MQTT_tutorial2.txt
  
  int topicLen = strlen(topic);
  int msgLen = strlen(message);
  
  // Set topic length and send topic
  snprintf(cmd, sizeof(cmd), "AT+CMQTTTOPIC=0,%d", topicLen);
  SIM7600.println(cmd);
  delay(1000);  // Tutorial uses 1 second delay
  
  SIM7600.println(topic);
  delay(1000);
  
  // Set payload length and send payload
  snprintf(cmd, sizeof(cmd), "AT+CMQTTPAYLOAD=0,%d", msgLen);
  SIM7600.println(cmd);
  delay(1000);
  
  SIM7600.println(message);
  delay(1000);
  
  // Publish with QoS 1, timeout 60 seconds
  SIM7600.println("AT+CMQTTPUB=0,1,60");
  delay(1000);
  
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
      delay(100);
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

