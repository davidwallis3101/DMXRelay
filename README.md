# DMXRelay
DMX Relay Board

Gathering info


## Programming
*Snipped from forum here https://www.qlcplus.org/forum/viewtopic.php?t=11071*

To load the hex file onto the mcu you'll need the STC ISP programming software from STC: http://www.stcmicro.com/rjxz.html

Programming is done using serial communication through the RX/TX lines of a comm port.

I programmed the STC11L04E mcu while it was plugged into the MINI-DMX-3CH board. The Rx pin on the mcu that is used for programming is also used to receive DMX512 so this pin should be isolated from the DMX512 circuitry while programming. I just used another DIP20 socket between the mcu and the board and bent the RX and TX pins out. You could also program the mcu by plugging it into a breadboard and providing power to it (I don't think an external clock is needed for programming).
I used a cheap USB-UART converter using the CP2102 IC. Connect RX/TX lines and ground between the mcu and the UART converter. Do not connect directly to an RS232 serial port, the voltages will probably kill the mcu. The RX pin on the mcu is Pin2 and the TX pin on the mcu is Pin3. Remember that TX from the mcu connects to RX on the PC and RX connects to TX ;)

In the STC-ISP software click "Open Code File" and load the HEX file. Then make sure the proper settings are ticked off in the H/W Option tab. You can see the existing options on the IC before programming by clicking "Check MCU" in the lower left on the screen. You'll need to cycle power to the mcu after pressing the button. The new HEX file DOES feed the watchdog so no need to disable it :D :D :D Next click on "Download/Program" to load the HEX file onto the mcu! I noticed that I sometimes needed to cycle power a few times to the mcu before the program would recognize it.
