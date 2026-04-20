# candlepin-pinsetter-controller

This code was meant to run a Bowl-Mor Model D candlepin bowling pinsetter. It runs an an Atmel atmega2850 chip and an
array of hardware debouncing circuits - one per input. 7 switches mounted to the pinsetter frame inform when to turn 
the solid state relays controlling the machine components (the sweep, tubes, pusher, and pin lift). This was taken
from the original Bowl-Mor design for these pinsetters with the logic only slightly modified over time to implement
quality of life updates. 


