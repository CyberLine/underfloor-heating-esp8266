#include <FS.h>
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>

#define ValvePin1  16 // D0
#define ValvePin2  14 // D5
#define ValvePin3  12 // D6
#define ValvePin4  13 // D7

// === BEGIN CONFIG ===
#define VALVES      4      // between 1 and 4. How many valves on your pcb are controllable

#define configSSID  "UHC"  // captive portal wifi name
#define configPW    ""     // captive portal wifi pw
// === END CONFIG ===

// webinterface defaults
char mqtt_server[16]     = "192.168.1.1";
char mqtt_port[6]        = "1883";
char mqtt_user[20]       = "";
char mqtt_pass[20]       = "";
char wifiTimeout[3]      = "5";
char Floor[20]           = "underfloor";

char Valve1Command[45]   = "";
char Valve1Status[45]    = "";
#if VALVES > 1
char Valve2Command[45]   = "";
char Valve2Status[45]    = "";
#endif
#if VALVES > 2
char Valve3Command[45]   = "";
char Valve3Status[45]    = "";
#endif
#if VALVES > 3
char Valve4Command[45]   = "";
char Valve4Status[45]    = "";
#endif

void callback(char* topic, byte* payload, unsigned int length);

bool shouldSaveConfig = false;
int connectionFails   = 0;

WiFiClient espClient;
PubSubClient client(espClient);

void saveConfigCallback()
{
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

void initPorts()
{
  pinMode(ValvePin1, OUTPUT);
  digitalWrite(ValvePin1, LOW);
  #if VALVES > 1
  pinMode(ValvePin2, OUTPUT);
  digitalWrite(ValvePin2, LOW);
  #endif
  #if VALVES > 2
  pinMode(ValvePin3, OUTPUT);
  digitalWrite(ValvePin3, LOW);
  #endif
  #if VALVES > 3
  pinMode(ValvePin4, OUTPUT);
  digitalWrite(ValvePin4, LOW);
  #endif
}

void setup()
{
  initPorts();

  Serial.begin(500000);
  Serial.println("Mounting FS...");

  if (SPIFFS.begin())
  {
    Serial.println("Mounted file system");

    if (SPIFFS.exists("/config.json"))
    {
      //File exists, reading and loading
      Serial.println("Reading config file");
      File configFile = SPIFFS.open("/config.json", "r");

      if (configFile)
      {
        Serial.println("Opened config file");
        size_t size = configFile.size();

        //Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);

        if (json.success())
        {
          Serial.println("\nParsed json");
          strcpy(mqtt_server, json["mqtt_server"]);
          strcpy(mqtt_port, json["mqtt_port"]);
          strcpy(mqtt_user, json["mqtt_user"]);
          strcpy(mqtt_pass, json["mqtt_pass"]);
          strcpy(wifiTimeout, json["wifiTimeout"]);
          strcpy(Floor, json["Floor"]);
        } else {
          Serial.println("Failed to load json config");
        }
      }
    }
  } else {
    Serial.println("Failed to mount FS");
  }

  WiFiManagerParameter custom_mqtt_server("server", "MQTT Server", mqtt_server, 16);
  WiFiManagerParameter custom_mqtt_port("port", "MQTT Port", mqtt_port, 9);
  WiFiManagerParameter custom_mqtt_user("user", "MQTT Username", mqtt_user, 20);
  WiFiManagerParameter custom_mqtt_pass("pass", "MQTT Password", mqtt_pass, 20);
  WiFiManagerParameter custom_wifiTimeout("timeout", "5", wifiTimeout, 3);
  WiFiManagerParameter custom_topic_Floor("Floor", "Floor name", Floor, 20);

  WiFiManager wifiManager;
  wifiManager.setDebugOutput(false);

  wifiManager.setSaveConfigCallback(saveConfigCallback);

  wifiManager.addParameter(&custom_mqtt_server);
  wifiManager.addParameter(&custom_mqtt_port);
  wifiManager.addParameter(&custom_mqtt_user);
  wifiManager.addParameter(&custom_mqtt_pass);
  wifiManager.addParameter(&custom_wifiTimeout);
  wifiManager.addParameter(&custom_topic_Floor);

  wifiManager.setMinimumSignalQuality(5);

  //Sets timeout until configuration portal gets turned off
  //and retries connecting to the preconfigured AP
  wifiManager.setConfigPortalTimeout(atoi(wifiTimeout) * 60);

  //Fetches ssid and pass and tries to connect
  //If it does not connect it starts an access point with the specified name
  //and goes into a blocking loop awaiting configuration
  if (!wifiManager.autoConnect(configSSID, configPW))
    {
      Serial.println("Failed to connect and hit timeout");
      delay(3000);
      //Reset and try again, or maybe put it to deep sleep
      ESP.reset();
      delay(5000);
    }

  //If you get here you have connected to the WiFi
  Serial.println("Connected...");

  //Read updated parameters
  strcpy(mqtt_server, custom_mqtt_server.getValue());
  strcpy(mqtt_port, custom_mqtt_port.getValue());
  strcpy(mqtt_user, custom_mqtt_user.getValue());
  strcpy(mqtt_pass, custom_mqtt_pass.getValue());
  strcpy(Floor, custom_topic_Floor.getValue());

  char topic_prefix[60] = "uhc/";
  strncat(topic_prefix, custom_topic_Floor.getValue(), 20);
  strcat(topic_prefix, "/valve/");

  char Command[9] = "/command";
  
  char Status1[2] = "1";

  strcat(Valve1Status, topic_prefix);
  strcat(Valve1Status, Status1);
  strcat(Valve1Command, topic_prefix);
  strcat(Valve1Command, Status1);
  strcat(Valve1Command, Command);

  Serial.println("Constructed topics:");
  Serial.println(Valve1Command);
  Serial.println(Valve1Status);
  #if VALVES > 1
  char Status2[2] = "2";
  strcat(Valve2Status, topic_prefix);
  strcat(Valve2Status, Status2);
  strcat(Valve2Command, topic_prefix);
  strcat(Valve2Command, Status2);
  strcat(Valve2Command, Command);

  Serial.println(Valve2Command);
  Serial.println(Valve2Status);
  #endif
  #if VALVES > 2
  char Status3[2] = "3";
  strcat(Valve3Status, topic_prefix);
  strcat(Valve3Status, Status3);
  strcat(Valve3Command, topic_prefix);
  strcat(Valve3Command, Status3);
  strcat(Valve3Command, Command);

  Serial.println(Valve3Command);
  Serial.println(Valve3Status);
  #endif
  #if VALVES > 3
  char Status4[2] = "4";
  strcat(Valve4Status, topic_prefix);
  strcat(Valve4Status, Status4);
  strcat(Valve4Command, topic_prefix);
  strcat(Valve4Command, Status4);
  strcat(Valve4Command, Command);

  Serial.println(Valve4Command);
  Serial.println(Valve4Status);
  #endif

  if (shouldSaveConfig)
  {
    Serial.println("Saving config...");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["mqtt_server"] = mqtt_server;
    json["mqtt_port"] = mqtt_port;
    json["mqtt_user"] = mqtt_user;
    json["mqtt_pass"] = mqtt_pass;
    json["wifiTimeout"] = wifiTimeout;
    json["Floor"] = Floor;
    
    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile)
    {
      Serial.println("Failed to open config file for writing");
    }

    json.printTo(Serial);
    json.printTo(configFile);
    configFile.close();
  }

  Serial.println();
  Serial.print("Local IP: ");
  Serial.println(WiFi.localIP());

  //Connect to MQTT broker and set callback
  client.setServer(mqtt_server, atoi(mqtt_port));
  client.setCallback(callback);
}

void callback(char* topic, byte* payload, unsigned int length)
{
  Serial.print("Message arrived on topic [");
  Serial.print(topic);
  Serial.print("]: ");

  char buffer[length + 1];

  for (int i = 0; i < length; i++)
    {
      buffer[i] = (char)payload[i];
      Serial.print((char)payload[i]);
    }

  buffer[length] = '\0';

  Serial.println();

  if (strcmp(topic, Valve1Command) == 0)
  {
    if (strcmp(buffer, "on") == 0)
    {
      digitalWrite(ValvePin1, HIGH);
      int val = digitalRead(ValvePin1);
      client.publish(Valve1Status, (val == HIGH ? "on" : "off"), true);
      return;
    }

    if (strcmp(buffer, "off") == 0)
    {
      digitalWrite(ValvePin1, LOW);
      int val = digitalRead(ValvePin1);
      client.publish(Valve1Status, (val == HIGH ? "on" : "off"), true);
      return;
    }

    return;
  }

  #if VALVES > 1
  if (strcmp(topic, Valve2Command) == 0)
  {
    if (strcmp(buffer, "on") == 0)
    {
      digitalWrite(ValvePin2, HIGH);
      int val = digitalRead(ValvePin2);
      client.publish(Valve2Status, (val == HIGH ? "on" : "off"), true);
      return;
    }

    if (strcmp(buffer, "off") == 0)
    {
      digitalWrite(ValvePin2, LOW);
      int val = digitalRead(ValvePin2);
      client.publish(Valve2Status, (val == HIGH ? "on" : "off"), true);
      return;
    }

    return;
  }
  #endif
  #if VALVES > 2
  if (strcmp(topic, Valve3Command) == 0)
  {
    if (strcmp(buffer, "on") == 0)
    {
      digitalWrite(ValvePin3, HIGH);
      int val = digitalRead(ValvePin3);
      client.publish(Valve3Status, (val == HIGH ? "on" : "off"), true);
      return;
    }

    if (strcmp(buffer, "off") == 0)
    {
      digitalWrite(ValvePin3, LOW);
      int val = digitalRead(ValvePin3);
      client.publish(Valve3Status, (val == HIGH ? "on" : "off"), true);
      return;
    }

    return;
  }
  #endif
  #if VALVES > 3
  if (strcmp(topic, Valve4Command) == 0)
  {
    if (strcmp(buffer, "on") == 0)
    {
      digitalWrite(ValvePin4, HIGH);
      int val = digitalRead(ValvePin4);
      client.publish(Valve4Status, (val == HIGH ? "on" : "off"), true);
      return;
    }

    if (strcmp(buffer, "off") == 0)
    {
      digitalWrite(ValvePin4, LOW);
      int val = digitalRead(ValvePin4);
      client.publish(Valve4Status, (val == HIGH ? "on" : "off"), true);
      return;
    }

    return;
  }
  #endif

  return;
}

void connect()
{
  while (!client.connected())
  {
    Serial.begin(500000);
    Serial.print("Attempting MQTT connection...");
    if (client.connect(configSSID, mqtt_user, mqtt_pass))
    {
      Serial.println("connected!");

      client.subscribe(Valve1Command, 1);
      #if VALVES > 1
      client.subscribe(Valve2Command, 1);
      #endif
      #if VALVES > 2
      client.subscribe(Valve3Command, 1);
      #endif
      #if VALVES > 3
      client.subscribe(Valve4Command, 1);
      #endif

      connectionFails = 0;

      //while(Serial.available()) Serial.read();
    } else{
      Serial.print("failed, rc = ");
      Serial.println(client.state());
      Serial.print("Failed connection attempts: ");
      Serial.println(connectionFails);

      if (++connectionFails == 3)
      {
        Serial.println("MQTT broker connection timeout... restarting!");
        delay(1000);
        ESP.restart();
        delay(5000);
        break;
      }
      
      Serial.println("Try again in 5 seconds...");
      delay(5000);
    }
  }
}

void loop()
{
  if (!client.connected())
  {
    connect();
  }

  client.loop();
}
