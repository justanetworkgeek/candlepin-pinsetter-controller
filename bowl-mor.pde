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

// Define HIGH and LOW to ON and OFF to make more logical sense
constexpr uint8_t ON = HIGH;
constexpr uint8_t OFF = LOW;
constexpr uint16_t debounceMs = 130;
constexpr uint8_t UNINITIALIZED_OUTPUT = 0xFF;

// Switch inputs
constexpr uint8_t resetSwitch = 22;
constexpr uint8_t tubeStartSwitch = 24;
constexpr uint8_t sweepStopSwitch = 26;
constexpr uint8_t tubeStopSwitch = 28;
constexpr uint8_t liftStopSwitch = 30;
constexpr uint8_t pusherStopSwitch = 32;

// Relay outputs
constexpr uint8_t sweepRelay = 23;
constexpr uint8_t tubeRelay = 25;
constexpr uint8_t pusherRelay = 27;
constexpr uint8_t liftRelay = 29;

// LED output
constexpr uint8_t statusLED = 31;

// Software flip-flop variables
bool sweepStopFlop = false;
bool tubeStopFlop = false;
bool pusherStopFlop = false;

// Detect that the reset button was pressed before the cycle completed.
bool holdResetValue = false;

uint8_t statusLedState = UNINITIALIZED_OUTPUT;
uint8_t sweepRelayState = UNINITIALIZED_OUTPUT;
uint8_t tubeRelayState = UNINITIALIZED_OUTPUT;
uint8_t pusherRelayState = UNINITIALIZED_OUTPUT;
uint8_t liftRelayState = UNINITIALIZED_OUTPUT;

// Instantiate debouncing for every switch
Bounce resetBounce = Bounce(resetSwitch, debounceMs);
Bounce sweepStopBounce = Bounce(sweepStopSwitch, debounceMs);
Bounce tubeStartBounce = Bounce(tubeStartSwitch, debounceMs);
Bounce tubeStopBounce = Bounce(tubeStopSwitch, debounceMs);
Bounce liftStopBounce = Bounce(liftStopSwitch, debounceMs);
Bounce pusherStopBounce = Bounce(pusherStopSwitch, debounceMs);

void startCycle();
void startTubes();
void stopSweep();
void stopTubes();
void stopLiftStartPusher();
void stopLift();
void stopPusherStartLift();

inline void writeOutputIfChanged(const uint8_t pin, const uint8_t value, uint8_t &cachedState) {
  if (cachedState != value) {
    digitalWrite(pin, value);
    cachedState = value;
  }
}

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

  // Initialize outputs to known defaults.
  writeOutputIfChanged(statusLED, OFF, statusLedState);
  writeOutputIfChanged(sweepRelay, OFF, sweepRelayState);
  writeOutputIfChanged(tubeRelay, OFF, tubeRelayState);
  writeOutputIfChanged(pusherRelay, OFF, pusherRelayState);

  // Start the pin lift as a priority.
  writeOutputIfChanged(liftRelay, ON, liftRelayState);
}

// The main method
void loop() {
  const bool resetChanged = resetBounce.update();
  const bool tubeStartChanged = tubeStartBounce.update();
  const bool sweepStopChanged = sweepStopBounce.update();
  tubeStopBounce.update();
  liftStopBounce.update();
  const bool pusherStopChanged = pusherStopBounce.update();

  // Read values after update() so decisions use current debounced state.
  const int resetValue = resetBounce.read();
  const int tubeStartValue = tubeStartBounce.read();
  const int sweepStopValue = sweepStopBounce.read();
  const int tubeStopValue = tubeStopBounce.read();
  const int liftStopValue = liftStopBounce.read();
  const int pusherStopValue = pusherStopBounce.read();

  // Start the cycle if pusher has completed the cycle.
  // If pusher has not completed the cycle, hold the request until complete.
  if (resetChanged) {
    if (resetValue == ON && pusherStopFlop) {
      startCycle();
    } else if (resetValue == ON && !pusherStopFlop) {
      holdResetValue = true;
      writeOutputIfChanged(statusLED, ON, statusLedState);
    }
  }

  // Start the tubes if tube start has been pressed and sweep is still moving.
  if (tubeStartChanged) {
    if (tubeStartValue == ON && !sweepStopFlop) {
      startTubes();
    }
  }

  // Stop the sweep if sweep stop has been pressed and tubes are still moving.
  if (sweepStopChanged) {
    if (sweepStopValue == ON && !tubeStopFlop) {
      stopSweep();
    }
  }

  // Stop the tubes if tube stop has been pressed and sweep has stopped.
  if (tubeStopValue == ON && sweepStopFlop) {
    stopTubes();
  }

  // Stop lift and start pusher if lift stop has been pressed and tubes have stopped.
  if (liftStopValue == ON && tubeStopFlop) {
    stopLiftStartPusher();
  } else if (liftStopValue == ON && !tubeStopFlop) {
    // If tubes have not stopped yet, only stop lift and wait.
    stopLift();
  }

  // Stop pusher and start lift if pusher stop has been pressed.
  if (pusherStopChanged) {
    if (pusherStopValue == ON) {
      stopPusherStartLift();
    }
  }
}

// Function to start the cycle.
void startCycle() {
  // Reset all flop variables to default false.
  sweepStopFlop = false;
  pusherStopFlop = false;
  tubeStopFlop = false;

  // Turn the status LED on.
  writeOutputIfChanged(statusLED, ON, statusLedState);

  // Turn the sweep on.
  writeOutputIfChanged(sweepRelay, ON, sweepRelayState);
}

// Function to start the tubes.
void startTubes() {
  // Turn the tubes on.
  writeOutputIfChanged(tubeRelay, ON, tubeRelayState);
}

// Function to stop the sweep.
void stopSweep() {
  // Set sweep stop flop to true since switch was hit.
  sweepStopFlop = true;

  // Turn the sweep off.
  writeOutputIfChanged(sweepRelay, OFF, sweepRelayState);
}

// Function to stop the tubes.
void stopTubes() {
  // Set tube stop flop to true since switch was hit.
  tubeStopFlop = true;

  // Turn the status LED off.
  writeOutputIfChanged(statusLED, OFF, statusLedState);

  // Turn the tubes off.
  writeOutputIfChanged(tubeRelay, OFF, tubeRelayState);
}

// Function to stop the lift and start the pusher.
void stopLiftStartPusher() {
  // Turn the lift off.
  writeOutputIfChanged(liftRelay, OFF, liftRelayState);

  // Turn the pusher on.
  writeOutputIfChanged(pusherRelay, ON, pusherRelayState);
}

// Function to only stop the lift.
void stopLift() {
  // Turn the lift off.
  writeOutputIfChanged(liftRelay, OFF, liftRelayState);
}

// Function to stop the pusher and start the lift.
void stopPusherStartLift() {
  // Set pusher stop flop to true since switch was hit.
  pusherStopFlop = true;

  // Turn the pusher off.
  writeOutputIfChanged(pusherRelay, OFF, pusherRelayState);

  // Turn the lift on.
  writeOutputIfChanged(liftRelay, ON, liftRelayState);

  // If reset was pressed before pusher completed, start cycle automatically.
  if (holdResetValue) {
    holdResetValue = false;
    startCycle();
  }
}
