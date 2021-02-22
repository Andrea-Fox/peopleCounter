# MQTT People counter

This repository contains a program (to be flashed, for example on an ESP32, using the Arduino IDE) which allows to create a sensor capable of detecting people going in and out of a room. It works using the VL53L1X sensor by ST Microelectronics. The passage detection is then shared through the MQTT protocol and the count of the people in the room is done directly on other platform such as Home Assistant (see the dedicated file for a smooth integration in Home Assistant).

## Idea behind the algorithm

The code contains an adaptation of the program  [STSW-IMG10](https://www.st.com/en/embedded-software/stsw-img010.html) (developed by ST Microelectronics), written using the [Sparkfun library for VL53L31X](https://learn.sparkfun.com/tutorials/qwiic-distance-sensor-vl53l1x-hookup-guide/all).  

**The library used is from SparkFun and is made for [their sensor](https://www.sparkfun.com/products/14722), however the same code can also be used alongside the [Pololu VL53L31X sensor](https://www.pololu.com/product/3415).**

The sensor uses the time of flight (ToF) of invisible, eye-safe laser pulses to measure absolute distances independent of ambient lighting conditions  and target characteristics like color, shape, and texture (though these  things will affect the maximum range).  Something is detected in a certain zone, when the distance read by the sensor os lower than the corresponding threshold. 

The idea behind the main algorithm counting for people is the following: after defining two different zones, a passage (entrance or exit) is registered only when:

1. a person is detected in the first zone
2. a person is detected in both zones simultaneously
3. a person is detected in the second zone
4. no person are detected in both zones

Then, depending on which are the first and the second zone, the movement will be either registered as an entrance or an exit. 

## Hardware

### Sensor

As I previuosly stated, the library used to write the code is built for the [Sparkfun Distance Sensor](https://www.sparkfun.com/products/14722), but also works using the [Pololu VL53L31X sensor](https://www.pololu.com/product/3415).
It probably should also work with other VL53L1X sensors, however I've never tried it.

### Board

The following tables contain the necessary wire connections, when using certain boards.

#### 5V boards

(including Arduino Uno, Leonardo, Mega)

```
Arduino   VL53L1X board
-------   -------------
     5V - VIN
    GND - GND
    SDA - SDA
    SCL - SCL
```

The Pololu sensor can also be connetced to 3.3V boards, such as the Arduino Due.

Clearly, to use the MQTT functionalities one has to use a board which supports WiFi connection, such as ESP32: in this case, the connections are the following:

#### ESP32

```
                    ESP32   VL53L1X board
-------------------------   -------------
                      3V3 - VIN
                      GND - GND
     SDA (pin 42, GPIO21) - SDA
     SCL (pin 39, GPIO22) - SCL
```
The sensors from Pololu and Adafruit can also be connected to the 5v pin (VIN)
#### ESP8266
It is also possible to use an ESP8266, using, for example, the following connections: (in that case, use the code specific for this board)
```
                  ESP8266   VL53L1X board
-------------------------   -------------
                      3V3 - VIN
                      GND - GND
                       D2 - SDA
                       D1 - SCL
```


## How to adapt the code to your case

### WiFi information

In order to connect to the WiFi, one has to specify the name of the WiFi network, its password, the MQTT broker address and ots port, the MQTT username and the corresponding password. All this values have to inserted at the beginning of the code, in the corresponding lines.

One might also edit the name of the device in the MQTT network: this can be easily done just by editing `mqtt_serial_publish_ch` and `mqtt_serial_receiver_ch`. The first one corresponds to the address used when publishing messages, while the second one corresponds to the address for messages sent to the board connected to the sensor.   
**Important:** one also has to specify the informations about the MQTT server, otherwise (using an ESP32) it will be impossible to connect to the WiFi, as noted in [#3](https://github.com/Andrea-Fox/peopleCounter/issues/3)


### Parameters one might want to set
The following parameteres are automatically set by the code one can find in the `autocalibration.ino` file. In that file, the only value one might want to add is `advised_orientation_of_the_sensor`, a boolean which is true if the sensor is positioned as in the following image:
<p float="left">
  <img src="autocalibration/sensor_orientation.png" width="300" />
</p>

The arrow indicates the direction of the people passing under the sensor

#### Relevant area

In order to find the correct distance, the sensor creates a 16x16 grid and the final distance is computed by taking the average of the distance of the values of the grid; to perform our task, one has to create two zones, by defining two different Region of Interest (ROI) inside this grid. Then the sensor will measure the two distances in the two zones and will detect any presence. 

However, the algorithm is very sensitive to the slightest modification of the ROI, regarding both its size and its positioning inside the grid.

In the original code, developed by ST Microelectronics, the values for the parameters are the following:

- `ROI_width = 8`
- `ROI_height = 16`
- `center = {167,231}`
however, I've noticed better performances with the values suggested in the code above.

Be careful that both `ROI_width` and `ROI_heigth` have to be at least 4. The center of the ROI you set is based on the table below and the optical center has to be set as the pad above and to the right of your exact center:



| 128  | 136  | 144  | 152  | 160  | 168  | 176  | 184  | 192  | 200  | 208  | 216  | 224  | 232  | 240  | 248  |
| ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- |
| 129  | 137  | 145  | 153  | 161  | 169  | 177  | 185  | 193  | 201  | 209  | 217  | 225  | 233  | 241  | 249  |
| 130  | 138  | 146  | 154  | 162  | 170  | 178  | 186  | 194  | 202  | 210  | 218  | 226  | 234  | 242  | 250  |
| 131  | 139  | 146  | 155  | 163  | 171  | 179  | 187  | 195  | 203  | 211  | 219  | 227  | 235  | 243  | 251  |
| 132  | 140  | 147  | 156  | 164  | 172  | 180  | 188  | 196  | 204  | 212  | 220  | 228  | 236  | 244  | 252  |
| 133  | 141  | 148  | 157  | 165  | 173  | 181  | 189  | 197  | 205  | 213  | 221  | 229  | 237  | 245  | 253  |
| 134  | 142  | 149  | 158  | 166  | 174  | 182  | 190  | 198  | 206  | 214  | 222  | 230  | 238  | 246  | 254  |
| 135  | 143  | 150  | 159  | 167  | 175  | 183  | 191  | 199  | 207  | 215  | 223  | 231  | 239  | 247  | 255  |
| 127  | 119  | 111  | 103  | 95   | 87   | 79   | 71   | 63   | 55   | 47   | 39   | 31   | 23   | 15   | 7    |
| 126  | 118  | 110  | 102  | 94   | 86   | 78   | 70   | 62   | 54   | 46   | 38   | 30   | 22   | 14   | 6    |
| 125  | 117  | 109  | 101  | 93   | 85   | 77   | 69   | 61   | 53   | 45   | 37   | 29   | 21   | 13   | 5    |
| 124  | 116  | 108  | 100  | 92   | 84   | 76   | 68   | 60   | 52   | 44   | 36   | 28   | 20   | 12   | 4    |
| 123  | 115  | 107  | 99   | 91   | 83   | 75   | 67   | 59   | 51   | 43   | 35   | 27   | 19   | 11   | 3    |
| 122  | 114  | 106  | 98   | 90   | 82   | 74   | 66   | 58   | 50   | 42   | 34   | 26   | 18   | 10   | 2    |
| 121  | 113  | 105  | 97   | 89   | 81   | 73   | 65   | 57   | 49   | 41   | 33   | 25   | 17   | 9    | 1    |
| 120  | 112  | 104  | 96   | 88   | 80   | 72   | 64   | 56   | 48   | 40   | 32   | 24   | 16   | 8    | 0    |





#### Threshold distance

Another crucial choice is the one corresponding to the threshold. Indeed a movement is detected whenever the distance read by the sensor is below this value. The code contains a vector as threshold, as one (as myself) might need a different threshold for each zone.

The SparkFun library also supports more formats for the threshold: for example one can set that a movement is detected whenever the distance is between two values. However, more information for the interested reader can be found on the corresponding page.

With the updated code (however only for esp32 at the moment) the threshold is automatically calculated by the sensor. To do so it is necessary to position the sensor and, after turning it on, wait for 10 seconds without passing under it. After this time, the average of the measures for each zone will be computed and the thereshold for each ROI will correspond to 80% of the average value. Also the value of 80% can be modified in the code, by editing the variable `threshold_percentage`

The calibration of the threshold can also be triggered by a MQTT message. An example for doing so is in the file `integration_with_home_assistant.md`.

### How to invert the two zones
If you observe that when you get in a room the number of people inside it decreases, while it increases when you get out it means that the values of the centers are inverted. to solve this issue, you can simply invert the values 1 and 2 in the automations which regulate the number of people in the `integration_with_home_assistant.md` file.

### OTA updates
The updated file allows OTA updates through the Arduino IDE. For more information about OTA updates in Arduino, one may look at [this article](https://lastminuteengineers.com/esp32-ota-updates-arduino-ide/)

## Case for the sensor
In the case folder you can find an stl file (created by @noxhirsch) for a case created if you are using a Wemos Mini ESP32 and [this sensor](https://de.aliexpress.com/item/4000065731557.html?spm=a2g0s.9042311.0.0.27424c4dgIS4KI). there is also the Fusion 360 file, if you want to modify it for other ESP32 or sensors. 


## Useful links

[PDf file with more information about the algorithm](https://www.google.com/url?sa=t&rct=j&q=&esrc=s&source=web&cd=&ved=2ahUKEwilvayvxY7rAhUDDOwKHXhZCqkQFjAFegQIAxAB&url=https%3A%2F%2Fwww.st.com%2Fresource%2Fen%2Fuser_manual%2Fdm00626942-counting-people-with-the-vl53l1x-longdistance-ranging-timeofflight-sensor-stmicroelectronics.pdf&usg=AOvVaw3-q-bXHDXmQx6cFFnkOOUs): this PDF is an in-depth explanation of the algorithm and contains technical details about the sensor

[SparkFun library guide](https://learn.sparkfun.com/tutorials/qwiic-distance-sensor-vl53l1x-hookup-guide/all) with more information about the functions used in the code

[MQTT with ESP32 tutorial](https://iotdesignpro.com/projects/how-to-connect-esp32-mqtt-broker)



## Discord server
If you want to discuss about the idea of finding the ultimate room presence sensor, feel free to join the dedicate Discord (created by @DutchDeffy): https://discord.gg/65eBamz7AS


