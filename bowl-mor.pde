/*
 *
 * This application is a complex set of conditional statements which determine how
 * candlepin pinsetters run, and which mechanical components are allowed to go and 
 * when. There are 7 inputs, and 9 outputs. Each input represents a mechanical limit 
 * switch, which machine components trigger as they run to control the start/stop of
 * machine parts. There are 5 status LED outputs, and 4 solid state relay outputs. 
 *
 * When limit switches are triggered, the corresponding solid state relay will turn 
 * on or off, thus performing mechanical work, and keeping the machine in sequence.
 *
 * This code implements software debouncing methods to correct mechanical switch bounces. 
 * Each switch is tuned to a specific debouncing interval. Inputs read as latching
 * switches have higher debouncing intervals, because the subroutines and
 * conditions which utilize them read their states as always ON do work, if not on, do not
 * do work.
 *
 * This code is the creative and intellectual design of myself as a result of years
 * reverse engineering the Bowl-Mor pinsetter control logic. Use of this software is forbidden
 * for COMMERCIAL purposes.
 */

// Included libraries
#include <Bounce.h>

// Define HIGH and LOW to On and OFF to make more logical sense
#define ON HIGH
#define OFF LOW

// Switch inputs
const int resetSwitch = 22;
const int tubeStartSwitch = 24;
const int sweepStopSwitch = 26;
const int tubeStopSwitch = 28;
const int liftStopSwitch = 30;
const int pusherStopSwitch = 32;

// Relay outputs
const int sweepRelay = 23;
const int tubeRelay = 25;
const int pusherRelay = 27;
const int liftRelay = 29;

// LED Outputs
const int statusLED = 31;

// Software flip-flop variables
boolean sweepStopFlop = false;
boolean tubeStopFlop = false;
boolean pusherStopFlop = false;

// Boolean variable to detect that the reset button has been pressed.
boolean holdResetValue = false;

// Instantiate debouncing for every switch
Bounce resetBounce = Bounce(resetSwitch, 130);
Bounce sweepStopBounce = Bounce(sweepStopSwitch, 130);
Bounce tubeStartBounce = Bounce(tubeStartSwitch, 130);
Bounce tubeStopBounce = Bounce(tubeStopSwitch, 130);
Bounce liftStopBounce = Bounce(liftStopSwitch, 130);
Bounce pusherStopBounce = Bounce(pusherStopSwitch, 130);

void setup() {
  // Inputs
  pinMode(resetSwitch, INPUT);
  pinMode(sweepStopSwitch, INPUT);
  pinMode(tubeStartSwitch, INPUT);
  pinMode(tubeStopSwitch, INPUT);
  pinMode(liftStopSwitch, INPUT);
  pinMode(pusherStopSwitch, INPUT);
  
  // Outputs
  pinMode(statusLED, OUTPUT);
  pinMode(sweepRelay, OUTPUT);
  pinMode(liftRelay, OUTPUT);
  pinMode(pusherRelay, OUTPUT);
  pinMode(tubeRelay, OUTPUT);
  
  // Start the pin lift as a priority. This will start the lift on powerup,
  // which allows it to be set low later on in the cycles. If started at beginning
  // of loop(), it will always stay on, and be unable to set LOW without delay() 
  digitalWrite(liftRelay, ON);
}

// The main method
void loop() {
  // Make all debounce functions equal to readable variables
  int resetValue = resetBounce.read();
  int tubeStartValue = tubeStartBounce.read();
  int sweepStopValue = sweepStopBounce.read();
  int tubeStopValue = tubeStopBounce.read();
  int liftStopValue = liftStopBounce.read();
  int pusherStopValue = pusherStopBounce.read();

  // Update the pin lift constantly, it can stop at any time in the cycle,
  // this prevents pin override.
  liftStopBounce.update();
  
  // Update the tubes to prevent override.
  tubeStopBounce.update();
  
  // Start the cycle if pusher has completed the cycle.
  // If pusher has not completed the cycle, wait until it
  // does, then automatically start the cycle again.
  if(resetBounce.update()){
    if(resetValue == ON && pusherStopFlop == true){
      startCycle();
    } 
    else if(resetValue == ON && pusherStopFlop == false){
      holdResetValue = true;

      digitalWrite(statusLED, ON);
    }
  }

  // Start the tubes if the tube start has been pressed, and
  // the sweep is still in motion.
  if(tubeStartBounce.update()){
    if(tubeStartValue == ON && sweepStopFlop == false){
      startTubes();
    }
  }

  // Stop the sweep if the sweep stop has been pressed,
  // and the tubes are still in motion.
  if(sweepStopBounce.update()){
    if(sweepStopValue == ON && tubeStopFlop == false){
      stopSweep();
    }
  }

  // Stop the tubes if the tube stop has been pressed, and
  // the sweep has stopped.S
  if(tubeStopValue == ON && sweepStopFlop == true){
    stopTubes();
  }
  // Stop the lift and start the pusher if the lift stop has been pressed,
  // and the tubes have stopped.
  if(liftStopValue == ON && tubeStopFlop == true){
    stopLiftStartPusher();
  }
  // If tubes have not stopped yet, wait to push.
  else if(liftStopValue == ON && tubeStopFlop == false){
    stopLift();

    if(tubeStopFlop == true){
      startPusher();
    }
  }

  // Stop the pusher and start the lift if pusher stop has been pressed.
  if(pusherStopBounce.update()){
    if(pusherStopValue == ON){
      stopPusherStartLift();
    }
  }
}

// Function to start the cycle.
void startCycle(){
  // Reset all flop variables to default 'false'.
  sweepStopFlop = false;
  pusherStopFlop = false;
  tubeStopFlop = false;

  // Turn the status LED on.
  digitalWrite(statusLED, ON);

  // Turn the sweep on.
  digitalWrite(sweepRelay, ON);
}

// Function to start the tubes.
void startTubes(){
  
  // Turn the tubes on.
  digitalWrite(tubeRelay, ON);
}

// Function to stop the sweep
void stopSweep(){
  // Set sweep stop flop to true since switch was hit
  sweepStopFlop = true;

  // Turn the sweep off.
  digitalWrite(sweepRelay, OFF);
}

// Function to stop the tubes
void stopTubes(){
  // Set tube stop flop to true since switch was hit
  tubeStopFlop = true;

  // Turn the status LED off
  digitalWrite(statusLED, OFF);

  // Turn the tubes off
  digitalWrite(tubeRelay, OFF);
}

// Function to stop the lift and start the pusher
void stopLiftStartPusher(){
  // Turn the lift off.
  digitalWrite(liftRelay, OFF);

  // Turn the pusher on.
  digitalWrite(pusherRelay, ON);
}

// Function to only stop the lift
void stopLift(){
  // Turn the lift and lift LED off
  digitalWrite(liftRelay, OFF);
}

// Function to only start the pusher
void startPusher(){
  // Turn the pusher on
  digitalWrite(pusherRelay, ON);
}

// Function to stop the pusher and start the lift
void stopPusherStartLift(){
  // Set the pusher stop flop to true since switch was hit
  pusherStopFlop = true;

  // Turn the pusher off.
  digitalWrite(pusherRelay, OFF);

  // Turn the lift on.
  digitalWrite(liftRelay, ON);

  // If the reset button was pressed prior to pusher completing
  // the cycle, then start the cycle automatically.
  if(holdResetValue == true){
    // Set the hold reset value back to false since it's value has been received
    holdResetValue = false;    

    // Automatically start the cycle
    startCycle();
  }
}
