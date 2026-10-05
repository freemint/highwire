Since version 0.4.8 HighWire gets its online capability by loading an external
LDG located in its folder ./modules/. Choose the adequate file for your
prefered IP-stack and rename it into 'network.ldg' (or symlink to it, on a more
comfortable filesystem).

HighWire will always try to load 'network.ldg' from this directory;  if this
doesn't exist, HighWire will give up and doesn't support any online stuff for
this session.
