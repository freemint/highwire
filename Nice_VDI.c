#ifdef LATTICE
#include <osbind.h>
#endif

#ifdef __PUREC__
#include <tos.h>
#endif

#ifdef __GNUC__
#include <osbind.h>
#endif

#include <gemx.h>

#include <stdlib.h>

#include "global.h"

WORD
V_Opnvwk (VDI_Workstation * dev)
{
	WORD i, in[11];

	in[0] = Getrez () + 2;
	dev->dev_id = in[0];
	for (i = 1; i < 10; in[i++] = 1) ;
	in[10] = 2;
	i = graf_handle (&dev->wchar, &dev->hchar, &dev->wbox, &dev->hbox);
	v_opnvwk (in, &i, &dev->xres);
	dev->handle = i;

	if (i)
		vq_extnd (i, 1, &dev->screentype);

	return (i);
}

WORD
V_Opnwk (WORD devno, VDI_Workstation * dev)
{
	WORD i, in[11];

	in[0] = dev->dev_id = devno;
	for (i = 1; i < 10; in[i++] = 1) ;
	in[10] = 2;
	i = devno;

	v_opnwk (in, &i, &dev->xres);
	dev->handle = i;

	if (i)
		vq_extnd (i, 1, &dev->screentype);

	return (i);
}
