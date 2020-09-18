# Integration with the Home Assistant

In order to integrate this sensor inside your Home Assistant setup, the following chunks of code need to be added to the appropriate files.

**We will have to define an input number**, that simply will count the number of people inside the room. It will be called `input_number.people_in_the_room`



#### configuration.yaml

Inside *configuration.yaml*, we need to add the following code:

```yaml
sensor:
	- platform: mqtt
      name: "People counter"
      state_topic: "peopleCounter/serialdata/tx"
```

The state topic must contain whatever is saved as `define_mqtt_serial_publish_ch` in the file `peopleCounter.ino`.

This chunk will allow us to read a value given by the sensor, which will be called `sensor.people_counter` . Whenever one person moves across the ROI the sensor will output for a small amount of time either 1 or 2 (depending on the direction of the movement). 



#### automations.yaml

We now need to define two automations that will allow us to understand when someone is getting in or out of the room. This first one will take account of the first case.

```yaml
alias: 'Person entering the room'
trigger:
- entity_id: sensor.people_counter
  from: '0'
  platform: state
  to: '1'
action:
    - data: {}
      entity_id: input_number.people_in_the_room
      service: input_number.increment
mode: single

```

The following automation will take account of the case of someone exiting the room:

```yaml
alias: 'Person exiting the room'
description: ''
trigger:
- entity_id: sensor.people_counter
  from: '0'
  platform: state
  to: '2'
condition: []
action:
- data: {}
  entity_id: input_number.people_in_the_room
  service: input_number.decrement
mode: single

```



Finally, this automation will turn on a light when there is at least one person in the room:

```yaml
alias: 'Turn on the light'
trigger:
- entity_id: input_number.people_in_the_room
  above: '0'
  platform: numeric_state
action:
- data: {}
  entity_id: switch.light_in_the_room
  service: switch.turn_on
mode: single
```

and this one turns the light off when the last person gets out of the room:

```yaml
alias: 'Turn the light off'
description: ''
trigger:
- entity_id: input_number.people_in_the_room
  below: '1'
  platform: numeric_state
condition: []
action:
- data: {}
  entity_id: switch.light_in_the_room
  service: switch.turn_off
mode: single

```

