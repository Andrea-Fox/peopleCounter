#include <Wire.h>
#include "SparkFun_VL53L1X.h"
#include <WiFi.h>
#include <PubSubClient.h>

const char* ssid = "YOUR-WIFI-NETWORK";     //wi-fi netwrok name
const char* password = "YOUR WIFI PASSWORD";  //wi-fi network password
const char* mqtt_server = "YOUR-MQTT-BROKER-ADDRESS";   // mqtt broker ip address (without port)
const int mqtt_port = MQTT-BROKER-PORT;                   // mqtt broker port 
const char *mqtt_user = "MQTT-USERNAME";
const char *mqtt_pass = "MQTT-PASSWORD";
#define mqtt_serial_publish_ch "sensoreCorridoioSoggiorno/serialdata/tx"
#define mqtt_serial_receiver_ch "sensoreCorridoioSoggiorno/serialdata/rx"

WiFiClient espClient;
PubSubClient client(espClient);

char peopleCounterArray[50];


//Optional interrupt and shutdown pins.  Vanno cambiati e messi quelli che hanno i collegamenti i^2C
#define SHUTDOWN_PIN 2    
#define INTERRUPT_PIN 3

SFEVL53L1X distanceSensor(Wire);//, SHUTDOWN_PIN, INTERRUPT_PIN);

static int NOBODY = 0;
static int SOMEONE = 1;
static int LEFT = 0;
static int RIGHT = 1;

static int DIST_THRESHOLD_MAX[] = {1850, 1650};   // treshold of the two zones

static int PathTrack[] = {0,0,0,0};
static int PathTrackFillingSize = 1; // init this to 1 as we start from state where nobody is any of the zones
static int LeftPreviousStatus = NOBODY;
static int RightPreviousStatus = NOBODY;
static int PeopleCount = 0;

static int center[2] = {239,175}; /* center of the two zones */  
static int Zone = 0;
static int PplCounter = 0;

static int ROI_height = 5;
static int ROI_width = 5;

void setup_wifi() 
{
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) 
  {
    delay(500);
    Serial.print(".");
    WiFi.begin(ssid, password);
  }
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP32Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str(),mqtt_user,mqtt_pass)) {
      Serial.println("connected");
      //Once connected, publish an announcement...
      publishSerialData(PeopleCount);
      // ... and resubscribe
      client.subscribe(mqtt_serial_receiver_ch);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void callback(char* topic, byte *payload, unsigned int length) {
    Serial.println("-------new message from broker-----");
    Serial.print("channel:");
    Serial.println(topic);
    Serial.print("data:");  
    Serial.write(payload, length);
    Serial.println();
    String newTopic = topic;
    payload[length] = '\0';
    String newPayload = String((char *)payload);
    if (newTopic == mqtt_serial_receiver_ch) 
    {
      if (newPayload == "announce")
      {
        publishSerialData(PeopleCount);
      }
      if (newPayload == "reset")
      {
        PeopleCount = 0;
        Serial.print("The number of people in the room has been resetted. People in the room now: ");
        Serial.print(PeopleCount);
        publishSerialData(PeopleCount);
      }
      if (newPayload == "increase")
      {
        PeopleCount = PeopleCount +1;
        Serial.print("The number of people in the room has been increased. People in the room now: ");
        Serial.print(PeopleCount);
        publishSerialData(PeopleCount);
      }
      if (newPayload == "decrease")
      {
        PeopleCount = PeopleCount-1;
        Serial.print("The number of people in the room has been decreased. People in the room now: ");
        Serial.print(PeopleCount);
        publishSerialData(PeopleCount);
      }
    }
}

void publishSerialData(int serialData){
  serialData = max(0, serialData);
  if (!client.connected()) {
    reconnect();
  }
  String stringaCounter = String(serialData);
  stringaCounter.toCharArray(peopleCounterArray, stringaCounter.length() +1);
  client.publish(mqtt_serial_publish_ch, peopleCounterArray);
}

void setup(void)
{
  Wire.begin();

  Serial.begin(9600);
  Serial.println("VL53L1X Qwiic Test");

  if (distanceSensor.init() == false)
    Serial.println("Sensor online!");
  distanceSensor.setIntermeasurementPeriod(100);
  distanceSensor.setDistanceModeLong();

  Serial.setTimeout(500);// Set time out for setup_wifi();
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  delay(1000);
  publishSerialData(0);
  reconnect();  
  
}



void loop(void)
{
  uint16_t distance;
  client.loop();
  if (!client.connected()) 
  {
    reconnect();
  }
  
  
  distanceSensor.setROI(ROI_height, ROI_width, center[Zone]);  // first value: height of the zone, second value: width of the zone
  delay(50);
  distanceSensor.setTimingBudgetInMs(50);
  distanceSensor.startRanging(); //Write configuration bytes to initiate measurement
  distance = distanceSensor.getDistance(); //Get the result of the measurement from the sensor
  distanceSensor.stopRanging();

  Serial.println(distance);

   // inject the new ranged distance in the people counting algorithm
  PplCounter = ProcessPeopleCountingData(distance, Zone);

  Zone++;
  Zone = Zone%2;


}

// NOBODY = 0, SOMEONE = 1, LEFT = 0, RIGHT = 1

int ProcessPeopleCountingData(int16_t Distance, uint8_t zone) {

    int CurrentZoneStatus = NOBODY;
    int AllZonesCurrentStatus = 0;
    int AnEventHasOccured = 0;

  if (Distance < DIST_THRESHOLD_MAX[Zone]) {
    // Someone is in !
    CurrentZoneStatus = SOMEONE;
  }

  // left zone
  if (zone == LEFT) {

    if (CurrentZoneStatus != LeftPreviousStatus) {
      // event in left zone has occured
      AnEventHasOccured = 1;

      if (CurrentZoneStatus == SOMEONE) {
        AllZonesCurrentStatus += 1;
      }
      // need to check right zone as well ...
      if (RightPreviousStatus == SOMEONE) {
        // event in left zone has occured
        AllZonesCurrentStatus += 2;
      }
      // remember for next time
      LeftPreviousStatus = CurrentZoneStatus;
    }
  }
  // right zone
  else {

    if (CurrentZoneStatus != RightPreviousStatus) {

      // event in left zone has occured
      AnEventHasOccured = 1;
      if (CurrentZoneStatus == SOMEONE) {
        AllZonesCurrentStatus += 2;
      }
      // need to left right zone as well ...
      if (LeftPreviousStatus == SOMEONE) {
        // event in left zone has occured
        AllZonesCurrentStatus += 1;
      }
      // remember for next time
      RightPreviousStatus = CurrentZoneStatus;
    }
  }

  // if an event has occured
  if (AnEventHasOccured) {
    if (PathTrackFillingSize < 4) {
      PathTrackFillingSize ++;
    }

    // if nobody anywhere lets check if an exit or entry has happened
    if ((LeftPreviousStatus == NOBODY) && (RightPreviousStatus == NOBODY)) {

      // check exit or entry only if PathTrackFillingSize is 4 (for example 0 1 3 2) and last event is 0 (nobobdy anywhere)
      if (PathTrackFillingSize == 4) {
        // check exit or entry. no need to check PathTrack[0] == 0 , it is always the case
        Serial.println();
        if ((PathTrack[1] == 1)  && (PathTrack[2] == 3) && (PathTrack[3] == 2)) {
          // This an entry
          PeopleCount ++;
          Serial.print("One person has entered in the room. People in the room now: ");
          Serial.print(PeopleCount);
          publishSerialData(PeopleCount);
        } else if ((PathTrack[1] == 2)  && (PathTrack[2] == 3) && (PathTrack[3] == 1)) {
          // This an exit
          PeopleCount --;
          Serial.print("One person has exited the room. People in the room now: ");
          Serial.print(PeopleCount);  
          publishSerialData(PeopleCount);
          }
      }
      for (int i=0; i<4; i++){
        PathTrack[i] = 0;
      }
      PathTrackFillingSize = 1;
    }
    else {
      // update PathTrack
      // example of PathTrack update
      // 0
      // 0 1
      // 0 1 3
      // 0 1 3 1
      // 0 1 3 3
      // 0 1 3 2 ==> if next is 0 : check if exit
      PathTrack[PathTrackFillingSize-1] = AllZonesCurrentStatus;
    }
  }

  // output debug data to main host machine
  return(PeopleCount);     
}
