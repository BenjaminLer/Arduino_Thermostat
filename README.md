# Arduino_Thermostat
Arduino project for making a thermostat

UART command:
- Configure Time:
0x10 <Hour> <Minutes> <Seconds>
return nothing
  
- Request Temperature:
0x20
Return temperature as a string of character (ex: 17.3)
  
- Request Temperature Threshold:
0x30
Return the temperature threshold as a string of character (ex: 19.7)
