# ESP-01(ESP8266) LED string controller 

Code for LED controller circuit based on ESP-01(ESP8266) 
## Circuit

![image](https://github.com/ZahariaLevy/ESP01LEDController/assets/1616964/d7797884-2a89-4ecb-bb97-0bf0533daff5)
![image](https://github.com/ZahariaLevy/ESP01LEDController/assets/1616964/e433a132-afdc-4513-8e0d-d5ec1d725e91)

## Basic functionality
Connects to WiFi and retrieves useful data such as location, timezone, sunset and sunrise time
Initialized current time via internet
Turns 3.3v outputs on and off based on specified rules, such as: 
- turns light on in dim mode after `sunset`
- turns ligts on full after `last light` time
- changes light mode to dim after specified time
- turns light off after `sunrise`
Disabled WiFi in between light changing events
