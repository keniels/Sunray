/*
License
Copyright (c) 2013-2017 by Alexander Grau

Private-use only! (you need to ask for a commercial-use)
 
The code is open: you can modify it under the terms of the 
GNU General Public License as published by the Free Software Foundation, 
either version 3 of the License, or (at your option) any later version.

The code is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

Private-use only! (you need to ask for a commercial-use)
  
 */

#include "motor.h"
#include "config.h"
#include "imu.h"
#include "buzzer.h"
#include "battery.h"
#include "Arduino.h"
#include "helper.h"
#include "adcman.h"
#include "pinman.h"
#include "robot.h"

MotorClass Motor;

volatile uint16_t odoTicksLeft = 0;
volatile uint16_t odoTicksRight = 0;
volatile byte oldOdoPins = 0;
volatile boolean oldOdoLeft = false;
volatile boolean oldOdoRight = false;



// odometry signal change interrupt
void OdometryLeftInt(){			
  odoTicksLeft++;
}

void OdometryRightInt(){			
  odoTicksRight++;
}


void MotorClass::begin() {
  // left wheel motor
  pinMode(pinMotorEnable, OUTPUT);
  digitalWrite(pinMotorEnable, HIGH);
  pinMode(pinMotorLeftPWM, OUTPUT);
  pinMode(pinMotorLeftDir, OUTPUT);
  pinMode(pinMotorLeftSense, INPUT);
  pinMode(pinMotorLeftFault, INPUT);

  // right wheel motor
  pinMode(pinMotorRightPWM, OUTPUT);
  pinMode(pinMotorRightDir, OUTPUT);
  pinMode(pinMotorRightSense, INPUT);
  pinMode(pinMotorRightFault, INPUT);

  // mower motor
  pinMode(pinMotorMowDir, OUTPUT);
  pinMode(pinMotorMowPWM, OUTPUT);
  pinMode(pinMotorMowSense, INPUT);
  pinMode(pinMotorMowRpm, INPUT);
  pinMode(pinMotorMowEnable, OUTPUT);
  digitalWrite(pinMotorMowEnable, HIGH);
  pinMode(pinMotorMowFault, INPUT);

  // odometry
  pinMode(pinOdometryLeft, INPUT_PULLUP);
  pinMode(pinOdometryLeft2, INPUT_PULLUP);
  pinMode(pinOdometryRight, INPUT_PULLUP);
  pinMode(pinOdometryRight2, INPUT_PULLUP);
	
  ADCMan.setupChannel(pinMotorMowSense, 1, false);
  ADCMan.setupChannel(pinMotorLeftSense, 1, false);
  ADCMan.setupChannel(pinMotorRightSense, 1, false);  
 
  // enable interrupts
  attachInterrupt(pinOdometryLeft, OdometryLeftInt, RISING);  
  attachInterrupt(pinOdometryRight, OdometryRightInt, RISING);  
	
	PinMan.setDebounce(pinOdometryLeft, 100);  // reject spikes shorter than usecs on pin
	PinMan.setDebounce(pinOdometryRight, 100);  // reject spikes shorter than usecs on pin	

	verboseOutput = false;
  lowPass = true;
  paused = false;	
  pwmMax = 255;
  pwmMaxMow = 255;
  rpmMax = 25;
  mowSenseMax = 4.0;
  motorFrictionMax = 3400;
	motorFrictionMin = 0.2;
	robotMass = 10;  
  ticksPerRevolution = 1060/2;
	wheelDiameter              = 250;        // wheel diameter (mm)
	wheelBaseCm = 36;    // wheel-to-wheel distance (cm)
	ticksPerCm         = ((float)ticksPerRevolution) / (((float)wheelDiameter)/10.0) / (2*3.1415);    // computes encoder ticks per cm (do not change)  
	stuckMaxDiffOdometryIMU = 2;
	stuckMaxIMUerror = 50;

  motorLeftPID.Kp       = 2.0;  
  motorLeftPID.Ki       = 0.03; 
  motorLeftPID.Kd       = 0.03; 
  motorRightPID.Kp       = motorLeftPID.Kp;
  motorRightPID.Ki       = motorLeftPID.Ki;
  motorRightPID.Kd       = motorLeftPID.Kd;
		
  imuAnglePID_Kp       = 1.0;
  imuAnglePID_Ki       = 0; 
  imuAnglePID_Kd       = 0;  
  
  imuPID_Kp       = 0.7;
  imuPID_Ki       = 0.1; 
  imuPID_Kd       = 3.0;    

  motorLeftSense = 0;
  motorRightSense = 0;
  motorMowSense = 0;
  overCurrentTimeout = 0;

  lastControlTime = 0;
  deltaControlTimeSec = 0;
  motion = MOT_STOP;
  motorLeftPWMCurr = 0;
  motorRightPWMCurr = 0;
  motorLeftPWMSet = 0;
  motorRightPWMSet = 0;
  motorLeftSwapDir = false;
  motorRightSwapDir = false;

  motorStopTime = 0;
  motorLeftTicks = 0;
  motorRightTicks = 0;
  motorLeftTicksZero = 0;
  motorRightTicksZero = 0;

  motorLeftRpmCurr = 0;
  motorRightRpmCurr = 0;
	motorLeftRpmLast = 0;
	motorRightRpmLast = 0;
	motorLeftRpmAcceleration = 0;
	motorRightRpmAcceleration = 0;
  mowerPWMSet = 0;
  mowerPWMCurr = 0;
  speedDpsCurr = 0;
  distanceCmCurr = 0;
	diffOdoIMU = 0;
  isStucked = false;
  motorPosX = 0;
  motorPosY = 0;  
}


// MC33926 motor driver
// Check http://forum.pololu.com/viewtopic.php?f=15&t=5272#p25031 for explanations.
//(8-bit PWM=255, 10-bit PWM=1023)
// IN1 PinPWM         IN2 PinDir
// PWM                L     Forward
// nPWM               H     Reverse
void MotorClass::setMC33926(int pinDir, int pinPWM, int speed) {
  //DEBUGLN(speed);
  if (speed < 0) {
    digitalWrite(pinDir, HIGH) ;
    PinMan.analogWrite(pinPWM, 255 - ((byte)abs(speed)));
  } else {
    digitalWrite(pinDir, LOW) ;
    PinMan.analogWrite(pinPWM, ((byte)speed));
  }
}


void MotorClass::speedPWM ( MotorSelect motor, int speedPWM )
{
  if (motor == MOTOR_MOW) {
    if (speedPWM > pwmMaxMow) speedPWM = pwmMaxMow;
    else if (speedPWM < -pwmMaxMow) speedPWM = -pwmMaxMow;
  } else {
    if (speedPWM > pwmMax) speedPWM = pwmMax;
    else if (speedPWM < -pwmMax) speedPWM = -pwmMax;
  }
  switch (motor) {
    case MOTOR_LEFT:
      if (motorLeftSwapDir) speedPWM *= -1;
      setMC33926(pinMotorLeftDir, pinMotorLeftPWM, speedPWM);
      break;
    case MOTOR_RIGHT:
      if (motorRightSwapDir) speedPWM *= -1;
      setMC33926(pinMotorRightDir, pinMotorRightPWM, speedPWM);
      break;
    case MOTOR_MOW:        
      setMC33926(pinMotorMowDir, pinMotorMowPWM, speedPWM);
      break;
  }
}

void MotorClass::speedControlLine() {
  // compute distance to line (https://en.wikipedia.org/wiki/Distance_from_a_point_to_a_line#Line_defined_by_two_points)
  float x1 = angleRadSetStartX;
  float y1 = angleRadSetStartY;
  float x2 = angleRadSetStartX + cos(angleRadSet) * 10000.0;
  float y2 = angleRadSetStartY + sin(angleRadSet) * 10000.0;  
  distToLine = ((y2-y1)*motorPosX-(x2-x1)*motorPosY+(x2*y1-y2*x1)) / sqrt(sq(y2-y1)+sq(x2-x1));
  int correctLeft = 0;
  int correctRight = 0;
  float angleToTargetRad = distancePI(angleRadCurr, angleRadSet); // w-x
  if (motion == MOT_ANGLE_DISTANCE) {
    imuPID.x = angleToTargetRad / PI * 180.0;
    imuPID.Kp       = imuAnglePID_Kp;
    imuPID.Ki       = imuAnglePID_Ki; 
    imuPID.Kd       = imuAnglePID_Kd;  
  } else {
    imuPID.x = distToLine;
    imuPID.Kp       = imuPID_Kp;
    imuPID.Ki       = imuPID_Ki; 
    imuPID.Kd       = imuPID_Kd;
  }   
  imuPID.w = 0;
  imuPID.y_min = -rpmMax;
  imuPID.y_max = rpmMax;
  imuPID.max_output = rpmMax;
  imuPID.compute();
  if (imuPID.y > 0) correctRight = abs(imuPID.y);
  if (imuPID.y < 0) correctLeft  = abs(imuPID.y);
	if (speedRpmSet < 0) { // reverse 
		correctLeft *= -1;
	  correctRight *= -1;
	}
  motorLeftPID.x = motorLeftRpmCurr;
  motorLeftPID.w  = speedRpmSet - correctLeft;
  motorLeftPID.y_min = -pwmMax;
  motorLeftPID.y_max = pwmMax;
  motorLeftPID.max_output = pwmMax;
  motorLeftPID.compute();
  motorLeftPWMCurr = motorLeftPWMCurr + motorLeftPID.y;
  if (speedRpmSet >= 0) motorLeftPWMCurr = min( max(0, (int)motorLeftPWMCurr), pwmMax); // 0.. pwmMax
  if (speedRpmSet < 0) motorLeftPWMCurr = max(-pwmMax, min(0, (int)motorLeftPWMCurr));  // -pwmMax..0
  motorRightPID.x = motorRightRpmCurr;
  motorRightPID.w = speedRpmSet - correctRight;
  motorRightPID.y_min = -pwmMax;
  motorRightPID.y_max = pwmMax;
  motorRightPID.max_output = pwmMax;
  motorRightPID.compute();
  motorRightPWMCurr = motorRightPWMCurr + motorRightPID.y;
  if (speedRpmSet >= 0) motorRightPWMCurr = min( max(0, (int)motorRightPWMCurr), pwmMax);  // 0.. pwmMax
  if (speedRpmSet < 0) motorRightPWMCurr = max(-pwmMax, min(0, (int)motorRightPWMCurr));   // -pwmMax..0
  //  if ( (abs(motorLeftPID.x)  < 2) && (motorLeftPID.w  == 0) ) leftSpeed = 0; // ensures PWM is really zero
  // if ( (abs(motorRightPID.x) < 2) && (motorRightPID.w == 0) ) rightSpeed = 0; // ensures PWM is really zero
  /*DEBUG(" w=");
    DEBUG(speedRpmSet);
    DEBUG(" x=");
    DEBUG(motorLeftPID.x);
    DEBUG(" y=");
    DEBUGLN(motorLeftPWMCurr);      */
}


void MotorClass::speedControlRotateAngle() {
  motorLeftPID.x = motorLeftRpmCurr;
  motorLeftPID.y_min = -pwmMax;
  motorLeftPID.y_max = pwmMax;
  motorLeftPID.max_output = pwmMax;
  motorLeftPID.compute();
  motorLeftPWMCurr = motorLeftPWMCurr + motorLeftPID.y;

  motorRightPID.x = motorRightRpmCurr;
  motorRightPID.y_min = -pwmMax;
  motorRightPID.y_max = pwmMax;
  motorRightPID.max_output = pwmMax;
  motorRightPID.compute();
  motorRightPWMCurr = motorRightPWMCurr + motorRightPID.y;

  if (motorLeftPID.w >= 0) motorLeftPWMCurr = min( max(0, (int)motorLeftPWMCurr), pwmMax);
  if (motorLeftPID.w < 0) motorLeftPWMCurr = max(-pwmMax, min(0, (int)motorLeftPWMCurr));
  if (motorRightPID.w >= 0) motorRightPWMCurr = min( max(0, (int)motorRightPWMCurr), pwmMax);
  if (motorRightPID.w < 0) motorRightPWMCurr = max(-pwmMax, min(0, (int)motorRightPWMCurr));
}


void MotorClass::speedControl() {
  float speedRight;
  float speedLeft;
  float deltaRad;
  float angleToTargetRad;  
  float Ta = deltaControlTimeSec;
  float RLdiff;
  
  // mower motor  
  float goalpwm;
  if (paused) goalpwm = 0;
    else goalpwm = mowerPWMSet;  
  mowerPWMCurr = 0.9 * mowerPWMCurr + 0.1 * goalpwm;
  speedPWM ( MOTOR_MOW, mowerPWMCurr );

  if (paused) {
		resetPID();
		return;  // no gear control
	}

  // gear motors
  if (Ta > 0.5) Ta = 0.001;

  switch (motion) {
    case MOT_CAL_RAMP:
      if (millis() >= motorStopTime) {
        motorStopTime = millis() + 1000;
        motorLeftPWMCurr = max(0.0f, motorLeftPWMCurr - 10);
        motorRightPWMCurr = -motorLeftPWMCurr;
        if (motorLeftPWMCurr < 0.1) motion = MOT_STOP;
      }
      break;
    case MOT_PWM:
      motorLeftPWMCurr = motorLeftPWMSet;
      motorRightPWMCurr = motorRightPWMSet;
      break;
    case MOT_ANGLE_DISTANCE:      
    case MOT_LINE_DISTANCE:
    case MOT_LINE_TIME:
      if (motion == MOT_LINE_DISTANCE) {
        if (distanceCmCurr >= distanceCmSet) {
          stopSlowly();
        }
      }
      speedControlLine();
      break;
    case MOT_ROTATE_ANGLE:
    case MOT_ROTATE_TIME:
      if (motion == MOT_ROTATE_ANGLE) {
        float deltaRad = distancePI(angleRadCurr, angleRadSet); // w-x
        if (fabs(deltaRad) < PI / 180) {
          stopImmediately();
          return;
        }
        motorLeftPID.w  = -speedRpmSet;
        motorRightPID.w  = speedRpmSet;
        if (deltaRad < 0) {
          motorLeftPID.w *= -1;
          motorRightPID.w *= -1;
        }
      }
      speedControlRotateAngle();
      break;
  }
  if ((motorStopTime != 0) && (millis() > motorStopTime - 2000)) { // should stop soon? => set zero setpoint
    if (speedRpmSet >= 0) speedRpmSet = 0.1; // keep direction in set-point
    else speedRpmSet = -0.1;
  }
  speedPWM(MOTOR_LEFT, motorLeftPWMCurr);
  speedPWM(MOTOR_RIGHT, motorRightPWMCurr);
}

void MotorClass::stopMowerImmediately(){
  DEBUGLN(F("stopMowerImmediately"));
  mowerPWMCurr = 0;
  speedPWM ( MOTOR_MOW, mowerPWMCurr );
}

void MotorClass::resetPID(){
	motorLeftPID.reset();
  motorRightPID.reset();
  imuPID.reset();
}


void MotorClass::stopImmediately() {
  if (motion == MOT_STOP) return;
  DEBUGLN(F("stopImmediately"));
  motion = MOT_STOP;
  motorStopTime = 0;
  motorLeftFriction = 0;
  motorRightFriction = 0;
  motorLeftPWMSet = 0;
  motorRightPWMSet = 0;
  motorLeftPWMCurr = motorLeftPWMSet;
  motorRightPWMCurr = motorRightPWMSet;
  speedControl();
	resetPID();  
}

void MotorClass::stopSlowly() {
  if (motorStopTime == 0) {
    DEBUGLN("stopSlowly");
    motorStopTime = millis() + 2000;
  }
}

void MotorClass::travelAngleDistance(int distanceCm, float angleRad, float speedRpmPerc){
  resetPID();  
	distanceCmSet = distanceCm;
  distanceCmCurr = 0;
  speedRpmSet = speedRpmPerc * rpmMax;
  angleRadSet = angleRad;
  angleRadSetStartX = motorPosX;
  angleRadSetStartY = motorPosY;  
  //motorStopTime = millis() + 60000;
  motion = MOT_ANGLE_DISTANCE;
}

// rpm: 1.0 is max
void MotorClass::travelLineDistance(int distanceCm, float angleRad, float speedRpmPerc) {
  resetPID();  
	distanceCmSet = distanceCm;
  distanceCmCurr = 0;
  speedRpmSet = speedRpmPerc * rpmMax;
  angleRadSet = angleRad;
  angleRadSetStartX = motorPosX;
  angleRadSetStartY = motorPosY;  
  //motorStopTime = millis() + 60000;
  motion = MOT_LINE_DISTANCE;
}

// rpm: 1.0 is max
void MotorClass::travelLineTime(int durationMS, float angleRad, float speedRpmPerc) {
  resetPID();  
	distanceCmSet = 0;
  speedRpmSet = speedRpmPerc * rpmMax;
  angleRadSet = angleRad;
  angleRadSetStartX = motorPosX;
  angleRadSetStartY = motorPosY;
  motorStopTime = millis() + durationMS;
  motion = MOT_LINE_TIME;
}

void MotorClass::rotateTime(int durationMS, float speedRpmPerc) {
  resetPID();  
	distanceCmSet = 0;
  angleRadSet = 0;
  speedRpmSet = speedRpmPerc * rpmMax;
  motorStopTime = millis() + durationMS;
  motion = MOT_ROTATE_TIME;
  motorLeftPID.w  = -speedRpmSet;
  motorRightPID.w  = speedRpmSet;
}

void MotorClass::rotateAngle(float angleRad, float speedRpmPerc) {
  resetPID();  
	distanceCmSet = 0;
  angleRadSet = angleRad;
  speedRpmSet = speedRpmPerc * rpmMax;
  //motorStopTime = millis() + 60000;
  motion = MOT_ROTATE_ANGLE;
}


void MotorClass::run() {

  int ticksLeft = odoTicksLeft;
  odoTicksLeft = 0;
  int ticksRight = odoTicksRight;
  odoTicksRight = 0;

  if (motorLeftPWMCurr < 0) ticksLeft *= -1;
  if (motorRightPWMCurr < 0) ticksRight *= -1;
  motorLeftTicks += ticksLeft;
  motorRightTicks += ticksRight;

  unsigned long currTime = millis();
  deltaControlTimeSec =  ((float)(currTime - lastControlTime)) / 1000.0;
  lastControlTime = currTime;

  // calculate speed via tick count
  // 2000 ticksPerRevolution: @ 30 rpm  => 0.5 rps => 1000 ticksPerSec
  // 20 ticksPerRevolution: @ 30 rpm => 0.5 rps => 10 ticksPerSec
  motorLeftRpmCurr = 60.0 * ( ((float)ticksLeft) / ((float)ticksPerRevolution) ) / deltaControlTimeSec;
  motorRightRpmCurr = 60.0 * ( ((float)ticksRight) / ((float)ticksPerRevolution) ) / deltaControlTimeSec;
	motorLeftRpmAcceleration = motorLeftRpmCurr - motorLeftRpmLast;
	motorRightRpmAcceleration = motorRightRpmCurr - motorRightRpmLast; 
	motorLeftRpmLast = motorLeftRpmCurr;
	motorRightRpmLast = motorRightRpmCurr;
	

  // calculate speed via tick time
  /*//motorLeftRpmCurr = 60.0 / ((float)ticksPerRevolution) / (((float)odoTriggerTimeLeft)/1000.0/1000.0);
    motorLeftRpmCurr = 60.0 / ((float)ticksPerRevolution) / (((float)odoTriggerTimeLeft)/1000.0);
    if (motorLeftPWMCurr<0) motorLeftRpmCurr*=-1;
    motorRightRpmCurr = 60.0 / ((float)ticksPerRevolution) / (((float)odoTriggerTimeRight)/1000.0);
    if (motorRightPWMCurr<0) motorRightRpmCurr*=-1;*/

  if (ticksLeft == 0) {
    motorLeftTicksZero++;
    if (motorLeftTicksZero > 2) motorLeftRpmCurr = 0;
  } else motorLeftTicksZero = 0;

  if (ticksRight == 0) {
    motorRightTicksZero++;
    if (motorRightTicksZero > 2) motorRightRpmCurr = 0;
  } else motorRightTicksZero = 0;

  //float yaw = IMU.getYaw();
  //speedDpsCurr = distancePI(angleRadCurr, yaw) / PI*180.0 / deltaControlTimeSec;
  speedDpsCurr = IMU.gyro.z;
  //angleRadCurr = yaw;

  /*if (motion == MOT_STOP){
    if (speedDpsCurr > 1) tone(0, 2500);
      else if (speedDpsCurr < -1) tone(0, 2000);
      else noTone(0);
    } else noTone(0);*/

  float distLeft = ((float)ticksLeft) / ((float)ticksPerCm) ;
  float distRight = ((float)ticksRight) / ((float)ticksPerCm);  
  distanceCmAvg  = (distLeft + distRight) / 2.0;
  distanceCmCurr += fabs(distanceCmAvg);
	angleRadCurrDeltaOdometry = -(distLeft - distRight) / wheelBaseCm;
	//angleRadCurrDeltaIMU = IMU.getYaw() - angleRadCurr;
	angleRadCurrDeltaIMU = speedDpsCurr * deltaControlTimeSec / 180.0 * PI;
  if (IMU.enabled){
    angleRadCurr = IMU.getYaw();    
  } else {
    angleRadCurr = scalePI(angleRadCurr + angleRadCurrDeltaOdometry);
  }
  motorPosX += distanceCmAvg * cos(angleRadCurr);
  motorPosY += distanceCmAvg * sin(angleRadCurr);  

  speedControl();
  if ((motorStopTime != 0) && (millis() >= motorStopTime)) stopImmediately();

  checkFault();
  		
	float scale       = 1.905;   // ADC voltage to amp
  // power in watt (P = I * V)
	//motorRightPower = ((float)ADCMan.getVoltage(pinMotorRightSense)) *scale * Battery.batteryVoltage * motorRightPWMCurr/((float)pwmMax) ;
  //motorLeftPower = ((float)ADCMan.getVoltage(pinMotorLeftSense)) *scale * Battery.batteryVoltage * motorLeftPWMCurr/((float)pwmMax) ;
  //motorMowPower = ((float)ADCMan.getVoltage(pinMotorMowSense)) *scale * Battery.batteryVoltage * mowerPWMCurr/((float)pwmMaxMow) ;	
	// torque (torque = current * motor_const)
  motorRightSense = ((float)ADCMan.getVoltage(pinMotorRightSense)) *scale;
  motorLeftSense = ((float)ADCMan.getVoltage(pinMotorLeftSense)) *scale;
  motorMowSense = ((float)ADCMan.getVoltage(pinMotorMowSense)) *scale;	

  /*if (abs(motorLeftPWMCurr) < 70) motorLeftEff = 1;
  else motorLeftEff  = min(1.0, abs(motorLeftRpmCurr / motorLeftSense));
  if (abs(motorRightPWMCurr) < 70) motorRightEff = 1;
  else motorRightEff  = min(1.0, abs(motorRightRpmCurr / motorRightSense));*/
	
	// friction = torque / rpm * mass * cos(pitch)		
	// uphill forward/downhill reverse: decrease friction by angle
	// downhill forward/uphill reverse: increase friction by angle	 			
	float leftRpm = max(0.1, abs(motorLeftRpmCurr));	
	float rightRpm = max(0.1, abs(motorRightRpmCurr));	
	float cosPitch = cos(IMU.ypr.pitch); 
	float pitchfactor;
	// left wheel friction
	if (  ((motorLeftPWMCurr >= 0) && (IMU.ypr.pitch <= 0)) || ((motorLeftPWMCurr < 0) && (IMU.ypr.pitch >= 0)) )
		pitchfactor = cosPitch; // decrease by angle
	else 
		pitchfactor = 2.0-cosPitch;  // increase by angle
	//motorLeftFriction = abs(motorLeftPower) / leftRpm * robotMass * pitchfactor * 10;  
	motorLeftFriction = abs(motorLeftSense) * robotMass * pitchfactor;  
	// right wheel friction
	if (  ((motorRightPWMCurr >= 0) && (IMU.ypr.pitch <= 0)) || ((motorRightPWMCurr < 0) && (IMU.ypr.pitch >= 0)) )
		pitchfactor = cosPitch;  // decrease by angle
	else 
		pitchfactor = 2.0-cosPitch; // increase by angle
  //motorRightFriction = abs(motorRightPower) / rightRpm  * robotMass * pitchfactor * 10; 
	motorRightFriction = abs(motorRightSense) * robotMass * pitchfactor; 

  //if (  (motorMowSense > mowSenseMax) || (motorLeftSense > motorSenseMax) || (motorRightSense > motorSenseMax)  ) {  
  if ( (!paused) && (motion != MOT_STOP) ){
    if ( (motorMowSense > mowSenseMax) || ((motorLeftFriction > motorFrictionMax) || (motorRightFriction > motorFrictionMax))  ) {  
      if (overCurrentTimeout == 0) overCurrentTimeout = millis() + 1000;
      if (millis() > overCurrentTimeout) {
        if (motorMowSense > mowSenseMax) Robot.sensorTriggered(SEN_MOTOR_FRICTION_MOW);
				if (motorLeftFriction > motorFrictionMax) Robot.sensorTriggered(SEN_MOTOR_FRICTION_LEFT);
				if (motorRightFriction > motorFrictionMax) Robot.sensorTriggered(SEN_MOTOR_FRICTION_RIGHT);
				DEBUG(F("FRICTION "));
        DEBUG(motorLeftFriction);
        DEBUG(F(","));
        DEBUG(motorRightFriction);
        DEBUG(F(","));
        DEBUGLN(motorMowSense);
        stopMowerImmediately();       
        stopImmediately();
        Buzzer.sound(SND_OVERCURRENT, true);
        overCurrentTimeout = 0;				
      }
    } else overCurrentTimeout = 0;
  }
	
	// stuck detection
	diffOdoIMU = angleRadCurrDeltaOdometry - angleRadCurrDeltaIMU;
	if ((abs(diffOdoIMU) > stuckMaxDiffOdometryIMU) || (abs(imuPID.eold) > stuckMaxIMUerror)){		
    Robot.sensorTriggered(SEN_MOTOR_STUCK);
		DEBUGLN(F("STUCKED "));		
		DEBUG(diffOdoIMU);
    DEBUG(F(","));		
		DEBUGLN(imuPID.eold);
		stopImmediately();
		Buzzer.sound(SND_STUCK, true);		
	} 

	if (verboseOutput){
		ROBOTMSG.print(F("!86,"));
    ROBOTMSG.print(motorLeftSense);
    ROBOTMSG.print(F(","));       
    ROBOTMSG.print(motorRightSense);    
    ROBOTMSG.print(F(","));       
    ROBOTMSG.print(motorMowSense);
    ROBOTMSG.print(F(","));           
    ROBOTMSG.print(motorLeftFriction);
    ROBOTMSG.print(F(","));               
    ROBOTMSG.print(motorRightFriction);
    ROBOTMSG.print(F(","));               		
		ROBOTMSG.print(diffOdoIMU, 4);		
		ROBOTMSG.print(F(","));
		ROBOTMSG.print(imuPID.eold, 4);				
		ROBOTMSG.print(F(","));
		ROBOTMSG.print(motorLeftPID.eold, 4);				
		ROBOTMSG.print(F(","));
		ROBOTMSG.print(motorRightPID.eold, 4);				
		ROBOTMSG.println();        
	}  
}


// pwm: 1.0 is max
void MotorClass::setSpeedPWM(float leftPWMPerc, float rightPWMPerc) {
  /* DEBUG("setSpeedPWM ");
    DEBUG(leftPWMPerc);
    DEBUG(",");
    DEBUGLN(rightPWMPerc);  */
  motorLeftPWMSet = (int) (leftPWMPerc * ((float)pwmMax));
  motorRightPWMSet = (int) (rightPWMPerc * ((float)pwmMax));
  motorStopTime = millis() + 2000;
  motion = MOT_PWM;
}

// pwm: 1.0 is max
void MotorClass::setMowerPWM(float pwmPerc) {
  //DEBUG(F("setMowerPWM: "));
  //DEBUGLN(pwmPerc);
  if (pwmPerc < 0.05) mowerPWMCurr = 0; // stop immediately
  mowerPWMSet = 255.0f * pwmPerc;  
}

void MotorClass::calibrateRamp() {
  motorLeftPWMCurr = pwmMax;
  motorRightPWMCurr = -pwmMax;
  motorStopTime = millis() + 1000;
  motion = MOT_CAL_RAMP;
}

void MotorClass::setPaused(bool flag) {
  DEBUG(F("setPaused="));
  DEBUGLN(flag);
  paused = flag;
  if (paused) {
    speedPWM(MOTOR_LEFT, 0);
    speedPWM(MOTOR_RIGHT, 0);
		resetPID();  
    //speedPWM(MOTOR_MOW, 0);            
  } 
}

// handle motor errors
void MotorClass::checkFault() {
  if (digitalRead(pinMotorLeftFault) == LOW) {
    DEBUGLN(F("Error: motor left fault"));
    stopImmediately();
    resetFault();
		Robot.sensorTriggered(SEN_MOTOR_ERROR_LEFT);
  }
  if  (digitalRead(pinMotorRightFault) == LOW) {
    DEBUGLN(F("Error: motor right fault"));
    stopImmediately();
    resetFault();
		Robot.sensorTriggered(SEN_MOTOR_ERROR_RIGHT);
  }
  if (digitalRead(pinMotorMowFault) == LOW) {
    DEBUGLN(F("Error: motor mow fault"));
    stopImmediately();
    setMowerPWM(0);    
    resetFault();
		Robot.sensorTriggered(SEN_MOTOR_ERROR_MOW);
  }
}


void MotorClass::resetFault() {
  if (digitalRead(pinMotorLeftFault) == LOW) {
    digitalWrite(pinMotorEnable, LOW);
    digitalWrite(pinMotorEnable, HIGH);
    DEBUGLN(F("Reset motor left fault"));
  }
  if  (digitalRead(pinMotorRightFault) == LOW) {
    digitalWrite(pinMotorEnable, LOW);
    digitalWrite(pinMotorEnable, HIGH);
    DEBUGLN(F("Reset motor right fault"));
  }
  if (digitalRead(pinMotorMowFault) == LOW) {
    digitalWrite(pinMotorMowEnable, LOW);
    digitalWrite(pinMotorMowEnable, HIGH);
    DEBUGLN(F("Reset motor mow fault"));
  }
}


