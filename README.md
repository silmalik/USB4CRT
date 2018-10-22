# USB4CRT
An USB to I2C bridge dedicated to CRT screen adjustment for Arcade emulation.

Main goal of this project is to allow dynamic adjustment of a "I2C" CRT screen image geometry from PC.

In case of several arcade game emulation, many different screen resolution and refresh rates are employed and require to make some adjustments. This project makes possible to apply accurate settings to CRT televesion/monitor as long as geometry is managed by an I2C jungle chip. 

The bridge is versatile, its specificity is:
 - Write only
 - I2C Multi-Master mode compability, messages are sent on I2C bus in graceful manner.


There are two parts:

## USB4CRT device

Custom developpment to be compiled and loaded on Cypress PSoC(r) 5LP device.

## USB4CRT Tools

Some Powershell script to help application of custom CRT settings during emulation sessions.

## Requirements

- A Cypress PSoC 5LP device - tested on the prototyping board.
- Cypress Creator software, Free download but Windows only, registration required.
