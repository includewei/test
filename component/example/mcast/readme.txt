##################################################################################
#                                                                                #
#                             example_mcast                                      #
#                                                                                #
##################################################################################

Date: 2018-06-11

Table of Contents
~~~~~~~~~~~~~~~~~
 - Description
 - Setup Guide
 - Parameter Setting and Configuration
 - Result description
 - Supported List

 
Description
~~~~~~~~~~~
        This example will join multicast group of 224.0.0.251 and listen on port 5353 
        Send packet with the content of received packet to multicast group of 224.0.0.251
	
Setup Guide
~~~~~~~~~~~
	1.In lwipopts.h, please set #define LWIP_UDP  and #define LWIP_IGMP    1
    2.In platform_opts.h  #define CONFIG_EXAMPLE_MCAST    1
	For AmebaD2 :Amebad2 Changes how example is compiled, and Removed macro controls (CONFIG_EXAMPLE_XXX)in platform_opts.h
		GCC:  use CMD "make xip EXAMPLE=mcast" to compile mcast example.

Parameter Setting and Configuration
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        Can make automatical Wi-Fi connection when booting by using wlan fast connect example.
		
Result description
~~~~~~~~~~~~~~~~~~
        A multicast example thread will be started automatically when booting.

Supported List
~~~~~~~~~~~~~~
[Supported List]
        Supported IC :
                Ameba-pro, Ameba-z2, Ameba-D, AmebaD2