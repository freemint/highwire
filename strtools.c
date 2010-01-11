/*
 **
 ** Changes
 ** Author         Date           Desription
 ** P Slegg        8-Jan-2010     New module. String functions.
 **
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

char* rtrim(char* string, char junk);
char* ltrim(char* string, char junk);

/*----------------------------------------------------------------------------*/

/* Remove character junk from right of string */
char* rtrim(char* string, char junk)
{
	char* original = string + strlen(string);
	while(*--original == junk);
	*(original + 1) = '\0';
	return string;
}  /* rtrim */

/*----------------------------------------------------------------------------*/

/* Remove character junk from left of string */
char* ltrim(char *string, char junk)
{
	char* original = string;
	char *p = original;
	int trimmed = 0;
	do
	{
		if (*original != junk || trimmed)
		{
			trimmed = 1;
			*p++ = *original;
		}
	}
	while (*original++ != '\0');
	return string;
}  /* ltrim */