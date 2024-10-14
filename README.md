# standalone_freertos
Standalone FreeRTOS implementation for Kria KV260.


Kria KV260 board boot-mode is set to SD boot if you want to boot the device by JTAG then follow the steps below.
  - Connect the JTAG. (J3/J4)
  - Open XCST console and run following commands.
  
   targets -set -filter {name =~ "PSU"}
   
   mwr 0xffca0010 0x0
   
   mwr 0xff5e0200 0x0100
   
   st -system


Now your device ready to boot from JTAG. (!If board power down then boot-mode turn to default configuration, you must follow same steps in order to boot the device via JTAG.)


