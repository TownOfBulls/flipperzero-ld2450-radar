This is a simple radar application to accompany the flipper zero that reads the raw input of the LD2450 Sensors and displays it in either its raw numeric form or in a radar.
Nice to Test if your sensors are responding properly

![Screenshot](screenshots/screenshot1.jpg)

![Screenshot](screenshots/screenshot2.mp4)

The Gpios in your flipper should be set to 5V.
*   **On USB:** If connected via USB, the 5V rail is active automatically, so this step is not required.

Connection Diagram for the LD2450 module -> Flipper Zero GPIO header:

| LD2410 Pin | Flipper Zero Pin | Note |
| :--- | :--- | :--- |
| **VCC** | **5V** (Pin 1) | but 5V is standard. |
| **GND** | **GND** (Pin 8) | Any GND pin works. |
| **TX** | **RX** (Pin 14) | Flipper GPIO 14 (USART RX). |
| **RX** | **TX** (Pin 13) | Flipper GPIO 13 (USART TX). |


In the code This section controls the "sensitivity" for the x axis, making the dot exaggarate left or right movements on the radar.
The x axis in the raw input screen may be confusing the way it works is; Y is the distance away from the sensor, while x is the distance from the center line of the sensor:

Top-down view

            + Y
             ↑
             |
   -X        |        +X
----------LD2450----------
             |

*   **Work in Progress** ; but still figured I share since it has gathered more interest than I had anitcipated.
