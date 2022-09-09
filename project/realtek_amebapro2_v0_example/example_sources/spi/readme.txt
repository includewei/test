Example Description

This example describes how to use SPI read/write by mbed api.


The SPI Interface provides a "Serial Peripheral Interface" Master.

This interface can be used for communication with SPI slave devices,
such as modules or integrated circuits.

In this example, it use 2 sets of SPI. One is master, the other is slave.
By default it use SPI0 as slave, and use SPI1 as master.
So we connect them as below:
    Connect SPI0_MOSI (PE_7) to SPI1_MOSI (PF_9)
    Connect SPI0_MISO (PE_6) to SPI1_MISO (PF_7)
    Connect SPI0_SCLK (PE_5) to SPI1_SCLK (PF_8)
    Connect SPI0_CS   (PE_8) to SPI1_CS   (PF_6)


After boot up, the master will send data to slave and shows result on LOG_OUT.