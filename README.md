# Arduino_Thermostat
Arduino project for making a thermostat

UART command:
- Configure Time:<br>
Send to the arduino: 0x10 [Hour] [Minutes] [Seconds]<br>
Receive from the Arduino: nothing<br>
  
- Request Temperature:<br>
Send to the arduino: 0x20<br>
Receive from the Arduino: The temperature as a string of character (ex: 17.3)<br>
  
- Request Temperature Threshold:<br>
Send to the arduino: 0x30<br>
Receive from the Arduino: The temperature threshold as a string of character (ex: 19.7)<br>
