/**
  ESP32 smoke detector firmware for the Open eXtensible Rack System
  
  See https://oxrs.io/docs/firmware/smoke-detector-esp32.html for documentation.

  Compile options:
    ESP32

  External dependencies. Install using the Arduino library manager:
    "Adafruit_MCP23X17" (requires recent "Adafruit_BusIO" library)
    "OXRS-IO-Rack32-ESP32-LIB" by OXRS Core Team (requires MQTT and LCD libraries)
    "OXRS-SHA-IOHandler-ESP32-LIB" by SuperHouse Automation Pty

  Compatible with the Smoke Detector hardware found here:
    https://bmdesigns.com.au/

  GitHub repository:
    https://github.com/Bedrock-Media-Designs/OXRS-BMD-SmokeDetector-ESP32-FW
    
  Bugs/Features:
    See GitHub issues list

  Copyright 2019-2021 Bedrock Media Designs Ltd
*/

/*--------------------------- Version ------------------------------------*/
#define FW_NAME       "OXRS-BMD-SmokeDetector-ESP32-FW"
#define FW_SHORT_NAME "Smoke Detector"
#define FW_MAKER_CODE "BMD"
#define FW_VERSION    "1.0.0"
#define FW_CODE       "osd"

/*--------------------------- Configuration ------------------------------*/
// Should be no user configuration in this file, everything should be in;
#include "config.h"

/*--------------------------- Libraries ----------------------------------*/
#include <Wire.h>                     // For I2C
#include <Adafruit_MCP23X17.h>        // For MCP23017 I/O buffers
#include "OXRS_Rack32.h"              // Rack32 support
#include <OXRS_Input.h>               // For input handling
#include <OXRS_Output.h>              // For output handling

/*--------------------------- Constants ----------------------------------*/
// Each MCP23017 has 16 I/O pins
#define MCP_PIN_COUNT   16

// Define the MCP addresses
#define MCP_INPUT_ADDR    0x20
#define MCP_OUTPUT1_ADDR  0x21
#define MCP_OUTPUT2_ADDR  0x22

/*--------------------------- Global Variables ---------------------------*/
// Each bit corresponds to an MCP found on the IC2 bus
uint8_t g_mcps_found = 0;

/*--------------------------- Instantiate Global Objects -----------------*/
// Rack32 handler
OXRS_Rack32 rack32(FW_NAME, FW_SHORT_NAME, FW_MAKER_CODE, FW_VERSION, FW_CODE);

// I/O buffers
Adafruit_MCP23X17 mcp23017[3];

// Output handlers
OXRS_Output oxrsOutput[2];

// Input handlers
OXRS_Input oxrsInput;

/*--------------------------- Program ------------------------------------*/
/**
  Setup
*/
void setup()
{
  // Set up Rack32 config
  rack32.setMqttBroker(MQTT_BROKER, MQTT_PORT);
  rack32.setMqttAuth(MQTT_USERNAME, MQTT_PASSWORD);
  rack32.setMqttTopicPrefix(MQTT_TOPIC_PREFIX);
  rack32.setMqttTopicSuffix(MQTT_TOPIC_SUFFIX);
  
  // Start Rack32 hardware
  rack32.begin(jsonConfig, jsonCommand);

  // Scan the I2C bus and set up I/O buffers
  scanI2CBus();

  // Set up port display
  rack32.setDisplayPorts(g_mcps_found, PORT_LAYOUT_IO_48);
  
  // Speed up I2C clock for faster scan rate (after bus scan)
  Wire.setClock(I2C_CLOCK_SPEED);
}

/**
  Main processing loop
*/
void loop()
{
  // Iterate through each of the MCP23017s
  for (uint8_t mcp = 0; mcp < 3; mcp++)
  {
    if (bitRead(g_mcps_found, mcp) == 0)
      continue;

    // Check for any output events
    if (mcp > 0)
    {
      oxrsOutput[mcp - 1].process();
    }

    // Read the values for all 16 pins on this MCP
    uint16_t io_value = mcp23017[mcp].readGPIOAB();

    // Show port animations
    rack32.updateDisplayPorts(mcp, io_value);

    // Check for any input events
    if (mcp == 0)
    {
      oxrsInput.process(0, io_value);
    }
  }

  // Let Rack32 hardware handle any events etc
  rack32.loop();
}

void jsonConfig(JsonObject json)
{
  uint8_t index = getIndex(json);
  if (index == 0) return;

  if ((index % 3) == 0)
  {
    jsonInputConfig(index, json);
  }
  else
  {
    jsonOutputConfig(index, json);
  }
}

void jsonInputConfig(uint8_t index, JsonObject json)
{
  // Work out which pin on the input MCP we are configuring
  uint8_t pin = (index / 3) - 1;

  if (json.containsKey("type"))
  {
    if (json["type"].isNull() || strcmp(json["type"], "switch") == 0)
    {
      oxrsInput.setType(pin, SWITCH);
    }
    else if (strcmp(json["type"], "button") == 0)
    {
      oxrsInput.setType(pin, BUTTON);
    }
    else if (strcmp(json["type"], "contact") == 0)
    {
      oxrsInput.setType(pin, CONTACT);
    }
    else if (strcmp(json["type"], "toggle") == 0)
    {
      oxrsInput.setType(pin, TOGGLE);
    }
    else 
    {
      Serial.println(F("[erro] invalid input type"));
    }
  }
  
  if (json.containsKey("invert"))
  {
    if (json["invert"].isNull())
    {
      oxrsInput.setInvert(pin, false);
    }
    else
    {
      oxrsInput.setInvert(pin, json["invert"].as<bool>());
    }
  }
}

void jsonOutputConfig(uint8_t index, JsonObject json)
{
  // Work out which output MCP and pin we are configuring
  uint8_t mcp = getOutputMcp(index);
  uint8_t pin = getOutputPin(index);

  if (json.containsKey("type"))
  {
    if (json["type"].isNull() || strcmp(json["type"], "relay") == 0)
    {
      oxrsOutput[mcp].setType(pin, RELAY);
    }
    else if (strcmp(json["type"], "motor") == 0)
    {
      oxrsOutput[mcp].setType(pin, MOTOR);
    }
    else if (strcmp(json["type"], "timer") == 0)
    {
      oxrsOutput[mcp].setType(pin, TIMER);
    }
    else 
    {
      Serial.println(F("[erro] invalid output type"));
    }
  }
  
  if (json.containsKey("timerSeconds"))
  {
    if (json["type"].isNull())
    {
      oxrsOutput[mcp].setTimer(pin, DEFAULT_TIMER_SECS);
    }
    else
    {
      oxrsOutput[mcp].setTimer(pin, json["timerSeconds"].as<int>());      
    }
  }
  
  if (json.containsKey("interlockIndex"))
  {
    // If an empty message then treat as 'unlocked' - i.e. interlock with ourselves
    if (json["interlockIndex"].isNull())
    {
      oxrsOutput[mcp].setInterlock(pin, pin);
    }
    else
    {
      uint8_t interlock_index = json["interlockIndex"].as<uint8_t>();
     
      uint8_t interlock_port = ((interlock_index - 1) / 3) + 1;
      uint8_t interlock_channel = (interlock_port * 3) - 1 == interlock_index ? 2 : 1;
  
      uint8_t interlock_mcp = (interlock_index < 24) ? 0 : 1;
      uint8_t interlock_pin = ((interlock_port * 2) - (interlock_channel == 1 ? 2 : 1)) % 16;
  
      if (interlock_mcp == mcp)
      {
        oxrsOutput[mcp].setInterlock(pin, interlock_pin);
      }
      else
      {
        Serial.println(F("[erro] lock must be with pin on same mcp"));
      }
    }
  }
}

void jsonCommand(JsonObject json)
{
  uint8_t index = getIndex(json);
  if (index == 0) return;

  // Only outputs can receive commands - i.e. 1/2, 4/5, 7/8... 46/47
  if ((index % 3) == 0)
  {
    Serial.println(F("[erro] command sent to input channel"));
    return;
  }

  // Work out which output MCP and pin we need to send the command to
  uint8_t mcp = getOutputMcp(index);
  uint8_t pin = getOutputPin(index);

  // Get the output type for this pin
  uint8_t type = oxrsOutput[mcp].getType(pin);
  
  if (json.containsKey("type"))
  {
    if ((strcmp(json["type"], "relay") == 0 && type != RELAY) ||
        (strcmp(json["type"], "motor") == 0 && type != MOTOR) ||
        (strcmp(json["type"], "timer") == 0 && type != TIMER))
    {
      Serial.println(F("[erro] command type doesn't match configured type"));
      return;
    }
  }

  if (json.containsKey("command"))
  {
    if (json["command"].isNull() || strcmp(json["command"], "query") == 0)
    {
      // Publish a status event with the current state
      uint8_t state = mcp23017[mcp + 1].digitalRead(pin);
      publishOutputEvent(index, type, state);
    }
    else
    {
      // Send this command down to our output handler to process
      if (strcmp(json["command"], "on") == 0)
      {
        oxrsOutput[mcp].handleCommand(mcp, pin, RELAY_ON);
      }
      else if (strcmp(json["command"], "off") == 0)
      {
        oxrsOutput[mcp].handleCommand(mcp, pin, RELAY_OFF);
      }
      else 
      {
        Serial.println(F("[erro] invalid command"));
      }
    }
  }
}

uint8_t getIndex(JsonObject json)
{
  if (!json.containsKey("index"))
  {
    Serial.println(F("[erro] missing index"));
    return 0;
  }
  
  uint8_t index = json["index"].as<uint8_t>();
  
  // Check the index is valid for this device
  if (index <= 0 || index > (3 * MCP_PIN_COUNT))
  {
    Serial.println(F("[erro] invalid index"));
    return 0;
  }

  return index;
}

void publishInputEvent(uint8_t index, uint8_t type, uint8_t state)
{
  char inputType[8];
  getInputType(inputType, type);
  char eventType[7];
  getInputEventType(eventType, type, state);

  publishEvent(index, inputType, eventType);  
}

void publishOutputEvent(uint8_t index, uint8_t type, uint8_t state)
{
  char outputType[8];
  getOutputType(outputType, type);
  char eventType[7];
  getOutputEventType(eventType, type, state);

  publishEvent(index, outputType, eventType);  
}

void publishEvent(uint8_t index, char * type, char * event)
{
  // Calculate the port and channel for this index (all 1-based)
  uint8_t port = getPort(index);
  uint8_t channel = getChannel(index);

  // Show event on screen
//  char display[32];
//  sprintf_P(display, PSTR("idx:%2d %s %s   "), index, type, event);
//  screen.show_event(display);

  // Build JSON payload for this event
  StaticJsonDocument<128> json;
  json["port"] = port;
  json["channel"] = channel;
  json["index"] = index;
  json["type"] = type;
  json["event"] = event;

  // Publish event
  if (!rack32.publishStatus(json.as<JsonObject>()))
  {
    // TODO: add any failover handling in here!
    Serial.println("FAILOVER!!!");    
  }
}

void getInputType(char inputType[], uint8_t type)
{
  // Determine what type of input we have
  sprintf_P(inputType, PSTR("error"));
  switch (type)
  {
    case BUTTON:
      sprintf_P(inputType, PSTR("button"));
      break;
    case CONTACT:
      sprintf_P(inputType, PSTR("contact"));
      break;
    case SWITCH:
      sprintf_P(inputType, PSTR("switch"));
      break;
    case TOGGLE:
      sprintf_P(inputType, PSTR("toggle"));
      break;
  }
}

void getInputEventType(char eventType[], uint8_t type, uint8_t state)
{
  // Determine what event we need to publish
  sprintf_P(eventType, PSTR("error"));
  switch (type)
  {
    case BUTTON:
      switch (state)
      {
        case HOLD_EVENT:
          sprintf_P(eventType, PSTR("hold"));
          break;
        case 1:
          sprintf_P(eventType, PSTR("single"));
          break;
        case 2:
          sprintf_P(eventType, PSTR("double"));
          break;
        case 3:
          sprintf_P(eventType, PSTR("triple"));
          break;
        case 4:
          sprintf_P(eventType, PSTR("quad"));
          break;
        case 5:
          sprintf_P(eventType, PSTR("penta"));
          break;
      }
      break;
    case CONTACT:
      switch (state)
      {
        case LOW_EVENT:
          sprintf_P(eventType, PSTR("closed"));
          break;
        case HIGH_EVENT:
          sprintf_P(eventType, PSTR("open"));
          break;
      }
      break;
    case SWITCH:
      switch (state)
      {
        case LOW_EVENT:
          sprintf_P(eventType, PSTR("on"));
          break;
        case HIGH_EVENT:
          sprintf_P(eventType, PSTR("off"));
          break;
      }
      break;
    case TOGGLE:
      sprintf_P(eventType, PSTR("toggle"));
      break;
  }
}

void getOutputType(char outputType[], uint8_t type)
{
  // Determine what type of output we have
  sprintf_P(outputType, PSTR("error"));
  switch (type)
  {
    case MOTOR:
      sprintf_P(outputType, PSTR("motor"));
      break;
    case RELAY:
      sprintf_P(outputType, PSTR("relay"));
      break;
    case TIMER:
      sprintf_P(outputType, PSTR("timer"));
      break;
  }
}

void getOutputEventType(char eventType[], uint8_t type, uint8_t state)
{
  // Determine what event we need to publish
  sprintf_P(eventType, PSTR("error"));
  switch (state)
  {
    case RELAY_ON:
      sprintf_P(eventType, PSTR("on"));
      break;
    case RELAY_OFF:
      sprintf_P(eventType, PSTR("off"));
      break;
  }
}

uint8_t getOutputMcp(uint8_t index)
{
  return (index < 24) ? 0 : 1;
}

uint8_t getOutputPin(uint8_t index)
{
  uint8_t port = getPort(index);
  uint8_t channel = getChannel(index);

  return ((port * 2) - (channel == 1 ? 2 : 1)) % MCP_PIN_COUNT;  
}

uint8_t getPort(uint8_t index)
{
  if (index % 3 == 0)
  {
    return index / 3;
  }
  else
  {
    return ((index - 1) / 3) + 1;
  }
}

uint8_t getChannel(uint8_t index)
{
  if (index % 3 == 0)
  {
    return 3;
  }
  else
  {
    uint8_t port = getPort(index);
    return (port * 3) - 1 == index ? 2 : 1;
  }
}

/**
  Event handlers
*/
void inputEvent(uint8_t id, uint8_t input, uint8_t type, uint8_t state)
{
  // Determine index for this input event (1-based)
  uint8_t index = (input + 1) * 3;
  
  // Publish the event
  publishInputEvent(index, type, state);
}

void outputEvent(uint8_t id, uint8_t output, uint8_t type, uint8_t state)
{
  // Determine the index for this output event
  // Horrible calcs to work out the index - surely must be an easier way!
  uint8_t mcp = id;
  uint8_t pin = output;
  uint8_t output_delta = output % 2 == 0 ? 0 : 1;
  uint8_t raw_index = output + ((output - output_delta) / 2) + (mcp * 24);  
  uint8_t index = raw_index + 1;
  
  // Update the MCP pin - i.e. turn the relay on/off
  mcp23017[mcp + 1].digitalWrite(pin, state);

  // Publish the event
  publishOutputEvent(index, type, state);
}

/**
  I2C
 */
void scanI2CBus()
{
  Serial.println(F("Scanning for MCP23017s on I2C bus..."));

  // Initialise the 3 MCP I/O buffers
  initialiseMCP23017(0, MCP_INPUT_ADDR);
  initialiseMCP23017(1, MCP_OUTPUT1_ADDR);
  initialiseMCP23017(2, MCP_OUTPUT2_ADDR);
  
  // Listen for input events
  oxrsInput.onEvent(inputEvent);

  // Listen for output events
  oxrsOutput[0].onEvent(outputEvent);
  oxrsOutput[1].onEvent(outputEvent);
}

void initialiseMCP23017(int mcp, int address)
{
  Serial.print(F(" - 0x"));
  Serial.print(address, HEX);
  Serial.print(F("..."));

  Wire.beginTransmission(address);
  if (Wire.endTransmission() == 0)
  {
    bitWrite(g_mcps_found, mcp, 1);
    
    // If an MCP23017 was found then initialise and configure the inputs/outputs
    mcp23017[mcp].begin_I2C(address);
    for (uint8_t pin = 0; pin < MCP_PIN_COUNT; pin++)
    {
      mcp23017[mcp].pinMode(pin, address == MCP_INPUT_ADDR ? INPUT : OUTPUT);
    }

    Serial.println(F("MCP23017"));
  }
  else
  {
    // No MCP found at this address
    Serial.println(F("empty"));
  }
}
