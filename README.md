# Knx_Esp32_Server
Knx running on ESP32 multicore with Webserber, Webclient, Timeserver, Mqtt-> Knx Bridge

based on **WT32_ETH01**  http://www.wireless-tag.com/portfolio/wt32-eth01/  and a serial2usb stick (USB2.0 to TTL UART 1,50€ on ebay) for download an powersupply and NanoBcu for Knx commcunication

based on knx driver from Franc Marini with small changes -> update driver with fork: https://github.com/AndreasMainz/KnxDevice.git
**Tpuart is connected to RXD(5)/TXD(17) pins of WT32**

Additional buzzer for criticals events and on own GP is available
Time via NTP also available and Webinterface to configure ip adresses (feature not yet finished)
Runtime supervision for Knx.task with own mqtt message

Get your Esp Knx Server for less than 20 bucks ;-)
