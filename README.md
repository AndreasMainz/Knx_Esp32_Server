# Knx_Esp32_Server
Knx running on ESP32 multicore with Webserber, Webclient, Timeserver, Mqtt-> Knx Bridge

based on WT32_ETH01 and a serial2usb stick for download an powersupply and NanoBcu for Knx commcunication
based on knx driver from Franc Marini with small changes -> update repo with tpuart.cpp fix for WT32!
Tpuart is connected to RXD(5)/TXD(17) pins of WT32 
Additional buzzer for criticals events and on own GP is available
