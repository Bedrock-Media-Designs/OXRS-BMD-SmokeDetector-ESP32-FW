# ESP32 smoke detector firmware for the Open eXtensible Rack System.

Control output relays and monitor binary inputs all on a single CAT5/6 cable (2x outputs, 1x input per port).

Uses MCP23017 I2C I/O buffers for controlling relays and to detect digital signals being pulled to GND before publishing events to MQTT.

Each port on a UIO can control 2 outputs and monitor 1 input and are numbered (for a 16-port UIO);

|INDEX|PORT|CHANNEL|TYPE  |RJ45 Pin|
|-----|----|-------|------|--------|
|1    |1   |1      |Output|2       |
|2    |1   |2      |Output|3       |
|3    |1   |3      |Input |6       |
|4    |2   |1      |Output|2       |
|5    |2   |2      |Output|3       |
|6    |2   |3      |Input |6       |
|...  |    |       |      |        |
|46   |16  |1      |Output|2       |
|47   |16  |2      |Output|3       |
|48   |16  |3      |Input |6       |


## Inputs
### Configuration
Each INPUT can be configured by publishing an MQTT message to one of these topics;
```
[BASETOPIC/]conf/CLIENTID/[LOCATION/]INDEX/type
[BASETOPIC/]conf/CLIENTID/[LOCATION/]INDEX/invt
```    
where;
- `BASETOPIC`:   Optional base topic if your topic structure requires it
- `CLIENTID`:    Client id of device, defaults to `uio-<MACADDRESS>`
- `LOCATION`:    Optional location if your topic structure requires it
- `INDEX`:       Index of the input to configure (1-based)
    
The message should be;
- `/type`:       One of `button`, `contact`, `switch` or `toggle`
- `/invt`:       Either `off` or `on` (to invert event)
    
A null or empty message will reset the input to;
- `/type`:       `switch`
- `/invt`:       `off` (non-inverted)
    
A retained message will ensure the UIO auto-configures on startup.

**NOTE: inverting a normally-open (NO) button input will result in a constant stream of `hold` events!**

### Events
An input EVENT or output STATE CHANGE is reported to a topic of the form;
```
[BASETOPIC/]stat/CLIENTID/[LOCATION/]INDEX
```
where; 
- `BASETOPIC`:   Optional base topic if your topic structure requires it
- `CLIENTID`:    Client id of device, defaults to `uio-<MACADDRESS>`
- `LOCATION`:    Optional location if your topic structure requires it
- `INDEX`:       Index of the input/output causing the (1-based)

The message is a JSON payload of the form; 
```
{"port":2,"channel":3,"index":7,"type":"contact","event":"open"}
```
where `event` can be one of (depending on type);
- `button`:      `single`, `double`, `triple`, `quad`, `penta`, or `hold`
- `contact`:     `open` or `closed`
- `switch`:      `on` or `off`
- `toggle`:      `toggle`


## Outputs
### Configuration
Each OUTPUT can be configured by publishing an MQTT message to one of these topics;
```
[BASETOPIC/]conf/CLIENTID/[LOCATION/]INDEX/type
[BASETOPIC/]conf/CLIENTID/[LOCATION/]INDEX/lock
[BASETOPIC/]conf/CLIENTID/[LOCATION/]INDEX/time
```    
where;
- `BASETOPIC`:   Optional base topic if your topic structure requires it
- `CLIENTID`:    Client id of device, defaults to `uio-<MACADDRESS>`
- `LOCATION`:    Optional location if your topic structure requires it
- `INDEX`:       Index of the output to configure (1-based)

The message should be;
- `/type`:       One of `motor`, `relay` or `timer`
- `/lock`:       Output index to interlock with (lock the opposite for interlocking both ways)
- `/time`:       Number of seconds an output stays `on` when type set to `timer`
    
A null or empty message will reset the output to;
- `/type`:       `relay`
- `/lock`:       Unlocked
- `/time`:       60 seconds

A retained message will ensure the UIO auto-configures on startup.

The only difference between `MOTOR` and `RELAY` outputs is the interlock delay (if an interlock is configured). 

|Output Type |Interlock delay|
|------------|---------------|
|`motor`     |2000ms         |
|`relay`     |500ms          |

### Commands
Each OUTPUT can be controlled by publishing an MQTT message to the topic;
```
[BASETOPIC/]cmnd/CLIENTID/[LOCATION/]INDEX
```
where;
- `BASETOPIC`:   Optional base topic if your topic structure requires it
- `CLIENTID`:    Client id of device, defaults to `uio-<MACADDRESS>`
- `LOCATION`:    Optional location if your topic structure requires it
- `INDEX`:       Index of the output to update (1-based)
    
The message should be;
- `0` or `off`:  Turn output OFF (deactivate the relay)
- `1` or `on`:   Turn output ON (activate the relay)
  
A null or empty message will cause the current output state to be published. I.e. can be used to query the current state.

### Events
An output STATE CHANGE is reported to a topic of the form;
```
[BASETOPIC/]stat/CLIENTID/[LOCATION/]INDEX
```
where; 
- `BASETOPIC`:   Optional base topic if your topic structure requires it
- `CLIENTID`:    Client id of device, defaults to `uio-<MACADDRESS>`
- `LOCATION`:    Optional location if your topic structure requires it
- `INDEX`:       Index of the output causing the event (1-based)

The message is a JSON payload of the form; 
```
{"port":2,"channel":2,"index":5,"type":"relay","event":"on"}
```
where;
- `type`:        One of `motor`, `relay` or `timer`
- `event`:       One of `on` or `off`

### Interlocking
Interlocking two outputs allows them to control equipment such as roller blinds, garage doors, louvre roofing etc.

However if you are planning to control a motor of any sort then it is important that the two outputs are configured as type `motor` and that both are interlocked with each other. This is to ensure that both outputs will not be commanded to operate at the same time and adds a 2 second delay between any changes of output.

Example configuration if using outputs 4 & 5 to control a set of roller blinds;
```
conf/usc-abcdef/4/type motor
conf/usc-abcdef/5/type motor
conf/usc-abcdef/4/lock 5
conf/usc-abcdef/5/lock 4
```

The operation of the interlocked outputs should be verified before connecting to any external equipment. External interlocking equipment may be required for some equipment. Most importantly, the manufacturers wiring and installation guides must be adhered to.


## Hardware
This firmware is compatible with the [Univeral I/O Controller](https://bmdesigns.com.au/shop/universal-input-output-uio/) and is designed to run on the [Universal Rack Controller](https://www.superhouse.tv/product/rack32-universal-rack-controller-board/) (URC).

The UIO hardware provides 12V down the line, which can be used for powering sensors (e.g. PIRs), or illuminating LEDs. 

There are 2 relays for each port which connect the OUTPUT wires in the cable to a shared CMN. 

A sensor or switch should pull the INPUT wire in the cable to GND to indicate that it has been activated. 

The UIO hardware has physical pull up resistors to pull the INPUT wires high and detects when they are pulled low.

The RJ45 pinout for each port is;

|PIN|DESCRIPTION|
|---|-----------|
|1  |OUTPUT CMN |
|2  |OUTPUT 1   |
|3  |OUTPUT 2   |
|4  |VIN        |
|5  |VIN        |
|6  |INPUT 1    |
|7  |GND        |
|8  |GND        |

More information:

 * https://wiki.bmdesigns.com.au/en/BMD-urc-uio
 * http://www.superhouse.tv/urc

## Credits
 * Jonathan Oxer <jon@oxer.com.au>
 * James Kennewell <James@bmdesigns.com.au>
 * Ben Jones <https://github.com/sumnerboy12>
 * Moin <https://github.com/moinmoin-sh>


## License
Copyright 2020-2021 SuperHouse Automation Pty Ltd  www.superhouse.tv  

The software portion of this project is licensed under the Simplified
BSD License. The "licence" folder within this project contains a
copy of this license in plain text format.
