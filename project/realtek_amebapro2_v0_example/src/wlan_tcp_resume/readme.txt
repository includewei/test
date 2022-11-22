Test Description

This is a resume test example.

Requirement Components:
1. create a TCP server
2. replace main.c
   -- modify server_ip and server_port according the TCP server

Execution
1. example will connect to TCP server and write application data
2. input "PS=wowlan" to enter sleep.
3. wlan fw will send keep alive data.
4. If TCP server sends data to device, device will wakeup, and example will do wlan resume, tcp resume, and write application data.
5. input "PS=wowlan" to enter sleep again or TCP server close TCP connection to stop example