/*
 *		SAMPLE.C
 *
 *		Source code of sample SOS application.
 */

#include	"sosdef.h"

void	app_entry()
{
	printfxy( 40 - sizeof( "Hello, world!" ) / 2, 12, "Hello, world!" );
	halt();
}

