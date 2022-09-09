SSL SERVER EXAMPLE

Description:
A simple SSL server which can reply for the https request.

Configuration:

Modify SERVER_PORT and response content in example_ssl_server.c based on your SSL server.

For PolarSSL:
[..\polarssl\config_rsa.h]
	#define POLARSSL_CERTS_C
	#define POLARSSL_SSL_SRV_C
For MbedTLS:
[..\bedtls\config_rsa.h]
	#define MBEDTLS_CERTS_C
	#define MBEDTLS_SSL_SRV_C

[platform_opts.h]
	#define CONFIG_EXAMPLE_SSL_SERVER	1
For AmebaD2 :Amebad2 Changes how example is compiled, and Removed macro controls (CONFIG_EXAMPLE_XXX)in platform_opts.h
	GCC:use CMD "make xip EXAMPLE=ssl_server" to compile ssl_server example

Execution:
Can make automatical Wi-Fi connection when booting by using wlan fast connect example.
A ssl server example thread will be started automatically when booting.

[Supported List]
	Supported :
	    Ameba-1, Ameba-z, Ameba-z2, AmebaD, AmebaD2
	Source code not in project:
	    Ameba-pro