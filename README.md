# NTP

This is my implementation of a NTP client/server. To make, simply type ```make``` to build the client and server binaries. The NTP server expects to be ran on port 8080. Currently, the basic NTP implementation seems to be working, but a potential source of error is in the offset/delay calculations. Instead of doing 64 bit floating point operations to deduce time, I am subtracting NTP time (seconds and fractional components). In this case, offset and delay are measured as NTP fractional components. I am treating offset as a signed integer as outlined by the NTP documentation, but am leaving delay unsigned for now, as delay is not something that can be interpreted as a negative number.

## Raw Data

Raw data has the form | T1.seconds | T1.fraction | T2.seconds | T2.fraction | T3.seconds | T3.fraction | T4.seconds | T4.fraction 