# AutoShade :sunglasses:

*Dynamically creates an adaptive sun-blocking windshield display* 
  - Tested on Raspberry Pi 3 running Raspbian Jessie
  - Utilizes [Adafruit Ultimate GPS Breakout - 66 channel w/10 Hz updates - Version 3](https://nodesource.com/products/nsolid)
  - Communicates via UART onboard Raspberry Pi 3

## Installation and Setup

### Dependencies
##### You will need a Raspberry Pi with the following dependencies installed:
>	GTK2.0

>	GPSD
##### You can install the above using apt-get. Adafruit also has a wonderful [tutorial](https://learn.adafruit.com/adafruit-ultimate-gps-on-the-raspberry-pi/introduction) on how to setup GPSD on a Raspberry Pi using UART. Note that you do not need a USB to TTL adapter if you use the UART pins onbaord the Raspberry Pi.

### Building the Program
Once you have GTK2 and GPSD installed on your Raspberry Pi you can start using the AutoShade program.
First, make sure to run the following command to start GPSD
```sudo gpsd /dev/ttyS0 -F /var/run/gpsd.sock```
**_You will need to run this program everytime the Raspberry Pi reboots._** Therefore, you might want to include this command in ```/etc/rc.local``` to run it on startup.

If you need to build AutoShade from the C-file, you can use the following command
```
gcc -Wall AutoShade.c spa.c -o AutoShade `pkg-config --cflags --libs gtk+-2.0` -lm -lgps -w
```

### Running the Program
Once you have built the program you need a graphical environment to run it in. If you setup your Raspberry Pi to include a GUI login manager and X window server, you can simply VNC into your Raspberry Pi. 
You can also connect your Raspberry Pi via HDMI to a display to access the GUI. If you did a light operating system install without GUI compatability, you will need to install a login and deskop manager (like lightdm). 
You could also reinstall the OS with a GUI.

If your GPS module is wired up to the Raspberry Pi and you are in an area with a good GPS signal, you can run the program using the following command:
``` ./AutoShade```
This will open up a window that will display a black spot that emulates a shaded region on your windshield to block sunlight from entering your eyes.

### Testing Without GPS
If you do not have a GPS signal, or you simply want to test the program given certain inputs, the AutoShade program takes three arguments in the following order:
>	Azimuth (the solar azimuth - north is 0, east is 90, south is 180, west is 270)

>	Zenith (the solar zenith - note the zenith is the normal vector to gravity so when the sun is on the horizon, the zenith angle is 90 not 0)

>	Heading	(the direction in which the user is heading - uses same numbering system as azimuth)

For example, if the solar azimuth is 90 and its zenith is 80 and the user's heading is 100, we expect to see a black spot somewhere near the left of our simulated windshield. 
The following command runs the program with these specific parameters:
```./AutoShade 90 80 100```

## Solar Position Algorithm

### Important Definitions
>	[Azimuth](https://en.wikipedia.org/wiki/Azimuth)

>	[Zenith](https://en.wikipedia.org/wiki/Zenith)

>	[Solar Azimuth](https://en.wikipedia.org/wiki/Solar_azimuth_angle)

>	[Solar Zenith](https://en.wikipedia.org/wiki/Solar_zenith_angle)

### Adjustable Field of View

The program has three parameters that can be tweaked to adjust for different FOV in different cars.
>	PHI1 - The driver's left FOV angle. This measures the angle to the driver's [left A-pillar](https://en.wikipedia.org/wiki/Pillar_(car))

>	PHI2 - The driver's right FOV angle. This measures the angle to the driver's [right A-pillar](https://en.wikipedia.org/wiki/Pillar_(car))

Note: in cars where the driver sits on the left hand side, the driver's right FOV angle should be slightly larger than his/her left FOV. The reverse applies for cars in which the driver sits on the right - like in Europe or India.

>	GAMMA - The minimum solar zenith angle the driver can view. In cars with [flat windshields](https://en.wikipedia.org/wiki/Vehicle_blind_spot#Flat_windshields) the driver can view the sun when it is higher in the sky (lower zenith angle) than they can in cars with steeper windshields (like semitrucks).

**For all questions related to the Solar Position Alogrithm (SPA) please refer to the NREL_SPA.pdf document.**

## Further Work
The next task is to implement an [IMU/Magntometer sensor](https://learn.adafruit.com/adafruit-bno055-absolute-orientation-sensor/overview) in order to pull heading data from the magnetometer rather than the GPS heading, which will improve shade spot accuracy at lower speeds.
We also wish to use gyroscopic data to determine the Euler angles of the vehicle to correct for driver field-of-view. For example, when driving up a hill, the driver can view the sun at a lower zenith angle.

## References
Reda, I.; Andreas, A. (2003). Solar Position Algorithm for Solar Radiation Applications. 55 pp.; NREL Report No. TP-560-34302, Revised January 2008. 
