#include "sonar.h"
#include "config.h"
#include "motor.h"
#include "buzzer.h"
#include "robot.h"
#include "RunningMedian.h"
#include <Arduino.h>

SonarClass Sonar;

#define MAX_DURATION 4000
#define OBSTACLE 2000

RunningMedian<unsigned int,20> sonarLeftMeasurements;
RunningMedian<unsigned int,20> sonarRightMeasurements;
RunningMedian<unsigned int,20> sonarCenterMeasurements;

volatile unsigned long startTime = 0;
volatile unsigned long echoTime = 0;
volatile unsigned long echoDuration = 0;
volatile byte idx = 0;
bool added = false;
unsigned long timeoutTime = 0;
unsigned long nextEvalTime = 0;



// HC-SR04 ultrasonic sensor driver (2cm - 400cm)
void startHCSR04(int triggerPin, int aechoPin){
  unsigned int uS;            
  digitalWrite(triggerPin, HIGH);
  delayMicroseconds(10);   
  digitalWrite(triggerPin, LOW);      
  /*// if there is no reflection, we will get 0  (NO_ECHO)
  uS = pulseIn(echoPin, HIGH, MAX_ECHO_TIME);  
  //if (uS == MAX_ECHO_TIME) uS = NO_ECHO;
  //if (uS < MIN_ECHO_TIME) uS = NO_ECHO;
  return uS;*/  
  
}

void echoLeft(){
  if (idx != 0) return;
  if (digitalRead(pinSonarLeftEcho) == HIGH) {    
    echoTime = 0;
    startTime = micros();           
  } else {    
    echoTime = micros();            
    echoDuration = echoTime - startTime;          
  }
}

void echoCenter(){
  if (idx != 1) return;
  if (digitalRead(pinSonarCenterEcho) == HIGH) {    
    echoTime = 0;
    startTime = micros();           
  } else {    
    echoTime = micros();            
    echoDuration = echoTime - startTime;          
  }
}
void echoRight(){
  if (idx != 2) return;
  if (digitalRead(pinSonarRightEcho) == HIGH) {    
    echoTime = 0;
    startTime = micros();           
  } else {    
    echoTime = micros();            
    echoDuration = echoTime - startTime;          
  }
}

void SonarClass::run(){      
  if (millis() > timeoutTime){                    
    if (!added) {                      
      if (idx == 0) sonarLeftMeasurements.add(MAX_DURATION);        
        else if (idx == 1) sonarCenterMeasurements.add(MAX_DURATION);        
        else sonarRightMeasurements.add(MAX_DURATION);             
    }
    added = false;
    //if (millis() > nextSonarTime){        
    idx = (idx + 1) % 3;
      //nextSonarTime = millis() + 100;
    //}
    if (idx == 0) startHCSR04(pinSonarLeftTrigger, pinSonarLeftEcho);        
      else if (idx == 1) startHCSR04(pinSonarCenterTrigger, pinSonarCenterEcho);        
      else startHCSR04(pinSonarRightTrigger, pinSonarRightEcho);        
    timeoutTime = millis() + 10;    
  }
  if (echoDuration != 0) {            
    added = true;
    unsigned long raw = echoDuration;    
    if (raw > MAX_DURATION) raw = MAX_DURATION;
    if (idx == 0) sonarLeftMeasurements.add(raw);        
      else if (idx == 1) sonarCenterMeasurements.add(raw);        
      else sonarRightMeasurements.add(raw);        
    echoDuration = 0;
  }
  if (millis() > nextEvalTime){
    nextEvalTime = millis() + 200;        		
    //sonar1Measurements.getAverage(avg);      
    sonarLeftMeasurements.getLowest(distanceLeft);   
    sonarRightMeasurements.getLowest(distanceRight);   
    sonarCenterMeasurements.getLowest(distanceCenter);   		
  }     
}

void SonarClass::begin()
{
  pinMode(pinSonarCenterTrigger , OUTPUT);
  pinMode(pinSonarLeftTrigger , OUTPUT);
  pinMode(pinSonarRightTrigger , OUTPUT);
  pinMode(pinSonarCenterEcho , INPUT);  
  pinMode(pinSonarLeftEcho , INPUT);  
  pinMode(pinSonarRightEcho , INPUT);  

  attachInterrupt(pinSonarCenterEcho, echoCenter, CHANGE);
  attachInterrupt(pinSonarRightEcho, echoRight, CHANGE);
  attachInterrupt(pinSonarLeftEcho, echoLeft, CHANGE); 
	verboseOutput = false;
}


bool SonarClass::obstacle()
{
  return ( (distanceLeft < OBSTACLE) || (distanceRight < OBSTACLE) ||(distanceCenter < OBSTACLE) );
}


