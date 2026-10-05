#include "network.h"

static unsigned short flag;

int set_flag(int bit_nr)
{
	unsigned short mask = 1 << bit_nr;
	if (flag & mask)
		return 0;
	flag |= mask;
	return 1;
}

void clear_flag(int bit_nr)
{
	flag &= ~(1 << bit_nr);
}