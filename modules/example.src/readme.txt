SAMPLE.OVL
Dan Ackeman aka baldrick@netset.com
January 9, 2003

This is example HighWire OVL source code.
It does the basics that we need an OVL to do.
And nothing else...

	I've set this example up to have the client do the 2 Pexecs (3 & 4).
You don't have to do this.  If you decide you don't want to use this
method, you can get by with just a Pexec(4).  However you will want
to remove the appl_init and appl_exit calls from the OVL.  You will
also need to look at the Basepage information from the ovl and do 
a Mshrink on it, or else you will discover your machine does not have
as much memory as it used to.

	Some extra informational notes on the archive:
	
	1. I used PureC to compile this milelage on other compilers may
			vary.
			
	2. The ovl_sys.h file is the same for the OVL and HighWire.
	   The programmer needs to define OVL_MODULE in there OVL
	   to keep the compiler from adding in conflicting defines
	   that HighWire will use.
		
	3. Code for the OVL is in the root directory of the archive.
	   
