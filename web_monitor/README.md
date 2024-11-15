# ucesb for WebSocket Monitoring

ucesb (Unpack and Check Every Single Bit) is a general analysis unpacker and "utility"

This version is a skeleton for online monitor as used in DESPEC (and soon FRS)
It can be used to unpack useful variables to monitor and send them to a web page, for example as a scaler reader
This does *not* work as a (useful) unpacker, it only unpacks a few variables for monitoring.

## Compiling
To compile you can just run `make` in this directory, which will run the proper ucesb make to build `web_monitor` 

You can change some options in `makefile_addictional.inc` to control the building.
Because this program will be useless without the ZeroMQ stuff, you will need to edit the paths 
for Google Protobuf and ZeroMQ for C and C++

In the subfolder ExampleWS is a relay program, you should build this after ucesb

## Basic Flow

This hijacks the ucesb 'watcher' to collect information, which it should use
to fill a google protobuf structure defined in ucesb.proto
Periodically the watcher will send this protobuf over a 0MQ socket to any listeners
(The default is to send it every second from the \_display function, which
is controlled by ucesb --watcher=TIMEOUT=X functions, and to update
a potentially smaller amount of data every 0.1 secs (used for more fluid scalers, for example)

The 0MQ socket is read by the ExampleWS utility, which should be running on
some Internet enabled server which can Reverse Proxy WebSocket requests
It then forwards the protobuf JSON encoded over a WebSocket connection
to any client which connects

The clients can then process the data in JavaScript

For testing you can connect directly to the ExampleWS server (it listens on port 9001)
You can visit http://localhost:9001/ws\_debug for example to view the JSON without 
a websocket connection

For a more complete example of what can be monitored, check the 'despec' folder
The idea is this is a simpler "skeleton" to start with.
The "DAQ Status" part of 'despec' is relatively generic and could probably
be imported to any watcher intended to spy on a 'time orderer' stream.

Real-time performance depends mostly on DAQ buffering, you should assume
a latency of a few seconds, if you need faster feedback you should bypass the
DAQ (see KN1681 FRS scalers, for example)

## Ucesb is kinda hacked

This probably won't work on the upstream ucesb, there are some modifications
to make it work. In particular it has some hacks to spot WR outside of the
.spec file, as well as to send some more callbacks to the watcher
(such as when the DAQ stops sending data to ucesb)
