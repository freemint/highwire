# Vendored libraries

The image decoders HighWire links against, prebuilt for m68k-atari-mint.
Neither the toolkit container nor the FreeMiNT packages CI installs carry
them, so they live here and every build links the same files. zlib is not
here: the toolchain already has it.

| Library | Version | Origin | Licence |
|---|---|---|---|
| `giflib/` | 5.1.4 | `giflib-5.1.4-mint-dev.tar.xz` | MIT, in `COPYING` |
| `libpng/` | 1.6.34 | `libpng-1.6.34-mint-dev.tar.xz` | libpng, in `LICENSE` |
| `jpeg/` | 8d | `jpeg-8d-mint-dev.tar.xz` | IJG, under LEGAL ISSUES in `README` |

The tarballs are Thorsten Otto's MiNT builds, from
<https://mikro.atari.org/tho-otto.de/mint/>. Only the headers HighWire
includes and the three multilibs the Makefile picks are kept:

- the top level, for the 68000 and any build with `FPU=0`
- `m68020-60/`, for the 020 and up with an FPU
- `m5475/`, for ColdFire

The tarballs carry no licence texts, so each comes from that project's own
release of the same version. libpng's archive ships as `libpng16.a` behind a
`libpng.a` symlink; it is stored here as `libpng.a` so a checkout needs no
links.

The IJG licence asks that a binary-only release say, in its documentation,
that it "is based in part on the work of the Independent JPEG Group".
