# Replacement Tige TAPS Trim Controller

This project implements a replacement for the trim controller found on my 2003 Tige 22ve wakeboard boat.
The *electric actuator* is made by Lenco, the Tige logo'd trim indicator gauge is probably made by Faria, and the
"black box" controller is under the dashboard.  The "black box" takes the trim switch input and drives the actuator
and gauge.  Coupled with a plate on the transom, Tige calls the entire system **TAPS**.

The original "black box" is prone to failure, and is no longer available from Lenco or Tige.  There are
other solutions from Lenco but I don't know of any which retain the use of the original trim switch and Tige
gauge.

This project improves on the original implementation by remembering the final actuator position at the next
restart.  The original TAPS controller fully retracts the actuator at each start so you need to
reposition it every time.  This replacement leaves the actuator alone.

The controller's external connections are power+ground, trim switch top/middle/bottom,
gauge power/ground/signal, and two actuator motor leads.  The controller box has a redundant trim switch,
status light, and pushbutton.

This project includes the controller's schematic diagram and PCB layout, a 3D printable enclosure, and the C++
software to run the show.  Assuming you have a 3D printer, the component cost of the controller should be less
than $50.

