# <img src="html/images/logotst2.gif" alt="HighWire" />

Welcome to the home of the HighWire web browser for the Atari ST, TT, Falcon and FireBee.

HighWire runs on any ST compatible machine, no GDOS required, and renders using SpeedoGDOS, NVDI or fVDI if you have one, but you'll need to install a TCP/IP stack (e.g. STinG, STiK2, MiNTnet) to get online.

If your ST isn't already online, take a look at [MD/Net](https://downloads.neilrackett.com/md-net), which has everything you need to get up and running.

[![Build Status](https://github.com/neilrackett/atarist-highwire/actions/workflows/release.yml/badge.svg?branch=main)](https://github.com/neilrackett/atarist-highwire/actions)

## Install

- Download the latest snapshot below
- Extract the zip file to your hard disk
- Run `HIGHWIRE.PRG`

It arrives set up for a plain ST running STinG: `HIGHWIRE.PRG` is the 68000
build, and `MODULES\NETWORK.OVL` is STinG. Both are copies, so everything else
is still in the archive to swap in.

**For a faster machine**, copy the build that suits it over `HIGHWIRE.PRG`:

- `.000` — any 68000: ST, STE, Mega ST/STE _(what you get by default)_
- `.030` — 68030: Falcon, TT (works with or without an FPU)
- `.03F` — 68030 with a 68881/2 FPU fitted (same, but faster)
- `.040` / `.060` — accelerated machines with FPU
- `.v4e` — ColdFire (FireBee)

**For a different TCP/IP stack**, copy the matching `.OVL` from `MODULES` over
`MODULES\NETWORK.OVL` — `MINTNET.OVL`, `STIK2.OVL`, `MAGICNET.OVL` or
`ICONNECT.OVL`.

## Downloads

- [Latest snapshot](https://downloads.neilrackett.com/atarist-highwire/highwire-latest.zip)
- [Earlier builds](https://downloads.neilrackett.com/atarist-highwire)
- [Snapshots before August 2026](https://atari.joska.no/snapshots/highwire/)

## Building

With [atarist-toolkit-docker](https://github.com/sidecartridge/atarist-toolkit-docker)
installed:

```
stcmd make
```

This produces the 68000 build, `dist/highwire.prg`. `stcmd make dist` builds
every CPU variant and the release zip, exactly as the snapshots are built.
[COMPILIN.G](COMPILIN.G) lists the other targets.
