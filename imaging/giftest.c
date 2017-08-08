#include <gem.h>
#include <osbind.h>
#include <string.h>
#include <dos.h>

#include "convert.h"
#include "hwgif.h"

short appl_id, handle, Wx, Wy, Ww, Wh, w_handle;
short i, j, error, button;
short work_in[11], work_out[57], pxy[8];
char in_path[128], in_select[20], *title;
MFDB *mfdb, screen_mfdb;
PALETTE palette;

void main()
{

	appl_id = appl_init();
	handle = graf_handle(&i, &i, &i, &i);
	for (i = 1; i < 10; i++)
	    work_in[i] = 1 ;
	work_in[0] = Getrez() + 2;
	work_in[10] = 2;
	v_opnvwk(work_in, &handle, work_out);
	error = wind_get(0, 5, &Wx, &Wy, &Ww, &Wh);
	Wy += 10; Wh -= 10;
	w_handle = wind_create(0, Wx, Wy, Ww, Wh);
	error = wind_open(w_handle, Wx, Wy, Ww, Wh);
	strcpy(in_path, "*.GIF");
	in_select[0] = '\0';
	title = "Select .GIF File";
	error = fsel_exinput(in_path, in_select, &button, title);
	if (button == 1)
	{  i = 0; j = 1;
	   while (in_path[i])
	   {  if (in_path[i] == '\\')
	         j = i;
	      i++;
	   }
	   in_path[j+1] = '\0';
	   strcat(in_path, in_select);
	   hw_do_gif(in_path, &mfdb, &palette);

     if (mfdb != NULL)
     {
        transform_image(mfdb, palette);
        screen_mfdb.fd_addr = NULL;
        pxy[0] = pxy[1] = 0;
        pxy[2] = (*mfdb).fd_w; pxy[3] = (*mfdb).fd_h;
        pxy[4] = 10; pxy[5] = 20;
        pxy[6] = 10 + pxy[2]; pxy[7] = 20 + pxy[3];
        vro_cpyfm(handle, 3, pxy, mfdb, &screen_mfdb);
     }
     error = getch();
  }
  wind_close(w_handle);
  wind_delete(w_handle);
  v_clsvwk(handle);
  appl_exit();
}
