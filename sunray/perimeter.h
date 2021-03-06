// Ardumower perimeter receiver (in/out detection and magnitude)
// for 2 coils, using PRN signal code correlation

#ifndef PERIMETER_H
#define PERIMETER_H

#include <Arduino.h>

//#define RAW_SIGNAL_SAMPLE_SIZE 32

#define IDX_LEFT   0
#define IDX_CENTER 0
#define IDX_RIGHT  1


class PerimeterClass
{
  public:
    PerimeterClass();
    // set ADC pins
	  void begin(byte idx0Pin, byte idx1Pin);
    const int8_t* getRawSignalSample(byte idx);
    // get perimeter magnitude
    int getMagnitude(byte idx);    
    int getSmoothMagnitude(byte idx);
    // inside perimeter (true) or outside (false)?  
    boolean isInside();
    boolean isInside(byte idx);
    bool enabled;
		// perimeter signal timed out? (e.g. due to broken wire)
    bool signalTimedOut();
    void resetTimedOut();
    boolean signalTimedOut(byte idx);
    int16_t getSignalMin(byte idx);
    int16_t getSignalMax(byte idx);    
    int16_t getSignalAvg(byte idx);
    float getFilterQuality(byte idx); 
    void speedTest();
    void run();
    int16_t timedOutIfBelowSmag;
    int16_t timeOutSecIfNotInside;
    // use differential perimeter signal as input for the matched filter? 
    bool useDifferentialPerimeterSignal;
    // swap coil polarity?
    bool swapCoilPolarity;  
    char subSample;  	
  private:
    unsigned long lastInsideTime[2];
    byte idxPin[2]; // channel for idx
    int callCounter;
    int16_t mag [2]; // perimeter magnitude per channel
    float smoothMag[2];
    float filterQuality[2];
    int16_t signalMin[2];
    int16_t signalMax[2];
    int16_t signalAvg[2];    
    int signalCounter[2];    
    //int8_t rawSignalSample[2][RAW_SIGNAL_SAMPLE_SIZE];
    void matchedFilter(byte idx);
    int16_t corrFilter(int8_t *H, int8_t subsample, int16_t M, int8_t *ip, int16_t nPts, float &quality);
    void printADCMinMax(int8_t *samples);
};

extern PerimeterClass Perimeter;

#endif

