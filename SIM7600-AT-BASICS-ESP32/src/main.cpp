#include <Arduino.h>
#include <Adafruit_MCP23X17.h>

#define DO_1_ADDR 0x27
bool initDOModules();
Adafruit_MCP23X17 mcpDO_1;  // 0x27 - DRV0-DRV15 (outputs 0-15)

// ===== CONFIGURATION =====
// Use HardwareSerial instead of SoftwareSerial for better reliability
// ESP32-S3 has multiple UARTs: Serial (USB), Serial0 (GPIO43/44), Serial1 (custom pins)

// UART Pin Configuration - Adjust these based on your schematic
#define SIM7600_RX 19  // ESP32 RX <- SIM7600 TX
#define SIM7600_TX 20  // ESP32 TX -> SIM7600 RX

// ====================================================================
// IMPORTANT: If LED_WWAN is blinking, the module is ALREADY POWERED ON!
// In that case, comment out the line below to skip the PWRKEY sequence:
// ====================================================================
// #define USE_PWRKEY_SEQUENCE  // <-- COMMENT THIS OUT FIRST!

// If you need to control power on/off, uncomment above and set pin below:
#define SIM7600_PWRKEY 9  // MCP23017 pin number for PWRKEY

// Baud Rate - Common options: 115200 (recommended), 9600, 57600
#define SIM7600_BAUD 115200

// Alternative configurations to try if above doesn't work:
// For GPIO43/44 (default UART0 on some ESP32-S3): 
//   #define SIM7600_RX 44
//   #define SIM7600_TX 43
// For GPIO16/17:
//   #define SIM7600_RX 16
//   #define SIM7600_TX 17

// If module doesn't respond, try:
// 1. Different baud rate (change SIM7600_BAUD to 9600)
// 2. Swap RX/TX if wired incorrectly
// 3. Check if PWRKEY needs to be connected/toggled
// 4. Verify power supply (SIM7600 needs stable 3.4-4.2V, recommend 3.8V)
// ===== END CONFIGURATION =====

HardwareSerial SIM7600(1); // Use Serial1

bool smsSent = false;
String response = "";

// Forward declarations
void powerOnModule();
void sendATCommand(String command, int timeout);
String waitForResponse(int timeout);
void sendSMS(String number, String message);

void setup() {
  // Initialize Serial for debugging
  Serial.begin(115200);
  initDOModules();
  Serial.println("\n\n=== SIM7600 SMS & AT Command Bridge ===");
  
  // Initialize SIM7600 communication with HardwareSerial
  // Serial1.begin(baud, protocol, RX pin, TX pin)
  SIM7600.begin(SIM7600_BAUD, SERIAL_8N1, SIM7600_RX, SIM7600_TX);
  Serial.printf("Initialized SIM7600 UART on RX:%d, TX:%d at %d baud\n", 
                SIM7600_RX, SIM7600_TX, SIM7600_BAUD);
  
  // Power on the module (if PWRKEY sequence is enabled)
  #if defined(SIM7600_PWRKEY) && defined(USE_PWRKEY_SEQUENCE)
  powerOnModule();
  #else
  Serial.println("Skipping PWRKEY sequence (module should already be on)...");
  Serial.println("Waiting for module to stabilize...");
  delay(3000);
  #endif
  
  delay(2000); // Additional stabilization time
  
  // Test if module is responding - try multiple baud rates
  Serial.println("\n=== Testing module communication ===");
  
  bool moduleResponding = false;
  int baudRates[] = {115200, 9600, 57600, 19200};
  int numBaudRates = 4;
  
  for (int i = 0; i < numBaudRates && !moduleResponding; i++) {
    if (baudRates[i] != SIM7600_BAUD) {
      Serial.printf("Trying baud rate: %d\n", baudRates[i]);
      SIM7600.end();
      delay(100);
      SIM7600.begin(baudRates[i], SERIAL_8N1, SIM7600_RX, SIM7600_TX);
      delay(500);
    } else {
      Serial.printf("Testing current baud rate: %d\n", SIM7600_BAUD);
    }
    
    sendATCommand("AT", 2000);
    
    if (response.indexOf("OK") != -1 || response.indexOf("AT") != -1) {
      Serial.printf("✓ Module is responding at %d baud!\n", baudRates[i]);
      moduleResponding = true;
      break;
    }
  }
  
  if (!moduleResponding) {
    Serial.println("\n✗ No response from module at any baud rate!");
    Serial.println("Troubleshooting tips:");
    Serial.println("1. Check UART connections (RX/TX might be swapped)");
    Serial.println("2. Verify module is powered on (check LED_WWAN blinking)");
    Serial.println("3. Check if PWRKEY is connected to correct pin");
    Serial.println("4. Try manually powering module before starting ESP32");
    Serial.println("5. Verify SIM card is inserted properly");
    Serial.println("\nEntering bridge mode anyway for manual testing...");
    Serial.println("Try sending: AT (to test), AT+IPR=115200 (to set baud)");
    smsSent = true;
    return; // Skip SMS sending, go straight to bridge mode
  }
  
  // Disable echo
  Serial.println("\nDisabling echo...");
  sendATCommand("ATE0", 1000);
  
  // Check network registration
  Serial.println("\nChecking network registration...");
  sendATCommand("AT+CREG?", 2000);
  
  // Check signal quality
  Serial.println("Checking signal quality...");
  sendATCommand("AT+CSQ", 1000);
  
  // Send SMS
  Serial.println("\n=== Sending SMS ===");
  sendSMS("+254729399246", "Hello from ESP32-S3 and SIM7600!");
  
  smsSent = true;
  
  Serial.println("\n\n=== SMS Sent! ===");
  Serial.println("=== Now entering AT Command Bridge Mode ===");
  Serial.println("You can now type AT commands in the Serial Monitor");
  Serial.println("Commands will be forwarded to SIM7600 and responses printed\n");
}

void loop() {
  // Bridge mode: Forward commands from Serial to SIM7600
  if (smsSent) {
    // Check if data available from Serial Monitor
    if (Serial.available()) {
      String command = Serial.readStringUntil('\n');
      command.trim();
      if (command.length() > 0) {
        Serial.print(">> Sending: ");
        Serial.println(command);
        SIM7600.println(command);
      }
    }
    
    // Check if data available from SIM7600
    if (SIM7600.available()) {
      String response = SIM7600.readString();
      Serial.print(response);
    }
  }
}

void powerOnModule() {
  #if defined(SIM7600_PWRKEY) && defined(USE_PWRKEY_SEQUENCE)
  Serial.println("Executing PWRKEY power-on sequence...");
  
  // PWRKEY is connected to MCP23017, not ESP32 GPIO
  // Pin mode was already set in initDOModules()
  
  Serial.println("Setting PWRKEY HIGH...");
  mcpDO_1.digitalWrite(SIM7600_PWRKEY, HIGH);
  delay(300);
  
  // Pull PWRKEY low for at least 1 second to power on
  Serial.println("Pulling PWRKEY LOW (power on pulse)...");
  mcpDO_1.digitalWrite(SIM7600_PWRKEY, LOW);
  delay(1500); // Hold for 1.5 seconds
  
  Serial.println("Setting PWRKEY back to HIGH...");
  mcpDO_1.digitalWrite(SIM7600_PWRKEY, HIGH);
  
  Serial.println("PWRKEY sequence complete. Waiting for module boot...");
  delay(8000); // Wait longer - SIM7600 can take 5-10 seconds to boot
  #else
  Serial.println("Warning: powerOnModule() called but USE_PWRKEY_SEQUENCE not defined");
  #endif
}

void sendATCommand(String command, int timeout) {
  Serial.print("Sending: ");
  Serial.println(command);
  
  SIM7600.println(command);
  
  long startTime = millis();
  response = "";
  
  while (millis() - startTime < timeout) {
    while (SIM7600.available()) {
      char c = SIM7600.read();
      response += c;
    }
  }
  
  Serial.print("Response: ");
  Serial.println(response);
}

String waitForResponse(int timeout) {
  long startTime = millis();
  String response = "";
  
  while (millis() - startTime < timeout) {
    while (SIM7600.available()) {
      char c = SIM7600.read();
      response += c;
      Serial.print(c); // Echo to serial for debugging
    }
    
    // Check if we got the expected prompts
    if (response.indexOf("OK") != -1 || 
        response.indexOf("ERROR") != -1 || 
        response.indexOf(">") != -1) {
      break;
    }
  }
  
  return response;
}

void sendSMS(String number, String message) {
  // Set SMS to text mode
  Serial.println("Setting SMS text mode...");
  SIM7600.println("AT+CMGF=1");
  String resp1 = waitForResponse(2000);
  
  if (resp1.indexOf("OK") == -1) {
    Serial.println("ERROR: Failed to set text mode");
    return;
  }
  
  delay(500);
  
  // Set character set to GSM
  Serial.println("Setting character set...");
  SIM7600.println("AT+CSCS=\"GSM\"");
  String resp2 = waitForResponse(2000);
  
  if (resp2.indexOf("OK") == -1) {
    Serial.println("ERROR: Failed to set character set");
    return;
  }
  
  delay(500);
  
  // Send SMS command with recipient number
  Serial.print("Setting recipient: ");
  Serial.println(number);
  SIM7600.print("AT+CMGS=\"");
  SIM7600.print(number);
  SIM7600.println("\"");
  
  // Wait for ">" prompt
  String resp3 = waitForResponse(5000);
  
  if (resp3.indexOf(">") == -1) {
    Serial.println("ERROR: Did not receive '>' prompt");
    return;
  }
  
  Serial.println("Got '>' prompt, sending message...");
  delay(500);
  
  // Send the message
  SIM7600.print(message);
  delay(100);
  
  // Send Ctrl+Z to indicate end of message
  SIM7600.write(26);
  Serial.println("Sent Ctrl+Z, waiting for confirmation...");
  
  // Wait for confirmation (this may take a while)
  String resp4 = waitForResponse(10000);
  
  if (resp4.indexOf("OK") != -1 || resp4.indexOf("+CMGS") != -1) {
    Serial.println("\nSUCCESS: SMS sent successfully!");
  } else if (resp4.indexOf("ERROR") != -1) {
    Serial.println("\nERROR: Failed to send SMS");
  } else {
    Serial.println("\nWARNING: Unclear response - SMS may or may not have been sent");
  }
}

bool initDOModules() {
    Serial.println("Initializing MCP23017 modules...");
    
    // Initialize I2C with custom pins if needed (ESP32-S3 default: SDA=8, SCL=9)
    // Wire.begin(); // Use default pins
    
    // Initialize mcpDO_1 at 0x27 (DRV0-DRV15, outputs 0-15)
    if (!mcpDO_1.begin_I2C(DO_1_ADDR)) {
        Serial.printf("✗ Failed to initialize MCP23017 at 0x%02X\r\n", DO_1_ADDR);
        return false;
    }
    Serial.printf("✓ MCP23017 at 0x%02X initialized (mcpDO_1 = DRV0-DRV15)\r\n", DO_1_ADDR);
    
    // Configure all pins as outputs and set to LOW
    for (int i = 0; i < 16; i++) {
        mcpDO_1.pinMode(i, OUTPUT);
        mcpDO_1.digitalWrite(i, LOW);
    }
    
    // Test read/write to verify communication
    delay(100);
    Serial.println("Testing MCP23017 communication...");
    mcpDO_1.digitalWrite(0, HIGH);
    delay(10);
    mcpDO_1.digitalWrite(0, LOW);
    delay(10);
    
    Serial.println("✓ All 16 outputs configured and set to LOW");
    return true;
}