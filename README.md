# Glucoduino

This is the Arduino side of the Glucoduino project. 

It requires a USB host shield and the [USB host library](https://github.com/felis/USB_Host_Shield_2.0). 

## Installation

*Place this folder into your Arduino > Libraries directory.
*After doing so, launch the Arduino IDE and the code can be accessed via File > Examples > glucoduino > glucoduino_read.
*Upload the code to the Arduino and open the serial monitor.
*Once the code is running ("Wating for USB to be connected..." should be repeatedly printed) insert the glucometer into the USB host shield.
*The glucometer's header and readings will be displayed on the serial monitor.

## Notes

Only works with the Contour NEXT USB and Contour NEXT Link glucometers.
Has only been tested with Arduino IDE version 1.0.6. 
