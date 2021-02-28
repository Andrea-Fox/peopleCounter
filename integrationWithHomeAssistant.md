# Integration with the Home Assistant
These example of automations are based on the topics defined in the file with automatic calibration called either `people_counter_esp32.ino` or `people_counter_esp8266`. If you are using the file in the `manual calibration` folder, you will have to change the topics.

In order to integrate this sensor inside your Home Assistant setup, the following chunks of code need to be added to the appropriate files.

**We will have to define an input number**, that simply will count the number of people inside the room. It will be called `input_number.people_in_the_room`

The parameter DEVICENAME has to be set in the code, at the beginning of it.

#### automations.yaml

We now need to define two automations that will allow us to understand when someone is getting in or out of the room. This first one will take account of the first case.

```yaml
- alias: 'Person entering the room'
  description: ''
  id: 'person_entering_the_room' # unique id here
  trigger:
     - platform: mqtt
       topic: "people_counter/DEVICENAME/counter"
       payload: '1'
  action:
     - service: input_number.increment
       data: 
            entity_id: input_number.people_in_the_room
  mode: single

```

The following automation will take account of the case of someone exiting the room:

```yaml
- alias: 'Person exiting the room'
  description: ''
  id: 'person_exiting_the_room' # unique id here
  trigger:
     - platform: mqtt
       topic: "people_counter/DEVICENAME/counter"
       payload: '2'
  action:
     - service: input_number.decrement
       data: 
            entity_id: input_number.people_in_the_room
  mode: single

```

If you are using a version of HA >=113.0, you can use only the followning automation (suggested by @noxhirsch) to handle the number of people in the room:
```yaml
alias: Person Counter
trigger:
  - platform: mqtt
    topic: people_counter/DEVICENAME/counter
action:
  - choose:
      - conditions:
          - condition: template
            value_template: '{{ trigger.payload == "1" }}'
        sequence:
          - service: input_number.increment
            entity_id: input_number.people_counter
      - conditions:
          - condition: template
            value_template: '{{ trigger.payload == "2" }}'
        sequence:
          - service: input_number.decrement
            entity_id: input_number.people_counter
mode: single
```

Finally, this automation will turn on a light when there is at least one person in the room:

```yaml
- alias: 'Turn on the light'
  description: ''
  id: 'turn_on_the_light' # unique id here
  trigger:
    - entity_id: input_number.people_in_the_room
      above: '0'
      platform: numeric_state
  action:
    - service: switch.turn_on
      entity_id: switch.light_in_the_room
  mode: single
```

and this one turns the light off when the last person gets out of the room:

```yaml
- alias: 'Turn on the light'
  description: ''
  id: 'turn_on_the_light' # unique id here
  trigger:
    - entity_id: input_number.people_in_the_room
      below: '1'
      platform: numeric_state
  action:
    - service: switch.turn_off
      entity_id: switch.light_in_the_room
  mode: single

```


One might also define an automation for recalibrating the thresholds every day at a certain time (let's say 4 am):
```yaml
- alias: 'Recalibrate threshold'
  description: ''
  id: 'recalibrate_threshold' # unique id here
  trigger:
    - at: '04:00:00'
      platform: time
  action:
    - service: mqtt.publish
      data:
        topic: people_counter/DEVICENAME/receiver
        payload_template: "new_threshold"
  mode: single

```
