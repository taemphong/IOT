#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <HTTPClient.h>



int Relay = 4;
#define DHTPIN 16
#define DHTTYPE DHT11   
DHT dht(DHTPIN, DHTTYPE);

int Mode;
const char *ssid = "CCTV01_2.4GHz"; 
const char *password = "0912322063"; 

const char *mqtt_broker = "broker.hivemq.com"; 
const char *topic_control = "Pump/Control";
const char *topic_temp = "temp";
const char *topic_humi = "humi";
const char *topic_pump = "Pump/State";
const char *topic_state = "soil/State";
const int mqtt_port = 1883;
const char *line_notify_token = "psdnPD3s0p4JW9F8nv8A7uYOUqEjA1QBPdI5reF5SAj"; 
WiFiClient espClient;
PubSubClient client(espClient);

void setup() {
  
 Serial.begin(115200);
 dht.begin();
 pinMode(Relay, OUTPUT);
 WiFi.begin(ssid, password);
 while (WiFi.status() != WL_CONNECTED) {
  delay(500);
  Serial.println("Connecting to WiFi..");
 }
 
 Serial.println("Connected to the WiFi network");
 
 client.setServer(mqtt_broker, mqtt_port);
 client.setCallback(Callback);
 
 while (!client.connected()) {
  String client_id = "esp8266-client-";
  client_id += String(WiFi.macAddress());
  
  Serial.printf("The client %s connects to mosquitto mqtt broker\n", client_id.c_str());
  
  if (client.connect(client_id.c_str())) {
    Serial.println("Public emqx mqtt broker connected");
  } else {
    Serial.print("failed with state ");
    Serial.print(client.state());
    delay(2000);
  }
 }

 client.publish(topic_control, "อุปกรณ์เชื่อมต่อแล้ว");
 client.subscribe(topic_control);
}

void sendLineNotify(String message) {
  HTTPClient http;
  http.begin("https://notify-api.line.me/api/notify");
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  http.addHeader("Authorization", "Bearer " + String(line_notify_token));
  
  String postData = "message=" + message;
  int httpResponseCode = http.POST(postData);
  
  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.println(httpResponseCode);
    Serial.println(response);
  } else {
    Serial.println("Error on sending message.");
  }
  
  http.end();
}

void Callback(char *topic_control, byte *payload, unsigned int length) {
  String string;
  
  Serial.print(topic_control);
  for (int i = 0; i < length; i++) {
    string += (char)payload[i];  
    
    Serial.println(string);
    
    if (((char) payload[i] == '1') and (Mode == 1)) {
      digitalWrite(Relay, 1);
      client.publish(topic_state, String(1).c_str());
      client.publish(topic_pump, String("กำลังทำงาน").c_str());
    }
    if (((char) payload[i] == '0') and (Mode == 1)) {
      digitalWrite(Relay, 0);
      client.publish(topic_state, String(0).c_str());
      client.publish(topic_pump, String("หยุดทำงาน").c_str());
    }
    if ((char) payload[i] == 'M') {
      Mode = 1;
    }
    if ((char) payload[i] == 'A') {
      Mode = 0;
    }
  }
  Serial.println(string);
  Serial.println(" - - - - - - - - - - - -");
}

void loop() {
  client.loop();
  
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  if (isnan(h) || isnan(t)) {
    Serial.println(F("Failed to read from DHT sensor!"));
    return;
  }
  client.publish(topic_temp, String(t).c_str());
  client.publish(topic_humi, String(h).c_str());
  
  if (t >= 30 and Mode == 0) {
    digitalWrite(Relay, 1);
    client.publish(topic_state, String(1).c_str());
    client.publish(topic_pump, String("กำลังทำงาน").c_str());
    sendLineNotify("อุณหภูมิสูงเกินไป!");
  }
  if (t <= 30 and Mode == 0) {
    digitalWrite(Relay, 0);
    client.publish(topic_state, String(0).c_str());
    client.publish(topic_pump, String("หยุดทำงาน").c_str());
    sendLineNotify("อุณหภูมิปกติ");
  }
  delay(1000);
}
