9# AgOpenGps section control for Amatron sprayer

Project is based on MCP2515 and ENC28J60 module and arduino nano. 
MCP215 CS pin: D9
ENC28J60 CS pin: D10

AMATRON connection to DB9 socket (Y spliter)

    GND - pin 3
    CAN H - pin 7
    CAN L - pin 2
    +12V always - pin 9  
    +12V on signal pin 8 

It allow to operate 13 section

To avoid using physical switch for power on, use pin 8 to trigger relay (through transistor circuit) to deliver power from pin 9 (i'm not sure if it's safe to use pin 8 as power source)
