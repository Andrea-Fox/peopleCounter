## Additional informations about the parameters

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



## Useful links

[PDf file with more information about the algorithm](https://www.google.com/url?sa=t&rct=j&q=&esrc=s&source=web&cd=&ved=2ahUKEwilvayvxY7rAhUDDOwKHXhZCqkQFjAFegQIAxAB&url=https%3A%2F%2Fwww.st.com%2Fresource%2Fen%2Fuser_manual%2Fdm00626942-counting-people-with-the-vl53l1x-longdistance-ranging-timeofflight-sensor-stmicroelectronics.pdf&usg=AOvVaw3-q-bXHDXmQx6cFFnkOOUs): this PDF is an in-depth explanation of the algorithm and contains technical details about the sensor

[SparkFun library guide](https://learn.sparkfun.com/tutorials/qwiic-distance-sensor-vl53l1x-hookup-guide/all) with more information about the functions used in the code

[MQTT with ESP32 tutorial](https://iotdesignpro.com/projects/how-to-connect-esp32-mqtt-broker)

[This article](https://lastminuteengineers.com/esp32-ota-updates-arduino-ide/) gives additional information about the OTA updates in Arduino using the Arduino IDE.
