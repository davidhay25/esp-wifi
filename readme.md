
# Wifi on the ESP32 


Working code to use wifi.


When it starts, it uses a 'default' IP of 192.168.20.99  The API endpoint POST /api/setip is used to change the
IP address. The IP address is saved in NVS (Non Volatile Storage) to make it easier to have multiple ESPs.

An obvious security risk, but given that it's inside the home network should be OK for now...


Establishes the following endpoints:


GET [host]/api/info - information about the chip - memory ATM
