#include <Wire.h>
#include "SparkFun_VL53L1X.h"
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoOTA.h>
#include <EEPROM.h>

const char* devicename = "name_for_this_device"; // sets MQTT topics and hostname for ArduinoOTA

const char* ssid = "";     //wi-fi netwrok name
const char* password = "";  //wi-fi network password
const char* mqtt_server = "";   // mqtt broker ip address (without port)
const int mqtt_port = ;                   // mqtt broker port
const char *mqtt_user = "";
const char *mqtt_pass = "";

// MQTT Topics
// people_counter/DEVICENAME/counter
// people_counter/DEVICENAME/distance
// people_counter/DEVICENAME/receiver

const int threshold_percentage = 80;

// if "true", the raw measurements are sent via MQTT during runtime (for debugging) - I'd recommend setting it to "false" to save traffic and system resources.
// in the calibration phase the raw measurements will still be sent through MQTT
static bool update_raw_measurements = false;

// this value has to be true if the sensor is oriented as in Duthdeffy's picture
static bool advised_orientation_of_the_sensor = true;

// this value has to be true if you don't need to compute the threshold every time the device is turned on
static bool save_calibration_result = true;


//*******************************************************************************************************************
// all the code from this point and onwards doesn't have to be touched in order to have everything working (hopefully)

char mqtt_serial_publish_ch_cache[50];
char mqtt_serial_publish_distance_ch_cache[50];
char mqtt_serial_receiver_ch_cache[50];

int mqtt_counter = sprintf(mqtt_serial_publish_ch_cache,"%s%s%s","people_counter/", devicename,"/counter");
const PROGMEM char* mqtt_serial_publish_ch = mqtt_serial_publish_ch_cache;
int mqtt_distance = sprintf(mqtt_serial_publish_distance_ch_cache,"%s%s%s","people_counter/", devicename,"/distance");
const PROGMEM char* mqtt_serial_publish_distance_ch = mqtt_serial_publish_distance_ch_cache;
int mqtt_receiver = sprintf(mqtt_serial_receiver_ch_cache,"%s%s%s","people_counter/", devicename,"/receiver");
const PROGMEM char* mqtt_serial_receiver_ch = mqtt_serial_receiver_ch_cache;

#define EEPROM_SIZE 8

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

static int DIST_THRESHOLD_MAX[] = {0, 0};   // treshold of the two zones
static int MIN_DISTANCE[] = {0, 0};

static int PathTrack[] = {0,0,0,0};
static int PathTrackFillingSize = 1; // init this to 1 as we start from state where nobody is any of the zones
static int LeftPreviousStatus = NOBODY;
static int RightPreviousStatus = NOBODY;

static int center[2] = {0,0}; /* center of the two zones */  
static int Zone = 0;
static int PplCounter = 0;

static int ROI_height = 0;
static int ROI_width = 0;


void zones_calibration_boot(){
  if (save_calibration_result){
    // if possible, we take the old values of the zones contained in the EEPROM memory
    client.publish(mqtt_serial_publish_distance_ch, "save calibration result true");
    if (EEPROM.read(0) == 1){
      // we have data in the EEPROM
      client.publish(mqtt_serial_publish_distance_ch, "EEPROM memroy not empty");
      center[0] = EEPROM.read(1);
      center[1] = EEPROM.read(2);
      ROI_height = EEPROM.read(3);
      ROI_width = EEPROM.read(3);
      DIST_THRESHOLD_MAX[0] = EEPROM.read(4)*100 + EEPROM.read(5);;
      DIST_THRESHOLD_MAX[1] = EEPROM.read(6)*100 + EEPROM.read(7);
      
      publishDistance(center[0], 0);
      publishDistance(center[1], 0);
      publishDistance(ROI_width, 0);
      publishDistance(ROI_height, 0);
      publishDistance(DIST_THRESHOLD_MAX[0], 0);
      publishDistance(DIST_THRESHOLD_MAX[1], 0);
      client.publish(mqtt_serial_publish_distance_ch, "All values updated");
    }
    else{
      // there are no data in the EEPROM memory
      zones_calibration();
    }
  }
  else
    zones_calibration();
}

void zones_calibration(){
  // the sensor does 100 measurements for each zone (zones are predefined)
  // each measurements is done with a timing budget of 100 ms, to increase the precision
  client.publish(mqtt_serial_publish_distance_ch, "Computation of new threshold");
  center[0] = 167;
  center[1] = 231;
  ROI_height = 8;
  ROI_width = 8;
  delay(500);
  Zone = 0;
  float sum_zone_0 = 0;
  float sum_zone_1 = 0;
  uint16_t distance;
  int number_attempts = 20;
  for (int i=0; i<number_attempts; i++){
      // increase sum of values in Zone 0
      distanceSensor.setROI(ROI_height, ROI_width, center[Zone]);  // first value: height of the zone, second value: width of the zone
      delay(50);
      distanceSensor.setTimingBudgetInMs(50);
      distanceSensor.startRanging(); //Write configuration bytes to initiate measurement
      distance = distanceSensor.getDistance(); //Get the result of the measurement from the sensor
      distanceSensor.stopRanging();      
      sum_zone_0 = sum_zone_0 + distance;
      publishDistance(distance, 0);
      Zone++;
      Zone = Zone%2;

      // increase sum of values in Zone 1
      distanceSensor.setROI(ROI_height, ROI_width, center[Zone]);  // first value: height of the zone, second value: width of the zone
      delay(50);
      distanceSensor.setTimingBudgetInMs(50);
      distanceSensor.startRanging(); //Write configuration bytes to initiate measurement
      distance = distanceSensor.getDistance(); //Get the result of the measurement from the sensor
      distanceSensor.stopRanging();      
      sum_zone_1 = sum_zone_1 + distance;
      publishDistance(distance, 1);
      Zone++;
      Zone = Zone%2;
  }
  // after we have computed the sum for each zone, we can compute the average distance of each zone
  float average_zone_0 = sum_zone_0 / number_attempts;
  float average_zone_1 = sum_zone_1 / number_attempts;
  // the value of the average distance is used for computing the optimal size of the ROI and consequently also the center of the two zones
  int function_of_the_distance = 16*(1 - (0.22 * 2) / (0.34 * (min(average_zone_0, average_zone_1)/1000) ));
  publishDistance(function_of_the_distance, 1);
  delay(1000);
  int ROI_size = min(8, max(4, function_of_the_distance));
  ROI_width = ROI_size;
  ROI_height = ROI_size;
  if (advised_orientation_of_the_sensor){
    
    switch (ROI_size) {
        case 4:
          center[0] = 150;
          center[1] = 247;
          break;
        case 5:
          center[0] = 159;
          center[1] = 239;
          break;
        case 6:
          center[0] = 159;
          center[1] = 239;
          break;
        case 7:
          center[0] = 167;
          center[1] = 231;
          break;
        case 8:
          center[0] = 167;
          center[1] = 231;
          break;
      }
  }
  else{
    switch (ROI_size) {
        case 4:
          center[0] = 193;
          center[1] = 58;
           break;
        case 5:
          center[0] = 194;
          center[1] = 59;
          break;
        case 6:
          center[0] = 194;
          center[1] = 59;
          break;
        case 7:
          center[0] = 195;
          center[1] = 60;
          break;
        case 8:
          center[0] = 195;
          center[1] = 60;
          break;
      }
  }
  client.publish(mqtt_serial_publish_distance_ch, "ROI size");
  publishDistance(ROI_size, 0);  
  client.publish(mqtt_serial_publish_distance_ch, "centers of the ROIs defined");
  publishDistance(center[0], 0);   
  publishDistance(center[1], 1);
  delay(2000);
  // we will now repeat the calculations necessary to define the thresholds with the updated zones
  Zone = 0;
  sum_zone_0 = 0;
  sum_zone_1 = 0;
  for (int i=0; i<number_attempts; i++){
      // increase sum of values in Zone 0
      distanceSensor.setROI(ROI_height, ROI_width, center[Zone]);  // first value: height of the zone, second value: width of the zone
      delay(50);
      distanceSensor.setTimingBudgetInMs(50);
      distanceSensor.startRanging(); //Write configuration bytes to initiate measurement
      distance = distanceSensor.getDistance(); //Get the result of the measurement from the sensor
      distanceSensor.stopRanging();      
      sum_zone_0 = sum_zone_0 + distance;
      publishDistance(distance, 0);
      Zone++;
      Zone = Zone%2;

      // increase sum of values in Zone 1
      distanceSensor.setROI(ROI_height, ROI_width, center[Zone]);  // first value: height of the zone, second value: width of the zone
      delay(50);
      distanceSensor.setTimingBudgetInMs(50);
      distanceSensor.startRanging(); //Write configuration bytes to initiate measurement
      distance = distanceSensor.getDistance(); //Get the result of the measurement from the sensor
      distanceSensor.stopRanging();      
      sum_zone_1 = sum_zone_1 + distance;
      publishDistance(distance, 1);
      Zone++;
      Zone = Zone%2;
  }
  average_zone_0 = sum_zone_0 / number_attempts;
  average_zone_1 = sum_zone_1 / number_attempts;
  float threshold_zone_0 = average_zone_0 * threshold_percentage/100; // they can be int values, as we are not interested in the decimal part when defining the threshold
  float threshold_zone_1 = average_zone_1 * threshold_percentage/100;
  
  DIST_THRESHOLD_MAX[0] = threshold_zone_0;
  DIST_THRESHOLD_MAX[1] = threshold_zone_1;
  client.publish(mqtt_serial_publish_distance_ch, "new threshold defined");
  publishDistance(threshold_zone_0, 0);   
  publishDistance(threshold_zone_1, 1);
  delay(2000);

  // we now save the values into the EEPROM memory
  int hundred_threshold_zone_0 = threshold_zone_0 / 100;
  int hundred_threshold_zone_1 = threshold_zone_1 / 100;
  int unit_threshold_zone_0 = threshold_zone_0 - 100* hundred_threshold_zone_0;
  int unit_threshold_zone_1 = threshold_zone_1 - 100* hundred_threshold_zone_1;

  EEPROM.write(0, 1);
  EEPROM.write(1, center[0]);
  EEPROM.write(2, center[1]);
  EEPROM.write(3, ROI_size);
  EEPROM.write(4, hundred_threshold_zone_0);
  EEPROM.write(5, unit_threshold_zone_0);
  EEPROM.write(6, hundred_threshold_zone_1);
  EEPROM.write(7, unit_threshold_zone_1);
  EEPROM.commit();
  
}


void setup_wifi() 
{
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) 
  {
    delay(500);
    Serial.print(".");
//    WiFi.begin(ssid, password);
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
      if (newPayload == "zones_calibration")
      {
        zones_calibration();
      }
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




void publishPersonPassage(int serialData){
  serialData = max(0, serialData);
  if (!client.connected()) {
    reconnect();
  }
  String stringaCounter = String(serialData);
  stringaCounter.toCharArray(peopleCounterArray, stringaCounter.length() +1);
  client.publish(mqtt_serial_publish_ch, peopleCounterArray);
}

void publishDistance(int serialData, int zona){
  //serialData = max(0, serialData);
  if (!client.connected()) {
    reconnect();
  }
  String stringaZona = "\t zona = ";
  String stringaCounter = String(serialData)+ stringaZona + String(zona) + "\t" + String(PathTrack[0]) + String(PathTrack[1]) + String(PathTrack[2]) +String(PathTrack[3]);
  stringaCounter.toCharArray(peopleCounterArray, stringaCounter.length() +1);
  client.publish(mqtt_serial_publish_distance_ch, peopleCounterArray);
}

void setup(void)
{
  Wire.begin();
  // initialize the EEPROM memory
  EEPROM.begin(EEPROM_SIZE);
  
  Serial.begin(9600);
  Serial.println("VL53L1X Qwiic Test");

  if (distanceSensor.init() == false)
    Serial.println("Sensor online!");
  distanceSensor.setIntermeasurementPeriod(50);
  distanceSensor.setDistanceModeLong();

  Serial.setTimeout(500);// Set time out for setup_wifi();
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  delay(1000);
  zones_calibration_boot();
  reconnect();  
  
 ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_FS
      type = "filesystem";
    }

    // NOTE: if updating FS this would be the place to unmount FS using FS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin();
  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}


void loop(void)
{
  ArduinoOTA.handle();
  uint16_t distance;
  client.loop();
  if (!client.connected()) 
  {
    reconnect();
  }
  
  
  distanceSensor.setROI(ROI_height, ROI_width, center[Zone]);  // first value: height of the zone, second value: width of the zone
  delay(52);
  distanceSensor.setTimingBudgetInMs(50);
  distanceSensor.startRanging(); //Write configuration bytes to initiate measurement
  distance = distanceSensor.getDistance(); //Get the result of the measurement from the sensor
  distanceSensor.stopRanging();

  Serial.println(distance);
  if(update_raw_measurements == true) {
    publishDistance(distance, Zone);
  }

   // inject the new ranged distance in the people counting algorithm
  processPeopleCountingData(distance, Zone);

  Zone++;
  Zone = Zone%2;


}

// NOBODY = 0, SOMEONE = 1, LEFT = 0, RIGHT = 1

void processPeopleCountingData(int16_t Distance, uint8_t zone) {

    int CurrentZoneStatus = NOBODY;
    int AllZonesCurrentStatus = 0;
    int AnEventHasOccured = 0;

  if (Distance < DIST_THRESHOLD_MAX[Zone] && Distance > MIN_DISTANCE[Zone]) {
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
          // this is an entry
          publishPersonPassage(1);
        } else if ((PathTrack[1] == 2)  && (PathTrack[2] == 3) && (PathTrack[3] == 1)) {
          // This an exit
          publishPersonPassage(2);
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
}
