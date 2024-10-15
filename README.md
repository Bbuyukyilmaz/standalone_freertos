# standalone_freertos
Standalone FreeRTOS implementation for Kria KV260. The source code in reporsitory are tested in Vitis 2024.1 IDE and the XSA file was created by Vivado 2024.1.


Kria KV260 board boot-mode is set to SD boot if you want to boot the device by JTAG then follow the steps below.
  - Connect the JTAG. (J3/J4)
  - Open XCST console and run following commands.
  
   targets -set -filter {name =~ "PSU"}
   
   mwr 0xffca0010 0x0
   
   mwr 0xff5e0200 0x0100
   
   rst -system


Now your device is ready to boot from JTAG. (If the board was powered down, the boot mode will return to its default configuration. You must follow the same steps again to boot the device via JTAG.)


