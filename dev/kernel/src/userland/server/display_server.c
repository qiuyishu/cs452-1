#include <types.h>
#include <user/syscall.h>
#include <user/display.h>
#include <user/assert.h>
#include <user/time.h>
#include <user/lib/cursor_control.h>
#include <lib/str.h>
#include <lib/valist.h>
#include <user/uart.h>
#include <err.h>
#include <config.h>

#define DISPLAY_MAGIC           0xd1597a11
#define DISPLAY_SERVER_TID      6
#define DISPLAY_SIZE            ( DISPLAY_WIDTH * DISPLAY_HEIGHT )
#define DISPLAY_PORT            COM_2

enum Display_request_type {
	DISPLAY_INIT,
	DISPLAY_REGION_INIT,
	DISPLAY_REGION_DRAW,
	DISPLAY_REGION_CLEAR,
	DISPLAY_DRAWER_READY,
	DISPLAY_QUIT
};

enum Display_drawer_cursor {
	DRAWER_CLS,
	DRAWER_SAVE,
	DRAWER_RESTORE,
	DRAWER_SETPOSN
};


typedef struct Display_request_s Display_request;
typedef struct Display_reply_s Display_reply;

struct Display_request_s {
	struct {
#ifdef IPC_MAGIC
		uint magic;
#endif
		uint type;
	} metadata;
	Region regspec;
	char msg[ DISPLAY_MAX_MSG ];
};

struct Display_reply_s {
#ifdef IPC_MAGIC
	uint magic;
#endif
	char* screen;
};

static int display_drawer_ready( char** screen );

static inline int display_drawer_cursor_request( uint type, uint col, uint row )
{
	char cmdbuf[ 32 ];
	int size;
	int i;
	int status;

	switch( type ){
	case DRAWER_CLS:
		size = cursor_control_cls( cmdbuf );
		break;
	case DRAWER_SAVE:
		size = cursor_control_save( cmdbuf );
		break;
	case DRAWER_RESTORE:
		size = cursor_control_restore( cmdbuf );
		break;
	case DRAWER_SETPOSN:
		size = cursor_control_setposn( cmdbuf, row, col );
		break;
	}

	for( i = 0; i < size; i += 1 ){
		status = Putc( DISPLAY_PORT, cmdbuf[ i ] );
		assert( status == PUTC_SUCCESS );
	}

	return ERR_NONE;
}

static void display_drawer()
{
	char* screen;
	int row, col;
	int reloc;
	int status;
	char ch;

	while( 1 ){
		status = Delay( DISPLAY_REFRESH_RATE );
		assert( status == SYSCALL_SUCCESS );

		status = display_drawer_ready( &screen );
		assert( status == ERR_NONE );

		if( ! screen ){
			break;
		}

		DEBUG_PRINT( DBG_DISP, "receive screen 0x%x\n", screen );

		status = display_drawer_cursor_request( DRAWER_SAVE, 0, 0 );
		assert( status == ERR_NONE );

		DEBUG_NOTICE( DBG_DISP, "cursor saved\n" );

		for( row = 0; row < DISPLAY_HEIGHT; row += 1 ){
			reloc = 1;
			for( col = 0; col < DISPLAY_WIDTH; col += 1 ){
				ch = screen[ col + row * DISPLAY_WIDTH ];
				if( ch ){
					if( reloc ){
						display_drawer_cursor_request( DRAWER_SETPOSN, col + 1, row + 1 );
						DEBUG_NOTICE( DBG_DISP, "cursor reset\n" );
					}
					reloc = 0;
					status = Putc( DISPLAY_PORT, ch );
				} else {
					reloc = 1;
				}
			}
		}
		
		status = display_drawer_cursor_request( DRAWER_RESTORE, 0, 0 );
		assert( status == ERR_NONE );
	}

	DEBUG_NOTICE( DBG_DISP, "quit!\n" );
	Exit();
}

static inline void display_modify( uint col, uint row, char ch, const char* current, char* target )
{
	if( current[ row * DISPLAY_WIDTH + col ] != ch ){
		target[ row * DISPLAY_WIDTH + col ] = ch;
	}
}

static inline void display_region_clear( Region* reg, const char* current, char* target )
{
	uint row, col;
	
	for( row = 0; row < reg->height; row += 1 ){
		for( col = 0; col < reg->width; col += 1 ){
			if( reg->boundary ){
				if( row == 0 || row == reg->height - 1 ){
					if( col == 0 || col == reg->width - 1 ){
						display_modify( col + reg->col, row + reg->row, 'o', current, target );
					} else {
						display_modify( col + reg->col, row + reg->row, '-', current, target );
					}
				} else {
					if( col == 0 || col == reg->width - 1 ){
						display_modify( col + reg->col, row + reg->row, '|', current, target );
					} else {
						display_modify( col + reg->col, row + reg->row, ' ', current, target );
					}
				}
			} else {
				display_modify( col + reg->col, row + reg->row, ' ', current, target );
			}
		}
	}
}

static inline void display_fill( Region* reg, const char* msg, const char* current, char* target )
{
	int row, col;
	char ch;
	int clear_line = 0;

	/* The 1 added/subtracted is for the boundry */
	for( row = reg->margin / 2 + 1; row < ( reg->height - reg->margin / 2 - 1 ); row += 1 ){
		clear_line = 0;
		for( col = reg->margin + 1; col < ( reg->width - reg->margin - 1); col += 1 ){
			if( clear_line ){
				display_modify( col + reg->col, row + reg->row, ' ', current, target );
			} else {
				ch = *(msg++);
				if( ch == '\n' ){
					clear_line = 1;
				} else if( ch ) {
					display_modify( col + reg->col, row + reg->row, ch, current, target );
				} else {
					goto Done;
				}
			}
		}
	}

 Done:
	return;
}

void display_server()
{
	Display_request request;
	Display_reply reply;
	char buffers[ 3 * DISPLAY_SIZE ];
	char* commited = buffers;
	char* staging = commited + DISPLAY_SIZE;	
	char* editing = staging + DISPLAY_SIZE;
	char* temp_buf;
	int tid;
	int chc;
	int status;

	DEBUG_PRINT( DBG_DISP, "launching, size = %d\n", DISPLAY_SIZE );

	assert( MyTid() == DISPLAY_SERVER_TID );

#ifdef IPC_MAGIC
	reply.magic = DISPLAY_MAGIC;
#endif

	/* Initialize editing buffer to cls */
	for( chc = 0; chc < DISPLAY_SIZE; chc += 1 ){
		commited[ chc ] = 0;
		staging[ chc ] = 0;
		editing[ chc ] = '~';
	}

	while( 1 ){
		status = Receive( &tid, ( char* )&request, sizeof( request ) );
		assert( status >= sizeof( request.metadata ) );
#ifdef IPC_MAGIC
		assert( request.metadata.magic == DISPLAY_MAGIC );
#endif
		DEBUG_PRINT( DBG_DISP, "received request %d from %d\n", request.metadata.type, tid );
		
		reply.screen = 0;
		
		switch( request.metadata.type ){
		case DISPLAY_INIT:
			/* Create drawer */

			status = Create( SERVICE_PRIORITY, display_drawer );
			assert( status > 0 );
			break;
		case DISPLAY_REGION_INIT:
		case DISPLAY_REGION_CLEAR:
			display_region_clear( &request.regspec, commited, editing );
			break;
		case DISPLAY_REGION_DRAW:
			display_fill( &request.regspec, request.msg, commited, editing );
			break;
		case DISPLAY_DRAWER_READY:
			/* TODO: one optimization here is to not to reply if no edit is done */
			temp_buf = staging;
			staging = editing;
			editing = temp_buf;
			
			/* Send data to drawer */
			reply.screen = staging;

			/* Commit staging data into commited, and clear editing */
			for( chc = 0; chc < DISPLAY_SIZE; chc += 1 ){
				if( staging[ chc ] != 0 ){
					commited[ chc ] = staging[ chc ];
				}
				editing[ chc ] = 0;
			}
			
			break;
		case DISPLAY_QUIT:
			break;
		}

		status = Reply( tid, ( char* )&reply, sizeof( reply ) );
		assert( status == SYSCALL_SUCCESS );
	}

	Exit();
}

static int display_request( uint type, Region* regspec, char* msg, uint msglen, char** screen )
{
	Display_request request;
	Display_reply reply;
	int size = sizeof( request.metadata );
	int status;

#ifdef IPC_MAGIC
	request.metadata.magic = DISPLAY_MAGIC;
#endif
	request.metadata.type = type;
	
	if( regspec ){
		memcpy( ( uchar* )&request.regspec, ( uchar* )regspec, sizeof( Region ) );
		size += sizeof( Region );
	}
	
	if( msg ){
		memcpy( ( uchar* )&request.msg, ( uchar* )msg, msglen );
		size += msglen;
	}
	
	DEBUG_PRINT( DBG_DISP, "about to request %d\n", type );

	status = Send( DISPLAY_SERVER_TID, ( char* )&request, size, ( char* )&reply, sizeof( reply ) );
	assert( status == sizeof( reply ) );
#ifdef IPC_MAGIC
	assert( reply.magic == DISPLAY_MAGIC );
#endif

	if( screen ){
		*screen = reply.screen;
	}

	DEBUG_PRINT( DBG_DISP, "request %d processed\n", type );

	return ERR_NONE;
}

int display_init()
{
	return display_request( DISPLAY_INIT, 0, 0, 0, 0 );
}

int region_init( Region* region )
{
	assert( region );
	return display_request( DISPLAY_REGION_INIT, region, 0, 0, 0 );
}

int region_printf( Region* region, char* fmt, ... )
{
	va_list va;
	int size;
	char dst[ DISPLAY_MAX_MSG ];

	va_start( va, fmt );
	assert( region );
	size = sformat( dst, fmt, va );
	va_end( va );

	return display_request( DISPLAY_REGION_DRAW, region, dst, size, 0 );
}

int region_clear( Region* region )
{
	assert( region );
	
	return display_request( DISPLAY_REGION_CLEAR, region, 0, 0, 0 );
}

static int display_drawer_ready( char** screen )
{
	return display_request( DISPLAY_DRAWER_READY, 0, 0, 0, screen );
}
