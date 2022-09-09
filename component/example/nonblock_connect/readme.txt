LWIP SOCKET NONBLOCKING CONNECT EXAMPLE

Description:
TCP nonblocking connect with use of select() for connection timeout handling.

Configuration:
Modify SERVER_IP and SERVER_PORT in example_nonblock_connect.c for server connection

[platform_opts.h]
	#define CONFIG_EXAMPLE_NONBLOCK_CONNECT    1
	For AmebaD2 :Amebad2 Changes how example is compiled, and Removed macro controls (CONFIG_EXAMPLE_XXX)in platform_opts.h
		GCC: use CMD "make xip EXAMPLE=nonblock_connect" to compile nonblock_connect example.

[lwipopts.h]
	#define LWIP_SOCKET_SET_ERRNO 1

Execution:
Can make automatical Wi-Fi connection when booting by using wlan fast connect example.
A socket nonblocking connect example thread will be started automatically when booting.

[Supported List]
	Supported :
	    Ameba-1, Ameba-z, Ameba-pro, Ameba-z2, Ameba-D, AmebaD2