/*
 *	queues.h
 *
 *	Macros to handle queues, lists etc.
 */

#ifndef	QUEUES__H
 #define	QUEUES__H

//	Cyclic queue
#define	q_is_empty( hd, tl ) ( hd == tl )
#define	q_next( p, max ) ( ( p + 1 == max ) ? 0 : p + 1 )
#define	q_is_full( hd, tl, max ) ( ( q_next( tl, max ) ) == hd ? 1 : 0 )
#define	q_insert( q, hd, tl, max, val ) { if ( !q_is_full( hd, tl, max ) ) { q[ tl ] = val; tl = q_next( tl, max ); } }
#define	q_delete( hd, tl, max ) { if ( !q_is_empty( hd, tl ) ) hd = q_next( hd, max ); }

//	Doubly-linked lists.
typedef	struct list_t
{
	void	*datum;
	struct	list_t	*prev, *next;
} list_t;


static void	list_init_entry(list_t *entry)
{
	entry->prev = entry->next = NULL;
}


static void	list_insert(list_t **hd, list_t **tl, list_t *entry)
{
	list_init_entry(entry);

	if (!*hd)
		*hd = *tl = entry;
	else
	{
		entry->prev = *tl;
		*tl = (*tl)->next = entry;
	}
}


static void	list_sorted_insert(list_t **hd, list_t **tl, list_t *entry, long(*cmp)(void*, void*))
{
	list_init_entry(entry);

	if (!*hd)
		*hd = *tl = entry;
	else
	{
		list_t	*p;

		for (p = *hd; p && cmp(p->datum, entry->datum) < 0; p = p->next)
			;

		if (!p)
		{
			entry->prev = *tl;
			*tl = (*tl)->next = entry;
		}
		else
		{
			entry->prev = p->prev;
			entry->next = p;
			p->prev = entry;
			if (entry->prev == NULL)
				*hd = entry;
			else
				entry->prev->next = entry;
		}
	}
}


static void	list_delete(list_t **hd, list_t **tl, list_t *entry)
{
	if (*hd == entry && *tl == entry)
		*hd = *tl = NULL;
	else
	{
		if (entry->prev)
			entry->prev->next = entry->next;
		else
			*hd = entry->next;

		if (entry->next)
			entry->next->prev = entry->prev;
		else
			*tl = entry->prev;
	}
}


static list_t	*list_find(list_t *hd, void *arg, long(*cmp)(void*, void*))
{
	list_t	*p;

	for (p = hd; p; p = p->next)
		if (!cmp(p->datum, arg))
			return	p;

	return	NULL;
}


static	long	cmp_list_entries(void *tm1, void *tm2)
{
	return	!(tm1 == tm2);
}

#if 0
#define	DEFINE_LIST( datum_type )	\
	typedef	struct	\
	{ 	\
		datum_type* datum; \
		int prev; \
		int next;	\
	}	datum_type##_list_t;	\
	\
	datum_type##_list_t	*datum_type##_list_head, *datum_type##_list_tail;

#define	l_init_entry( entry )	\
	entry -> prev = entry -> next = 0;

#define	l_insert( datum_type, entry )	\
do	\
{	\
	l_init_entry( entry )	\
	if ( !datum_type##_list_head )	\
		datum_type##_list_head = datum_type##_list_tail = entry;	\
	else	\
	{	\
		entry -> prev = datum_type##_list_tail;	\
		datum_type##_list_tail = datum_type##_list_tail -> next = entry;	\
	}	\
}	\
while( 0 );

//#define	l_sorted_insert()

#define	l_delete( entry )	\
	if ( entry -> prev )	\
		entry -> prev -> next = entry -> next;	\
	if ( entry -> next )	\
		entry -> next -> prev = entry -> prev;

//#define	l_find()
#endif

#endif	// QUEUES__H
