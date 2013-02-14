@mainpage Carambola client overview

The software detailed in this documentation was designed and implemented for
the purposes of connecting a
[Carambola](http://www.8devices.com/product/3/carambola "Carambola") to the
Firefly's (designed by Sidhant Gupta) microprocessor and a server hosted on
a local network.

Software
========

The client software consists of two main entities: the consumer and relay.
Their purposes are as follows.

Consumer
--------

The consumer is a realtime process designed to handle all the communication
with the Firefly's microprocessor. Its task is singular: to move any data
available in the micro's data buffer to a larger buffer located on the
Carambola. This buffer, described in buffer.h, is shared between the
consumer and relay such that the relay may take data in that buffer and
process it. Because of the memory constraints of the microprocessor, the
consumer must always be able to read data in the micro's buffer at a
consistent fast pace. For this reason, the consumer does little additional
processing. In the case of the relay process dying, the consumer will handle
restarting the relay and keeping the data safe on the large SD card storage
medium.

Relay
-----

The relay is a process tasked with moving any data available in either a)
the shared buffer, or b) the SD card storage medium. The destination for
this data will be a server located on the same local network as the
Carambola. This is to ensure that network latency is minimal, so that a
minimal amount of processor time will be used sending the data across the
network.

@authors Larson, Patrick; Pickett, Cameron

