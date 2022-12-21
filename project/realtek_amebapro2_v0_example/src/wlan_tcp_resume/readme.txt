Test Description

This is a resume test example.

[TCP reume]
Requirement Components:
1. create a TCP server
2. replace main.c
   -- modify server_ip and server_port according the TCP server
   -- #define SSL_KEEPALIVE 0

Execution
1. example will connect to TCP server and write application data
2. input "PS=wowlan" to enter sleep.
3. wlan fw will send keep alive data.
4. If TCP server sends data to device, device will wakeup, and example will do wlan resume, tcp resume, and write application data.
5. input "PS=wowlan" to enter sleep again or TCP server close TCP connection to stop example

[SSL resume]
Requirement Components:
1. create a SSL server (ex. openssl s_server -port xxxx -cert ./xxx.crt -key ./xxx.key -CAfile ./xxx.crt -debug)
2. replace main.c
   -- modify server_ip and server_port according the SSL server
   -- #define SSL_KEEPALIVE 1

Execution
1. example will connect to SSL server and write application data
2. input "PS=wowlan" to enter sleep.
3. wlan fw will send keep alive data.
4. If SSL server sends data to device, device will wakeup, and example will do wlan resume, tcp resume, ssl resume, and write application data.
5. input "PS=wowlan" to enter sleep again or SSL server close SSL connection to stop example
