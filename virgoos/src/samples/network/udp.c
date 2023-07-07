/*
 *		udo.c
 *
 *		Source code of UDP messaging sample SOS application.
 */

#include	"sosdef.h"
#include	"taskman.h"
#include	"config.h"
#include	"drvint.h"
#include	"socket.h"
#include	"errno.h"

byte *timer_ptr = ( byte* )0xB8000;

int	task1_attr, task2_attr;
char task1_msg[] = "Task1";
char task2_msg[] = "Task2";
int	task2_started = 0;

int	sock;
struct	sockaddr_in	my_addr;
struct	sockaddr_in	peer_addr;
struct	sockaddr_in	from_addr;

socklen_t	from_addr_len;

char	msg[256];
int	msg_len;

char	from_msg[256];
int	from_msg_len;


extern unsigned char	def_ip_addr[4];
const unsigned char	*my_ip_addr = def_ip_addr;
const char	peer_ip_addr[4] = {172, 20, 2, 110};

/*
void	task2( void )
{
	int	i;
	int	j = 0;
	char	buf[ 256 ];

	while ( 1 )
	{
		for ( i = 0; i < strlen( task1_msg ); ++i )
		{
			*( ( byte * )( 0xB8000 + ( 80 * 24 + i + 30 ) * 2 ) ) = task2_msg[ i ];
			*( ( byte * )( 0xB8000 + ( 80 * 24 + i + 30 ) * 2 + 1 ) ) = task2_attr++;
		}

		open_drv( DEV_ID( TERM_DEV_ID, 0 ) );
		sprintf( buf, "Task2: %d\r\n", ++j );
		write_drv( DEV_ID( TERM_DEV_ID, 0 ), buf, strlen( buf ) );
	}
}
*/


void	task1( void )
{
	int	i;
	int	ch = 0;
	char	ascii;

	int	rv;

// Single-task is OK
/*
	if ( !task2_started )
	{
		start_task( task2, 1, OPT_TIMESHARE );
		++task2_started;
	}
*/
	sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);


	// Should be added functions inet_ntoa() and inet_addr()

	my_addr.sin_family = AF_INET;
	my_addr.sin_port = htons(10002);		// Port 10002
	memcpy(&my_addr.sin_addr.s_addr, my_ip_addr, 4);
	rv = bind(sock, (struct sockaddr*)&my_addr, sizeof(struct sockaddr_in));

	peer_addr.sin_family = AF_INET;
	peer_addr.sin_port = htons(10003);		// Port 10003
	memcpy(&peer_addr.sin_addr.s_addr, peer_ip_addr, 4);

	// TODO: print what bind() returned

	while ( 1 )
	{
		ch = 0;

		for ( i = 0; i < strlen( task2_msg ); ++i )
		{
			*( ( byte * )( 0xB8000 + ( 80 * 24 + i + 50 ) * 2 ) ) = task1_msg[ i ];
			*( ( byte * )( 0xB8000 + ( 80 * 24 + i + 50 ) * 2 + 1 ) ) = task1_attr++;
		}

		rv = read_drv( DEV_ID( KBD_DEV_ID, 0 ), &ch, sizeof( int ) );

		if (0 == rv)
			continue;

		if ( ch >= 1 )
			printfxy( 2, 24, "Read from keyboard: %x, %c     ", ( ch & 0xFF ), ( ch >> 8 ) & 0xFF );

		ascii = (ch >> 8) & 0xFF;
		msg[msg_len++] = ascii;
		if ('\n' == ascii || '\r' == ascii || 128 == msg_len)
		{
			msg[msg_len] = '\0';
			// Message is ready

			rv = sendto(sock, msg, msg_len, 0, (struct sockaddr*)&peer_addr, sizeof(struct sockaddr_in));
			// TODO: print that the message is sent
			msg_len = 0;

			from_addr_len = sizeof(struct sockaddr_in);
			rv = recvfrom(sock, from_msg, from_msg_len, 0, (struct sockaddr*)&from_addr, &from_addr_len);

			// TODO: print receiveed message
		}
	}

	// Never reached
	terminate();
}


void	app_entry()
{
	unsigned char	*p = ( unsigned char *) 0xB8000;
	int	i;

	char	*p1;
	long	*p2;
	char	buf[ 256 ];

	start_task(task1, 1, OPT_TIMESHARE, NULL);

//	After start_task() this code will never be reached.
	for (;;)
		;
/*
label:
	_asm	mov eax, offset label
	_asm	nop
	_asm	hlt
	_asm	jmp	label
*/
}
