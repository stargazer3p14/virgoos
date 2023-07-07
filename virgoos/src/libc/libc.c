/*
 *	libc.c
 *
 *	Implements functions from standard C library for September OS.
 *	File, stream (stdio) functions etc. which need special facilities
 *	depend on CFG_POSIXIO.
 *
 */

// STANDALONE macro is define when testing this libc implementation in regular hosted envorinment. It takes care that standard library functions names differ so they don't interfere with the same names in the hosted library's implementation
//#define	STANDALONE

//
// For now, implementation will be 32-bit only
//

#if 0
#ifdef	__int64
 #undef __int64
#endif
#define	__int64	long long
#endif

#ifndef STANDALONE
#define	LIBC__SOURCE
#include "sosdef.h"
#include "io.h"
#else
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#endif


#ifndef	STANDALONE
FILE	*stdin, *stdout, *stderr;
#endif


enum {GEN_SCANF_SRC_STRING, GEN_SCANF_SRC_FILE};
static int	generic_scanf(void *src, int src_type, const char *fmt, va_list argp, int (*get_next_char)(void **src));


#if 0
#ifndef STANDALONE
// Shut up the linker
void	_alloca(void) {}
#endif
#endif

#if 0
unsigned long long __umoddi3(unsigned long long dividend, unsigned long long divisor)
{
	// Not entirely correct, but will do for now: divisor is really 32-bit and
	// result is 32-bit too (cast to 64-bit)
	unsigned long	res;
	unsigned long	temp1, temp2, temp3;

	temp1 = dividend >> 32;
	temp2 = dividend & 0xFFFFFFFF;
	temp3 = divisor & 0xFFFFFFFF;

	__asm__ __volatile__ ("push	eax\n"
							"push	edx\n"
							"push	ecx\n");
	__asm__ __volatile__ ("div	ecx\n" : "=d"(res): "d"(temp1), "a"(temp2));
	__asm__ __volatile__ ("pop	ecx\n"
							"pop	edx\n"
							"pop	eax\n");

	return	(unsigned long long) res;
}

unsigned long long __udivdi3(unsigned long long dividend, unsigned long long divisor)
{
	// Not entirely correct, but will do for now: divisor is really 32-bit and
	// result is 32-bit too (cast to 64-bit)
	unsigned long	res;
	unsigned long	temp1, temp2, temp3;

	temp1 = dividend >> 32;
	temp2 = dividend & 0xFFFFFFFF;
	temp3 = divisor & 0xFFFFFFFF;

	__asm__ __volatile__ ("push	eax\n"
							"push	edx\n"
							"push	ecx\n");
	__asm__ __volatile__ ("div	ecx\n" : "=a"(res): "d"(temp1), "a"(temp2));
	__asm__ __volatile__ ("pop	ecx\n"
							"pop	edx\n"
							"pop	eax\n");

	return	(unsigned long long) res;
}
#endif

#ifdef STANDALONE
#include <stdio.h>
#include <stdarg.h>
#include <limits.h>
#include <ctype.h>

typedef	unsigned char	byte;
typedef	unsigned short	word;
typedef	unsigned long	dword;

// #define	__int64	long long
#define	intmax_t	long
#define	ptrdiff_t	long

#endif

//
//	Convert integer into hex string representation
//
void	__hextoa( byte hex, char *a )
{
	a[ 0 ] = ( hex >> 4 );
	a[ 1 ] = ( hex & 0xF );

	a[ 0 ] += '0';
	if ( a[ 0 ] > '9' )
		a[ 0 ] += 'A' - '0' - 10;

	a[ 1 ] += '0';
	if ( a[ 1 ] > '9' )
		a[ 1 ] += 'A' - '0' - 10;

	a[ 2 ] = '\0';
}


void	__hex16toa(word hex16, char *a)
{
	__hextoa((byte)((hex16 >> 8) & 0xFF), a);
	__hextoa((byte)(hex16 &0xFF), a + 2);
}


void	__hex32toa(dword hex32, char *a)
{
	__hex16toa((word)((hex32 >> 16 ) & 0xFFFF), a);
	__hex16toa((word)(hex32 &0xFFFF), a + 4);
}


void	__hex64toa(__int64 hex64, char *a)
{
	__hex32toa((dword)((hex64 >> 32 ) & 0xFFFFFFFF), a);
	__hex32toa((dword)(hex64 &0xFFFFFFFF), a + 8);
}


//
//	Convert integer to string representation
//
int	__itoa(char *buf, unsigned __int64 n, int radix, int _case, int _signed)
{
	char s[256];
	int	i = 0, ndigits;

	if (_signed && (signed __int64)n < 0)
	{
		s[i++] = '-';
		n = -(signed __int64)n;
	}

	do
	{
		s[i] = (char)(n % (unsigned __int64)radix);
		if (s[i] < 10)
			s[i] += '0';
		else if (_case)
			s[i] += 'A' - 10;
		else
			s[i] += 'a' - 10;
		++i;
		n /= (unsigned __int64)radix;
	} while (n != 0);

	ndigits = i;
	--i;

	if ('-' == s[0])
		*buf++ = '-';

	while (i >= 0 && s[i] != '-')
		*buf++ = s[i--];
	
	return	ndigits;
}


//
//	Convert double-precision IEEE-764 floating point number to string representaiton
//	according to ISO C9x format specification
//
//	outform: 1 <-> f; 2 <-> e; 3 <-> g; 4 <-> a
//	_case specifies whether letters are uppercase (1) or lowercase (0)
//
//	Conversion issues: meanwhile values with significand > 52 don't display correctly
//	TODO: conversions of g/G, a/A
//
int	__ftoa(char *buf, double n, int outform, int _case, int precision)
{
	char s[4096];
	int	i = 0, ndigits = 0;
	unsigned __int64	significand, _temp, _temp1;
	int	exponent;
	char	_int[32], _fract[32];
	int	l, l1;
	int	j;
	char	nan1[] = "nan";
	char	nan2[] = "NAN";
	char	infi1[] = "infinity";
	char	infi2[] = "INFINITY";
	char	*p;
	int		e;


	// Store sign bit
	if ((*((unsigned char*)&n + 7) & 0x80) != 0)
	{
		s[ndigits++] = '-';
		*((unsigned char*)&n + 7) &= ~0x80;
	}


	// Get the exponent
	exponent = ((int)*((unsigned char*)&n + 6) >> 4) + ((int)*((unsigned char*)&n + 7) << 4);

	// Check for NaN and Infinity
	if (0x7FF == exponent)
	{
		for (i = 0; i < 52; ++i)
			if ((*((unsigned char*)&n + i/8) & 1 << i%8) != 0)
				break;

		if (i < 52)
		{
			if (_case)
				p = nan2;
			else
				p = nan1;

			for (j = 0; j < sizeof(nan1) - 1; ++j)
				s[ndigits++] = p[j];
		}
		else
		{
			if (_case)
				p = infi2;
			else
				p = infi1;

			for (j = 0; j < sizeof(infi1) - 1; ++j)
				s[ndigits++] = p[j];		
		}

		return	ndigits;
	}

	// Normalized number
	exponent -= 1023;		// Subtract bias

	// Get the significand

	significand = ((unsigned __int64)*(unsigned char*)&n + 0) + ((unsigned __int64)*((unsigned char*)&n + 1) << 8) +
		((unsigned __int64)*((unsigned char*)&n + 2) << 16) + ((unsigned __int64)*((unsigned char*)&n + 3) << 24) +
		((unsigned __int64)*((unsigned char*)&n + 4) << 32) + ((unsigned __int64)*((unsigned char*)&n + 5) << 40) +
		(((unsigned __int64)*((unsigned char*)&n + 6) & 0x0F) << 48);


	// If both integer and fraction parts are present
	if (exponent < 52 && exponent > 0)
	{
		_temp = significand >> (52 - exponent) | 1 << exponent;
		l1 = __itoa(_int, _temp, 10, 0, 1);

		n = (n - (signed)_temp) * 10.0;
do_fract:
		for (i = 0; n != 0.0 && i < 31; n *= 10.0, ++i)
		{
			j = (int)n;
			_fract[i] = j + '0';
			n -= (double)(int)n;
		}

		l = i;
	}
	// Integer part is not present
	else if (exponent < 0)
	{
		_int[0] = '0';
		l1 = 1;

		n *= 10.0;
		goto	do_fract;
	}

	{
/*
		_temp = (unsigned __int64)1 << (52 - exponent);
		j = 52 - exponent - 1;
		for (_temp = 0, _temp1 = 5; j > 0 && _temp < ((unsigned __int64)1 << 63) / 10; --j)
		{
			if ((significand & (unsigned __int64)1 << j) != 0)
				_temp += _temp1;

			_temp *= (unsigned __int64)10;
			_temp1 *= (unsigned __int64)5;
		}
*/

		// For e and E formats we need to shift to the normal form
		if (2 == outform)
		{
			j = 0;

			if (l1 > 1)
			{
				e = l1 - 1;

				for (i = l - e; i >= 0; --i)
					_fract[i+e] = _fract[i];

				for (i = 0; i < e; ++i)
					_fract[i] = _int[i+1];

				l += e;

				if (l > precision)
					l = precision;
				l1 -= e;
			}
			else if ('0' == _int[0])
			{
				for (e = 0; '0' == _fract[e] && e < sizeof(_fract); ++e)
					;

				if (e != sizeof(_fract))
				{
					_int[0] = _fract[e++];
					for (j = e; j < sizeof(_fract); ++j)
						_fract[j-e] = _fract[j];

					e = -e;
				}
				// It's 0
				else
				{
					e = 0;
				}
			}
		}

		// Round due to precision
		if (precision < l)
		{
			for (i = precision - 1; i >= 0; --i)
				if (_fract[i+1] >= '5')
				{
					if (++_fract[i] != '9' + 1)
						break;
				}
				else
					break;

			if (-1 == i)
			{
				if (_int[l1-1] == '9')
					for (i = l1-1; i >= 0; --i)
						if (++_int[i] != '9' + 1)
							break;
			}

			if (-1 == i)
			{
				for (i = l1 - 1; i >= 0; --i)
					_int[i+1] = _int[i];
				_int[0] = '1';
				++l1;
			}

			for (j = 0; j < precision; ++j)
				if ('9' + 1 == _fract[j])
					_fract[j] = '0';

			for (j = 0; j < l1; ++j)
				if ('9' + 1 == _int[j])
					_int[j] = '0';

			l = precision;
		}

		for (i = 0; i < l1; ++i)
			s[ndigits++] = _int[i];
		s[ndigits++] = '.';

		for (i = 0; i < l; ++i)
			s[ndigits++] = _fract[i];

		if (2 == outform)
		{
			if (_case)
				s[ndigits++] = 'E';
			else
				s[ndigits++] = 'e';

			if (e < 0)
			{
				s[ndigits++] = '-';
				e = -e;
			}
			else
				s[ndigits++] = '+';
			j = __itoa(s+ndigits, e, 10, 0, 0);
			if (1 == j)
			{
				s[ndigits+1] = s[ndigits];
				s[ndigits] = '0';
				++j;
			}
			ndigits += j;
		}
	}

	for (i = 0; i < ndigits; ++i)
		buf[i] = s[i];

	return ndigits;
}


#ifndef STANDALONE
int	vsprintf(char *str, const char *fmt, va_list argp)
#else
int	vsprintf1(char *str, const char *fmt, va_list argp)
#endif
{
	int len;
	int	flags;		// 0x1 <-> '-'; 0x2 <-> '+'; 0x4 <-> '#'; 0x8 <-> ' '; 0x10 <-> '0'
	int	state;		// 0 - initial state;
	int	min_len;		// Minimal length
	int	precision;		// Precision
	int	length_mod;		// Length modifier: 1 <-> hh; 2 <-> h; 3 <-> l; 4 <-> ll; 5 <-> j; 6 <-> z; 7 <-> t; 8 <-> L
	char	conv_spec;		// Conversion specifier

	int	n;

	len = 0;

	// Parse fmt and fill str accordingly
	state = 0;

	while (*fmt != 0)
	{
		if (*fmt != '%')
		{
			str[len++] = *fmt++;
			continue;
		}

		++state;
		flags = 0;
		min_len = 0;
		precision = INT_MAX;		// Default precision
		length_mod = 0;
		while (state != 0)
		{
state1:
			// State 1: parsing flags
			switch(*++fmt)
			{
			case	'-':
				flags |= 0x1;
				break;
			case '+':
				flags |= 0x2;
				break;
			case '#':
				flags |= 0x4;
				break;
			case ' ':
				flags |= 0x8;
				break;
			case '0':
				flags |= 0x10;
				break;
				// Fall through
			default:
				++state;
				break;
			}

			if (1 == state)
				continue;

			// State 2: parsing minimum field width
			if ('*' == *fmt)
			{
				min_len = (int)va_arg(argp, int);
				++fmt;
			}
			else
				while (isdigit(*fmt))
					min_len = min_len * 10 + (int)(*fmt++ - '0');
			++state;

			// State 3: parsing precision
			if ('.' == *fmt)
			{
				precision = 0;
				++fmt;
				if ('*' == *fmt)
				{
					precision = (int)va_arg(argp, int);
					++fmt;
				}
				else
					while (isdigit(*fmt))
						precision = precision * 10 + (int)(*fmt++ - '0');
			}
			++state;

			// State 4: parsing length modifier
			switch (*fmt)
			{
				default:
					break;

				case 'h':
					++fmt;
					length_mod = 2;
					if ('h' == *fmt)
					{
						length_mod = 1;
						++fmt;
					}
					break;

				case 'l':
					++fmt;
					length_mod = 3;
					if ('l' == *fmt)
					{
						length_mod = 4;
						++fmt;
					}
					break;

				case 'j':
					length_mod = 5;
					++fmt;
					break;

				case 'z':
					length_mod = 6;
					++fmt;
					break;

				case 't':
					length_mod = 7;
					++fmt;
					break;

				case 'L':
					length_mod = 8;
					++fmt;
					break;
			}
			++state;

			// State 5: parsing conversion specifier
			conv_spec = *fmt++;
			++state;
			
			// State 6: parameter's conversion
			switch(conv_spec)
			{
				unsigned __int64	d;
				int	p;
				char	buf[256], prec_buf[256];
				int l;
				int i;
				int	*pn;
				char	*s;
				double	f;
				int	_case;

			case 'i':
			case 'd':
				if (1 == length_mod)
					d = (signed __int64)(signed char)va_arg(argp, int);
				else if (2 == length_mod)
					d = (signed __int64)(signed short)va_arg(argp, int);
				else if (3 == length_mod)
					d = (signed __int64)va_arg(argp, signed long);
				else if (4 == length_mod)
					d = (signed __int64)va_arg(argp, signed __int64);
				else if (5 == length_mod)
					d = (unsigned __int64)va_arg(argp, intmax_t);
				else if (6 == length_mod)
					d = (unsigned __int64)va_arg(argp, size_t);
				else if (7 == length_mod)
					d = (unsigned __int64)va_arg(argp, ptrdiff_t);
				else
					d = (signed __int64)va_arg(argp, signed int);
				
				n = __itoa(buf, d, 10, 0, 1);
				goto	apply_mods;

			case 'u':
			case 'x': case 'X':
			case 'o':
				if (1 == length_mod)
					d = (unsigned char)va_arg(argp, int);
				else if (2 == length_mod)
					d = (unsigned short)va_arg(argp, int);
				else if (3 == length_mod)
					d = (unsigned long)va_arg(argp, unsigned long);
				else if (4 == length_mod)
					d = (unsigned __int64)va_arg(argp, unsigned __int64);
				else if (5 == length_mod)
					d = (unsigned __int64)va_arg(argp, intmax_t);
				else if (6 == length_mod)
					d = (unsigned __int64)va_arg(argp, size_t);
				else if (7 == length_mod)
					d = (unsigned __int64)va_arg(argp, ptrdiff_t);
				else
					d = (unsigned __int64)va_arg(argp, signed int);

				if ('u' == conv_spec)
					n = __itoa(buf, d, 10, 0, 0);
				else if ('o' == conv_spec)
					n = __itoa(buf, d, 8, 0, 0);
				else if ('x' == conv_spec)
					n = __itoa(buf, d, 16, 0, 0);
				else	// 'X'
					n = __itoa(buf, d, 16, 1, 0);

				goto	apply_mods;

apply_mods:
				//
				// Apply conversion modifiers, flags, etc.
				//
				if (INT_MAX == precision)		// Default precision
					precision = 1;

				// Take into account precision. Precision 0 means no digits
				if (0 == precision && 0 == d)
					break;

				// Adjust buf according to precision into prec_buf
				// and apply '+' flag
				l = 0;
				p = precision - n;
				if ('-' == buf[0])
					prec_buf[l++] = '-';
				else if (flags & 0x2)
					prec_buf[l++] = '+';

				i = 0;
				if ('-' == buf[0])
				{
					++i;
					++p;
				}

				// Fill leading 0s if necessary
				while (p-- > 0)
					prec_buf[l++] = '0';

				for (; i < n; ++i)
					prec_buf[l++] = buf[i];

				// Now l holds chars count in prec_buf

				// Apply minimal field length.
				// The result will be back in buf (from prec_buf)
				p = 0;
				if (min_len > l)
				{
					// If left-pad with spaces (right-justified)
					if ((flags & 0x1) == 0)
						while (min_len-- > l)
							if ((flags & 0x10) != 0)
								buf[p++] = '0';
							else
								buf[p++] = ' ';

					for (i = 0; i < l; ++i)
						buf[p++] = prec_buf[i];

					// If right-pad with spaces (left-justified)
					if ((flags & 0x1) != 0)
						while (min_len-- > l)
							buf[p++] = ' ';						
				}
				else
					for (p = 0; p < l; ++p)
						buf[p] = prec_buf[p];

				// Now p holds chars count in buf.
				// Copy them to str (output buffer)
				for (i = 0; i < p; ++i)
					str[len++] = buf[i];

				break;

			case 'c':
				str[len++] = (char)va_arg(argp, int);
				break;

			case 's':
				s = (char*)va_arg(argp, char*);
				for (i = 0; i < precision && s[i] != '\0'; ++i)
					str[len++] = s[i];
				break;

			case 'p':
				s = (char*)va_arg(argp, char*);			// void* and char* have the same representation
				__hex32toa((dword)s, str+len);
				len += 8;
				break;

			case '%':
				str[len++] = '%';
				break;

			case 'n':
				pn = (int*)va_arg(argp, int*);
				*pn = len;
				break;

			case 'f':
			case 'F':
				if (INT_MAX == precision)
					precision = 6;
				f = va_arg(argp, double);
				n = __ftoa(buf, f, 1, 0, precision);
				for (i = 0; i < n; ++i)
					str[len++] = buf[i];
				break;

			case 'e':
			case 'E':
				if ('e' == conv_spec)
					_case = 0;
				else
					_case = 1;
				if (INT_MAX == precision)
					precision = 6;
				f = va_arg(argp, double);
				n = __ftoa(buf, f, 2, _case, precision);
				for (i = 0; i < n; ++i)
					str[len++] = buf[i];
				break;
			} // switch (conv_spec)

			// Switch back to state 0
			state = 0;
		}	
	}	// while (!*fmt)
	
	str[len] = '\0';
	return len;
}


// Variant of get_next_char() for generic_scanf() automaton that sources at string (char*)
int str_get_next_char(void **src)
{
	int	ch;
	char	*str = *src;

	if (!(ch = *str++))
	{
		ch = EOF;
		str = NULL;
	}
	*src = str;
	return	ch;
}

#ifdef CFG_POSIXIO
// Variant of get_next_char() for generic_scanf() automaton that sources at file (FILE*)
int file_get_next_char(void **src)
{
	return fgetc(*(FILE**)src);
}
#endif // CFG_POSIXIO

#ifndef STANDALONE
int	vsscanf(char *str, const char *fmt, va_list argp)
#else
int	vsscanf1(char *str, const char *fmt, va_list argp)
#endif
{
	return	generic_scanf(str, GEN_SCANF_SRC_STRING, fmt, argp, str_get_next_char);
}

#if 0
// Original vsscanf() version
static int	generic_scanf(void *src, int src_type, const char *fmt, va_list argp)
{
	int len;
	int	flags;		// 0x1 <-> '*'
	int	state;		// 0 - initial state;
	int	min_len;		// Minimal length
	int	precision;		// Precision
	int	length_mod;		// Length modifier: 1 <-> hh; 2 <-> h; 3 <-> l; 4 <-> ll; 5 <-> j; 6 <-> z; 7 <-> t; 8 <-> L
	char	conv_spec;		// Conversion specifier

	int	n;
	int	scan_cnt;	// Count of assigned parameters
	int	max_len;
	char	*str = src;
	FILE	*file = src;
	int	ch;


	len = 0;

	// Parse fmt and fill parameters from str accordingly
	scan_cnt = -1;	// return value is EOF at the beginning

	scan_cnt = 0;	// TODO: understand when should be returned -1 and when 0.

	while (*fmt != 0)
	{
		// White space directive: read up to first non-white space
		if (isspace(*fmt))
		{
			while (isspace(str[len]))
				++len;

			while (isspace(*fmt))
				++fmt;

			if ('\0' == str[len])
				return	scan_cnt;
		}

		// Ordinary character
		if (*fmt != '%')
		{
			if (str[len++] != *fmt++)
				return	scan_cnt;

			continue;
		}

		// Conversion ('%')

		++state;
		flags = 0;
		max_len = 0;
//		precision = INT_MAX;		// Default precision
		length_mod = 0;

		while (state != 0)
		{
state1:
			// State 1: optional '*'
			if ('*' == *++fmt)
				flags |= 0x1;
			if (*fmt == '\0')
				return	scan_cnt;

			++state;

			// State 2: optional maximum field width

			while (isdigit(*fmt))
				max_len = max_len * 10 + (int)(*fmt++ - '0');

			++state;

			// State 3: parsing length modifier

			switch (*fmt)
			{
				default:
					break;

				case 'h':
					++fmt;
					length_mod = 2;
					if ('h' == *fmt)
					{
						length_mod = 1;
						++fmt;
					}
					break;

				case 'l':
					++fmt;
					length_mod = 3;
					if ('l' == *fmt)
					{
						length_mod = 4;
						++fmt;
					}
					break;

				case 'j':
					length_mod = 5;
					++fmt;
					break;

				case 'z':
					length_mod = 6;
					++fmt;
					break;

				case 't':
					length_mod = 7;
					++fmt;
					break;

				case 'L':
					length_mod = 8;
					++fmt;
					break;
			}
			++state;

			// State 4: parsing conversion specifier
			conv_spec = *fmt++;
			++state;
			
			// State 5: parameter's conversion
			switch(conv_spec)
			{
				unsigned __int64	d;
				int	p;
				int l;
				int i;
				int	*pn;
				char	*s;
				char	**s2;
				double	f;
				int	sign;

			case 'i':
			case 'd':
				sign = 0;
				if ('-' == str[len])
				{
					sign = 1;
					++len;
				}
				if (5 == length_mod || 6 == length_mod || 7 == length_mod)
					for (d = 0; isdigit(str[len]); ++len)
						d = d * 10 + (unsigned __int64)(str[len] - '0');
				else
					for (d = 0; isdigit(str[len]); ++len)
						d = d * 10 + (signed __int64)(str[len] - '0');

				if (sign)
					d = -d;

				switch (length_mod)
				{
				default:
					*(int*)va_arg(argp, int*) = (int)d;
					break;
				case 1:
					*(signed char*)va_arg(argp, signed char*) = (signed char)d;
					break;
				case 2:
					*(signed short*)va_arg(argp, signed short*) = (signed short)d;
					break;
				case 3:
					*(signed long*)va_arg(argp, signed long*) = (signed long)d;
					break;
				case 4:
					*(signed __int64*)va_arg(argp, signed __int64*) = (signed __int64)d;
					break;
				case 5:
					*(intmax_t*)va_arg(argp, intmax_t*) = (intmax_t)d;
					break;
				case 6:
					*(size_t*)va_arg(argp, size_t*) = (size_t)d;
					break;
				case 7:
					*(ptrdiff_t*)va_arg(argp, ptrdiff_t*) = (ptrdiff_t)d;
					break;
				}
				++scan_cnt;
				break;

			case 'u':
				for (d = 0; isdigit(str[len]); ++len)
					d = d * 10 + (unsigned __int64)(str[len] - '0');
				goto	assign_uint;

			case 'x': case 'X':
				for (d = 0; isdigit(str[len]) || str[len] >= 'A' && str[len] <= 'F' ||
												 str[len] >= 'a' && str[len] <= 'f'; ++len)
				{
					d = d << 4;
					if (str[len] <= '9')
						d += (unsigned __int64)(str[len] - '0');
					else if (str[len] <= 'F')
						d += (unsigned __int64)(str[len] - 'A' + 10);
					else
						d += (unsigned __int64)(str[len] - 'a' + 10);
				}
				goto	assign_uint;

			case 'o':
				for (d = 0; str[len] >= '0' && str[len] <= '7'; ++len)
					d = d * 8 + (unsigned __int64)(str[len] - '0');
assign_uint:
				switch (length_mod)
				{
				default:
					*(unsigned*)va_arg(argp, unsigned*) = (unsigned)d;
					break;
				case 1:
					*(unsigned char*)va_arg(argp, unsigned char*) = (unsigned char)d;
					break;
				case 2:
					*(unsigned short*)va_arg(argp, unsigned short*) = (unsigned short)d;
					break;
				case 3:
					*(unsigned long*)va_arg(argp, unsigned long*) = (unsigned long)d;
					break;
				case 4:
					*(unsigned __int64*)va_arg(argp, unsigned __int64*) = (unsigned __int64)d;
					break;
				case 5:
					*(intmax_t*)va_arg(argp, intmax_t*) = (intmax_t)d;
					break;
				case 6:
					*(size_t*)va_arg(argp, size_t*) = (size_t)d;
					break;
				case 7:
					*(ptrdiff_t*)va_arg(argp, ptrdiff_t*) = (ptrdiff_t)d;
					break;
				}
				++scan_cnt;
				break;

			case 'c':
				if (0 == length_mod)
					length_mod = 1;
				s = (char*)va_arg(argp, char*);
				for (i = 0; i < length_mod && !isspace(str[len]) && str[len] != '\0'; ++i, ++len)
					s[i] = str[len];
				if (i < length_mod)
					return	scan_cnt;
				++scan_cnt;
				break;

			case 's':
				s = (char*)va_arg(argp, char*);
				for (i = 0; !isspace(str[len]) && str[len] != '\0'; ++i, ++len)
					s[i] = str[len];
				s[i] = '\0';
				++scan_cnt;
				break;

			case 'p':
				s2 = (char**)va_arg(argp, char**);
				for (d = 0; isdigit(str[len]) || str[len] >= 'A' && str[len] <= 'F' ||
												 str[len] >= 'a' && str[len] <= 'f'; ++len)
				{
					d = d << 4;
					if (str[len] <= '9')
						d += (unsigned __int64)(str[len] - '0');
					else if (str[len] <= 'F')
						d += (unsigned __int64)(str[len] - 'A' + 10);
					else
						d += (unsigned __int64)(str[len] - 'a' + 10);
				}
				*s2 = (char*)d;
				++scan_cnt;
				break;

			case '%':
				if (str[len++] != '%')
					return	scan_cnt;
				break;

			case 'n':
				pn = (int*)va_arg(argp, int*);
				*pn = len;
				++scan_cnt;
				break;

			case 'f':
			case 'F':
				f = 0.0;
				sign = 1;
				if ('-' == str[len])
					sign = -1;
				for (; isdigit(str[len]); ++len)
					f = f * 10 + (double)(str[len] - '0');
				if ('.' == str[len])
				{
					double	fract;

					for (fract = 0.1; isdigit(str[++len]); fract /= 10.0)
						f += fract * (double)(str[len] - '0');
				}

				f *= sign;
				*(double*)va_arg(argp, double*) = f;
				++scan_cnt;
				break;

			case 'e':
			case 'E':
				f = 0.0;
				sign = 1;
				if ('-' == str[len])
					sign = -1;
				if (!isdigit(str[len]))
					return	scan_cnt;
				f = f * 10 + (double)(str[len++] - '0');

				if ('.' == str[len])
				{
					double	fract;

					for (fract = 0.1; isdigit(str[++len]); fract /= 10.0)
						f += fract * (double)(str[len] - '0');
				}
				else
					return	scan_cnt;

				if (str[len] != 'e' && str[len] != 'E')
					return	scan_cnt;

				++len;
				if ('-' == str[len])
				{
					++len;
					for (d = 0; isdigit(str[len]); ++len)
						d = d * 10 + (signed __int64)(str[len] - '0');
					
					while (d-- > 0)
						f /= 10.0;
				}
				else if ('+' == str[len])
				{
					++len;
					for (d = 0; isdigit(str[len]); ++len)
						d = d * 10 + (signed __int64)(str[len] - '0');
					
					while (d-- > 0)
						f *= 10.0;
				}
				else
					return	scan_cnt;

				f *= sign;
				*(double*)va_arg(argp, double*) = f;
				++scan_cnt;
				break;
			} // switch (conv_spec)

			// Switch back to state 0
			state = 0;
		}	
	}	// while (!*fmt)
	
	str[len] = '\0';
	return scan_cnt;

}
#else
// Really generic version
// 'src' may be pointer to string (for sscanf() help) or to FILE (for fscanf() help)
static int	generic_scanf(void *src, int src_type, const char *fmt, va_list argp, int (*get_next_char)(void **src))
{
	int len;
	int	flags;		// 0x1 <-> '*'
	int	state;		// 0 - initial state;
	int	min_len;		// Minimal length
	int	precision;		// Precision
	int	length_mod;		// Length modifier: 1 <-> hh; 2 <-> h; 3 <-> l; 4 <-> ll; 5 <-> j; 6 <-> z; 7 <-> t; 8 <-> L
	char	conv_spec;		// Conversion specifier

	int	n;
	int	scan_cnt;	// Count of assigned parameters
	int	max_len;
	//char	*str = src;
	void	*str = src;
	//FILE	*file = src;
	int	ch;


	len = 0;

	// Parse fmt and fill parameters from str accordingly
	scan_cnt = -1;	// return value is EOF at the beginning

	scan_cnt = 0;	// TODO: understand when should be returned -1 and when 0.

	// We must start with 'ch' already initialized because inside the loop we deal with ch loaded from the last unsuccessful comparison in automaton
	ch = get_next_char(&str);
	while (*fmt != 0)
	{
		// White space directive: read up to first non-white space
		if (isspace(*fmt))
		{
			while (isspace(ch))
			{
				ch = get_next_char(&str);
				++len;
			}
			if (ch == EOF)
				return	scan_cnt;
			while (isspace(*fmt))
				++fmt;
		}

		// Ordinary character
		if (*fmt != '%')
		{
			if (ch != *fmt++)
				return	scan_cnt;
			ch = get_next_char(&str);
			++len;
			continue;
		}

		// Conversion ('%')

		++state;
		flags = 0;
		max_len = 0;
//		precision = INT_MAX;		// Default precision
		length_mod = 0;

		while (state != 0)
		{
state1:
			// State 1: optional '*'
			if ('*' == *++fmt)
				flags |= 0x1;
			if (*fmt == '\0')
				return	scan_cnt;

			++state;

			// State 2: optional maximum field width

			while (isdigit(*fmt))
				max_len = max_len * 10 + (int)(*fmt++ - '0');

			++state;

			// State 3: parsing length modifier

			switch (*fmt)
			{
				default:
					break;

				case 'h':
					++fmt;
					length_mod = 2;
					if ('h' == *fmt)
					{
						length_mod = 1;
						++fmt;
					}
					break;

				case 'l':
					++fmt;
					length_mod = 3;
					if ('l' == *fmt)
					{
						length_mod = 4;
						++fmt;
					}
					break;

				case 'j':
					length_mod = 5;
					++fmt;
					break;

				case 'z':
					length_mod = 6;
					++fmt;
					break;

				case 't':
					length_mod = 7;
					++fmt;
					break;

				case 'L':
					length_mod = 8;
					++fmt;
					break;
			}
			++state;

			// State 4: parsing conversion specifier
			conv_spec = *fmt++;
			++state;
			
			// State 5: parameter's conversion
			switch(conv_spec)
			{
				unsigned __int64	d;
				int	p;
				int l;
				int i;
				int	*pn;
				char	*s;
				char	**s2;
				double	f;
				int	sign;

			case 'i':
			case 'd':
				sign = 0;

				if (ch == '-')
				{
					sign = 1;
					ch = get_next_char(&str);
					++len;
				}
				++len;

				if (5 == length_mod || 6 == length_mod || 7 == length_mod)
					for (d = 0; isdigit(ch); ch = get_next_char(&str), ++len)
						d = d * 10 + (unsigned __int64)(ch - '0');
				else
					for (d = 0; isdigit(ch); ch = get_next_char(&str), ++len)
						d = d * 10 + (signed __int64)(ch - '0');

				if (sign)
					d = -d;

				switch (length_mod)
				{
				default:
					*(int*)va_arg(argp, int*) = (int)d;
					break;
				case 1:
					*(signed char*)va_arg(argp, signed char*) = (signed char)d;
					break;
				case 2:
					*(signed short*)va_arg(argp, signed short*) = (signed short)d;
					break;
				case 3:
					*(signed long*)va_arg(argp, signed long*) = (signed long)d;
					break;
				case 4:
					*(signed __int64*)va_arg(argp, signed __int64*) = (signed __int64)d;
					break;
				case 5:
					*(intmax_t*)va_arg(argp, intmax_t*) = (intmax_t)d;
					break;
				case 6:
					*(size_t*)va_arg(argp, size_t*) = (size_t)d;
					break;
				case 7:
					*(ptrdiff_t*)va_arg(argp, ptrdiff_t*) = (ptrdiff_t)d;
					break;
				}
				++scan_cnt;
				break;

			case 'u':
				for (d = 0; isdigit(ch); ch = get_next_char(&str), ++len)
					d = d * 10 + (unsigned __int64)(ch - '0');
				goto	assign_uint;

			case 'x': case 'X':
				for (d = 0; isdigit(ch) || ch >= 'A' && ch <= 'F' || ch >= 'a' && ch <= 'f'; ch = get_next_char(&str), ++len)
				{
					d = d << 4;
					if (ch <= '9')
						d += (unsigned __int64)(ch - '0');
					else if (ch <= 'F')
						d += (unsigned __int64)(ch - 'A' + 10);
					else
						d += (unsigned __int64)(ch - 'a' + 10);
				}
				goto	assign_uint;

			case 'o':
				for (d = 0; ch = '0' && ch <= '7'; ch = get_next_char(&str), ++len)
					d = d * 8 + (unsigned __int64)(ch - '0');
assign_uint:
				switch (length_mod)
				{
				default:
					*(unsigned*)va_arg(argp, unsigned*) = (unsigned)d;
					break;
				case 1:
					*(unsigned char*)va_arg(argp, unsigned char*) = (unsigned char)d;
					break;
				case 2:
					*(unsigned short*)va_arg(argp, unsigned short*) = (unsigned short)d;
					break;
				case 3:
					*(unsigned long*)va_arg(argp, unsigned long*) = (unsigned long)d;
					break;
				case 4:
					*(unsigned __int64*)va_arg(argp, unsigned __int64*) = (unsigned __int64)d;
					break;
				case 5:
					*(intmax_t*)va_arg(argp, intmax_t*) = (intmax_t)d;
					break;
				case 6:
					*(size_t*)va_arg(argp, size_t*) = (size_t)d;
					break;
				case 7:
					*(ptrdiff_t*)va_arg(argp, ptrdiff_t*) = (ptrdiff_t)d;
					break;
				}
				++scan_cnt;
				break;

			case 'c':
				if (0 == length_mod)
					length_mod = 1;
				s = (char*)va_arg(argp, char*);
				for (i = 0; i < length_mod && !isspace(ch) && ch != EOF; ch = get_next_char(&str), ++i, ++len)
					s[i] = ch;
				if (i < length_mod)
					return	scan_cnt;
				++scan_cnt;
				break;

			case 's':
				s = (char*)va_arg(argp, char*);
				for (i = 0; !isspace(ch) && ch != EOF; ch = get_next_char(&str), ++i, ++len)
					s[i] = ch;
				s[i] = '\0';
				++scan_cnt;
				break;

			case 'p':
				s2 = (char**)va_arg(argp, char**);
				for (d = 0; isdigit(ch) || ch >= 'A' && ch <= 'F' || ch >= 'a' && ch <= 'f'; ch = get_next_char(&str), ++len)
				{
					d = d << 4;
					if (ch <= '9')
						d += (unsigned __int64)(ch - '0');
					else if (ch <= 'F')
						d += (unsigned __int64)(ch - 'A' + 10);
					else
						d += (unsigned __int64)(ch - 'a' + 10);
				}
				*s2 = (char*)d;
				++scan_cnt;
				break;

			case '%':
				if (ch != '%')
					return	scan_cnt;
				ch = get_next_char(&str);
				++len;
				break;

			case 'n':
				pn = (int*)va_arg(argp, int*);
				*pn = len;
				++scan_cnt;
				break;

			case 'f':
			case 'F':
				f = 0.0;
				sign = 1;
				if ('-' == ch)
				{
					sign = -1;
					++len;
					ch = get_next_char(&str);
				}
				if (ch == EOF)
					return	scan_cnt;				// return EOF?
				for (; isdigit(ch); ch = get_next_char(&str), ++len)
					f = f * 10 + (double)(ch - '0');
				if ('.' == (ch))
				{
					double	fract;

					++len;
					for (fract = 0.1; isdigit((ch = get_next_char(&str))); fract /= 10.0, ++len)
						f += fract * (double)(ch - '0');
				}

				f *= sign;
				*(double*)va_arg(argp, double*) = f;
				++scan_cnt;
				break;

			case 'e':
			case 'E':
				f = 0.0;
				sign = 1;
				if ('-' == ch)
				{
					sign = -1;
					++len;
					ch = get_next_char(&str);
				}
				if (!isdigit(ch))
					return	scan_cnt;
				f = f * 10 + (double)(ch - '0');

				if ('.' == (ch = get_next_char(&str)))
				{
					double	fract;

					++len;
					for (fract = 0.1; isdigit((ch = get_next_char(&str))); fract /= 10.0, ++len)
						f += fract * (double)(ch - '0');
				}
				else
					return	scan_cnt;

				if (ch != 'e' && ch != 'E')
					return	scan_cnt;

				if ('-' == (ch = get_next_char(&str)))
				{
					++len;
					for (d = 0; isdigit(ch = get_next_char(&str)); ++len)
						d = d * 10 + (signed __int64)(ch - '0');
					
					while (d-- > 0)
						f /= 10.0;
				}
				else if ('+' == ch)
				{
					for (d = 0; isdigit(ch = get_next_char(&str)); ++len)
						d = d * 10 + (signed __int64)(ch - '0');
					
					while (d-- > 0)
						f *= 10.0;
				}
				else
					return	scan_cnt;

				f *= sign;
				*(double*)va_arg(argp, double*) = f;
				++scan_cnt;
				break;
			} // switch (conv_spec)

			// If reached EOF, return whatever we were able to scan so far
			if (ch == EOF)
				return	scan_cnt;

			// Switch back to state 0
			state = 0;
		} // while (state != 0) -- do we really need this? The automaton is linear in processing
	} // while (!*fmt)
	
	return scan_cnt;
}
#endif


#ifndef STANDALONE
int	sprintf(char *str, const char *fmt, ...)
#else
int	sprintf1(char *str, const char *fmt, ...)
#endif
{
	int	n;

	va_list	argp;
	va_start(argp, fmt);
#ifndef STANDALONE
	n = vsprintf(str, fmt, argp);
#else
	n = vsprintf1(str, fmt, argp);
#endif
	va_end(argp);

	return	n;
}


#ifndef STANDALONE
int	sscanf(char *str, const char *fmt, ...)
#else
int	sscanf1(char *str, const char *fmt, ...)
#endif
{
	int	n;

	va_list	argp;
	va_start(argp, fmt);
#ifndef STANDALONE
	n = vsscanf(str, fmt, argp);
#else
	n = vsscanf1(str, fmt, argp);
#endif
	va_end(argp);

	return	n;
}


#ifndef STANDALONE
void *memcpy( void *dest, const void *src, size_t count )
{
	size_t	i;

	for (i = 0; i < count; ++i)
		((char*)dest)[i] = ((char*)src)[i];

	return	dest;
}


void *memmove(void *dest, const void *src, size_t count)
{
	size_t	i;

	if (dest == src || !count)
		return	dest;

	if ((char*)dest - (char*)src > 0 && (char*)dest - (char*)src < count)
	{
		do
		{
			--count;
			((char*)dest)[count] = ((char*)src)[count];
		} while (count > 0);
	}
	else
	{
		for (i = 0; i < count; ++i )
			((char*)dest)[i] = ((char*)src)[i];
	}

	return	dest;
}


void *memset(void *dest, int c, size_t count)
{
	char	ch;

	if (!dest || !count)
		return	dest;

	ch = c;

	do
	{
		--count;
		((char*)dest)[count] = ch;
	} while (count > 0);

	return	dest;
}


char *strcpy(char *to, const char *from)
{
	char *save = to;

	for (; (*to = *from); ++from, ++to);

	do
		*to++ = *from;
	while(*from++);

	return save;
}


char *strncpy(char *to, const char *from, size_t count)
{
	char *save = to;

	for (; count > 0; ++from, ++to, --count)
		if ((*to = *from) == '\0')
			break;

	while (count-- > 0)
		*to++ = '\0';

	return save;
}


char *strcat(char *to, const char *from)
{
	char *save = to;

	while (*to)
		to++;

	for (; (*to = *from); ++from, ++to);
	return save;
}


char *strncat(char *to, const char *from, size_t count)
{
	char *save = to;

	while (*to)
		to++;

	for (; count > 0; ++from, ++to, --count)
		if ((*to = *from) == '\0')
			break;

	*to = '\0';

	return save;
}


int	memcmp(const void *p1, const void *p2, size_t n)
{
	const unsigned char *c1, *c2;
	int	res;
	int	i;

	for (c1 = p1, c2 = p2, i = 0; i < n; ++i)
	{
		res = (int)c1[i] - (int)c2[i];
		if (res != 0)
			return	res;
	}

	return	0;
}


int	strcmp(const char *p1, const char *p2)
{
	int	res;

	for (;*p1 && *p2; ++p1, ++p2)
	{
		res = (int)*p1 - (int)*p2;
		if (res != 0)
			return	res;
	}

	// Terminating '\0' must also match
	res = (int)*p1 - (int)*p2;
	return	res;
}


int	strcasecmp(const char *p1, const char *p2)
{
	int	res;

	for (;*p1 && *p2; ++p1, ++p2)
	{
		res = toupper((int)*p1) - toupper((int)*p2);
		if (res != 0)
			return	res;
	}

	// Terminating '\0' must also match
	res = (int)*p1 - (int)*p2;
	return	res;
}


int	strcoll(const char *p1, const char *p2)
{
	return	strcmp(p1, p2);
}


int	strncmp(const char *p1, const char *p2, int n)
{
	int	res;

	for (; *p1 && *p2; ++p1, ++p2)
	{
		res = (int)*p1 - (int)*p2;
		if (res != 0)
			return	res;
		if (0 == --n)
			return	0;
	}

	// Terminating '\0' must also match
	res = (int)*p1 - (int)*p2;
	return	res;
}


size_t strxfrm(char *s1, const char *s2, size_t n)
{
	strcpy(s1, s2);
	return	strlen(s2);
}


void	*memchr(const void *s, int c, size_t n)
{
	char	*p;
	char	ch;

	for ( p = ( char* )s, ch = c; n; --n )
	{
		if ( *p == ch )
			return	p;
		++p;
	}

	return	NULL;
}


char	*strchr(const char *s, int c)
{
	char	ch;

	for (ch = c; *s; ++s )
	{
		if ( *s == ch )
			return	(void*)s;
	}

	return	NULL;
}


char	*strrchr(const char *s, int c)
{
	char	ch;
	const char	*s1;

	for (s1 = s + strlen(s) - 1, ch = c; s1 != s; --s1 )
	{
		if (*s1 == ch)
			return	(void*)s1;
	}

	if (*s1 == ch)
		return  (void*)s1;
	return	NULL;
}

size_t strcspn(const char *s1, const char *s2)
{
	size_t	n;
	size_t	i, sz2;

	sz2 = strlen(s2);

	for (n = 0; s1[n]; ++n)
		if (strchr(s2, s1[n]) != NULL)
			return	n;

	return	n;
}


char *strpbrk(const char *s1, const char *s2)
{
	return	(char*)(s1 + strcspn(s1, s2));
}


size_t strspn(const char *s1, const char *s2)
{
	size_t	n;
	size_t	i, sz2;

	sz2 = strlen(s2);

	for (n = 0; s1[n]; ++n)
		if (strchr(s2, s1[n]) == NULL)
			return	n;

	return	n;
}


char *strstr(const char *s1, const char *s2)
{
	size_t	sz1, sz2, i, j;

	if (!*s2)
		return	(char*)s1;

	sz1 = strlen(s1);
	sz2 = strlen(s2);

	for (i = 0; i < sz1; ++i)
	{
		if (i + sz2 > sz1)
			return	NULL;

		if (strncmp(s1+i, s2, sz2) == 0)
			return	(char*)(s1+i);
	}
}


char *strtok(char *s1, const char *s2)
{
	static char	*saveptr;

	if (s1 != NULL)
		// First call in a sequence
	{
		if (!s1)
			return	NULL;

		saveptr = s1 + strspn(s1, s2);
		if (!*saveptr)
		{
			saveptr = NULL;
			return	NULL;
		}
	}
	saveptr = strpbrk(saveptr, s2);
	if (*saveptr)
		*saveptr++ = '\0';

	if (*saveptr)
		return	saveptr;
	else
		return	saveptr = NULL;
}


size_t	strlen(const char *str)
{
  int	len;

  if ( !str )
	  return	0;

  for ( len = 0; str[ len ]; ++len )
	  ;

  return len;
}


char	*strerror(int err)
{
	static char	err_msg[256];

	sprintf(err_msg, "err=%d", err);
	return	err_msg;
}

#if 0
/*
 *	MSDEV's compiler-generated function.
 *
 *	In:	ST(0) = float
 *	Out: eax = long
 *
 *	Converts float to long
 */
void	_ftol( void )
{
	_asm
	{
		push	eax
		fist	dword ptr [ esp ]
		pop	eax
	}
}
#endif

extern	dword	timer_counter;
extern	time_t	system_time;

unsigned long	random(void)
{
	unsigned long	a, b;
	int	i;

	a = timer_counter;

	for (i = 0; i < 5; ++i)
	{
		b = (a >> 16) + (a << 16);

		if (b == 0)
			break;

		a = a / b;
		b = a % b;
		a = (b << 16) + (a & 0xFFFF);
	}

	return	a;
}

time_t	time(time_t *tloc)
{
	// It actually may appear that a timer interrupt occurs between write to *tloc and return, so
	// return value and 'tloc'ed value will differ. We can never be too precise...
	if (tloc != NULL)
		*tloc = system_time;
	return	system_time;
}

static int	days_in_months[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

static struct tm	tm;
	
static char *wdays[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
static char *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
static char tm_str[256];

char	*asctime(struct tm *tm)
{
	sprintf(tm_str, "%s %s %d %02d:%02d:%02d %4d\n", wdays[tm->tm_wday], months[tm->tm_mon], tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec, 1900+tm->tm_year);
	return	tm_str;
}

struct tm	*localtime(const time_t *timep)
{
	time_t	day, month, year;
	int	mon;
	int	days_in_year;

	day = *timep / (60*60*24);
	tm.tm_wday = (day + 4) % 7;		// Jan 1 1970 was Thursday - day 4, counting from 0

	year = day / 365 + 70;
	if (year > 72)
		day -= (year-73) / 4 + 1;

	days_in_year = (year % 4 == 0 ? 366 : 365);
	day = day % days_in_year;

	tm.tm_yday = day;

	for (mon = 0; mon < 12; ++mon)
	{
		if (day < days_in_months[mon])
			break;
		day -= days_in_months[mon];
		if (days_in_year == 366 && mon == 1)
			--day;
	}
	tm.tm_year = year;
	tm.tm_mon = mon;
	tm.tm_mday = day+1;
	tm.tm_hour = *timep % (60*60*24) / (60*60);
	tm.tm_min = *timep % (60*60) / 60;
	tm.tm_sec = *timep % 60;
	tm.tm_isdst = -1;			// Not supported	

	return	&tm;
}

char	*ctime(const time_t *timep)
{
	return	asctime(localtime(timep));
}

// Later we will support GMT
struct tm       *gmtime(const time_t *timep)
{
	return	localtime(timep);
}

time_t	mktime(struct tm *tm)
{
	time_t	days, made_time;
	int	mon;
	int	year, month, date;

	date = tm->tm_mday;
	month = tm->tm_mon;
	year = tm->tm_year;

	if (year < 70)
		year += 100;

	days = (year-70) * 365;
	if (year > 72)
		days += (year-73) / 4 + 1;

	for (mon = 0; mon < month; ++mon)
	{
		days += days_in_months[mon];
		if (year % 4 == 0 && 1 == mon)
			++days;
	}
	days += (date-1);
	
	made_time = days * (60*60*24) + tm->tm_hour * (60*60) + tm->tm_min * 60 + tm->tm_sec;
	*tm = *localtime(&made_time);
	return	made_time;
}


#ifdef	CFG_POSIXIO

int	rename(const char *oldpath, const char *newpath)
{
	struct fs	*fs_old, *fs_new;
	char	*pold, *pnew;
	int	rv;

	errno = 0;

	fs_old = get_fs(oldpath);
	if (NULL == fs_old)
	{
		errno = ENOENT;
		return	-1;
	}
	fs_new = get_fs(newpath);
	if (NULL == fs_new)
	{
		errno = ENOENT;
		return	-1;
	}
	pold = strchr(oldpath+1, '/');
	pnew = strchr(newpath+1, '/');

	if (pold == NULL || pnew == NULL)
	{
		errno = EINVAL;
		return	-1;
	}

	// If both files are on the same FS, call FS's rename() procedure
	if (fs_old == fs_new)
		return	fs_old->file_rename(fs_old, pold, pnew);
	
	// Else, copy file and unlink old
	rv = copy_file(oldpath, newpath);
	if (rv != 0)
		return	rv;
	return	unlink(oldpath);
}


static int	mode_to_pos(const char *mode, int *fd_mode, int *prpos, int *pwpos, unsigned long *pind)

{
	int	m;
	unsigned long	ind;		// Indicator
	int	rpos = 0, wpos = 0;	// initial read and write position at the beginning (0) or at the end (1)

	m = 0;
	ind = FILE_IND_TEXT;
	if (mode[0] == 'r')
	{
		m = O_RDONLY;
		if (mode[1] == '+')
		{
			m |= O_WRONLY;
			if (mode[2] == 'b')
				ind &= ~FILE_IND_TEXT;
		}
		else if (mode[1] == 'b')
			ind &= ~FILE_IND_TEXT;
		rpos = 0;
		wpos = 0;
	}
	else if (mode[0] == 'w')
	{
		m = O_WRONLY | O_CREAT | O_TRUNC;
		if (mode[1] == '+')
		{
			m |= O_RDONLY;
			if (mode[2] == 'b')
				ind &= ~FILE_IND_TEXT;
		}
		else
		{
			if (mode[1] == 'b')
			{
				ind &= ~FILE_IND_TEXT;
				if (mode[2] == '+')
					m |= O_RDONLY;		
			}
		}
		rpos = 0;
		wpos = 0;
	}
	else if (mode[0] == 'a')
	{
		m = O_WRONLY | O_CREAT;
		if (mode[1] == '+')
		{
			m |= O_RDONLY;
			if (mode[2] == 'b')
				ind &= ~FILE_IND_TEXT;
		}
		else 
		{
			if (mode[1] == 'b')
			{
				ind &= ~FILE_IND_TEXT;
				if (mode[2] == '+')
					m |= O_RDONLY;		
			}
		}
		rpos = 0;
		wpos = 1;
	}
	else
	{
		// Invalid open mode
		errno = EINVAL;
		return	0;
	}

	*fd_mode = m;
	*prpos = rpos;
	*pwpos = wpos;
	*pind = ind;
	return	1;
}

FILE	*fopen(const char *path, const char *mode)
{
	return	freopen(path, mode, NULL);
}

FILE *fdopen(int fd, const char *mode)
{
	int	m;
	size_t	sz;
	FILE	*f;
	unsigned long	ind;		// Indicator
	int	rpos = 0, wpos = 0;	// initial read and write position at the beginning (0) or at the end (1)

	if (!mode_to_pos(mode, &m, &rpos, &wpos, &ind))
		return	NULL;	

	f = calloc(1, sizeof(FILE));
	if (f == NULL)
	{
		close(fd);
		errno = ENOMEM;
		return	NULL;
	}
	f->buf = malloc(BUFSIZ);
	if (f->buf == NULL)
	{
		close(fd);
		free(f);
		errno = ENOMEM;
		return	NULL;
	}
	f->buf_size = BUFSIZ;

	if (rpos == 0)
		f->rd_pos = 0;
	else
		f->rd_pos = get_file_size(fd);
	if (wpos == 0)
		f->wr_pos = 0;
	else
		f->wr_pos = get_file_size(fd);
	f->fd = fd;
	f->ind = ind;
	f->buf_st = 0;
	f->buf_end = 0;
	f->buf_mode = _IOFBF;

	return	f;
}

FILE	*freopen(const char *path, const char *mode, FILE *fp)
{
	int	rv;
	int	fd;
	int	m;
	size_t	sz;
	FILE	*f;
	unsigned long	ind;		// Indicator
	int	rpos = 0, wpos = 0;	// initial read and write position at the beginning (0) or at the end (1)


	if (fp != NULL)
	{
		rv = close(fp->fd);
		if (rv != 0)
			rv = EOF;
	}
	f = fp;

	if (!mode_to_pos(mode, &m, &rpos, &wpos, &ind))
		return	NULL;	

	fd = open(path, m);
	if (fd < 0)
		return	NULL;

	if (f == NULL)
	{
		f = calloc(1, sizeof(FILE));
		if (f == NULL)
		{
			close(fd);
			errno = ENOMEM;
			return	NULL;
		}
		f->buf = malloc(BUFSIZ);
		if (f->buf == NULL)
		{
			close(fd);
			free(f);
			errno = ENOMEM;
			return	NULL;
		}
		f->buf_size = BUFSIZ;
	}

	if (rpos == 0)
		f->rd_pos = 0;
	else
		f->rd_pos = get_file_size(fd);
	if (wpos == 0)
		f->wr_pos = 0;
	else
		f->wr_pos = get_file_size(fd);
	f->fd = fd;
	f->ind = ind;
	f->buf_st = 0;
	f->buf_end = 0;
	f->buf_mode = _IOFBF;

	return	f;
}

int	fclose(FILE *fp)
{
	int	rv;

	rv = close(fp->fd);
	if (rv != 0)
		rv = EOF;
	if (!fp->ind & FILE_IND_CUSTOM_BUF)
		free(fp->buf);
	free(fp);
	return	rv;
}

size_t	fread(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
	ssize_t	nbytes;

	nbytes = read(stream->fd, ptr, (size*nmemb));
	if (nbytes < 0)
	{
		stream->ind |= FILE_IND_ERR;
		return	0;
	}
	if (stream->wr_pos == stream->rd_pos)
		stream->wr_pos += nbytes;
	stream->rd_pos += nbytes;
	if (stream->rd_pos >= get_file_size(stream->fd))
		stream->ind |= FILE_IND_EOF;
	return	nbytes / size;
}

// TODO: everything except for _IONBF looks wrong and non-working
size_t	fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream)
{
	ssize_t	nbytes, buf_bytes, sz_left, rv;
	char	*p = (char*)ptr;
	size_t	nmemb1 = nmemb;
	int	done;

	if (stream->buf_mode == _IONBF || size > stream->buf_size)
	{
		if (stream->buf_mode != _IONBF)
			fflush(stream);
		nbytes = write(stream->fd, ptr, (size*nmemb));
		if (nbytes < 0)
		{
			stream->ind |= FILE_IND_ERR;
			return	0;
		}
		if (stream->wr_pos == stream->rd_pos)
			stream->rd_pos += nbytes;
		stream->wr_pos += nbytes;
		if (stream->rd_pos >= get_file_size(stream->fd))
			stream->ind |= FILE_IND_EOF;
		return	nbytes / size;
	}

	buf_bytes = stream->buf_end - stream->buf_st;
	if (stream->buf_st > stream->buf_end)
		buf_bytes += stream->buf_size;	
	sz_left = stream->buf_size - buf_bytes;

	done = 0;
	while (!done)
	{
		char	*q, *r;
		size_t	b, m;

		// If there is not enough space in buffer for an element, flush the buffer, line by line
		q = stream->buf;
		b = stream->buf_st;
		r = q + b;

		if (stream->buf_mode == _IOLBF)
		{
			while (sz_left < size)
			{
				m = b;
				while (b < stream->buf_size && b != stream->buf_end && q[b] != '\n')
					++b;
				if (q[b] == '\n' && b < stream->buf_size)
					++b;
				rv = write(stream->fd, r, b);
				if (rv < 0)
				{
					stream->ind |= FILE_IND_ERR;
					return	nmemb - nmemb1;
				}
				if (b == stream->buf_size)
					b = 0;
				stream->buf_st = b;
				buf_bytes -= b - m;
				sz_left += b - m;
			}
		}
		else	// _IOFBF
		{
			if (sz_left < size)
			{
				rv = fflush(stream);
				if (rv == EOF)
					return	rv;
				buf_bytes = 0;
				sz_left = stream->buf_size;
			}
		}
			
		// Write a new element to buffer
		nbytes = size;
		if (nbytes <= stream->buf_size - stream->buf_end)
		{
			 memcpy((unsigned char*)stream->buf + stream->buf_end, p, size);
		}
		else
		{
			nbytes = stream->buf_size - stream->buf_end;
			memcpy((unsigned char*)stream->buf + stream->buf_end, p, nbytes);
			memcpy((unsigned char*)stream->buf, p + nbytes, size - nbytes);
		}
		p += size;
		--nmemb1;
		buf_bytes += size;
		sz_left -= size;

		if (nmemb1 == 0)
			done = 1;
	}

	if (stream->rd_pos >= get_file_size(stream->fd))
		stream->ind |= FILE_IND_EOF;
	return	nmemb - nmemb1;
}

int	fprintf(FILE *stream, const char *format, ...)
{
	int	n;
	va_list	argp;

	va_start(argp, format);
	n = vfprintf(stream, format, argp);
	va_end(argp);

	return	n;
}

// This function is intended to be set at printf() function pointer
int	printf(const char *format, ...)
{
	int	n;
	va_list	argp;

	va_start(argp, format);
	n = vfprintf(stdout, format, argp);
	va_end(argp);

	return	n;
}

int	vfprintf(FILE *stream, const char *format, va_list ap)
{
	char	buf[BUFSIZ];
	int	rv;

	vsprintf(buf, format, ap);
	rv = fwrite(buf, 1, strlen(buf), stream);
	return	rv;
}

int	fscanf(FILE *stream, const char *format, ...)
{
	int	n;

	va_list	argp;
	va_start(argp, format);
	n = vfscanf(stream, format, argp);
	va_end(argp);

	return	n;
}

// TODO -- meanwhile isn't implemented
int	vfscanf(FILE *stream, const char *format, va_list ap)
{
	return	generic_scanf(stream, GEN_SCANF_SRC_FILE, format, ap, file_get_next_char);
}

int	fgetc(FILE *stream)
{
	byte	ch;
	int	rv;

	rv = read(stream->fd, &ch, 1);
	if (rv != 1)
	{
		if (rv == 0)
			stream->ind |= FILE_IND_EOF;
		else
			stream->ind |= FILE_IND_ERR;
		rv = EOF;
	}
	else
		rv = ch;
	return	rv;
}

char	*fgets(char *s, int size, FILE *stream)
{
	int	ch;
	char	*rets = NULL;

	while (size > 1)
	{
		ch = fgetc(stream);		
		if (ch == EOF)
			break;
		if (rets == NULL)
			rets = s;
		*s++ = ch;
		if (ch == '\n')
			break;
		--size;
	}
	if (rets != NULL)
		*s = '\0';
	return	rets;
}

int	getc(FILE *stream)
{
	return	fgetc(stream);
}

// TODO
int	ungetc(int c, FILE *stream)
{
}

int	fputs(const char *s, FILE *stream)
{
	int	rv;

	rv = fwrite(s, 1, strlen(s), stream);
	return	rv;
}

int	fputc(int c, FILE *stream)
{
	int	rv;
	char	ch = c;

	rv = fwrite(&ch, 1, 1, stream);
	if (rv != 1)
	{
		stream->ind |= FILE_IND_ERR;
		return	EOF;
	}
	return	c; 	
}

int	putc(int c, FILE *stream)
{
	return	fputc(c, stream);
}

int	setvbuf(FILE *stream, char *buf, int mode, size_t size)
{
	stream->buf_mode = mode;
	if (mode == _IONBF)
		return	0;

	if (buf != NULL)
	{
		free(stream->buf);
		stream->buf = buf;
		stream->buf_size = size;
		stream->ind |= FILE_IND_CUSTOM_BUF;
	}
	return	0;
}

void	setbuf(FILE *stream, char *buf)
{
	setvbuf(stream, buf, buf ? _IOFBF : _IONBF, BUFSIZ);
}

int	fflush(FILE *stream)
{
	int	rv = 0;

	if (stream->buf_end == stream->buf_st)
		return	0;

	else if (stream->buf_end > stream->buf_st)
		rv = write(stream->fd, (char*)stream->buf + stream->buf_st, stream->buf_end - stream->buf_st);
	else
	{
		rv = write(stream->fd, (char*)stream->buf + stream->buf_st, stream->buf_size - stream->buf_st);
		if (rv >= 0)
			rv = write(stream->fd, (char*)stream->buf, stream->buf_end);
	}
	stream->buf_st = stream->buf_end;
	if (rv < 0)
		rv = EOF;

	return	rv;
}

int	feof(FILE *stream)
{
	return	(stream->ind & FILE_IND_EOF) != 0;
}

int	ferror(FILE *stream)
{
	return	(stream->ind & FILE_IND_ERR) != 0;
}

void	clearerr(FILE *stream)
{
	stream->ind &= ~(FILE_IND_ERR | FILE_IND_EOF);
}

int	fseek(FILE *stream, long offset, int whence)
{
	off_t	rv;

	// lseek() takes care about wrong whence
	rv = lseek(stream->fd, offset, whence);
	if (rv == (off_t)-1) 
		return	-1;
	stream->rd_pos = stream->wr_pos = rv;
	return	0;
}

long	ftell(FILE *stream)
{
	return	stream->rd_pos;
}

void	rewind(FILE *stream)
{
	fseek(stream, 0, SEEK_SET);
}

int	fgetpos(FILE *stream, fpos_t *pos)
{
	*pos = ftell(stream);
	return	0;
}

int	fsetpos(FILE *stream, fpos_t *pos)
{
	return	fseek(stream, *pos, SEEK_SET);
}

#endif	// CFG_POSIXIO

// Initialization (open stdin, stdout, stderr, etc.)
// (!) Called in initialization context, interrupts are disabled
void	init_libc(void)
{
#ifdef	CFG_POSIXIO
	stdin = calloc(sizeof(FILE), 1);
	if (stdin != NULL)
	{
		stdin->fd = STDIN;
		stdin->buf = malloc(BUFSIZ);
		stdin->buf_size = BUFSIZ;
		stdin->buf_mode = _IONBF;
		stdin->ind = FILE_IND_TEXT;
	}
	stdout = calloc(sizeof(FILE), 1);
	if (stdout != NULL)
	{
		stdout->fd = STDOUT;
		stdout->buf = malloc(BUFSIZ);
		stdout->buf_size = BUFSIZ;
		stdout->buf_mode = _IONBF;
		stdout->ind = FILE_IND_TEXT;
	}
	stderr = calloc(sizeof(FILE), 1);
	if (stderr != NULL)
	{
		stderr->fd = STDERR;
		stderr->buf = malloc(BUFSIZ);
		stderr->buf_size = BUFSIZ;
		stderr->buf_mode = _IONBF;
		stderr->ind = FILE_IND_TEXT;
	}
#endif	// CFG_POSIXIO
}

// Handler of system() call - may be seen as system command processor.
// In order to install system processor the application simply needs to write its handler pointer here.
int	(*system_cmd_handler)(const char*command) = NULL;

int system(const char *command)
{
	if (system_cmd_handler != NULL)
		return	system_cmd_handler(command);
}

#endif

#ifdef	STANDALONE
int main(void)
{
	char	buf[256];
	char	c;
	char	s[256];
	short	sh;
	int		i;
	unsigned	u;
	long	l;
	double	d;
	float	f;
	int	n;
	unsigned char	*p;

///////////////////////////////////////////// sscanf()
// Works!
// (!) There is a BUG in MSVC's library: they treat "%f" as corresponding to "float" argument, not "double".
// (!) Another BUG in MSVC's library: they don't recognize correctly an fscanf() directive composed of more than one white-space

/*
	strcpy(buf, "     abcde 4.5656638451e-2 f 54321   ");
	n = sscanf1(buf, " %s	%e	%c     %d ", s, &d, &c, &i);
	printf("%s %f %c %d %d\n", s, d, c, i, n);
	n = sscanf(buf, " %s	%e	%c     %d ", s, &f, &c, &i);
	printf("%s %f %c %d %d\n", s, f, c, i, n);
*/
/*
	strcpy(buf, "4.5656638451e-2");
	n = sscanf1(buf, "%e", &d);
	printf("%f %d\n", d, n);
	n = sscanf(buf, "%e", &f);
	printf("%f %d\n", f, n);
*/
/*
	strcpy(buf, "456.638451");
	n = sscanf1(buf, "%f", &d);
	printf("%f %d\n", d, n);
	n = sscanf(buf, "%f", &d);
	printf("%f %d\n", d, n);
*/
// Works!
/*
	strcpy(buf, "abcde 578");
	n = sscanf1(buf, "%s %d", s, &i);
	printf("%s %d %d\n", s, i, n);
	n = sscanf(buf, "%s %d", s, &i);
	printf("%s %d %d\n", s, i, n);
*/
// Works!

/*
	strcpy(buf, "578");
	n = sscanf1(buf, "%d", &i);
	printf("%d %d\n", i, n);
	n = sscanf(buf, "%d", &i);
	printf("%d %d\n", i, n);
*/

///////////////////////////////////////////// sprintf()
// Works
//	sprintf1(buf, "hahaha\n", 5);

// Works!
/*
	n = sprintf1(buf, "%08X\n", p);
	printf("%s %d\n", buf, n);
	n = sprintf(buf, "%08X\n", p);
	printf("%s %d\n", buf, n);
*/
// Works
/*
	n = sprintf1(buf, "%8.3lx\n", -15*8);
	printf("%s %d\n", buf, n);
	n = sprintf(buf, "%8.3lx\n", -15*8);
	printf("%s %d\n", buf, n);
*/

// Works!
/*
	n = sprintf1(buf, "%f\n", -456.638451);
	printf("%s %d\n", buf, n);
	n = sprintf(buf, "%f\n", -456.638451);
	printf("%s %d\n", buf, n);
*/

// Works!
/*
	n = sprintf1(buf, "%E\n", 0.00638451);
	printf("%s %d\n", buf, n);
	n = sprintf(buf, "%E\n", 0.00638451);
	printf("%s %d\n", buf, n);
*/

// Works!
/*
	n = sprintf1(buf, "%f\n", 199.999999999);
	printf("%s %d\n", buf, n);
	n = sprintf(buf, "%f\n", 199.999999999);
	printf("%s %d\n", buf, n);
*/
}
#endif

