/* For Ardumower PCB 1.3 and Arduino Due

*/

#define IOREF 3.3   // I/O reference voltage 


// ------ pins---------------------------------------
#define pinMotorEnable  37         // EN motors enable
#define pinMotorLeftPWM 5          // M1_IN1 left motor PWM pin
#define pinMotorLeftDir 31         // M1_IN2 left motor Dir pin
#define pinMotorLeftSense A1       // M1_FB  left motor current sense
#define pinMotorLeftFault 25       // M1_SF  left motor fault
                                                             
#define pinMotorRightPWM  3        // M2_IN1 right motor PWM pin
#define pinMotorRightDir 33        // M2_IN2 right motor Dir pin
#define pinMotorRightSense A0      // M2_FB  right motor current sense
#define pinMotorRightFault 27      // M2_SF  right motor fault
                                    
#define pinMotorMowPWM 2           // M1_IN1 mower motor PWM pin (if using MOSFET, use this pin)
#define pinMotorMowDir 29          // M1_IN2 mower motor Dir pin (if using MOSFET, keep unconnected)
#define pinMotorMowSense A3        // M1_FB  mower motor current sense  
#define pinMotorMowFault 26        // M1_SF  mower motor fault   (if using MOSFET/L298N, keep unconnected)
#define pinMotorMowEnable 28       // EN mower motor enable      (if using MOSFET/L298N, keep unconnected)
#define pinMotorMowRpm A11
    
#define pinBumperLeft 39           // bumper pins
#define pinBumperRight 38

#define pinDropLeft 45           // drop pins                                                                                          Dropsensor - Absturzsensor
#define pinDropRight 23          // drop pins                                                                                          Dropsensor - Absturzsensor

#define pinSonarCenterTrigger 24   // ultrasonic sensor pins
#define pinSonarCenterEcho 22
#define pinSonarRightTrigger 30    
#define pinSonarRightEcho 32
#define pinSonarLeftTrigger 34         
#define pinSonarLeftEcho 36
#define pinPerimeterRight A4       // perimeter
#define pinPerimeterLeft A5

#define pinLED 13                  // LED
#define pinBuzzer 53               // Buzzer
#define pinTilt 35                 // Tilt sensor (required for TC-G158 board)
#define pinButton 51               // digital ON/OFF button
#define pinBatteryVoltage A2       // battery voltage sensor
#define pinBatterySwitch 4         // battery-OFF switch   
#define pinChargeVoltage A9        // charging voltage sensor
#define pinChargeCurrent A8        // charge current sensor
#define pinChargeRelay 50          // charge relay
#define pinRemoteMow 12            // remote control mower motor
#define pinRemoteSteer 11          // remote control steering 
#define pinRemoteSpeed 10          // remote control speed
#define pinRemoteSwitch 52         // remote control switch
#define pinVoltageMeasurement A7   // test pin for your own voltage measurements
#ifdef __AVR__
  #define pinOdometryLeft A12      // left odometry sensor
  #define pinOdometryLeft2 A13     // left odometry sensor (optional two-wire)
  #define pinOdometryRight A14     // right odometry sensor 
  #define pinOdometryRight2 A15    // right odometry sensor (optional two-wire)  
#else
  #define pinOdometryLeft DAC0     // left odometry sensor
  #define pinOdometryLeft2 DAC1    // left odometry sensor (optional two-wire)
  #define pinOdometryRight CANRX   // right odometry sensor  
  #define pinOdometryRight2 CANTX  // right odometry sensor (optional two-wire)  
#endif
#define pinLawnFrontRecv 40        // lawn sensor front receive
#define pinLawnFrontSend 41        // lawn sensor front sender 
#define pinLawnBackRecv 42         // lawn sensor back receive
#define pinLawnBackSend 43         // lawn sensor back sender 
#define pinUserSwitch1 46          // user-defined switch 1
#define pinUserSwitch2 47          // user-defined switch 2
#define pinUserSwitch3 48          // user-defined switch 3
#define pinRain 44                 // rain sensor

// IMU (compass/gyro/accel): I2C  (SCL, SDA) 
// Bluetooth: Serial2 (TX2, RX2)
// GPS: Serial3 (TX3, RX3) 


// ------- baudrates---------------------------------
#define ROBOTMSG_BAUDRATE    115200       // baudrate used for console
//#define ROBOTMSG_BAUDRATE    921600  // baudrate used for console
#define PFODAPP_BAUDRATE  19200 
#define RANGING_BAUDRATE  115200       // baudrate for ranging module (DW1000)
#define BLUETOOTH_PIN       1234


#define ROBOTMSG Serial
#define PFODAPP Serial2
#define BLUETOOTH Serial2
#define RANGING Serial3

#define DEBUG(x) ROBOTMSG.print(x)
#define DEBUGLN(x) ROBOTMSG.println(x)
