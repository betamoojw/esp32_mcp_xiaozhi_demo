
#include <Arduino.h>
#include <WiFi.h>
#include <WebSocketMCP.h>



const char* WIFI_SSID = "ssid";
const char* WIFI_PASS = "pass_ssid";


const char* MCP_ENDPOINT = "wss://api.xiaozhi.me/mcp/?token=eyJh-----------------------------------------------------------------------------VRw4x0w";


#define DEBUG_SERIAL Serial
#define DEBUG_BAUD_RATE 115200


#define LED_PIN 2  


WebSocketMCP mcpClient;


#define MAX_INPUT_LENGTH 1024
char inputBuffer[MAX_INPUT_LENGTH];
int inputBufferIndex = 0;
bool newCommandAvailable = false;


bool wifiConnected = false;
bool mcpConnected = false;


void setupWifi();
void onMcpOutput(const String &message);
void onMcpError(const String &error);
void onMcpConnectionChange(bool connected);
void processSerialCommands();
void blinkLed(int times, int delayMs);
void registerMcpTools();
void printHelp();
void printStatus();


void printHelp() {
  DEBUG_SERIAL.println("Available commands:");
  DEBUG_SERIAL.println("  help     - Show this help message");
  DEBUG_SERIAL.println("  status   - Show current connection status");
  DEBUG_SERIAL.println("  reconnect - Reconnect to the MCP server");
  DEBUG_SERIAL.println("  tools    - List registered tools");
  DEBUG_SERIAL.println("  Any other text will be sent directly to the MCP server.");
}

void printStatus() {
  DEBUG_SERIAL.println("Status:");
  DEBUG_SERIAL.print("  WiFi: ");
  DEBUG_SERIAL.println(wifiConnected ? "Connected" : "Not connected");
  if (wifiConnected) {
    DEBUG_SERIAL.print("  IP: ");
    DEBUG_SERIAL.println(WiFi.localIP());
    DEBUG_SERIAL.print("  RSSI: ");
    DEBUG_SERIAL.println(WiFi.RSSI());
  }
  DEBUG_SERIAL.print("  MCPServer: ");
  DEBUG_SERIAL.println(mcpConnected ? "Connected" : "Not connected");
}

void setup() {
 
  DEBUG_SERIAL.begin(DEBUG_BAUD_RATE);
  DEBUG_SERIAL.println("\n\n[ESP32 MCP] Initializing...");
  
 
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  
  
  setupWifi();
  
 
  if (mcpClient.begin(MCP_ENDPOINT, onMcpConnectionChange)) {
    DEBUG_SERIAL.println("[ESP32 MCP] Initialized. Attempting to connect to the MCP server....");
  } else {
    DEBUG_SERIAL.println("[ESP32 MCP] Initialization failed!");
  }
  
  
  DEBUG_SERIAL.println("Usage guide:");
  DEBUG_SERIAL.println("- Type a command in the serial console and press Enter to send.");
  DEBUG_SERIAL.println("- Messages received from the MCP server will be shown on the serial console.");
  DEBUG_SERIAL.println("- Type \"help\" to see available commands.");
  DEBUG_SERIAL.println();
}

void loop() {
 
  mcpClient.loop();
  
  processSerialCommands();
  
  if (!wifiConnected) {
    blinkLed(1, 100);
  } else if (!mcpConnected) {
    blinkLed(1, 500);
  }// else {
  //  digitalWrite(LED_PIN, HIGH);
  //}
}


void setupWifi() {
  DEBUG_SERIAL.print("[WiFi] Connecting to ");
  DEBUG_SERIAL.println(WIFI_SSID);
  
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    DEBUG_SERIAL.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    DEBUG_SERIAL.println();
    DEBUG_SERIAL.println("[WiFi] Connected!");
    DEBUG_SERIAL.print("[WiFi] Local IP: ");
    DEBUG_SERIAL.println(WiFi.localIP());
  } else {
    wifiConnected = false;
    DEBUG_SERIAL.println();
    DEBUG_SERIAL.println("[WiFi] Connection failed! Will keep retrying...");
  }
}


void onMcpOutput(const String &message) {
  DEBUG_SERIAL.print("[MCP Out] ");
  DEBUG_SERIAL.println(message);
}


void onMcpError(const String &error) {
  DEBUG_SERIAL.print("[MCPError] ");
  DEBUG_SERIAL.println(error);
}


void registerMcpTools() {
  DEBUG_SERIAL.println("[MCP] Registering tools...");
  
 
  mcpClient.registerTool(
    "led_blink",  
    "LED-control", 
    "{\"properties\":{\"state\":{\"title\":\"LEDStatus\",\"type\":\"string\",\"enum\":[\"on\",\"off\",\"blink\"]}},\"required\":[\"state\"],\"title\":\"ledControlArguments\",\"type\":\"object\"}",  
    [](const String& args) {     
      DEBUG_SERIAL.println("[Tool] LED control: " + args);
      DynamicJsonDocument doc(256);
      DeserializationError error = deserializeJson(doc, args);
      
      if (error) {
       
        WebSocketMCP::ToolResponse response("{\"success\":false,\"error\":\"Invalid parameter format\"}", true);
        return response;
      }
      
      String state = doc["state"].as<String>();
      DEBUG_SERIAL.println("[Tool] LED control: " + state);
      
      
      if (state == "on") {
        digitalWrite(LED_PIN, HIGH);
      } else if (state == "off") {
        digitalWrite(LED_PIN, LOW);
      } else if (state == "blink") {
        for (int i = 0; i < 5; i++) {
          digitalWrite(LED_PIN, HIGH);
          delay(200);
          digitalWrite(LED_PIN, LOW);
          delay(200);
        }
      }
      
      
      String resultJson = "{\"success\":true,\"state\":\"" + state + "\"}";
      return WebSocketMCP::ToolResponse(resultJson);
    }
  );
  DEBUG_SERIAL.println("[MCP] LED-Control registered.");
  
  
  mcpClient.registerTool(
    "system-info",
    "ESP32-info",
    "{\"properties\":{},\"title\":\"systemInfoArguments\",\"type\":\"object\"}",
    [](const String& args) {
      DEBUG_SERIAL.println("[Tool] ESP32 info: " + args);
      
      String chipModel = ESP.getChipModel();
      uint32_t chipId = ESP.getEfuseMac() & 0xFFFFFFFF;
      uint32_t flashSize = ESP.getFlashChipSize() / 1024;
      uint32_t freeHeap = ESP.getFreeHeap() / 1024;
      
      
      String resultJson = "{\"success\":true,\"model\":\"" + chipModel + "\",\"chipId\":\"" + String(chipId, HEX) + 
                         "\",\"flashSize\":" + String(flashSize) + ",\"freeHeap\":" + String(freeHeap) + 
                         ",\"wifiStatus\":\"" + (WiFi.status() == WL_CONNECTED ? "connected" : "disconnected") + 
                         "\",\"ipAddress\":\"" + WiFi.localIP().toString() + "\"}";
      
      return WebSocketMCP::ToolResponse(resultJson);
    }
  );
  DEBUG_SERIAL.println("[MCP] ESP32-info registered.");
  
 
  mcpClient.registerTool(
    "calculator",
    "Calculator",
    "{\"properties\":{\"expression\":{\"title\":\"expression\",\"type\":\"string\"}},\"required\":[\"expression\"],\"title\":\"calculatorArguments\",\"type\":\"object\"}",
    [](const String& args) {
       DEBUG_SERIAL.println("[Tool] Calculator: " + args);
      DynamicJsonDocument doc(256);
      deserializeJson(doc, args);
      
      String expr = doc["expression"].as<String>();
      DEBUG_SERIAL.println("[Tool] Calculating: " + expr);
      
      
      int result = 0;
      if (expr.indexOf("+") > 0) {
        int plusPos = expr.indexOf("+");
        int a = expr.substring(0, plusPos).toInt();
        int b = expr.substring(plusPos + 1).toInt();
        result = a + b;
      } else if (expr.indexOf("-") > 0) {
        int minusPos = expr.indexOf("-");
        int a = expr.substring(0, minusPos).toInt();
        int b = expr.substring(minusPos + 1).toInt();
        result = a - b;
      }
      
      String resultJson = "{\"success\":true,\"expression\":\"" + expr + "\",\"result\":" + String(result) + "}";
      return WebSocketMCP::ToolResponse(resultJson);
    }
  );
  DEBUG_SERIAL.println("[MCP] Calculator tool registered.");
  
  DEBUG_SERIAL.println("[MCP] Tool registration complete, total " + String(mcpClient.getToolCount()) + " tools");
}


void onMcpConnectionChange(bool connected) {
  mcpConnected = connected;
  if (connected) {
    DEBUG_SERIAL.println("[MCP] Connected to MCP server");
    // Register tools after successful connection
    registerMcpTools();
  } else {
    DEBUG_SERIAL.println("[MCP] Disconnected from MCP server");
  }
}


void processSerialCommands() {
  while (DEBUG_SERIAL.available() > 0) {
    char inChar = (char)DEBUG_SERIAL.read();
    
    if (inChar == '\n' || inChar == '\r') {
      if (inputBufferIndex > 0) {
        inputBuffer[inputBufferIndex] = '\0';
        
        String command = String(inputBuffer);
        command.trim();
        
        if (command.length() > 0) {
          if (command == "help") {
            printHelp();
          } else if (command == "status") {
            printStatus();
          } else if (command == "reconnect") {
            DEBUG_SERIAL.println("Reconnecting...");
            mcpClient.disconnect();
          } else if (command == "tools") {
            // Show registered tools
            DEBUG_SERIAL.println("Registered tool count: " + String(mcpClient.getToolCount()));
          } else {
            if (mcpClient.isConnected()) {
              mcpClient.sendMessage(command);
              DEBUG_SERIAL.println("[sent] " + command);
            } else {
              DEBUG_SERIAL.println("Cannot connect to MCP server, unable to send command.");
            }
          }
        }
        inputBufferIndex = 0;
      }
    } 
    
    else if (inChar == '\b' || inChar == 127) {
      if (inputBufferIndex > 0) {
        inputBufferIndex--;
        DEBUG_SERIAL.print("\b \b"); 
      }
    }
    
    else if (inputBufferIndex < MAX_INPUT_LENGTH - 1) {
      inputBuffer[inputBufferIndex++] = inChar;
      DEBUG_SERIAL.print(inChar); 
    }
  }
}




void blinkLed(int times, int delayMs) {
  static int blinkCount = 0;
  static unsigned long lastBlinkTime = 0;
  static bool ledState = false;
  static int lastTimes = 0;

  if (times == 0) {
    digitalWrite(LED_PIN, LOW);
    blinkCount = 0;
    lastTimes = 0;
    return;
  }
  if (lastTimes != times) {
    blinkCount = 0;
    lastTimes = times;
    ledState = false;
    lastBlinkTime = millis();
  }
  unsigned long now = millis();
  if (blinkCount < times * 2) {
    if (now - lastBlinkTime > delayMs) {
      lastBlinkTime = now;
      ledState = !ledState;
      digitalWrite(LED_PIN, ledState ? HIGH : LOW);
      blinkCount++;
    }
  } else {
    digitalWrite(LED_PIN, LOW);
    blinkCount = 0;
    lastTimes = 0;
  }

}
