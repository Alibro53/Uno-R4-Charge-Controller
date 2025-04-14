Ver 2.5
Updated to use the onboard RTC in the Arduino Uno R4 WiFi due to intermittent loss of time and date when using the DS3231 RTC module.
Relays turn off in reverse sequence 
Clock is not very accurate so this may need updating.

Ver 2.4
Improved layout in BT Terminal with spaces between updates and updated every 5 seconds instead of 10.

Ver 2.3
Rewritten for the Arduino R4 Wifi to send the serial monitor information and control via Bluetooth.
Added Option of typing "emergency" in the BT terminal to turn off all relays immediately.


Ver 1.11
This works the same as Ver 10 but an issue with the relays staying on when CP_Present is disconnected is now fixed.


Ver 1.10
All relays come on in sequence only when timer or override triggers them and wait for 8 seconds for CP to connect
If CP does not connect all relays turn off again.
CP does not need to be connected for the charging to start but disconnecting CP or EVSE will turn off all relays.


Ver 1.9
Relays 7 and 8 come on for several seconds to allow Charging (CP) Due to boot up and close the CP relay. If it doesn't close withing set time they turn off again.

Ver 1.8 - 
CPPresent is not required before turning on the relays but loosing will turn them off.


Ver 1.7
Relays 7 and 8 turn off if EVSE is disconnected.

Written by DeepSeek rather than ChatGPT which failed miserably, this has been changed to use a 2 way relay module instead of the MOSFET. It is now working as intended but obviously will need to be monitored for a while.


The circuit board with the LM393 comparator IC detects PP when the EVSE is plugged in and connects GND to a relay. 
The relay sends GND to the Nano that controls charging. 
This ensures the Nano can gracefully power down everything in the correct sequence even if the charger plug is pulled as the last relay to power off will be latching power to the Nano. 
The GND being sent to the Nano from the PP Relay when activated will be sent to the throttle pedal when it is not activated. 
This will prevent the car from being driven if the cable is plugged in.


Connections
Pins 2 to 7 go to the 6 way module
Pins 8 and 9 go to the 2 way module
Pin 10 monitors EVSE Present
Pin 11 monitors CP Present
Pin 12 monitors the manual over ride button


The Nano gets time from RTC and switches relays on then off again at set times.

Commands:
  start HH:MM    - Set relay start time
  end HH:MM      - Set relay end time
  set YYYY-MM-DD HH:MM:SS - Set RTC time
  status         - Show current status
  help           - Show help message

Manual (Orange wire) start requires momentary switch to GND
CP detect (White wire) overides all and needs to be grounded for the relays to activate.

The Arduino Due that sends Voltage, Current and Power to the Android head unit controls charge end and will override by removing the CP when full charge is reached.

The VCU is an emergency safety overide as it will remove power from the charger if voltage exceeds 400V

My thoughts on how this should work. are in the Sequence file


The critical thing both now and when this is implemented is that the battery should never be allowed to charge above 400V.
The Due I use to control this gets it's voltage from the shunt and the VCU which is a fail safe gets it's voltage from the Inverter so hopefully I've mitigated as best I can.
