# ucesb for DESPEC

ucesb (Unpack and Check Every Single Bit) is a general analysis unpacker and "utility"

This version is specially modified for the DESPEC DAQ, where it functions as a general purpose utility
translating raw MBS data into structures easier to handle by the Go4 analysis procedure.

This does *not* work as a (useful) unpacker, it only unpacks a few variables for monitoring.

Its main purpose is AIDA event building and time-stitching, both online and offline.
It also contains a simple online monitor UI based on the ucesb watcher.

## Compiling
To compile you can just run `make` in this directory, which will run the proper ucesb make to build `despec` 

You can change some options in `makefile_addictional.inc` to control the building.
If you are at GSI you may want to disable ZeroMQ (the remote monitoring code) to avoid needing the libraries.
If you want to build it you will need Google Protobuf and ZeroMQ for C/C++

After compiling you can try `./despec --help` to see all the options


## AIDA
The command `--aida` activates AIDA event building mode, which transforms 64KB AIDA event blocks
from MIDAS into small blocks corresponding to physical events.
The default settings work fine, AIDA events are split by 2.2us gaps.

In addition the AIDA MBS header is shifted backwards by 14us, aligning it closer to other DAQs.

Finally this procedure enforces strict time order of the outcoming data, it will discard
events (warning you) if the time goes out of sync. This should NOT happen normally.

## Time-Stitching
This stitches MBS subevents together that occur "close" in time, allowing coincidences to be found.
Unlike vanilla `ucesbb` this time-stitching waits for *gaps* rather than defining a window from the first event.
This works a lot better for the uncorrelated noise that AIDA produces.

## Examples

To time-stitch a number of files and produce time-stitched LMDs
`./despec /path/to/*.lmd --aida --time-stitch=wr,2000 --output=wp,size=2000Mi,/path/to/ts/lmd_name_0001.lmd`
This will produce 2GB time-stitched LMD files, unpacking AIDA data.

To monitor the online DAQ and function as a Go4 event server
`./despec --aida --time-stitch=wr,2000 --server=trans stream://x86l-8 --watcher=TIMEOUT=1` 
This will output time-stitched MBS data as an "MBS transport server" for Go4.
It connects to MBS via the "stream server" protocol ensuring MBS never waits for ucesb/Go4
`--watcher` starts the UI to monitor the rates and scalers. The TIMEOUT makes it update every 1 second.


