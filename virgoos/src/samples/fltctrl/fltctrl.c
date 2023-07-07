/*
 *		fltctrl.c
 *
 *		Source code of sample SOS flight control application.
 */

#include	"sosdef.h"
#include	"taskman.h"
#include	"config.h"
#include	"drvint.h"
#include	"timers.h"

#define	ROUTE_ENTRIES	1
#define	ROUTE_TIME		60 * 60

#define	MAX_SPEED		287			// Speed m/s
#define	MAX_FLAPS_UP	50			// Maximum rize speed, m/s
#define	MAX_FLAPS_DOWN	50			// MAximum dive speed, m/s
#define	MAX_TURN		10			// Maximum turn, deg.

#define	OPT_FLAPS_UP	10
#define	OPT_FLAPS_DOWN	10
#define	OPT_TURN		5

struct	aircraft_route
{
	long	speed_fwd;
	long	speed_side;
	long	speed_vert;
	long	altitude;
	long	direction;
	long	seconds;
}	aircraft_route[ ROUTE_ENTRIES ] = { 277, 0, 0, 10000, 0, ROUTE_TIME };


struct	aircraft_condition
{
	long	speed_fwd;
	long	speed_side;
	long	speed_vert;
	long	altitude;
}	aircraft_condition;


struct
{
	long	speed_fwd;
	long	speed_side;
	long	speed_vert;
}	weather_condition, flaps_condition;


#define	MAX_WEATHER_VERT	30


void	get_conditions_timer_handler( void )
{
	weather_condition.speed_vert = ( random() % ( MAX_WEATHER_VERT + 1 ) );
	printfxy( 0, 18, "Flaps condition: speed_fwd = %ld, speed_side = %ld, speed_vert = %ld",
		flaps_condition.speed_fwd, flaps_condition.speed_side, flaps_condition.speed_vert );
}


void	get_conditions( void )
{
	timer_t	get_conditions_timer;

	get_conditions_timer.resolution = TICKS_PER_SEC;
	get_conditions_timer.timeout = TICKS_PER_SEC * 10;
	get_conditions_timer.flags = TF_PERIODIC;
	get_conditions_timer.callback = get_conditions_timer_handler;

	printfxy( 0, 10, "get_condition()" );
	install_timer( &get_conditions_timer );

inf_loop:
	goto	inf_loop;
}


void	get_flaps_timer_handler( void *unused )
{
	printfxy( 0, 17, "Flaps condition: speed_fwd = %ld, speed_side = %ld, speed_vert = %ld",
		flaps_condition.speed_fwd, flaps_condition.speed_side, flaps_condition.speed_vert );
}


void	get_flaps( void *unused )
{
	timer_t	get_flaps_timer;

	get_flaps_timer.resolution = TICKS_PER_SEC;
	get_flaps_timer.timeout = TICKS_PER_SEC / 4;
	get_flaps_timer.flags = TF_PERIODIC;
	get_flaps_timer.callback = get_flaps_timer_handler;

	printfxy( 0, 10, "get_flaps()" );
	install_timer( &get_flaps_timer );

inf_loop:
	goto	inf_loop;
}


long	alt_rem;


//	Updates an aircraft position every 1/4 a second
void	update_position_timer_handler( void *unused )
{
	aircraft_condition.altitude += ( weather_condition.speed_vert + flaps_condition.speed_vert ) / 4;
	alt_rem += abs( weather_condition.speed_vert + flaps_condition.speed_vert ) % 4;
	if ( alt_rem >= 4 )
	{
		alt_rem -= 4;
		aircraft_condition.altitude += ( weather_condition.speed_vert + flaps_condition.speed_vert ) / 
			abs( weather_condition.speed_vert + flaps_condition.speed_vert );
	}
	aircraft_condition.speed_vert = weather_condition.speed_vert + flaps_condition.speed_vert;
	aircraft_condition.speed_side = weather_condition.speed_side + flaps_condition.speed_side;
	aircraft_condition.speed_fwd = weather_condition.speed_fwd + flaps_condition.speed_fwd;

	printfxy( 0, 16, "Aircraft condition: alt. = %ld, speed_fwd = %ld, speed_side = %ld, speed_vert = %ld",
		aircraft_condition.altitude, aircraft_condition.speed_fwd, aircraft_condition.speed_side,
		aircraft_condition.speed_vert );
}


void	update_position( void *unused )
{
	timer_t	update_position_timer;

	printfxy( 0, 9, "update_position()" );

	update_position_timer.resolution = TICKS_PER_SEC;
	update_position_timer.timeout = TICKS_PER_SEC / 4;
	update_position_timer.flags = TF_PERIODIC;
	update_position_timer.callback = update_position_timer_handler;

	install_timer( &update_position_timer );

inf_loop:
	goto	inf_loop;
}


dword	route_timer = 0;
int		route_counter = 0;


long	last_alt_diff;

//	Main autopilot routine
void	main_ctl_timer_handler()
{
	if ( ++route_timer == ROUTE_TIME )
		++route_counter;

	if ( route_counter == ROUTE_ENTRIES )
	{
		printfxy( 0, 12, "Autopilot arrived at end of route. AUTOPILOT IS OFF" );
		halt();
	}

	if ( aircraft_condition.altitude < aircraft_route[ route_counter ].altitude )
	{
		if ( last_alt_diff < aircraft_route[ route_counter ].altitude - aircraft_condition.altitude )
			if ( ( flaps_condition.speed_vert += 10 ) > MAX_FLAPS_UP )
			   flaps_condition.speed_vert = MAX_FLAPS_UP;

		last_alt_diff = aircraft_route[ route_counter ].altitude - aircraft_condition.altitude;
		printfxy( 0, 15, "Autopilot: going up %ld m/s", flaps_condition.speed_vert );
	}
	else if ( aircraft_condition.altitude > aircraft_route[ route_counter ].altitude )
	{
		if ( last_alt_diff > MAX_FLAPS_DOWN )
			if ( ( flaps_condition.speed_vert -= 10 ) < -MAX_FLAPS_DOWN )
				flaps_condition.speed_vert = -MAX_FLAPS_DOWN;

		last_alt_diff = aircraft_condition.altitude - aircraft_route[ route_counter ].altitude;
		printfxy( 0, 15, "Autopilot: diving with %ld m/s", -flaps_condition.speed_vert );
	}
}


void	main_ctl( void *unused )
{
	timer_t	main_ctl_timer;

	printfxy( 0, 8, "main_ctl()" );

	start_task( get_flaps, 1, OPT_TIMESHARE, NULL );
	printfxy( 0, 7, "main_ctl()" );
	start_task( get_conditions, 1, OPT_TIMESHARE, NULL );
	printfxy( 0, 6, "main_ctl()" );
	start_task( update_position, 1, OPT_TIMESHARE, NULL );
	printfxy( 0, 5, "main_ctl()" );

	main_ctl_timer.resolution = TICKS_PER_SEC;
	main_ctl_timer.timeout = TICKS_PER_SEC;
	main_ctl_timer.flags = TF_PERIODIC;
	main_ctl_timer.callback = main_ctl_timer_handler;

	printfxy( 0, 8, "main_ctl()" );

	install_timer( &main_ctl_timer );

inf_loop:
	goto	inf_loop;
}


void	app_entry()
{
	start_task( main_ctl, 1, OPT_TIMESHARE, NULL );

//	p = big_buffer;
	__asm__ __volatile__ ("label:\n");
	__asm__ __volatile__ ("mov eax, offset label");
	__asm__ __volatile__ ("nop");
	__asm__ __volatile__ ("hlt");
	__asm__ __volatile__ ("jmp	label");
}

