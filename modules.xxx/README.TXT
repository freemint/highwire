Since version 0.1.2 HighWire gets its online capability by loading an external
OVL (to receive online documents).  There is support for the two main IP-stacks
now:  'STik2.ovl' and 'MiNTnet.ovl'.

HighWire will always try to load 'network.ovl' from this directory;  if this
doesn't exist, HighWire will give up and doesn't support any online stuff for
this session.

So rename the OVL with your prefered IP-stack (or symlink to it, on a more
comfortable filesystem) to 'network.ovl' and Highwire will happily load from
the net  :-)
