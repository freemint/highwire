Compiling Highwire Source Code
Author: Dan Ackerman
Date:  January 15, 2001

Purpose:  This text will cover some information necessary
to compile the Highwire Source code on your machine.

Which PRJ file do I use?

   Currently the Highwire Development team have programmers
using the following versions of C, this list will show what
file to use when compiling.

  PureC    ->  SRC\PUREWIRE.PRJ

  LatticeC ->  SRC\HIGHWIRE.PRJ

  GCC      ->  makefile

   The Pure C and Lattice C routes are no longer maintained and
nobody is known to build that way now.  Treat them as untested
rather than broken: the project files still list every source, and
they were kept in step when the sources moved into SRC, so they
ought to work.

Building with GCC

   The simplest way is the Atari cross compiler in a container,
which carries the m68k-atari-mint toolchain and its libraries, so
nothing has to be installed on the host:

  https://github.com/sidecartridge/atarist-toolkit-docker

   It provides stcmd, which runs a command inside the container.
From the top of the source tree:

  stcmd make            the default, a 68000 build
  stcmd make 000        68000: ST, STE, Mega ST and STE
  stcmd make 030        68030 with no FPU: any Falcon or TT
  stcmd make 03f        68030 with a 68881/2 fitted, and faster
  stcmd make 040        68040
  stcmd make 060        68060
  stcmd make v4e        ColdFire, for the FireBee
  stcmd make dist       every one of them, side by side

   Object files go in build/, and the program in
dist/highwire.prg.  The dist target puts every build in dist/
under its own name, then zips the lot into build/hwYYMMDD.zip.

   The image libraries -- giflib, libpng and libjpeg -- are in
neither the container nor the FreeMiNT packages, so lib/ carries
them prebuilt for each CPU.  lib/README.md says where they came
from.

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