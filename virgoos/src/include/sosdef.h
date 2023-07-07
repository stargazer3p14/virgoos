/*
 *	sosdef.h
 *
 *	Basic definitions for September OS. Basic OS services, libc, exec functions
 */

#ifndef	SOSDEF__H
 #define	SOSDEF__H

#include <stddef.h>
#include <limits.h>
#include <stdarg.h>
#include "timers.h"
#include "sosdef-arch.h"

#ifndef	NULL
 #define NULL	((void*)0)
#endif

struct timeval
{
	unsigned long	tv_sec;
	unsigned long	tv_usec;
};

struct tm
{
	int	tm_sec;
	int	tm_min;
	int	tm_hour;
	int	tm_mday;
	int	tm_mon;
	int	tm_year;
	int	tm_wday;
	int	tm_yday;
	int	tm_isdst;
};

// ISR prototype
typedef	int (*isr)(void);

// ISR callbacks interface (for OS internal use)
int	set_int_callback( dword int_no, isr proc );
void	call_callbacks( dword int_no );

// Architecture and platform-specific functions
void	arch_eoi(int int_no);
void	init_platform(void);
void	plat_init_timers(void);
void	plat_init_sys_time(void);
void	plat_timer_eoi(void);
void	plat_halt(void);
void	plat_reboot(void);
void    plat_mask_unhandled_int(void);


// ctype macros
static int isspace(int a)
{
	return	(a == ' ' || a == '\t');
}

static int	isdigit(int c)
{
	return	(c <= '9' && c >= '0');
}


static int	isascii(int c)
{
	return	(c <= 0x7F && c >= 0);
}

static int	toupper(int c)
{
	if (c < 'a' || c > 'z')
		return	c;
		
	return	c - ('a' - 'A');
}

static int	tolower(int c)
{
	if (c < 'A' || c > 'A')
		return	c;
		
	return	c + ('a' - 'A');
}

// LibC subset functions (strings)
void	__hextoa( byte hex, char *a );
void	__hex16toa( word hex16, char *a );
void	__hex32toa( dword hex32, char *a );
int	__itoa(char *buf, unsigned __int64 n, int radix, int _case, int _signed);
int	sprintf(char *str, const char *fmt, ...);
int	vsprintf(char *str, const char *fmt, va_list argp);
int	sscanf(char *str, const char *fmt, ...);
int	vsscanf(char *str, const char *fmt, va_list argp);
void *memcpy( void *dest, const void *src, size_t count );
void *memmove( void *dest, const void *src, size_t count );
void *memset( void *dest, int c, size_t count );
char *strcpy(char *to, const char *from);
char *strncpy(char *to, const char *from, size_t count);
char *strcat(char *to, const char *from);
char *strncat(char *to, const char *from, size_t count);
int	memcmp(const void *p1, const void *p2, size_t n);
int	strcmp(const char *p1, const char *p2);
int	strcasecmp(const char *p1, const char *p2);
int	strcoll(const char *p1, const char *p2);
int	strncmp(const char *p1, const char *p2, int n);
size_t strxfrm(char *s1, const char *s2, size_t n);
void *memchr(const void *s, int c, size_t n);
char	*strchr(const char *s, int c);
char	*strrchr(const char *s, int c);
size_t strcspn(const char *s1, const char *s2);
char *strpbrk(const char *s1, const char *s2);
size_t strspn(const char *s1, const char *s2);
char *strstr(const char *s1, const char *s2);
char *strtok(char *s1, const char *s2);
size_t strlen(const char *str);
char	*strerror(int err);

// Allocation functions
void	*malloc(size_t	size);
void	*calloc(size_t num, size_t size);
void	free(void *p);
void	*realloc(void *ptr, size_t size);

// Convenience output functions
int	printfxy(int x, int y, const char *fmt, ...);
int	cprintfxy(int color, int x, int y, const char *fmt, ...);
int	serial_printf(const char *fmt, ...);

// Device drivers interface
int open_drv(unsigned long drv_id);
int read_drv(unsigned long drv_id, void *buffer, unsigned long length);
int write_drv(unsigned long drv_id, const void *buffer, unsigned long length);
int ioctl_drv(unsigned long drv_id, int cmd, ...);
int close_drv(unsigned long drv_id);
int dev_name_to_id(const char *name);

// Timers interface
int	install_timer( timer_t *tm );
int	remove_timer( timer_t *tm );
unsigned long	timeval_to_ticks(const struct timeval *tv);

// Timeout (select() / poll()
#define	TIMEOUT_INFINITE	0xFFFFFFFF

// Misc stdlib functions
unsigned long	random(void);

// Time
time_t	time(time_t *tloc);
struct tm       *localtime(const time_t *timep);
char    *asctime(struct tm *tm);
char    *ctime(const time_t *timep);
struct tm       *gmtime(const time_t *timep);
time_t  mktime(struct tm *tm);

// Misc functions compatible with various systems with various purposes (mainly Linux and mainly for ease of porting programs)
unsigned int sleep(unsigned int seconds);

// stdio I/O interface
#define	BUFSIZ	4096
#define	EOF	-1

// Buffer (buf) in FILE implements a "sliding window" inside a file.
// If wr_pos != rd_pos (appending) then only rd_pos is related to buf[]. Writing at wr_pos goes directly to a file, unbuffered.
typedef struct FILE
{
	int	fd;
	void	*buf;
	size_t	buf_size;		// May be altered by setvbuf()
	unsigned buf_st, buf_end;	// Start and end of buffer - it's cyclic
	off_t	at_pos;			// File position that corresponds to buf[buf_st]. Current file position may already have moved with fseek().
	off_t	rd_pos, wr_pos;		// Read and write (separate) positions -- may be needed for "a+" opened files. fseek() will synchronize them
#define	FILE_IND_ERR	1
#define	FILE_IND_EOF	2
#define	FILE_IND_TEXT	0x80000000	// Text/binary file 
#define	FILE_IND_CUSTOM_BUF	0x40000000	// Set if custom buffer is supplied by setvbuf() (if yes, the caller is responsible to free() the buffer if needed)
	unsigned long	ind;		// Indicator
	int	buf_mode;
} FILE;

#ifndef	LIBC__SOURCE
extern FILE	*stdin, *stdout, *stderr;
#endif

typedef	off_t	fpos_t;

int	rename(const char *oldpath, const char *newpath);
FILE	*fopen(const char *path, const char *mode);
FILE	*freopen(const char *path, const char *mode, FILE *stream);
int	fclose(FILE *fp);
size_t	fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
size_t	fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);
int	fprintf(FILE *stream, const char *format, ...);
int	printf(const char *format, ...);
int	vfprintf(FILE *stream, const char *format, va_list ap);
int	fscanf(FILE *stream, const char *format, ...);
int	vfscanf(FILE *stream, const char *format, va_list ap);
int	fgetc(FILE *stream);
char	*fgets(char *s, int size, FILE *stream);
int	getc(FILE *stream);
int	ungetc(int c, FILE *stream);
int	fputs(const char *s, FILE *stream);
int	fputc(int c, FILE *stream);
int	putc(int c, FILE *stream);

// Buffering modes
enum {_IONBF, _IOLBF, _IOFBF};

int	setvbuf(FILE *stream, char *buf, int mode, size_t size);
void	setbuf(FILE *stream, char *buf);
int	fflush(FILE *stream);
int	feof(FILE *stream);
int	ferror(FILE *stream);
void	clearerr(FILE *stream);
int	fseek(FILE *stream, long offset, int whence);
long	ftell(FILE *stream);
void	rewind(FILE *stream);
int	fgetpos(FILE *stream, fpos_t *pos);
int	fsetpos(FILE *stream, fpos_t *pos);

// system() handling
int system(const char *command);

// exec() and friends
int execve(const char *filename, char *const argv[], char *const envp[]);
int execvp(const char *file, char *const argv[]);
int execv(const char *path, char *const argv[]);
int execle(const char *path, const char *arg, ...);
int execlp(const char *file, const char *arg, ...);
#define	execl	execlp
void exit(int status);
int atexit(void (*function)(void));
pid_t wait(int *status);
pid_t waitpid(pid_t pid, int *status, int options);
pid_t getpid(void);
pid_t getppid(void);
pid_t fork(void);

#ifndef	EXEC__C
extern char	**environ;
#endif

void	init_libc(void);

// It is probably possible to calculate precise values for every system and implement them in assembly using register-only operations, to make deterministic.
// But for now it's good, aggregate error on loop of 60 udelay(1000000) (!) on evmdm6467 was only less then 4 seconds (udelay() loop was faster).
// Also, it's probably pointless to run udelay() with interrupts disabled (otherwise caller may do it himself)
//
//static 	void udelay(dword n)
#if !CALIBRATE_UDELAY
#if defined (pc)
 #define CALIBRATED_UDELAY_COUNT1	0
 #define CALIBRATED_UDELAY_COUNT2	420
#elif defined (dm6467)
 #define CALIBRATED_UDELAY_COUNT1	0
 #define CALIBRATED_UDELAY_COUNT2	2
#elif defined (versatile)
 #define CALIBRATED_UDELAY_COUNT1	0
 #define CALIBRATED_UDELAY_COUNT2	91	
#else
 // Not a calibrated value
 #define CALIBRATED_UDELAY_COUNT1	0
 #define CALIBRATED_UDELAY_COUNT2	2
#endif
#else // CALIBRATE_UDELAY is defined, doesn't matter what we put here
 // Not a calibrated value
// #define CALIBRATED_USEC_VAL	1000
// #define CALIBRATED_UDELAY_COUNT1	0
// #define CALIBRATED_UDELAY_COUNT2	2
#endif

extern	volatile unsigned	calibrated_usec;
extern	volatile unsigned	calibrated_udelay_count1;
extern	volatile unsigned	calibrated_udelay_count2;

//
// We have currently a couple of problems with this delay
//
// 1) Outer loop for (n = m; ...) interferes with small counts of inner loops, it was observed even on VMVare (or was it due to high interrupt rate? Run it with interrupts disabled)
//
// 2) Count is really low on DM6467 - it gets roughly to 1, and outer for() is even more influental at all. Does this mean that we run without cache?
//

// Good only for small delays (so that calibrated_udelay_count2 * m <= UINT_MAX)
#define udelay(m)       \
do \
{       \
        volatile unsigned c1, c2;      \
        volatile unsigned n;      \
	uint32_t	intfl;	\
	static volatile unsigned this_udelay_count2;	\
\
	intfl = get_irq_state();	\
	/*disable_irqs();*/	\
	this_udelay_count2 = m * calibrated_udelay_count2;	\
        /*for (n = m; n-- > 0;)*/ \
                for (c1 = 0; c1 <= calibrated_udelay_count1; ++c1)   \
		{	\
                	for (c2 = 0; c2 < this_udelay_count2; ++c2)   \
                        	;       \
			if (c1 == calibrated_udelay_count1)	\
				break;	\
		}	\
	restore_irq_state(intfl);	\
}       \
while(0)

//	Application's entry point.
void	app_entry();

#define	abs(a)	(a >= 0 ? a : -a)

#endif	/*	SOSDEF__H	*/

