Compiling Highwire Source Code
Author: Dan Ackerman
Date:  January 15, 2001

Purpose:  This text will cover some information necessary
to compile the Highwire Source code on your machine.

Which PRJ file do I use?

   Currently the Highwire Development team have programmers
using the following versions of C, this list will show what
file to use when compiling.

  PureC    ->  PUREWIRE.PRJ

  LatticeC ->  HIGHWIRE.PRJ

  GCC      ->  makefile

GEM Lib use

   Highwire has now been converted to using the GEM Lib as
a standard across all compilers.  This simplifies things
across the project.  If you don't already have GEM Lib installed
on your system you can get Lattice and PureC versions from
the Highwire list and the GCC version is available at sparemint.

Compiling with a Menu Bar

   While a normal GEM Menu bar is not an active part of the
project.  It is possible to compile the code to use a GEM menubar,
by uncommenting the line #define GEM_MENU 1, in the GLOBAL.H file.
If you do not want a menu bar, make certain that this line is
commented out.

Other questions can be asked at the Highwire forums at
http://highwire.atari-users.net/ or via the Highwire mailing
list available for subscription at the same site.