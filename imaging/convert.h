typedef struct rbg255
          {  unsigned short red,
                            green,
                            blue;
          } RGB255;

typedef RGB255 * PALETTE;

extern unsigned short probit[];

int transform_image(MFDB *ptr, PALETTE pal);

/* transform_image takes an image in VDI standard format,
   defined by the MFDB structure *ptr and converts it to
   the current screen format, modifying the MFDB to suit.

   The colours in PALETTE are restricted to byte size,
   (0..255)
*/