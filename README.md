httpwatch
=======

Periodic HTTP watcher for subproject

This is a little subproject I did for a customer who had a particular need to get the HTTP result code of a few thousand of websites. 
Given a particular INI file filled with entries for the targets to hit, run a runscript in the event the expected response code fails.

# Purpose

This particular customers requirements were such that they wanted to monitor a couple of thousand websites periodically (every n seconds). Most commercial or open source software really isn't designed to scale to this.

This software is capable (bandwith permitting) of polling about 3000 website every minute or so, but this iteration is not designed to cope with a few thousand failures.

A future release for this particular client solved this problem by using an accumulator to send the result of a few thousand websites to one executable instead of spawning multiple executables at once.

Requirements
------------

This uses libcurl and libcap-ng (capng is used to drop privs before executing a runscript although I imagine its still not safe to run as root).

# Implementation

The program is pretty thin on external requirements, its not designed to run as root. We ran it in a non-root container for the client to limit exposure.

It uses timerfds, epoll and curl_multi to handle the scaled load. Also uses a signalfd since if you're gonna go epoll, do it properly.

# Performance

For the clients needs this was fine, but to scale this above thousands to tens of thousands of page impressions per minute the software would need to be written to use multiple threads (one per core) and the event management mechanism to post to a work queue instead.

I implemented such a scheme in a closed source program.

Horizontal scaling to millions would require the use of a system to distribute work to 'nodes agents' and wait on their responses instead. I haven't needed to do this though!  
