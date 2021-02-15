
# Miata Headlight Controller

This was a funny project for a friend owning a Mazda MX5 (Miata) with popup healight.

The goal was to be able to remotely control the position of these headlights and a led strip under the car. The assembly only use a ESP8266, two 4x relays board and a 12v-->5v converter.

Originally, the headlights are connected with 5 wires (more information here : http://dommelen.net/ephar/wirings.html) :

- \+ 12V permanent
-  \- GND
- \+12V UP
- \+12V DOWN
- \+ 12V retractor indicator 

An additional wire (+12V) is also required to control the light bulb.
![Wiring](wiring.pdf)

![Demo](demo.gif)