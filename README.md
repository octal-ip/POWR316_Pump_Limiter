# POWR316_Pump_Limiter
This project provides PlatformIO code for building custom firmware for the SonOff POWR316. It was developed to limit the amount of time a load can continuously run for - an important safety measure for situations where you want to avoid a runway condition occurring with your AC load. My specific use case was to ensure an automatic water pressure pump didn't have an opportunity to empty an entire water tank if there was a leak or rupture in the connected plumbing. With this firmware, the pump would only be permitted to run in 15 minute intervals. If the 15 minute threshold is reached, the load is switched off, and can be reset (turned on again) by pressing the button on the front of the POWR316.

The POWR316 contains an ESP32 with the 3.3v, GND, RX and TX pins all conveniently broken out to a standard 0.1" pin header on the PCB. The button is also connected to GPIO-0, so the serial bootloader can be easily accessed.

#### While I've not tested this, some sources say that the GND pin is not isolated from AC mains. For this reason, it's best NOT to connect the POWR316 to an AC source when uploading firmware through the pin headers. Provide an independent 3.3v power source while the POWR316 is open.


