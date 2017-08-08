/* based on Dan Ackerman's XIMGLOAD.C */


#include <gem.h>
#include <osbind.h>
#include <string.h>
#include <stdlib.h>
#include "global.h"
#include "convert.h"

typedef struct image_block
        {    MFDB *mfdb;
             PALETTE palette;
             UWORD * lines[32];
             long size, plane_size;
        } ImageBlock;

short vditodev8[] =
{0,255,1,2,4,6,3,5,7,8,9,10,12,14,11,13,16,17,18,19,20,21,22,23,24,25,26
,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50
,51,52,53,54,55,56,57,58,59,60,61,62,63,64,65,66,67,68,69
,70,71,72,73,74,75,76,77,78,79,80,81,82,83,84,85,86,87,88
,89,90,91,92,93,94,95,96,97,98,99,100,101,102,103,104,105,106,107,108,109,
110,111,112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,127,
128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,144,145,
146,147,148,149,150,151,152,153,154,155,156,157,158,159,160,161,162,163,
164,165,166,167,168,169,170,171,172,173,174,175,176,177,178,179,180,181,
182,183,184,185,186,187,188,189,190,191,192,193,194,195,196,197,198,199,
200,201,202,203,204,205,206,207,208,209,210,211,212,213,214,215,216,217,
218,219,220,221,222,223,224,225,226,227,228,229,230,231,232,233,234,235,
236,237,238,239,240,241,242,243,244,245,246,247,248,249,250,
251,252,253,254,15};

short vditodev4[] = {0,15,1,2,4,6,3,5,7,8,9,10,12,14,11,13};
short vditodev2[] = {0,3,1,2};

UWORD probit[] = {0x0001, 0x0002, 0x0004, 0x0008,
                  0x0010, 0x0020, 0x0040, 0x0080,
                  0x0100, 0x0200, 0x0400, 0x0800,
                  0x1000, 0x2000, 0x4000, 0x8000};

void match_palette(PALETTE pic, PALETTE screen,
                   UWORD pic_planes, UWORD screen_planes,
                   UWORD table[])
{
   UWORD i, j, k, screencolours, piccolours;
   RGB255 rgb;
   unsigned long ss, sss;
   UWORD *cptr;

   screencolours = 1 << screen_planes;
   piccolours = 1 << pic_planes;
   for (i = 0; i < piccolours; i++)
   {
      ss = 200000; /* set to > 3 * 255^2 - maximum possible */
      for (j = 0; j < screencolours; j++)
      {
         rgb.red = abs(screen[j].red - pic[i].red);
         rgb.green = abs(screen[j].green - pic[i].green);
         rgb.blue = abs(screen[j].blue - pic[i].blue);
         sss = (long) rgb.red * (long) rgb.red  +
               (long) rgb.green * (long) rgb.green +
               (long) rgb.blue * (long) rgb.blue;
         if (sss < ss)
         {  ss = sss;
            k = j;
         }
      }
      table[i] = k;
   }

   switch (screen_planes)
   {
      case 2 : cptr = vditodev2;
               break;
      case 4 : cptr = vditodev4;
               break;
      case 8 : cptr = vditodev8;
               break;
      default : cptr = NULL;
   }
   if (cptr != NULL)
      for (i = 0; i < piccolours; i++)
         table[i] = cptr[table[i]];

}
void mono_to_planes(ImageBlock *img, ImageBlock *newimg)
{
   UWORD line, plane, bit, wid, w;
   UWORD width = img->mfdb->fd_wdwidth;
   UWORD height = img->mfdb->fd_h;
   UWORD across = img->mfdb->fd_w;
   UWORD newplanes = newimg->mfdb->fd_nplanes;

   for (line = 0; line < height; line++)
   {  for (wid = 0; wid < across; wid++)
      {  w = wid >> 4;
         bit = 15 - (wid & 15);
         if (img->lines[0][w] & probit[bit])
            for (plane = 0; plane < newplanes; plane++)
               newimg->lines[plane][w] |= probit[bit];
      }
      img->lines[0] += width;
      for (plane = 0; plane < newplanes; plane++)
          newimg->lines[plane] += width;
   }
}

void planes_to_planes(ImageBlock *img, ImageBlock *newimg)
{
   UWORD line, plane, bit, wid, w, r, colour, temp, mask;
   UWORD width = img->mfdb->fd_wdwidth;
   UWORD height = img->mfdb->fd_h;
   UWORD across = img->mfdb->fd_w;
   UWORD oldplanes = img->mfdb->fd_nplanes;
   UWORD newplanes = newimg->mfdb->fd_nplanes;
   UWORD colour_chart[256];

   for (r = 0; r < 256; r++)
      colour_chart[r] = r;
   if (img->palette != NULL)
       match_palette(img->palette, newimg->palette,
                     img->mfdb->fd_nplanes,
                     newimg->mfdb->fd_nplanes,
                     colour_chart);
   for (line = 0; line < height; line++)
   {  for (wid = 0; wid < across; wid++)
      {  w = wid >> 4;
         bit = 15 - (wid & 15);
         mask = probit[bit];
         temp = 0;
         for (plane = 0; plane < oldplanes; plane++)
            if (img->lines[plane][w] & mask)
               temp |= probit[plane];
         colour = colour_chart[temp];
         for (plane = 0; plane < newplanes; plane++)
            if (colour & probit[plane])
               newimg->lines[plane][w] |= mask;
      }
      for (plane = 0; plane < oldplanes; plane++)
          img->lines[plane] += width;
      for (plane = 0; plane < newplanes; plane++)
          newimg->lines[plane] += width;
   }
}

void mono_to_TC(ImageBlock *img, ImageBlock *newimg)
{
   UWORD line, plane, bit, wid, w;
   UWORD width = img->mfdb->fd_wdwidth;
   UWORD height = img->mfdb->fd_h;
   UWORD across = img->mfdb->fd_w;
   UWORD newplanes = newimg->mfdb->fd_nplanes;

   for (line = 0; line < height; line++)
   {  for (wid = 0; wid < across; wid++)
      {  w = wid >> 4;
         bit = 15 - (wid & 15);
         if (!img->lines[0][w] | !probit[bit])
            for (plane = 8; plane < 32; plane++)
               newimg->lines[plane][w] |= probit[bit];
      }
      img->lines[0] += width;
      for (plane = 0; plane < newplanes; plane++)
          newimg->lines[plane] += width;
   }
}

void planes_to_TC(ImageBlock *img, ImageBlock *newimg)
{
   UWORD line, plane, bit, wid, w, temp, mask;
   UWORD width = img->mfdb->fd_wdwidth;
   UWORD height = img->mfdb->fd_h;
   UWORD across = img->mfdb->fd_w;
   UWORD oldplanes = img->mfdb->fd_nplanes;
   UWORD newplanes = newimg->mfdb->fd_nplanes;
   PALETTE pal;
   RGB255 rgb;

  if (img->palette != NULL)
      pal = img->palette;
   else
      pal = newimg->palette;

   for (line = 0; line < height; line++)
   {  for (wid = 0; wid < across; wid++)
      {  w = wid >> 4;
         bit = 15 - (wid & 15);
         mask = probit[bit];
         temp = 0;
         for (plane = 0; plane < oldplanes; plane++)
            if (img->lines[plane][w] & mask)
               temp |= probit[plane];
         rgb = pal[temp];
         for (plane = 0; plane < 8; plane++)
         {  if (rgb.red & probit[plane])
               newimg->lines[plane+8][w] |= mask;
            if (rgb.green & probit[plane])
               newimg->lines[plane+16][w] |= mask;
            if (rgb.blue & probit[plane])
               newimg->lines[plane+24][w] |= mask;
         }
      }
      for (plane = 0; plane < oldplanes; plane++)
          img->lines[plane] += width;
      for (plane = 0; plane < newplanes; plane++)
          newimg->lines[plane] += width;
   }
}

void TC24_to_TC32(ImageBlock *img, ImageBlock *newimg)
{
   memmove(img->lines[0], newimg->lines[8], img->size);
}

void same_format(ImageBlock *img, ImageBlock *newimg)
{
   void *temp;
   temp = newimg->mfdb->fd_addr;
   newimg->mfdb->fd_addr = img->mfdb->fd_addr;
   img->mfdb->fd_addr = temp;
}

int transform_image(MFDB *ptr, PALETTE pal)
{
   ImageBlock newimg, img;
   short handle, dummy, *rgb;
   UWORD i, num_colours, po, pn;
   short WorkIn[11], WorkOut[57];
   long plane_size_words;

   img.mfdb = ptr;
   img.palette = pal;
   po = img.mfdb->fd_nplanes;
   img.plane_size = (long) img.mfdb->fd_wdwidth *
                    (long) img.mfdb->fd_h * 2;
   img.size = img.plane_size * po;
   /* open virtual screen workstation (screen) */
   WorkIn[0] = 2 + Getrez(); WorkIn[10] = 2;
   for (i = 1; i < 9; i++)
      WorkIn[i] = 1;
   handle = graf_handle(&dummy, &dummy, &dummy, &dummy);
   v_opnvwk(WorkIn, &handle, WorkOut);
   vq_extnd(handle, 1, WorkOut);
   pn = WorkOut[4];
   newimg.mfdb = (MFDB *) malloc(sizeof(MFDB));
   newimg.mfdb->fd_w = img.mfdb->fd_w;
   newimg.mfdb->fd_h = img.mfdb->fd_h;
   newimg.mfdb->fd_wdwidth = img.mfdb->fd_wdwidth;
   newimg.mfdb->fd_stand = 1; /* vdi standard format */
   newimg.mfdb->fd_nplanes = pn;
   newimg.plane_size = img.plane_size;
   plane_size_words = img.plane_size / 2;
   newimg.size = newimg.plane_size * pn;
   if ((newimg.mfdb->fd_addr = malloc(newimg.size)) == NULL)
      return 0;
   memset(newimg.mfdb->fd_addr, 0, newimg.size);
   for (i = 0; i < pn; i++)
      newimg.lines[i] = (UWORD *) newimg.mfdb->fd_addr
                       + (plane_size_words * i); /* word pointer */
   for (i = 0; i < po; i++)
      img.lines[i] = (UWORD *) img.mfdb->fd_addr + (plane_size_words * i);
   if (pn < 8)
      num_colours = 1 << pn;
   else
      num_colours = 256;
   newimg.palette = malloc(256*6);
   rgb = (short *) newimg.palette;
   for (i = 0; i < num_colours; i++)
   {   vq_color(handle, i, 1, rgb);
       *rgb++ >>= 2;  /* change for 0..1000 to 0..250 */
       *rgb++ >>= 2;
       *rgb++ >>= 2;
   }
   switch (pn)
   {
      case 1 : switch (po)
               {
                  case 1 : same_format(&img, &newimg); break;
                  case 2 : break;
                  case 4 : break;
                  case 8 : break;
                  case 16 : break;
                  case 32 : break;
                  default : ;
               }
               break;

      case 2 : switch (po)
               {
                  case 1 : mono_to_planes(&img, &newimg); break;
                  case 2 :
                  case 3 :
                  case 4 :
                  case 5 :
                  case 6 :
                  case 7 :
                  case 8 : planes_to_planes(&img, &newimg); break;
                  case 16 : break;
                  case 32 : break;
                  default : ;
               }
               break;

      case 4 : switch (po)
               {
                  case 1 : mono_to_planes(&img, &newimg); break;
                  case 2 :
                  case 3 : planes_to_planes(&img, &newimg); break;
                  case 4 : if (img.palette == NULL)
                              same_format(&img, &newimg);
                           else
                              planes_to_planes(&img, &newimg);
                           break;
                  case 5 :
                  case 6 :
                  case 7 :
                  case 8 : planes_to_planes(&img, &newimg); break;
                  case 16 : break;
                  case 32 : break;
                  default : ;
               }
               break;

      case 8 : switch (po)
               {
                  case 1 : mono_to_planes(&img, &newimg); break;
                  case 2 :
                  case 3 :
                  case 4 :
                  case 5 :
                  case 6 :
                  case 7 : planes_to_planes(&img, &newimg); break;
                  case 8 : if (img.palette == NULL)
                              same_format(&img, &newimg);
                           else
                              planes_to_planes(&img, &newimg);
                           break;
                  case 16 : break;
                  case 32 : break;
                  default : ;
               }
               break;

     case 16 : switch (po)
               {
                  case 1 : break;
                  case 2 : break;
                  case 4 : break;
                  case 8 : break;
                  case 16 : same_format(&img, &newimg); break;
                  case 32 : break ;
                  default : ;
               }
               break;

     case 32 : switch (po)
               {
                  case 1 : mono_to_TC(&img, &newimg); break;
                  case 2 :
                  case 3 :
                  case 4 :
                  case 5 :
                  case 6 :
                  case 7 :
                  case 8 : planes_to_TC(&img, &newimg); break;
                  case 16 : break;
                  case 24 : TC24_to_TC32(&img, &newimg); break;
                  case 32 : same_format(&img, &newimg); break;
                  default : ;
               }
     default : ;
   }
   free(img.mfdb->fd_addr);
   free(newimg.palette);
   if ((img.mfdb->fd_addr = malloc(newimg.size)) == NULL)
      return 0;
   img.mfdb->fd_stand = 0; /* Device dependent */
   img.mfdb->fd_nplanes = pn;
   vr_trnfm(handle, newimg.mfdb, img.mfdb );
   free(newimg.mfdb->fd_addr);
   free(newimg.mfdb);
   v_clsvwk(handle);
   return 1;
}
