#ifndef _ERR_H_
#define _ERR_H_

#include <config.h>
#ifdef DEBUG
#include <bwio.h>
#endif

enum Debug_type {
	DBG_REGION,
	DBG_TIMER,
	DBG_BUFIO,
	DBG_CONS,
	DBG_TRAIN,
	DBG_PROMPT,
	DBG_APP,             /* For main.c */
	DBG_SENSORUI,
	DBG_SENSORCTL,
	DBG_TEMP,
	DBG_CMD,
	DBG_COUNT
};

#ifdef DEBUG
// #define TRACE          /* Flag for function call trace */
static inline int DEBUG_ENABLE( uint x )
{
	switch( x ){
		/* Disabled debug */
	case DBG_REGION:
	case DBG_TIMER:
	case DBG_BUFIO:
	case DBG_CONS:
	case DBG_COUNT:
	case DBG_TRAIN:
	case DBG_SENSORCTL:
	case DBG_PROMPT:
	case DBG_SENSORUI:
	case DBG_CMD:
		return 0;
	default:
		return 1;
	}
}	

#define DEBUG_PRINT( level, fmt, arg... )				\
	do {								\
		if( DEBUG_ENABLE( level ) )				\
			bwprintf( COM2, "%s: " fmt, __func__, arg );	\
	} while( 0 )

#define DEBUG_NOTICE( level, msg )				\
	do {							\
		if( DEBUG_ENABLE( level ) )			\
			bwprintf( COM2, "%s: " msg, __func__ );	\
	} while( 0 )

/* Clear asserts for now */
#define ASSERT( cond )							\
	do {								\
		if( ! ( cond ) ){					\
			bwprintf( COM2, "%s (%d) hit assert %s\n", __FILE__, __LINE__, #cond ); \
			while( 1 );					\
		}							\
	} while( 0 )
#define ASSERT_M( cond, fmt, arg... )					\
	do {								\
		if( ! (cond ) ){					\
			bwprintf( COM2, "%s (%d) hit assert %s: " fmt, __FILE__, __LINE__, #cond, arg ); \
		}							\
	} while( 0 )
#define WORDBINDUMP( word )						\
	do {								\
		uint i;							\
		for( i = 0; i < 32; i += 1 ){				\
			bwprintf( COM2, "%d ", ( word >> ( 32 - i - 1 ) ) % 2 ); \
		}							\
		bwprintf( COM2, "\n" );					\
	} while( 0 )
#else
#define DEBUG_PRINT( level, fmt, arg... )
#define DEBUG_NOTICE( level, msg )
#define ASSERT( cond )
#define ASSERT_M( cond, msg )
#endif /* DEBUG */

#ifdef TRACE
#define ENTERFUNC()							\
	do {								\
		bwprintf( COM2, "%s (%d): %s\n", __FILE__, __LINE__, __func__ ); \
	} while( 0 )
#else
#define ENTERFUNC()
#endif


enum Error {
	ERR_NONE                           = 0,
	ERR_INVALID_ARGUMENT               = 1,
	ERR_OUT_OF_MEMORY                  = 2,
	ERR_CONSOLE_NOT_READY              = 3,
	ERR_NOT_READY                      = 3,
	ERR_RBUF_EMPTY                     = 4,
	ERR_RBUF_FULL                      = 5,
	ERR_UI_PROMPT_NOT_DONE             = 6,
	ERR_COMMAND_NOT_SUPPORTED          = 7,
	ERR_COMMAND_WRONG_PARAMETER        = 8,
	ERR_UNKNOWN                        = 9
};

#endif /* _ERR_H_ */
