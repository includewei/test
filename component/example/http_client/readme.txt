##################################################################################
#                                                                                #
#                           example_http_client                                  #
#                                                                                #
##################################################################################

Date: 2018-06-11

Table of Contents
~~~~~~~~~~~~~~~~~
 - Description
 - Setup Guide
 - Result description
 - Parameter Setting and Configuration
 - Other Reference
 - Supported List

 
Description
~~~~~~~~~~~
        In this example, http client send request to get server response.
	
Setup Guide
~~~~~~~~~~~
        In platform_opts.h, please set #define CONFIG_EXAMPLE_HTTP_CLIENT 1
		For AmebaD2: Amebad2 Changes how example is compiled, and Removed macro controls (CONFIG_EXAMPLE_XXX)in platform_opts.h
			GCC: use CMD "make xip EXAMPLE=http_client" to compile http_client example.

Parameter Setting and Configuration
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        Change HOST_NAME and HOST_PORT to user define.
	
Result description
~~~~~~~~~~~~~~~~~~
        This example will call http_client() to send http request.

Supported List
~~~~~~~~~~~~~~
[Supported List]
        Supported IC :
                Ameba-pro, Ameba-z2, AmebaD, Ameba-D2