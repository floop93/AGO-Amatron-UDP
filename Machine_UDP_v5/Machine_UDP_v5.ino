    //Machine Control - Brian Tee - Cut and paste from everywhere


    //-----------------------------------------------------------------------------------------------
    // Change this number to reset and reload default parameters To EEPROM
    #define EEP_Ident 0x5425  
    
    //the default network address
    struct ConfigIP {
        uint8_t ipOne = 192;
        uint8_t ipTwo = 168;
        uint8_t ipThree = 5;
    };  ConfigIP networkAddress;   //3 bytes
    //-----------------------------------------------------------------------------------------------

    #include <EEPROM.h> 
    #include <Wire.h>
    #include "EtherCard_AOG.h"
    #include <IPAddress.h>
    #include <SPI.h>
    #include "mcp2515_can.h" 

    // Set SPI CS Pin according to your hardware !!
    // Arduino MCP2515 Hat: the cs pin of the version after v1.1 is default to D9 //v0.9b and v1.0 is default D10
    const int SPI_CS_PIN = 9;   //ATTENTION !! PIN9 is also used already in Machine USB but is PIN9 or 10 ar needed for CAN Shield. Therefore i replaced the PIN9 in the existing Machine code
    mcp2515_can CAN(SPI_CS_PIN); // Set CS pin
    
    // Set Amatron Claim adress to send data
    byte AmaClick_addressClaim[8] = {0x28, 0xEC, 0x44, 0x0C, 0x00, 0x80, 0x1A, 0x20};
    //set Amatron message to change Sections
    uint8_t AmaClick_data[8] = {0x21, 0xFA, 0x00, 0x00, 0x00, 0x00, 0x01, 0x09};
    byte AmaClick_data_byte2 = 0b00000000;
    byte AmaClick_data_byte3 = 0b00000000;  
    //----------------------

    // ethernet interface ip address
    static uint8_t myip[] = { 0,0,0,123 };

    // gateway ip address
    static uint8_t gwip[] = { 0,0,0,1 };

    //DNS- you just need one anyway
    static uint8_t myDNS[] = { 8,8,8,8 };

    //mask
    static uint8_t mask[] = { 255,255,255,0 };

    //this is port of this autosteer module
    uint16_t portMy = 5123;

    //sending back to where and which port
    static uint8_t ipDestination[] = { 0,0,0,255 };
    uint16_t portDestination = 9999; //AOG port that listens

    // ethernet mac address - must be unique on your network
    static uint8_t mymac[] = { 0x00,0x00,0x56,0x00,0x00,0x7B };

    uint8_t Ethernet::buffer[200]; // udp send and receive buffer

    //Variables for config - 0 is false  
    struct Config {
        uint8_t raiseTime = 2;
        uint8_t lowerTime = 4;
        uint8_t enableToolLift = 0;
        uint8_t isRelayActiveHigh = 0; //if zero, active low (default)

        uint8_t user1 = 0; //user defined values set in machine tab
        uint8_t user2 = 0;
        uint8_t user3 = 0;
        uint8_t user4 = 0;

    };  Config aogConfig;   //4 bytes

    //Program counter reset
    void(*resetFunc) (void) = 0;

    //ethercard 10,11,12,13 Nano = 10 depending how CS of ENC28J60 is Connected
    #define CS_Pin 10

    /*
    * Functions as below assigned to pins
    0: -
    1 thru 16: Section 1,Section 2,Section 3,Section 4,Section 5,Section 6,Section 7,Section 8,
                Section 9, Section 10, Section 11, Section 12, Section 13, Section 14, Section 15, Section 16,
    17,18    Hyd Up, Hyd Down,
    19 Tramline,
    20: Geo Stop
    21,22,23 - unused so far*/    
    uint8_t pin[] = { 1,2,3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };

    //read value from Machine data and set 1 or zero according to list
    uint8_t relayState[] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };

    //hello from AgIO
    uint8_t helloFromMachine[] = { 128, 129, 123, 123, 5, 0, 0, 0, 0, 0, 71 };

    const uint8_t LOOP_TIME = 200; //5hz
    uint32_t lastTime = LOOP_TIME;
    uint32_t currentTime = LOOP_TIME;
    uint32_t fifthTime = 0;
    uint16_t count = 0;

    //Comm checks
    uint8_t watchdogTimer = 20; //make sure we are talking to AOG
    uint8_t serialResetTimer = 0; //if serial buffer is getting full, empty it

    bool isRaise = false, isLower = false;

    //Communication with AgOpenGPS
    int16_t temp, EEread = 0;

    //Parsing PGN
    bool isPGNFound = false, isHeaderFound = false;
    uint8_t pgn = 0, dataLength = 0, idx = 0;
    int16_t tempHeader = 0;

    //settings pgn
    uint8_t PGN_237[] = { 0x80,0x81, 0x7f, 237, 8, 1, 2, 3, 4, 0,0,0,0, 0xCC };
    int8_t PGN_237_Size = sizeof(PGN_237) - 1;

    //The variables used for storage
    uint8_t relayHi = 0, relayLo = 0, tramline = 0, uTurn = 0, hydLift = 0, geoStop = 0;
    float gpsSpeed;
    uint8_t raiseTimer = 0, lowerTimer = 0, lastTrigger = 0;
 

    void setup()
    {

        //set the baud rate
        Serial.begin(38400);
        //while (!Serial) { ; } // wait for serial port to connect. Needed for native USB

        EEPROM.get(0, EEread);              // read identifier

        if (EEread != EEP_Ident)   // check on first start and write EEPROM
        {
            EEPROM.put(0, EEP_Ident);
            EEPROM.put(6, aogConfig);
            EEPROM.put(20, pin);
            EEPROM.put(50, networkAddress);
        }
        else
        {
            EEPROM.get(6, aogConfig);
            EEPROM.get(20, pin);
            EEPROM.get(50, networkAddress);
        }

        if (ether.begin(sizeof Ethernet::buffer, mymac, CS_Pin) == 0)
            Serial.println(F("Failed to access Ethernet controller"));

        //grab the ip from EEPROM
        myip[0] = networkAddress.ipOne;
        myip[1] = networkAddress.ipTwo;
        myip[2] = networkAddress.ipThree;

        gwip[0] = networkAddress.ipOne;
        gwip[1] = networkAddress.ipTwo;
        gwip[2] = networkAddress.ipThree;

        ipDestination[0] = networkAddress.ipOne;
        ipDestination[1] = networkAddress.ipTwo;
        ipDestination[2] = networkAddress.ipThree;

        //set up connection
        ether.staticSetup(myip, gwip, myDNS, mask);
        ether.printIp("_IP_: ", ether.myip);
        ether.printIp("GWay: ", ether.gwip);
        ether.printIp("AgIO: ", ipDestination);

        //register to port 8888
        ether.udpServerListenOnPort(&udpSteerRecv, 8888);

        //set the pins to be outputs (pin numbers)
        //pinMode(0, OUTPUT);
        //pinMode(1, OUTPUT);
        //pinMode(2, OUTPUT);
        //pinMode(3, OUTPUT);
        //pinMode(4, OUTPUT);
        //pinMode(5, OUTPUT);
        //pinMode(6, OUTPUT);
        //pinMode(7, OUTPUT);
        //pinMode(8, OUTPUT);
        //pinMode(9, OUTPUT);

        //pinMode(A0, OUTPUT);
        //pinMode(A1, OUTPUT);
        //pinMode(A2, OUTPUT);
        //pinMode(A3, OUTPUT);
        //pinMode(A4, OUTPUT);
        //pinMode(A5, OUTPUT);

        //------------added because of CAN Amatron-------

        
        while (CAN_OK != CAN.begin(CAN_500KBPS)) {        //init can bus: Amatron baudrate = 250k //was CAN_250KBPS in reality 125
            Serial.println("CAN init fail, retry...");
            delay(100);
        }
        Serial.println("CAN init ok!");

        //set CAN Filter and Mask to look for sending data from AMACLICK (if AMACLICK is activ and sending we don´t want to send CAN)
        //Therfore you can chose either use manually AMACLICK or automatic Section Control
        CAN.init_Mask(0,1,0xFFFFFFFF);        // Init first mask... allow all Bits to filtered
        CAN.init_Filt(0,1,0x18E6FFCE);       // Init first filter... check for specific sections ID --> 0x18E6FFCE is AMACLICK Messages ID
        CAN.init_Mask(1,0,0xFFFFFFFF);       // Init second mask... second mask has to be set otherwise it will not work
        
        //send first Claim Adress to Amatron to send future data
        CAN.sendMsgBuf(0x18EEFFCE, 1, 8,AmaClick_addressClaim); //input from Valentin
        //Serial.println("Address Claimed");  
        //-----------------------------------------------
        Serial.println("Setup complete, waiting for AgOpenGPS");

    }

    void loop()
    {
        //Loop triggers every 200 msec and sends back gyro heading, and roll, steer angle etc

        currentTime = millis();

        if (currentTime - lastTime >= LOOP_TIME)
        {
            lastTime = currentTime;

            //If connection lost to AgOpenGPS, the watchdog will count up 
            if (watchdogTimer++ > 250) watchdogTimer = 20;

            //clean out serial buffer to prevent buffer overflow
            if (serialResetTimer++ > 20)
            {
                while (Serial.available() > 0) Serial.read();
                serialResetTimer = 0;
            }

            if (watchdogTimer > 20)
            {
                if (aogConfig.isRelayActiveHigh) {
                    relayLo = 255;
                    relayHi = 255;
                }
                else {
                    relayLo = 0;
                    relayHi = 0;
                }
            }

            //hydraulic lift

            if (hydLift != lastTrigger && (hydLift == 1 || hydLift == 2))
            {
                lastTrigger = hydLift;
                lowerTimer = 0;
                raiseTimer = 0;

                //200 msec per frame so 5 per second
                switch (hydLift)
                {
                    //lower
                case 1:
                    lowerTimer = aogConfig.lowerTime;
                    break;

                    //raise
                case 2:
                    raiseTimer = aogConfig.raiseTime * 5;
                    break;
                }
            }

            //countdown if not zero, make sure up only
            if (raiseTimer)
            {
                raiseTimer--;
                lowerTimer = 0;
            }
            if (lowerTimer) lowerTimer--;

            //if anything wrong, shut off hydraulics, reset last
            if ((hydLift != 1 && hydLift != 2) || watchdogTimer > 10) //|| gpsSpeed < 2)
            {
                lowerTimer = 0;
                raiseTimer = 0;
                lastTrigger = 0;
            }

            if (aogConfig.isRelayActiveHigh)
            {
                isLower = isRaise = false;
                if (lowerTimer) isLower = true;
                if (raiseTimer) isRaise = true;
            }
            else
            {
                isLower = isRaise = true;
                if (lowerTimer) isLower = false;
                if (raiseTimer) isRaise = false;
            }

            //section relays
            SetRelays();

            //i dont know why but it was on working wersion based on USB
            /*AOG[5] = pin[0];
            AOG[6] = pin[1];
            AOG[7] = (uint8_t)tramline;*/

            //checksum
            int16_t CK_A = 0;
            for (uint8_t i = 2; i < PGN_237_Size; i++)
            {
                CK_A = (CK_A + PGN_237[i]);
            }
            PGN_237[PGN_237_Size] = CK_A;

            //off to AOG
            ether.sendUdp(PGN_237, sizeof(PGN_237), portMy, ipDestination, portDestination);

        } //end of timed loop

        delay(1);

        //this must be called for ethercard functions to work. Calls udpSteerRecv() defined way below.
        ether.packetLoop(ether.packetReceive());
    }

  //callback when received packets
    void udpSteerRecv(uint16_t dest_port, uint8_t src_ip[IP_LEN], uint16_t src_port, uint8_t* udpData, uint16_t len)
    {
        /* IPAddress src(src_ip[0],src_ip[1],src_ip[2],src_ip[3]);
        Serial.print("dPort:");  Serial.print(dest_port);
        Serial.print("  sPort: ");  Serial.print(src_port);
        Serial.print("  sIP: ");  ether.printIp(src_ip);  Serial.println("  end");

        //for (int16_t i = 0; i < len; i++) {
        //Serial.print(udpData[i],HEX); Serial.print("\t"); } Serial.println(len);
        */

        if (udpData[0] == 0x80 && udpData[1] == 0x81 && udpData[2] == 0x7F) //Data
        {

            if (udpData[3] == 239)  //machine data
            {
                uTurn = udpData[5];
                gpsSpeed = (float)udpData[6];//actual speed times 4, single uint8_t

                hydLift = udpData[7];
                tramline = udpData[8];  //bit 0 is right bit 1 is left

                relayLo = udpData[11];          // read relay control from AgOpenGPS
                relayHi = udpData[12];

                if (aogConfig.isRelayActiveHigh)
                {
                    tramline = 255 - tramline;
                    relayLo = 255 - relayLo;
                    relayHi = 255 - relayHi;
                }

                //Bit 13 CRC

                //reset watchdog
                watchdogTimer = 0;
            }

            else if (udpData[3] == 200) // Hello from AgIO
            {
                if (udpData[7] == 1)
                {
                    relayLo -= 255;
                    relayHi -= 255;
                    watchdogTimer = 0;
                }

                helloFromMachine[5] = relayLo;
                helloFromMachine[6] = relayHi;

                ether.sendUdp(helloFromMachine, sizeof(helloFromMachine), portMy, ipDestination, portDestination);
            }


            else if (udpData[3] == 238)
            {
                aogConfig.raiseTime = udpData[5];
                aogConfig.lowerTime = udpData[6];
                aogConfig.enableToolLift = udpData[7];

                //set1 
                uint8_t sett = udpData[8];  //setting0     
                if (bitRead(sett, 0)) aogConfig.isRelayActiveHigh = 1; else aogConfig.isRelayActiveHigh = 0;

                aogConfig.user1 = udpData[9];
                aogConfig.user2 = udpData[10];
                aogConfig.user3 = udpData[11];
                aogConfig.user4 = udpData[12];

                //crc

                //save in EEPROM and restart
                EEPROM.put(6, aogConfig);
                //resetFunc();
            }

            else if (udpData[3] == 201)
            {
                //make really sure this is the subnet pgn
                if (udpData[4] == 5 && udpData[5] == 201 && udpData[6] == 201)
                {
                    networkAddress.ipOne = udpData[7];
                    networkAddress.ipTwo = udpData[8];
                    networkAddress.ipThree = udpData[9];

                    //save in EEPROM and restart
                    EEPROM.put(50, networkAddress);
                    resetFunc();
                }
            }

            //Scan Reply
            else if (udpData[3] == 202)
            {
                //make really sure this is the subnet pgn
                if (udpData[4] == 3 && udpData[5] == 202 && udpData[6] == 202)
                {
                    uint8_t scanReply[] = { 128, 129, 123, 203, 7, 
                        networkAddress.ipOne, networkAddress.ipTwo, networkAddress.ipThree, 123,
                        src_ip[0], src_ip[1], src_ip[2], 23   };

                    //checksum
                    int16_t CK_A = 0;
                    for (uint8_t i = 2; i < sizeof(scanReply) - 1; i++)
                    {
                        CK_A = (CK_A + scanReply[i]);
                    }
                    scanReply[sizeof(scanReply)-1] = CK_A;

                    static uint8_t ipDest[] = { 255,255,255,255 };
                    uint16_t portDest = 9999; //AOG port that listens

                    //off to AOG
                    ether.sendUdp(scanReply, sizeof(scanReply), portMy, ipDest, portDest);
                }
            }

            else if (udpData[3] == 236) //EC Relay Pin Settings 
            {
                for (uint8_t i = 0; i < 24; i++)
                {
                    pin[i] = udpData[i + 5];
                }

                //save in EEPROM and restart
                EEPROM.put(20, pin);
            }
        }
    }

    void SetRelays(void)
    {
        //pin, rate, duration  130 pp meter, 3.6 kmh = 1 m/sec or gpsSpeed * 130/3.6 or gpsSpeed * 36.1111
        //gpsSpeed is 10x actual speed so 3.61111
        gpsSpeed *= 3.61111;
        //tone(13, gpsSpeed);

        //Load the current pgn relay state - Sections
        for (uint8_t i = 0; i < 8; i++)
        {
            relayState[i] = bitRead(relayLo, i);
        }

        for (uint8_t i = 0; i < 8; i++)
        {
            relayState[i + 8] = bitRead(relayHi, i);
        }

        // Hydraulics
        relayState[16] = isLower;
        relayState[17] = isRaise;

        //Tram
        relayState[18] = bitRead(tramline, 0); //right
        relayState[19] = bitRead(tramline, 1); //left

        //GeoStop
        relayState[20] = (geoStop == 0) ? 0 : 1;

        //if (pin[0]) digitalWrite(2, relayState[pin[0] - 1]);
        //if (pin[1]) digitalWrite(3, relayState[pin[1] - 1]);
        //if (pin[2]) digitalWrite(4, relayState[pin[2] - 1]);
        //if (pin[3]) digitalWrite(5, relayState[pin[3] - 1]);
        //if (pin[4]) digitalWrite(6, relayState[pin[4] - 1]);
        //if (pin[5]) digitalWrite(7, relayState[pin[5] - 1]);
        //if (pin[6]) digitalWrite(8, relayState[pin[6]-1]);
        //if (pin[7]) digitalWrite(9, relayState[pin[7]-1]);

        //if (pin[8]) digitalWrite(A0, relayState[pin[8]-1]);
        //if (pin[9]) digitalWrite(A1, relayState[pin[9]-1]);
        //if (pin[10]) digitalWrite(A2, relayState[pin[10]-1]);
        //if (pin[11]) digitalWrite(A3, relayState[pin[11]-1]);
        //if (pin[12]) digitalWrite(A4, relayState[pin[12]-1]);
        //if (pin[13]) digitalWrite(A5, relayState[pin[13]-1]);

        //if (pin[14]) digitalWrite(0, relayState[pin[14]-1]);
        //if (pin[15]) digitalWrite(1, relayState[pin[15]-1]);
        
        //if (pin[16]) digitalWrite(IO#Here, relayState[pin[16]-1]);
        //if (pin[17]) digitalWrite(IO#Here, relayState[pin[17]-1]);
        //if (pin[18]) digitalWrite(IO#Here, relayState[pin[18]-1]);
        //if (pin[19]) digitalWrite(IO#Here, relayState[pin[19]-1]);

        //------------added because of CAN Amatron-------------------------------------
      //define data for message to send to control Amatron sections  
      //See ExcelFile or als Valentin for mor details according to Send Message
      
      //check if minimum one of the section relay is on/activ -> Main switch in Amatron on/off
      //16 relays are defined in relay[0-15]
      int isOnerelayPositiv = 0;
      for (uint8_t i = 0; i < 16; i++)
      {
          isOnerelayPositiv = isOnerelayPositiv + relayState[i];
      }
      if (isOnerelayPositiv != 0) bitWrite(AmaClick_data_byte3, 7, 1); //byte 3, bit 7 defines MainSwitch
      if (isOnerelayPositiv == 0) bitWrite(AmaClick_data_byte3, 7, 0);
      
      //write relayState into CAN message
      if (relayState[0]==HIGH) bitWrite(AmaClick_data_byte2, 0, 1);   // byte 2 and part of byte3 defines sections
      if (relayState[1]==HIGH) bitWrite(AmaClick_data_byte2, 1, 1);
      if (relayState[2]==HIGH) bitWrite(AmaClick_data_byte2, 2, 1);
      if (relayState[3]==HIGH) bitWrite(AmaClick_data_byte2, 3, 1);
      if (relayState[4]==HIGH) bitWrite(AmaClick_data_byte2, 4, 1);
      if (relayState[5]==HIGH) bitWrite(AmaClick_data_byte2, 5, 1);
      if (relayState[6]==HIGH) bitWrite(AmaClick_data_byte2, 6, 1);
      if (relayState[7]==HIGH) bitWrite(AmaClick_data_byte2, 7, 1);
      if (relayState[8]==HIGH) bitWrite(AmaClick_data_byte3, 0, 1);
      if (relayState[9]==HIGH) bitWrite(AmaClick_data_byte3, 1, 1);
      if (relayState[10]==HIGH) bitWrite(AmaClick_data_byte3, 2, 1);
      if (relayState[11]==HIGH) bitWrite(AmaClick_data_byte3, 3, 1);
      if (relayState[12]==HIGH) bitWrite(AmaClick_data_byte3, 4, 1);
      //AMATRON supports max 13 sections
      
      if (relayState[0]==LOW) bitWrite(AmaClick_data_byte2, 0, 0);
      if (relayState[1]==LOW) bitWrite(AmaClick_data_byte2, 1, 0);
      if (relayState[2]==LOW) bitWrite(AmaClick_data_byte2, 2, 0);
      if (relayState[3]==LOW) bitWrite(AmaClick_data_byte2, 3, 0);
      if (relayState[4]==LOW) bitWrite(AmaClick_data_byte2, 4, 0);
      if (relayState[5]==LOW) bitWrite(AmaClick_data_byte2, 5, 0);
      if (relayState[6]==LOW) bitWrite(AmaClick_data_byte2, 6, 0);
      if (relayState[7]==LOW) bitWrite(AmaClick_data_byte2, 7, 0);
      if (relayState[8]==LOW) bitWrite(AmaClick_data_byte3, 0, 0);
      if (relayState[9]==LOW) bitWrite(AmaClick_data_byte3, 1, 0);
      if (relayState[10]==LOW) bitWrite(AmaClick_data_byte3, 2, 0);
      if (relayState[11]==LOW) bitWrite(AmaClick_data_byte3, 3, 0);
      if (relayState[12]==LOW) bitWrite(AmaClick_data_byte3, 4, 0);

      //assign byte2 and 3 to the CAN message
      AmaClick_data[2]= AmaClick_data_byte2,HEX;
      AmaClick_data[3]= AmaClick_data_byte3,HEX;

      //check if AMACLICK is active if not then send CAN message
      unsigned char len = 0;
      unsigned char buf[8];
      if (watchdogTimer < 10){  //when connection to AGO is lost then send nothing
      CAN.readMsgBuf(&len, buf); //read CAN with Filter and Mask for AMACLICK ID
      if (buf[6]!= 1) CAN.sendMsgBuf(0x18E6FFCE, 1, 8, AmaClick_data);
      }
    }
