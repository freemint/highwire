/*
 * A GIF decoder for HighWire. It is responsible for decoding GIF data
 * and transferring it to an MFDB defined block.
 *
 * It is adapted from the code in Dillo which was
 * adapted by Raph Levien from giftopnm.c as found in the
 * netpbm-1mar1994 release. The copyright notice for giftopnm.c
 * follows:
 */

/* +-------------------------------------------------------------------+ */
/* | Copyright 1990, 1991, 1993, David Koblas.  (koblas@netcom.com)    | */
/* |   Permission to use, copy, modify, and distribute this software   | */
/* |   and its documentation for any purpose and without fee is hereby | */
/* |   granted, provided that the above copyright notice appear in all | */
/* |   copies and that both that copyright notice and this permission  | */
/* |   notice appear in supporting documentation.  This software is    | */
/* |   provided "as is" without express or implied warranty.           | */
/* +-------------------------------------------------------------------+ */

#include "GLOBAL.H"
#include <gem.h>

/*#include <stdlib.h>*/
#include <osbind.h>
#include <string.h>

#include "convert.h"
#include "hwgif.h"

#define INTERLACE      0x40
#define LOCALCOLORMAP  0x80

#define LM_to_uint(a,b)   ((((unsigned char)b)<<8)|((unsigned char)a))

#define        MAXCOLORMAPSIZE         256
#define        MAX_LWZ_BITS            12

#define min(a, b)             ((a) < (b) ? (a) : (b))
#define max(a, b)             ((a) > (b) ? (a) : (b))


typedef struct _HWGif {
   MFDB *mfdb;

   WORD state;
   size_t Start_Ofs;
   UWORD Flags;

   unsigned char input_code_size;
   unsigned char *linebuf;
   WORD pass;

   UWORD y;

   /* state for lwz_read_byte */
   WORD code_size;

   /* The original GifScreen from giftopnm */
   UWORD Width;
   UWORD Height;
   PALETTE palette;
   UWORD ColourResolution;
   UWORD NumColours;
   WORD Background;
   UWORD spill_line_index;
#if 0
   UWORD AspectRatio;    /* AspectRatio (not used) */
#endif

   /* Gif89 extensions */
   WORD transparent;
#if 0
   /* None are used: */
   WORD delayTime;
   WORD inputFlag;
   WORD disposal;
#endif

   /* state for the new push-oriented decoder */
   WORD packet_size;       /* The amount of the data block left to process */
   unsigned long window;
   WORD bits_in_window;
   UWORD last_code;  /* Last "compressed" code in the look up table */
   UWORD line_index;
   unsigned char **spill_lines;
   WORD num_spill_lines_max;
   WORD length[(1 << MAX_LWZ_BITS) + 1];
   unsigned long code_and_byte[(1 << MAX_LWZ_BITS) + 1];
} HWGif;


/*
 * Create a new gif structure for decoding a gif into a MFDB buffer
 */
HWGif *Gif_new(char *Image)
{
   HWGif *gif = (HWGif *) malloc(sizeof(HWGif));
   gif->mfdb = (MFDB *) malloc(sizeof(MFDB));
   gif->Flags = 0;
   gif->state = 0;
   gif->Start_Ofs = 0;
   gif->linebuf = NULL;
   gif->palette = NULL;
   gif->Background = -1;
   gif->transparent = -1;
   gif->num_spill_lines_max = 0;
   gif->spill_lines = NULL;
   gif->window = 0L;
   gif->packet_size = 0;

   return gif;
}


void Gif_lwz_init(HWGif *gif)
{
   gif->num_spill_lines_max = 1;
   gif->spill_lines = (unsigned char **) malloc(sizeof(unsigned char *)
                         * gif->num_spill_lines_max);

   gif->spill_lines[0] = (unsigned char *) malloc(gif->Width);
   gif->bits_in_window = 0;

   /* First code in table = clear_code +1
    * Last code in table = first code in table
    * clear_code = (1<< input code size)
    */
   gif->last_code = (1 << gif->input_code_size) + 1;
   memset(gif->code_and_byte, 0,
          (1 + gif->last_code) * sizeof(gif->code_and_byte[0]));
   gif->code_size = gif->input_code_size + 1;
   gif->line_index = 0;
}

void write_image(MFDB *mfdb, const unsigned char *linebuf, UWORD y)
{
  unsigned char *pline[8], *buf, temp[8] = {0,0,0,0,0,0,0,0};
  UWORD i, inbit, outbit, w;
  UWORD nplanes = mfdb->fd_nplanes;
  UWORD width = mfdb->fd_w;

  buf = linebuf;
  for (i = 0; i < nplanes; i++)
  {  pline[i] = (unsigned char *) mfdb->fd_addr + mfdb->fd_wdwidth * y * 2;
     pline[i] += (long) mfdb->fd_wdwidth * (long) mfdb->fd_h * i * 2;
  }
  for (w = 0; w < width; w++)
  {
     outbit = 7 - (w & 7);
     for (inbit = 0; inbit < nplanes; inbit++)
        if (buf[w] & probit[inbit])
           temp[inbit] |= probit[outbit];
     if (outbit == 0)
        for (i = 0; i < nplanes; i++)
        {
           *pline[i]++ = temp[i];
           temp[i] = 0;
        }
  }
  if (outbit != 0)    /* write last byte if necessary */
     for (i = 0; i < nplanes; i++)
         *pline[i] = temp[i];
}

/*
 * Save the image line, also handling the interlacing.
 */
void Gif_emit_line(HWGif *gif, const unsigned char *linebuf)
{
   write_image(gif->mfdb, linebuf, gif->y);
   if (gif->Flags & INTERLACE) {
      switch (gif->pass) {
      case 0:
      case 1:
         gif->y += 8;
         break;
      case 2:
         gif->y += 4;
         break;
      case 3:
         gif->y += 2;
         break;
      }
      if (gif->y >= gif->Height) {
         gif->pass++;
         switch (gif->pass) {
         case 1:
            gif->y = 4;
            break;
         case 2:
            gif->y = 2;
            break;
         case 3:
            gif->y = 1;
            break;
         default:
            /* arriving here is an error in the input image. */
            gif->y = 0;
            break;
         }
      }
   } else {
      if (gif->y < gif->Height)
         gif->y++;
   }
}

/*
 * Decode the packetized lwz bytes
 */
void Gif_literal(HWGif *gif, UWORD code)
{
   gif->linebuf[gif->line_index++] = (unsigned char) code;
   if (gif->line_index >= gif->Width) {
      Gif_emit_line(gif, gif->linebuf);
      gif->line_index = 0;
   }
   gif->length[gif->last_code + 1] = 2;
   gif->code_and_byte[gif->last_code + 1] = (code << 8);
   gif->code_and_byte[gif->last_code] |= code;
}

/*
 * ?
 */
/* Profiling reveals over half the GIF time is spent here: */
void Gif_sequence(HWGif *gif, UWORD code)
{
   UWORD o_index, o_size, orig_code;
   unsigned char *obuf = NULL;
   UWORD sequence_length = gif->length[code];
   UWORD line_index = gif->line_index;
   UWORD num_spill_lines;
   UWORD spill_line_index = gif->spill_line_index;
   unsigned char *last_byte_ptr;

   gif->length[gif->last_code + 1] = sequence_length + 1;
   gif->code_and_byte[gif->last_code + 1] = (code << 8);

   /* We're going to traverse the sequence backwards. Thus,
    * we need to allocate spill lines if the sequence won't
    * fit entirely within the present scan line. */
   if (line_index + sequence_length <= gif->Width) {
      num_spill_lines = 0;
      obuf = gif->linebuf;
      o_index = line_index + sequence_length;
      o_size = sequence_length - 1;
   } else {
      num_spill_lines = (line_index + sequence_length - 1) /
          gif->Width;
      o_index = (line_index + sequence_length - 1) % gif->Width + 1;
      if (num_spill_lines > gif->num_spill_lines_max) {
         /* Allocate more spill lines. */
         spill_line_index = gif->num_spill_lines_max;
         gif->num_spill_lines_max = num_spill_lines << 1;
         gif->spill_lines = (unsigned char **) realloc(gif->spill_lines,
                      gif->num_spill_lines_max * sizeof(unsigned char *));

         for (; spill_line_index < gif->num_spill_lines_max;
              spill_line_index++)
            gif->spill_lines[spill_line_index] =
                  (unsigned char *) malloc(gif->Width);
      }
      spill_line_index = num_spill_lines - 1;
      obuf = gif->spill_lines[spill_line_index];
      o_size = o_index;
   }
   gif->line_index = o_index;   /* for afterwards */

   /* for fixing up later if last_code == code */
   orig_code = code;
   last_byte_ptr = obuf + o_index - 1;

   /* spill lines are allocated, and we are clear to
    * write. This loop does not write the first byte of
    * the sequence, however (last byte traversed). */
   while (sequence_length > 1) {
      sequence_length -= o_size;
      /* Write o_size bytes to
       * obuf[o_index - o_size..o_index). */
      for (; o_size > 0 && o_index > 0; o_size--) {
         unsigned long code_and_byte = gif->code_and_byte[code];

         obuf[--o_index] = code_and_byte & 255;
         code = code_and_byte >> 8;
      }
      /* Prepare for writing to next line. */
      if (o_index == 0) {
         if (spill_line_index > 0) {
            spill_line_index--;
            obuf = gif->spill_lines[spill_line_index];
            o_size = gif->Width;
         } else {
            obuf = gif->linebuf;
            o_size = sequence_length - 1;
         }
         o_index = gif->Width;
      }
   }
   /* Ok, now we write the first byte of the sequence. */
   /* We are sure that the code is literal. */
   obuf[--o_index] = (unsigned char) code;
   gif->code_and_byte[gif->last_code] |= code;

   /* Fix up the output if the original code was last_code. */
   if (orig_code == gif->last_code) {
      *last_byte_ptr = code;
   }

   /* Output any full lines. */
   o_index = gif->line_index;
   if (gif->line_index >= gif->Width) {
      Gif_emit_line(gif, gif->linebuf);
      gif->line_index = 0;
   }
   if (num_spill_lines) {
      if (gif->line_index)
         Gif_emit_line(gif, gif->linebuf);
      for (spill_line_index = 0;
           spill_line_index < num_spill_lines - (o_index ? 1 : 0);
           spill_line_index++)
         Gif_emit_line(gif, gif->spill_lines[spill_line_index]);
   }
   if (num_spill_lines) {
      /* Swap the last spill line with the gif line, using
       * linebuf as the swap temporary. */
      unsigned char *linebuf = gif->spill_lines[num_spill_lines - 1];

      gif->spill_lines[num_spill_lines - 1] = gif->linebuf;
      gif->linebuf = linebuf;
   }
   gif->spill_line_index = spill_line_index;
}


/*
 * ?
 *
 * Return Value:
 *   2 -- quit
 *   1 -- new last code needs to be done
 *   0 -- okay, but reset the code table
 *   < 0 on error
 *   -1 if the decompression code was not in the lookup table
 */
WORD Gif_process_code(HWGif *gif, UWORD code, UWORD clear_code)
{

   /* A short table describing what to do with the code:
    * code < clear_code  : This is uncompressed, raw data
    * code== clear_code  : Reset the decompression table
    * code== clear_code+1: End of data stream
    * code > clear_code+1: Compressed code; look up in table
    */

   if (code < clear_code)
   {  /* a literal code. */
      Gif_literal(gif, code);
      return 1;
   } else if (code >= clear_code + 2)
   {  /* a sequence code. */
      if (code > gif->last_code)
         return -1;
      Gif_sequence(gif, code);
      return 1;
   } else if (code == clear_code)
      /* clear code. Resets the whole table */
      return 0;
     else
      /* end code. */
      return 2;
}


WORD Gif_decode(HWGif *gif, const unsigned char *buf, UWORD BSize)
{
   /*
    * Data block processing.  The image stuff is a series of data blocks.
    * Each data block is 1 to 256 bytes long.  The first byte is the length
    * of the data block.  0 == the last data block.
    */
   size_t bufsize, packet_size;
   UWORD clear_code;
   unsigned long window;
   UWORD bits_in_window;
   UWORD code;
   WORD code_size;
   UWORD code_mask;

   bufsize = BSize;

   /* Want to get all inner loop state into local variables. */
   packet_size = gif->packet_size;
   window = gif->window;
   bits_in_window = gif->bits_in_window;
   code_size = gif->code_size;
   code_mask = (1 << code_size) - 1;
   clear_code = 1 << gif->input_code_size;

   /* If packet size == 0, we are at the start of a data block.
    * The first byte of the data block indicates how big it is (0 == last
    * datablock)
    * packet size is set to this size; it indicates how much of the data block
    * we have left to process.
    */
   while (bufsize > 0) {
      /* lwz_bytes is the number of remaining lwz bytes in the packet. */
      UWORD lwz_bytes = min(packet_size, bufsize);

      bufsize -= lwz_bytes;
      packet_size -= lwz_bytes;
      for (; lwz_bytes > 0; lwz_bytes--)
      {
         /* printf ("%d ", *buf) would print the depacketized lwz stream. */

         /* Push the byte onto the "end" of the window (MSB).  The low order
          * bits always come first in the LZW stream. */

         window = (window >> 8) | (*buf++ << 24);
         bits_in_window += 8;

         while (bits_in_window >= code_size)
         {
            /* Extract the code.  The code is code_size (3 to 12) bits long,
             * at the start of the window */
            code = (window >> (32 - bits_in_window)) & code_mask;

            /* printf ("%d\n", code); would print the code stream. */
            bits_in_window -= code_size;
            switch (Gif_process_code(gif, code, clear_code))
            {
               case 1:             /* Increment last code */
                  gif->last_code++;
                  /*gif->code_and_byte[gif->last_code+1]=0; */

                  if ((gif->last_code & code_mask) == 0)
                  {   if (gif->last_code == (1 << MAX_LWZ_BITS))
                         gif->last_code--;
                      else
                      { code_size++;
                        code_mask = (1 << code_size) - 1;
                      }
                  }
                  break;

               case 0:         /* Reset codes size and mask */
                  gif->last_code = clear_code + 1;
                  code_size = gif->input_code_size + 1;
                  code_mask = (1 << code_size) - 1;
                  break;

               case 2:         /* End code... consume remaining data chunks..? */
                  gif->state = 998;
                  goto error;  /* Could clean up better? */
               default:
                  gif->state = 999;
                  printf("dillo_gif_decode: error!\n");
                  goto error;
            }
         }
      }

      /* We reach here if
       * a) We have reached the end of the data block;
       * b) we ran out of data before reaching the end of the data block
       */
      if (bufsize <= 0)
         break;                 /* We are out of data; */

      /* Start of new data block */
      bufsize--;
      if (!(packet_size = *buf++))
      {  /* This is the "block terminator" -- the last data block */
         gif->state = 999;     /* BUG: should Go back to getting GIF blocks. */
         break;
      }
   }

   gif->packet_size = packet_size;
   gif->window = window;
   gif->bits_in_window = bits_in_window;
   gif->code_size = code_size;
   return BSize - bufsize;

 error:
   return BSize - bufsize;
}


UWORD Gif_check_sig(HWGif *gif, const unsigned char *ibuf, UWORD Size)
{
   /* at beginning of file - read magic number */

   if (Size < 6)
      return 0;
   if (strncmp(ibuf, "GIF", 3) != 0) {
      gif->state = 999;
      return 6;
   }
   if (strncmp(ibuf + 3, "87a", 3) != 0 &&
       strncmp(ibuf + 3, "89a", 3) != 0) {
      gif->state = 999;
      return 6;
   }
   gif->state = 1;
   return 6;
}


/* Read the colour map
 *
 * Implements, from the spec:
 * Global Colour Table
 * Local Colour Table
 */
UWORD Gif_do_colour_table(HWGif *gif, void *Buf,
                    const unsigned char *buf, UWORD BSize, UWORD CT_Size)
{
   UWORD Size = 3 * (1 << (1 + CT_Size));
   UWORD i;

   if (Size > BSize)
      return 0;

   gif->NumColours = (1 << (1 + CT_Size));
   if (gif->palette != NULL)
      free(gif->palette);
   gif->palette = (PALETTE) malloc(Size*2);
   for (i = 0; i < Size/3; i++)
   {  gif->palette[i].red = *buf++;
      gif->palette[i].green = *buf++;
      gif->palette[i].blue = *buf++;
   }
   return Size;
}


/*
 * This implements, from the spec:
 * <Logical Screen> ::= Logical Screen Descriptor [Global Colour Table]
 */
UWORD Gif_get_descriptor(HWGif *gif, void *Buf,
                                 const unsigned char *buf, UWORD BSize)
{

   /* screen descriptor */
   UWORD Size = 7,           /* Size of descriptor */
          bytes_used;             /* Size of colour table */
   unsigned char Flags;

   if (BSize < 7)
      return 0;
   Flags = buf[4];

   if (Flags & LOCALCOLORMAP) {
      bytes_used = Gif_do_colour_table(gif, Buf, buf + 7, BSize - 7, Flags & 0x7);
      if (!bytes_used)
         return 0;
      Size += bytes_used;           /* Size of the colour table that follows */
      gif->Background = buf[5];
   }
   /*   gif->Width = LM_to_uint(buf[0], buf[1]);
        gif->Height = LM_to_uint(buf[2], buf[3]); */
        gif->ColourResolution = (((buf[4] & 0x70) >> 4) + 1);
   /*   gif->AspectRatio     = buf[6]; */

   return Size;
}


/*
 * This reads a sequence of GIF data blocks.. and ignores them!
 * Buf points to the first data block.
 *
 * Return Value
 * 0 = There wasn't enough bytes read yet to read the whole datablock
 * otherwise the size of the data blocks
 */
UWORD Gif_data_blocks(const unsigned char *Buf, UWORD BSize)
{
   UWORD Size = 0;

   if (BSize < 1)
      return 0;
   while (Buf[0]) {
      if (BSize <= (UWORD)(Buf[0] + 1))
         return 0;
      Size += Buf[0] + 1;
      BSize -= Buf[0] + 1;
      Buf += Buf[0] + 1;
   }
   return Size + 1;
}


UWORD Gif_do_gc_ext(HWGif *gif, const unsigned char *Buf, UWORD BSize)
{
   /* Graphic Control Extension */
   UWORD Size = Buf[0] + 2;
   UWORD Flags;

   if (Size > BSize)
      return 0;
   Buf++;
   Flags = Buf[0];

   /* The packed fields */
#if 0
   gif->disposal = (Buf[0] >> 2) & 0x7;
   gif->inputFlag = (Buf[0] >> 1) & 0x1;

   /* Delay time */
   gif->delayTime = LM_to_uint(Buf[1], Buf[2]);
#endif

   /* Transparent colour index, may not be valid  (unless flag is set) */
   if ((Flags & 0x1)) {
      gif->transparent = Buf[3];
   }
   return Size;
}


/*
 * This is a GIF extension.  We ignore it with this routine.
 * Buffer points to just after the extension label.
 *
 * Return Value
 * 0 -- block not processed
 * otherwise the size of the extension label.
 */
UWORD Gif_do_generic_ext(const unsigned char *Buf, UWORD BSize)
{
   UWORD Size = Buf[0] + 1, DSize;

   /* The Block size (the first byte) is supposed to be a specific size
    * for each extension... we don't check.
    */

   if (Buf[0] > BSize)
      return 0;
   DSize = Gif_data_blocks(Buf + Size, BSize - Size);
   if (!DSize)
      return 0;
   Size += DSize;
   return Size <= BSize ? Size : 0;
}


#define App_Ext  (0xff)
#define Cmt_Ext  (0xfe)
#define GC_Ext   (0xf9)
#define Txt_Ext  (0x01)

/*
 * ?
 * Return value:
 *    TRUE when the extension is over
 */
UWORD Gif_do_extension(HWGif *gif, unsigned char Label,
                               const unsigned char *buf,
                               UWORD BSize)
{
   switch (Label) {
   case GC_Ext:         /* Graphics extension */
      return Gif_do_gc_ext(gif, buf, BSize);

   case Cmt_Ext:                /* Comment extension */
      return Gif_data_blocks(buf, BSize);

   case Txt_Ext:                /* Plain text Extension */
      /* This extension allows (rcm thinks) the image to be rendered as text.
       */
   case App_Ext:                /* Application Extension */
   default:
      return Gif_do_generic_ext(buf, BSize);    /*Ignore Extension */
   }
}


/*
 * This implements, from the spec:
 * <Table-Based Image> ::= Image Descriptor [Local Colour Table] Image Data
 *
 * ('Buf' points to just after the Image separator)
 * we should probably just check that the local stuff is consistent
 * with the stuff at the header. For now, we punt...
 */
UWORD Gif_do_img_desc(HWGif *gif, void *Buf,
                         const unsigned char *buf, UWORD bsize)
{
   unsigned char Flags;
   UWORD Size = 9 + 1; /* image descriptor size + first byte of image data */

   if (bsize < 10)
      return 0;

   gif->Width   = LM_to_uint(buf[4], buf[5]);
   gif->Height  = LM_to_uint(buf[6], buf[7]);
   gif->linebuf = (unsigned char *) malloc(gif->Width);

   gif->mfdb->fd_w = gif->Width;
   gif->mfdb->fd_wdwidth = (gif->Width + 15) >> 4;
   gif->mfdb->fd_h = gif->Height;
   gif->mfdb->fd_stand = 1; /* VDI standard */

   Flags = buf[8];

   gif->Flags |= Flags & INTERLACE;
   gif->pass = 0;
   bsize -= 9;
   buf += 9;
   if (Flags & LOCALCOLORMAP) {
      UWORD LSize = Gif_do_colour_table(gif, Buf, buf, bsize, Flags & 0x7);

      if (!LSize)
         return 0;
      Size += LSize;
      buf += LSize;
      bsize -= LSize;
   }

   gif->mfdb->fd_nplanes = gif->ColourResolution;
   gif->mfdb->fd_addr = (void *) malloc((long) gif->mfdb->fd_wdwidth  *
                        (long) gif->mfdb->fd_h * (long) gif->ColourResolution * 2);

   /* Finally, get the first byte of the LZW image data */
   if (bsize < 1)
      return 0;
   gif->input_code_size = *buf++;
   if (gif->input_code_size > 8) {
      gif->state = 999;
      return Size;
   }
   gif->y = 0;
   Gif_lwz_init(gif);
   gif->spill_line_index = 0;
   gif->state = 3;              /*Process the lzw data next */
   return Size;
}


/* --- Top level data block processors ------------------------------------ */
#define Img_Desc (0x2c)
#define Trailer  (0x3B)
#define Ext_Id   (0x21)


/*
 * This identifies which kind of GIF blocks are next, and processes them.
 * It returns if there isn't enough data to process the next blocks, or if
 * the next block is the lzw data (which is streamed differently)
 *
 * This implements, from the spec, <Data>* Trailer
 * <Data> ::= <Graphic Block> | <Special-Purpose Block>
 * <Special-Purpose Block> ::= Application Extension | Comment Extension
 * <Graphic Block> ::= [Graphic Control Extension] <Graphic-Rendering Block>
 * <Graphic-Rendering Block> ::= <Table-Based Image> | Plain Text Extension
 *
 * <Data>* --> GIF_Block
 * <Data>  --> while (...)
 * <Special-Purpose Block> --> Gif_do_extension
 * Graphic Control Extension --> Gif_do_extension
 * Plain Text Extension --> Gif_do_extension
 * <Table-Based Image> --> Gif_do_img_desc
 *
 * Return Value
 * 0 if not enough data is present, otherwise the number of bytes
 * "consumed"
 */
UWORD GIF_Block(HWGif *gif, void *Buf,
                       const unsigned char *buf, UWORD bsize)
{
   UWORD Size = 0, bytes_used;
   unsigned char C;

   if (bsize < 1)
      return 0;
   while (gif->state == 2) {
      if (bsize < 1)
         return Size;
      bsize--;
      switch (*buf++) {
      case Ext_Id:
         /* get the extension type */
         if (bsize < 2)
            return Size;

         /* Have the extension block intepreted. */
         C = *buf++;
         bsize--;
         bytes_used = Gif_do_extension(gif, C, buf, bsize);

         if (!bytes_used)
            /* Not all of the extension is there.. quit until more data
             * arrives */
            return Size;

         bsize -= bytes_used;
         buf += bytes_used;

         /* Increment the amount consumed by the extension introducer
          * and id, and extension block size */
         Size += bytes_used + 2;
         /* Do more GIF Blocks */
         continue;

      case Img_Desc:            /* Image descriptor */
         bytes_used = Gif_do_img_desc(gif, Buf, buf, bsize);
         if (!bytes_used)
            return Size;

         /* Increment the amount consumed by the Image Separator and the
          * Resultant blocks */
         Size += 1 + bytes_used;
         return Size;

      case Trailer:
         gif->state = 998;      /* BUG: should close the rest of the file */
         break;                 /* GIF terminator */

      default:                  /* Unknown */
         /* BUG: we should gripe and complain
          * g_print(0x%x\n", *(buf-1)); */
         gif->state = 999;
         return Size + 1;
      }
   }
   return Size;
}
/*
 * Process some bytes from the input gif stream. It's a state machine.
 *
 * From the GIF spec:
 * <GIF Data Stream> ::= Header <Logical Screen> <Data>* Trailer
 *
 * <GIF Data Stream> --> Gif_process_bytes
 * Header            --> State 0
 * <Logical Screen>  --> State 1
 * <Data>*           --> State 2
 * Trailer           --> State > 3
 *
 * State == 3 is special... this is inside of <Data> but all of the stuff in
 * there has been gotten and set up.  So we stream it outside.
 */

UWORD Gif_process_bytes(HWGif *gif, const unsigned char *buf,
                                UWORD bufsize, void *Buf)
{
   UWORD bytes_left = bufsize;
   size_t bytes_used;
   unsigned char *ibuf;

   ibuf = buf;

   switch (gif->state) {
   case 0:
      bytes_used = Gif_check_sig(gif, ibuf, bytes_left);
      if (!bytes_used)
         break;
      bytes_left -= bytes_used;
      ibuf += bytes_used;
      if (gif->state != 1)
         break;

   case 1:
      bytes_used = Gif_get_descriptor(gif, Buf, ibuf, bytes_left);
      if (!bytes_used)
         break;
      bytes_left -= bytes_used;
      ibuf += bytes_used;
      gif->state = 2;

   case 2:
      /* Ok, this loop construction looks weird.  It implements the <Data>* of
       * the GIF grammar.  All sorts of stuff is allocated to set up for the
       * decode part (state ==2) and then there is the actual decode part (3)
       */
      bytes_used = GIF_Block(gif, Buf, ibuf, bytes_left);
      if (!bytes_used)
         break;
      bytes_left -= bytes_used;
      ibuf += bytes_used;
      if (gif->state != 3)
         break;

   case 3:
      /* get an image byte */
      /* The users sees all of this stuff */
      bytes_used = Gif_decode(gif, ibuf, bytes_left);
      if (bytes_used == 0)
         break;
      ibuf += bytes_used;
      bytes_left -= bytes_used;

   default:
      /* error - just consume all input */
      bytes_left = 0;
      break;
   }
   return bufsize - bytes_left;
}

void hw_do_gif(char *file, MFDB **pntr, PALETTE *pal)
{
   HWGif *gif;
   UWORD num_bytes, bytes_used, bytes_left, end_of_file, to_read;
   unsigned char *temp_buf, *buffer;
   WORD fhandle;

   fhandle = Fopen (file, 0);
   if (((buffer = (unsigned char *) malloc(4096)) != NULL) &&
      ((temp_buf = (unsigned char *) malloc(256)) != NULL ))
   {  num_bytes = Fread(fhandle, 4096, buffer);
      if (num_bytes < 4096)
		      end_of_file = 1;
      else
		      end_of_file = 0;
      gif = Gif_new(file);
      for (;;)
      { bytes_used = Gif_process_bytes(gif, buffer, num_bytes, temp_buf);
          if (gif->state == 998)
             break;
          else if (gif->state == 999)
          {  free(gif->mfdb);
             gif->mfdb = NULL;
             break;
          }
          bytes_left = num_bytes - bytes_used;
          if (end_of_file)
              break;
          if (bytes_left)
             memmove(buffer, buffer+bytes_used, bytes_left);
          to_read = 4096 - bytes_left;
          num_bytes = Fread(fhandle, to_read, buffer+bytes_left);
          end_of_file = to_read - num_bytes;
          if (!num_bytes)
          {  free(gif->mfdb);
             gif->mfdb = NULL;
             break;
          }
          else
             num_bytes += bytes_left;
      }
     free(temp_buf);
     free(buffer);
   }
   Fclose(fhandle);
   *pntr = gif->mfdb;
   *pal = gif->palette;
   free(gif);
}
