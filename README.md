# AgOpenGps section control for Amatron sprayer

Project is based on MCP2515 and ENC28J60 module and arduino nano. 

    MCP215 CS pin: D9
    ENC28J60 CS pin: D10

AMATRON connection to DB9 socket (Y spliter)

    GND - pin 3
    CAN H - pin 7
    CAN L - pin 2
    +12V always - pin 9  
    +12V on signal pin 8 

It allow to operate 13 section. Based on amaclick module. It can be modified to utilize additional switches (to work as manual switch board and section control).

To avoid using physical switch for power on, use pin 8 to trigger relay (through transistor circuit) to deliver power from pin 9 (i'm not sure if it's safe to use pin 8 as power source)

![rev 2 0 1](https://github.com/user-attachments/assets/2ab7000b-50e2-49dd-8049-f113f91be028)
