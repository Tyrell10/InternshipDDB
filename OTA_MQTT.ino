#include <WiFi.h>
#include <Update.h>
#include <ESPmDNS.h>
#include <PubSubClient.h>

WiFiClient client;
PubSubClient mqtt(client);

const char* MQTT_SERVER = "36.94.105.74";
const char* HOSTNAME = "EspDuino32";
const char* MQTT_PASSWORD = "";
const char* WIFI_SSID = "AndroidAP";
const char* WIFI_SECRET = "081274695021";
const char* TOPIC = "EspDuino32/Update";

int contentLength = 0;
bool isValidContentType = false;

const int pinLed = 5;
unsigned long prevMillis = 0;
const long interval = 500;
int ledState = LOW;

String getHeaderValue(String header, String headerName)
{
    return header.substring(strlen(headerName.c_str()));
}

String getBinName(String url)
{
    int index = 0;
    String binName = "";
    
    for (int i = 0; i < url.length(); i++) {
        if (url[i] == '/') {
            index = i;
        }
    }

    for (int i = index; i < url.length(); i++) {
        binName += url[i];
    }

    return binName;
}

String getHostName(String url)
{
     int index = 0;
     String hostName = "";

    for (int i = 0; i < url.length(); i++) {
        if (url[i] == '/') {
            index = i;
        }
    }

    for (int i = 0; i < index; i++) {
        hostName += url[i];
    }

    return hostName;
}

void update(String url, int port) 
{
    String bin = getBinName(url);
    String host = getHostName(url);

    Serial.println("Connecting to: " + host);
    if(client.connect(host.c_str(), port)) 
    {
        // Fecthing the bin
        Serial.println("Fetching Bin: " + bin);

        // Get the contents of the bin file
        client.print(String("GET ") + bin + " HTTP/1.1\r\n" +
                        "Host: " + host + "\r\n" +
                        "Cache-Control: no-cache\r\n" +
                        "Connection: close\r\n\r\n");

        unsigned long timeout = millis();

        while (client.available() == 0) 
        {
            if (millis() - timeout > 5000) 
            {
                Serial.println("Client Timeout !");
                client.stop();
                return;
            }
        }
        
        while (client.available()) 
        {
            String line = client.readStringUntil('\n');
            
            // remove space, to check if the line is end of headers
            line.trim();

            /* if the the line is empty,
               this is end of headers
               break the while and feed the
               remaining `client` to the
               Update.writeStream();
             */
            if(!line.length()) {
                //headers ended
                break;
            }

            // Check if the HTTP Response is 200
            if (line.startsWith("HTTP/1.1")) 
            {
                if (line.indexOf("200") < 0) 
                {
                    Serial.println("Got a non 200 status code from server. Exiting OTA Update.");
                    break;
                }
            }

            // extract headers here
            if(line.startsWith("Content-Length: ")) 
            {
                contentLength = atoi((getHeaderValue(line, "Content-Length: ")).c_str());
                Serial.println("Got " + String(contentLength) + " bytes from server");
            }

            if(line.startsWith("Content-Type: ")) 
            {
                String contentType = getHeaderValue(line, "Content-Type: ");
                Serial.println("Got " + contentType + " payload.");
                
                if (contentType == "application/octet-stream") 
                    isValidContentType = true;
                
            }
        }
    }
    else 
        Serial.println("Connection to " + host + " failed. Please check your setup");

    // Check what is the contentLength and if content type is `application/octet-stream`
    Serial.println("contentLength : " + String(contentLength) + ", isValidContentType : " + String(isValidContentType));

    if (contentLength && isValidContentType) 
    {
        bool canBegin = Update.begin(contentLength);
        if (canBegin) 
        {
            Serial.println("Begin OTA. This may take 2 - 5 mins to complete. Things might be quite for a while.. Patience!");
            size_t written = Update.writeStream(client);

            if(written == contentLength) 
                Serial.println("Written : " + String(written) + " successfully");
            else 
                Serial.println("Written only : " + String(written) + "/" + String(contentLength) + ". Retry?" );

            if (Update.end()) 
            {
                Serial.println("OTA done!");
                if (Update.isFinished()) 
                {
                    Serial.println("Update successfully completed. Rebooting.");
                    ESP.restart();
                }
                else 
                    Serial.println("Update not finished? Something went wrong!");
            }
            else 
                Serial.println("Error Occurred. Error #: " + String(Update.getError()));

        }
        else {
            Serial.println("Not enough space to begin OTA");
            client.flush();
        }
    }
    else {
        Serial.println("There was no content in the response");
        client.flush();
    }
}

void callback(char* topic, byte* payload, unsigned int length)
{
    payload[length] = '\0';
    String _message = String((char*)payload);
    String _topic = String(topic);

    if(_topic.equals(TOPIC) == 1) update(_message, 6001);
    
}

void connectToWiFi()
{
    Serial.println();
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(WIFI_SSID);

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_SECRET);

    while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }

    if (!MDNS.begin(HOSTNAME)) 
    {
      Serial.println("Error setting up MDNS responder!");
      while (1) {
        delay(1000);
      }
    }

    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
}

void reconnect() 
{
    while(!mqtt.connected()) 
    {
        Serial.print("Attempting MQTT connection...");
        if(mqtt.connect("ESP32Client", HOSTNAME, MQTT_PASSWORD)) 
        {
            Serial.println("connected");
            Serial.println("EspDuino32 Ready!");
            mqtt.subscribe(TOPIC,1);
        }
        else
        {
            Serial.print("failed, rc=");
            Serial.print(mqtt.state());
            Serial.println(" try again in 5 seconds");
            mqtt.unsubscribe(TOPIC);
            delay(5000);
        }
    }
}

void setup() 
{
    pinMode(pinLed, OUTPUT);

    Serial.begin(115200);
    connectToWiFi();
    mqtt.setServer(MQTT_SERVER, 1883);
    mqtt.setCallback(callback);
}

void loop() 
{
    if(!mqtt.connected()) reconnect();
    
    mqtt.loop();

    unsigned long currentMillis = millis();
    if(currentMillis - prevMillis >= interval)
    {
      prevMillis = currentMillis;
      ledState = not(ledState);
      digitalWrite(pinLed, ledState);
    }
}
