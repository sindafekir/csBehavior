// csStateBehavior v0.93 -- 32 bit version (teensy)
//
// changes: added a stimGen function which is currently accessible in state 0.

#include <FlexiTimer2.h>
#include <Wire.h>
#include <Adafruit_NeoPixel.h>

// *********** UART Serial Outs
#define visualSerial Serial1 // out to a computer running psychopy
#define dashSerial Serial3 // out to a csDashboard object

// *********** IO Pin Defs
#define neoStripPin 22
#define forceSensorPin 20
#define lickPin  21
#define scopePin 24
#define motionPin  6
#define framePin  5
#define rewardPin  35
#define syncPin  25


// ****** Scale.
uint32_t weightOffset = 0;
float weightScale = 0;

// ****** Set DAC and ADC resolution in bits.
int adcResolution = 10;
int dacResolution = 12;




uint32_t writeValueA = 0;

volatile uint32_t encoderAngle = 0;
volatile uint32_t prev_time = 0;
volatile uint32_t pulseCount = 0;

// **** output pins
int dacChans[] = {A21, A22};

// ****** Interupt Timing
int sampsPerSecond = 1000;
float evalEverySample = 1.0; // number of times to poll the vStates funtion

// ****** Time & State Flow
uint32_t loopCount = 0;
uint32_t timeOffs;
uint32_t stateTimeOffs;
uint32_t trialTime;
uint32_t stateTime;
uint32_t trigTime = 10;


// ****** stim trains
// 0) pulsing? 1) sample counter 3) baseline dur 4) stim dur
// 5) baseline amp 6) stim amp 7) stim type 8) write value
uint16_t pulseTrain_chanA[] = {0, 0, 5, 5, 0, 4000, 0, 0};
uint16_t pulseTrain_chanB[] = {0, 0, 5, 5, 0, 4000, 0, 0};

// ***** reward stuff
int rewardDelivTypeA = 0; // 0 is solenoid; 1 is syringe pump; 2 is stimulus

// ****** state flow
bool blockStateChange = 0;
bool rewarding = 0;

// knownLabels[]={'tState','rewardTime','timeOut','contrast','orientation',
// 'sFreq','tFreq','visVariableUpdateBlock','loadCell','motion','lickSensor','pulseAPeak','pulseADur'};

char knownHeaders[] = {'a', 'r', 't', 'c', 'o', 's', 'f', 'v', 'w', 'm', 'l', 'x', 'y', 'z'};
int knownValues[] = {0, 5, 4000, 0, 0, 0, 0, 1, 0, 0, 0, pulseTrain_chanA[6], pulseTrain_chanA[5], pulseTrain_chanA[3]};
int knownCount = 14;

// ************ data
bool scopeState = 1;

int headerStates[] = {0, 0, 0, 0, 0, 0, 0};
int stateCount = 7;
int lastState = 0;


bool trigStuff = 0;

Adafruit_NeoPixel strip = Adafruit_NeoPixel(30, neoStripPin, NEO_GRB + NEO_KHZ800);

void setup() {
  // ****** Setup Cyclops
  // Start the device
  strip.begin();
  strip.show();
  //  setStripRed();
  strip.setBrightness(100);
  setStripRed();
  // ****** Setup Analog Out
  analogWriteResolution(12);
  attachInterrupt(motionPin, rising, RISING);
  attachInterrupt(framePin, frameCount, RISING);
  pinMode(syncPin, OUTPUT);
  digitalWrite(syncPin, LOW);
  pinMode(scopePin, INPUT);
  pinMode(rewardPin, OUTPUT);
  digitalWrite(rewardPin, LOW);

  dashSerial.begin(9600);
  visualSerial.begin(9600);
  Serial.begin(19200);
  delay(10000);
  FlexiTimer2::set(1, evalEverySample / sampsPerSecond, vStates);
  FlexiTimer2::start();
}

void loop() {
  // This is interupt based so nothing here.
}

void vStates() {

  // sometimes we block state changes, so let's log the last state.
  lastState = knownValues[0];

  // we then look for any changes to variables, or calls for updates
  flagReceive(knownHeaders, knownValues);

  // Some hardware actions need to complete before a state-change.
  // So, we have a latch for state change. We write over any change with lastState
  if (blockStateChange == 1) {
    knownValues[0] = lastState;
  }


  // **************************
  // State 0: Boot/Init State
  // **************************
  if (knownValues[0] == 0) {

    // a) run a header for state 0
    if (headerStates[0] == 0) {
      visStimEnd();
      genericHeader(0);
      loopCount = 0;
      setStripRed();
//      rainbowCycle(20);
    }

    // b) body for state 0
    genericStateBody();
    pulseTrain_chanA[5] = 0;
    pulseTrain_chanB[5] = 0;
    stimGen(pulseTrain_chanA);
    stimGen(pulseTrain_chanB);
    analogWrite(dacChans[0], pulseTrain_chanA[7]);
    analogWrite(dacChans[1], pulseTrain_chanA[7]);

  }


  // ******* ******************************
  // Some things we do for all non-boot states before the state code:
  if (knownValues[0] != 0) {


    // Get a time offset from when we arrived from 0.
    // This should be the start of the trial, regardless of state we start in.
    // Also, trigger anything that needs to be in sync.
    if (loopCount == 0) {
      timeOffs = millis();
      trigStuff = 0;
      digitalWrite(syncPin, HIGH);
    }

    // This ends the trigger.
    if (loopCount >= trigTime && trigStuff == 0) {
      digitalWrite(syncPin, LOW);
      trigStuff = 1;
    }

    //******************************************
    //@@@@@@ Start Non-Boot State Definitions.
    //******************************************

    // **************************
    // State 1: Boot/Init State
    // **************************
    if (knownValues[0] == 1) {
      if (headerStates[1] == 0) {
        //clearStrip();
        visStimOff();
        genericHeader(1);
        blockStateChange = 0;
      }
      genericStateBody();
      pulseTrain_chanA[3] = knownValues[13];
      pulseTrain_chanA[5] = 0;
      pulseTrain_chanA[6] = knownValues[11];
      stimGen(pulseTrain_chanA);
      analogWrite(dacChans[0], pulseTrain_chanA[7]);
      analogWrite(dacChans[1], pulseTrain_chanA[7]);
    }

    // **************************
    // State 2: Stim State
    // **************************
    else if (knownValues[0] == 2) {
      if (headerStates[2] == 0) {
        genericHeader(2);
        visStimOn();
        blockStateChange = 0;
      }
      genericStateBody();
      pulseTrain_chanA[3] = knownValues[13];
      pulseTrain_chanA[5] = knownValues[12];
      pulseTrain_chanA[6] = knownValues[11];
      stimGen(pulseTrain_chanA);
      analogWrite(dacChans[0], pulseTrain_chanA[7]);
      analogWrite(dacChans[1], pulseTrain_chanA[7]);
    }

    // **************************************
    // State 3: Catch-Trial (no-stim) State
    // **************************************
    else if (knownValues[0] == 3) {
      if (headerStates[3] == 0) {
        blockStateChange = 0;
        genericHeader(3);
        visStimOn();
      }
      genericStateBody();
      pulseTrain_chanA[3] = knownValues[13];
      pulseTrain_chanA[5] = knownValues[12];
      pulseTrain_chanA[6] = knownValues[11];
      stimGen(pulseTrain_chanA);
      analogWrite(dacChans[0], pulseTrain_chanA[7]);
    }

    // **************************************
    // State 4: Reward State
    // **************************************
    else if (knownValues[0] == 4) {
      if (headerStates[4] == 0) {
        genericHeader(4);
        visStimOff();
        rewarding = 0;
        blockStateChange = 1;
      }
      genericStateBody();
      pulseTrain_chanA[3] = knownValues[13];
      pulseTrain_chanA[5] = knownValues[12];
      pulseTrain_chanA[6] = knownValues[11];
      stimGen(pulseTrain_chanA);
      analogWrite(dacChans[0], pulseTrain_chanA[7]);
      if (rewardDelivTypeA == 0 && rewarding == 0) {
        digitalWrite(rewardPin, HIGH);
        rewarding = 1;
      }
      if (stateTime >= uint32_t(knownValues[1])) {
        digitalWrite(rewardPin, LOW);
        blockStateChange = 0;
      }
    }

    // **************************************
    // State 5: Time-Out State
    // **************************************
    else if (knownValues[0] == 5) {
      if (headerStates[5] == 0) {
        genericHeader(5);
        visStimOff();
        blockStateChange = 1;
      }

      genericStateBody();
      pulseTrain_chanA[3] = knownValues[13];
      pulseTrain_chanA[5] = knownValues[12];
      pulseTrain_chanA[6] = knownValues[11];
      stimGen(pulseTrain_chanA);
      analogWrite(dacChans[0], pulseTrain_chanA[7]);
      // trap the state in time-out til timeout time over.
      if (stateTime >= uint32_t(knownValues[2])) {
        blockStateChange = 0;
      }
    }

    // **************************************
    // State 6: Manual Reward State
    // **************************************
    else if (knownValues[0] == 6) {
      if (headerStates[6] == 0) {
        genericHeader(6);
        //        visStimOff();
        rewarding = 0;
        blockStateChange = 0;
      }
      genericStateBody();
      pulseTrain_chanA[3] = knownValues[13];
      pulseTrain_chanA[5] = knownValues[12];
      pulseTrain_chanA[6] = knownValues[11];
      stimGen(pulseTrain_chanA);
      analogWrite(dacChans[0], pulseTrain_chanA[7]);
      if (rewardDelivTypeA == 0 && rewarding == 0) {
        digitalWrite(rewardPin, HIGH);
        rewarding = 1;
      }
      if (stateTime >= uint32_t(knownValues[1])) {
        digitalWrite(rewardPin, LOW);
        blockStateChange = 0;
      }
    }

    // ******* Stuff we do for all non-boot states.
    trialTime = millis() - timeOffs;
    dataReport();
    loopCount++;
  }
}

void dataReport() {
  Serial.print("tData");
  Serial.print(',');
  Serial.print(loopCount);
  Serial.print(',');
  Serial.print(trialTime);
  Serial.print(',');
  Serial.print(stateTime);
  Serial.print(',');
  Serial.print(knownValues[0]); //state
  Serial.print(',');
  Serial.print(knownValues[8]);  //load cell
  Serial.print(',');
  Serial.print(knownValues[10]); // lick sensor
  Serial.print(',');
  Serial.print(encoderAngle);     //rotary encoder value
  Serial.print(',');
  Serial.print(pulseCount);
  Serial.print(',');
  Serial.print(pulseTrain_chanA[7]);
  Serial.print(',');
  Serial.println(pulseTrain_chanB[7]);
}


int flagReceive(char varAr[], int valAr[]) {
  static boolean recvInProgress = false;
  static byte ndx = 0;
  char endMarker = '>';
  char feedbackMarker = '<';
  char rc;
  int nVal;
  const byte numChars = 32;
  char writeChar[numChars];
  int selectedVar = 0;
  int newData = 0;

  while (Serial.available() > 0 && newData == 0) {
    rc = Serial.read();
    if (recvInProgress == false) {
      for ( int i = 0; i < knownCount; i++) {
        if (rc == varAr[i]) {
          selectedVar = i;
          recvInProgress = true;
        }
      }
    }

    else if (recvInProgress == true) {
      if (rc == endMarker ) {
        writeChar[ndx] = '\0'; // terminate the string
        recvInProgress = false;
        ndx = 0;
        newData = 1;

        nVal = int(String(writeChar).toInt());
        valAr[selectedVar] = nVal;

      }
      else if (rc == feedbackMarker) {
        writeChar[ndx] = '\0'; // terminate the string
        recvInProgress = false;
        ndx = 0;
        newData = 1;
        Serial.print("echo");
        Serial.print(',');
        Serial.print(varAr[selectedVar]);
        Serial.print(',');
        Serial.print(valAr[selectedVar]);
        Serial.print(',');
        Serial.println('~');
      }

      else if (rc != feedbackMarker || rc != endMarker) {
        writeChar[ndx] = rc;
        ndx++;
        if (ndx >= numChars) {
          ndx = numChars - 1;
        }
      }
    }
  }
  return newData; // tells us if a valid variable arrived.
}


void resetHeaders() {
  for ( int i = 0; i < stateCount; i++) {
    headerStates[i] = 0;
  }
}

void genericHeader(int stateNum) {
  stateTimeOffs = millis();
  resetHeaders();
  headerStates[stateNum] = 1;
}

void genericStateBody() {
  stateTime = millis() - stateTimeOffs;
  knownValues[10] = analogRead(lickPin);
  knownValues[9] = analogRead(motionPin);
  knownValues[8] = analogRead(forceSensorPin);
  scopeState = digitalRead(scopePin);
}

// ****************************************************************
// **************  Visual Stimuli *********************************
// ****************************************************************

void visStimEnd() {
  visualSerial.print('v');
  visualSerial.print(',');
  visualSerial.print(0);
  visualSerial.print(',');
  visualSerial.print(999);
  visualSerial.print(',');
  visualSerial.print(0);
  visualSerial.print(',');
  visualSerial.print(0);
  visualSerial.print(',');
  visualSerial.println(knownValues[7]);
}

void visStimOff() {
  visualSerial.print('v');
  visualSerial.print(',');
  visualSerial.print(0);
  visualSerial.print(',');
  visualSerial.print(0);
  visualSerial.print(',');
  visualSerial.print(0);
  visualSerial.print(',');
  visualSerial.print(0);
  visualSerial.print(',');
  visualSerial.println(knownValues[7]);
}

void visStimOn() {
  visualSerial.print('v');
  visualSerial.print(',');
  visualSerial.print(knownValues[4]);
  visualSerial.print(',');
  visualSerial.print(knownValues[3]);
  visualSerial.print(',');
  visualSerial.print(knownValues[5]);
  visualSerial.print(',');
  visualSerial.print(knownValues[6]);
  visualSerial.print(',');
  visualSerial.println(knownValues[7]);
}

// **************************************************************
// **************  Motion Interrupts  ***************************
// **************************************************************

void rising() {
  attachInterrupt(motionPin, falling, FALLING);
  prev_time = micros();
}

void falling() {
  attachInterrupt(motionPin, rising, RISING);
  encoderAngle = micros() - prev_time;
}

void frameCount() {
  pulseCount++;

}


// ****************************************************************
// **************  Pulse Train Function ***************************
// ****************************************************************

void stimGen(uint16_t pulseTracker[]) {
  if (pulseTracker[6] == 0) {

    // *** handle pulse state
    // 0 tracks in pulse and 2 is the pulseWidth; 1 is the counter
    // 6 is the output value
    if (pulseTracker[0] == 1) {
      pulseTracker[7] = pulseTracker[5]; // 5 is the pulse amp; 6 is the current output.
      if (pulseTracker[1] >= pulseTracker[3]) {
        pulseTracker[1] = 0; // reset counter
        pulseTracker[0] = 0; // stop pulsing
      }
    }

    // 0 tracks in pulse and 3 is the delayWidth; 1 is the counter
    else if (pulseTracker[0] == 0) {
      pulseTracker[7] = pulseTracker[4]; // 4 is the baseline amp; 7 is the current output.
      if (pulseTracker[1] >= pulseTracker[2]) {
        pulseTracker[1] = 0; // reset counter
        pulseTracker[0] = 1; // start pulsing
        pulseTracker[7] = pulseTracker[4]; // 4 is the baseline amp; 6 is the current output.
      }
    }
  }

  else if (pulseTracker[6] == 1) {
    int incToPeak = (pulseTracker[5] - pulseTracker[4]) / pulseTracker[3];
    // *** handle pulse state
    // 0 tracks in pulse and 2 is the pulseWidth; 1 is the counter
    // 7 is the output value
    if (pulseTracker[0] == 1) {
      pulseTracker[7] = pulseTracker[7] + incToPeak; // 5 is the pulse amp; 7 is the current output.
      if (pulseTracker[1] >= pulseTracker[3]) {
        pulseTracker[1] = 0; // reset counter
        pulseTracker[0] = 0; // stop pulsing
      }
    }

    // 0 tracks in pulse and 3 is the delayWidth; 1 is the counter
    else if (pulseTracker[0] == 0) {
      pulseTracker[7] = pulseTracker[4]; // 4 is the baseline amp; 7 is the current output.
      if (pulseTracker[1] >= pulseTracker[2]) {
        pulseTracker[1] = 0; // reset counter
        pulseTracker[0] = 1; // start pulsing
      }
    }

  }

  pulseTracker[1] = pulseTracker[1] + 1;

}

void setStripRed() {
  for (uint16_t i = 0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, strip.Color(255, 0, 0));
  }
  strip.show();
}

void setStripWhite() {
  for (uint16_t i = 0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, strip.Color(255, 200, 100));
  }
  strip.show();
}

void clearStrip() {
  for (uint16_t i = 0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, strip.Color(0, 0, 0));
  }
  strip.show();
}


void rainbowCycle(uint8_t wait) {
  uint16_t i, j;

  for (j = 0; j < 256 * 5; j++) { // 5 cycles of all colors on wheel
    for (i = 0; i < strip.numPixels(); i++) {
      strip.setPixelColor(i, Wheel(((i * 256 / strip.numPixels()) + j) & 255));
    }
    strip.show();
    delay(wait);
  }
}

// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
uint32_t Wheel(byte WheelPos) {
  WheelPos = 255 - WheelPos;
  if(WheelPos < 85) {
    return strip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  }
  if(WheelPos < 170) {
    WheelPos -= 85;
    return strip.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
  WheelPos -= 170;
  return strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
}
