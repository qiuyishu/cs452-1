#include <user/syscall.h>
#include <user/assert.h>
#include <ts7200.h>
#include <types.h>
#include <config.h>
#include <user/protocals.h>
#include <user/event.h>
#include <err.h>
#include <lib/rbuf.h>
#include <regopts.h>

#define UART_MAGIC       0x11a87017

typedef struct Uart_init_s Uart_init;
typedef struct Uart_request_s Uart_request;
typedef struct Uart_reply_s Uart_reply;

struct Uart_init_s {
#ifdef IPC_MAGIC
	uint magic;
#endif
	uint port;
	uint flow_ctrl;
	uint fifo;
	uint speed;
};

struct Uart_request_s {
#ifdef IPC_MAGIC
	uint magic;
#endif
	uint type;
	uint data;
};

struct Uart_reply_s {
#ifdef IPC_MAGIC
	uint magic;
#endif
	uint data;
};

enum Uart_request_type {
	UART_GET,
	UART_PUT,
	UART_TXRDY,
	UART_RXRDY,
	UART_GENERAL_INT,
	UART_QUIT
};

static int uart_txrdy( int tid );
static int uart_rxrdy( int tid );
static int uart_general( int tid );

static inline ptr uart_getbase( uint port )
{
	ptr base = 0;
	
	switch( port ){
	case UART_1:
		base = UART1_BASE;
		break;
	case UART_2:
		base = UART2_BASE;
		break;
	}

	return base;
}

#define uart_ready_read( flags )	( ! ( *flags & RXFE_MASK ) )
#define uart_ready_write( flags )	( ! ( *flags & TXFF_MASK ) )

static inline void uart_receive_config( Uart_init* config )
{
	Uart_reply reply;
	int tid;
	int status;

#ifdef IPC_MAGIC
	reply.magic = UART_MAGIC;
#endif
	status = Receive( &tid, ( char* )config, sizeof( Uart_init ) );
	assert( status == sizeof( Uart_init ) );
#ifdef IPC_MAGIC
	assert( config->magic == UART_MAGIC );
#endif

	status = Reply( tid, ( char* )&reply, sizeof( reply ) );
	assert( status == SYSCALL_SUCCESS );
}

static inline void uart_send_config( Uart_init* config )
{
	Uart_reply reply;
}

static inline void uart_receive_request( int* tid, Uart_request* request )
{
	int status;
	
	status = Receive( tid, ( char* )request, sizeof( Uart_request ) );
	assert( status == sizeof( Uart_request ) );
#ifdef IPC_MAGIC
	assert( request->magic == UART_MAGIC );
#endif
}

static inline void uart_config_interrupt( volatile uint* uart_ctrl, uint intr, uint on )
{
	if( on ){
		*uart_ctrl |= intr;
	} else {
		*uart_ctrl &= ( ~intr );
	}
}

void uart_driver()
{
	ptr base;
	Uart_init config;
	Uart_request request;
	Uart_reply reply;
	int tid;
	int status;
	int txrdy_waiting = 0;
	int txrdy_handler_tid;
	int rxrdy_waiting = 0;
	int rxrdy_handler_tid;
	int general_waiting = 0;
	int general_handler_tid;
	int read_tid = 0;
	int can_reply;
	volatile uint* data;
	volatile uint* flags;
	volatile uint* intr;
	volatile uint* ctrl;

	/* Receive config */
	uart_receive_config( &config );

	assert( MyTid() == ( ( config.port == UART_1 ) ? UART1_DRV_TID : UART2_DRV_TID ) );

	base = uart_getbase( config.port );

	data = ( uint* )HW_ADDR( base, UART_DATA_OFFSET );
	flags = ( uint* )HW_ADDR( base, UART_FLAG_OFFSET );
	intr = ( uint* )HW_ADDR( base, UART_INTR_OFFSET );
	ctrl = ( uint* )HW_ADDR( base, UART_CTRL_OFFSET );

	/* Initialized interrupt handler */
	txrdy_handler_tid = Create( 0, event_handler );
	assert( txrdy_handler_tid > 0 );
	status == event_init( txrdy_handler_tid, ( config.port == UART_1 ) ? EVENT_SRC_UART1TXINTR1 : EVENT_SRC_UART2TXINTR2, uart_txrdy );
	assert( status ==  ERR_NONE );
	
	rxrdy_handler_tid = Create( 0, event_handler );
	assert( rxrdy_handler_tid > 0 );
	status == event_init( rxrdy_handler_tid, ( config.port == UART_1 ) ? EVENT_SRC_UART1RXINTR1 : EVENT_SRC_UART2RXINTR2, uart_rxrdy );
	assert( status ==  ERR_NONE );
	
	general_handler_tid = Create( 0, event_handler );
	assert( general_handler_tid > 0 );
	status == event_init( general_handler_tid, ( config.port == UART_1 ) ? EVENT_SRC_INT_UART1 : EVENT_SRC_INT_UART2, uart_general );
	assert( status ==  ERR_NONE );

#ifdef IPC_MAGIC
	reply.magic = UART_MAGIC;
#endif

	while( 1 ){
		/* By definition there should only be one task asking for input from any serial port */
		uart_receive_request( &tid, &request );

		can_reply = 0;

		/* Immediately reply if received interrupt notification to release event handler */
		switch( request.type ){
		case UART_RXRDY:
			rxrdy_waiting = 0;
			break;
		case UART_TXRDY:
			txrdy_waiting = 0;
			break;
		case UART_GENERAL_INT:
			general_waiting = 0;
			break;
		default:
			break;
		}

		switch( request.type ){
		case UART_RXRDY:
		case UART_TXRDY:
		case UART_GENERAL_INT:
			status = Reply( tid, ( char* )&reply, sizeof( reply ) );
			tid = 0;
		default:
			break;
		}

		/* Accept request */
		switch( request.type ){
		case UART_GET:
			if( read_tid ){
				assert( 0 );
			}
			read_tid = tid;
			request.type = UART_RXRDY;
			break;
		case UART_PUT:
			// Not implemented yet
			assert( 0 );
			request.type = UART_TXRDY;
			break;
		case UART_GENERAL_INT:
			if( ( *intr & MIS_MASK ) || ( *intr & TIS_MASK ) ){
				request.type = UART_TXRDY;
			} else if( ( *intr & RTIS_MASK ) || ( *intr & RIS_MASK ) ){
				request.type = UART_RXRDY;

				/* Clear RTIS */
				*intr = 0;
			} else {
				assert( 0 );
			}
		default:
			break;
		}

		switch( request.type ){
		case UART_RXRDY:
			/* There is a race condition: since we are reading data in the user space driver, what if the
			   data here is already overwritten?  The solution to this problem is to wish that we will
			   eventually only need to turn fifo on */
			/* Disable interrupt */
			if( uart_ready_read( flags ) ){
				/* Disable interrupt */
				uart_config_interrupt( ctrl, RIEN_MASK | RTIEN_MASK, 0 );
				reply.data = *data;
				tid = read_tid;
				read_tid = 0;
				can_reply = 1;
			} else {
				/* Enable interrupt */
				uart_config_interrupt( ctrl, RIEN_MASK | RTIEN_MASK, 1 );
				if( ! rxrdy_waiting ){
					uart_config_interrupt( ctrl, RIEN_MASK, 1 );
					event_start( rxrdy_handler_tid );
					rxrdy_waiting = 1;
				}
			}
			break;
		case UART_TXRDY:
			// Not Implemented yet
			assert( 0 );
			break;
		default:
			break;
		}

		if( ! general_waiting ){
			event_start( general_handler_tid );
			general_waiting = 1;
		}

		if( request.type == UART_QUIT ){
			/* Not implemented yet, given general event handler implementation, we need to generate an interrupt to wake up general_handler */
			assert( 0 );
			break;
		}

		if( can_reply ){
			status = Reply( tid, ( char* )&reply, sizeof( reply ) );
			assert( status == SYSCALL_SUCCESS );
		}

		
	}
}

int uart_init( uint port, uint speed, uint fifo, uint flow_ctrl, uint dbl_stop )
{
	ptr base = uart_getbase( port );
	
	volatile uint* line_ctrl_high = ( uint* )HW_ADDR( base, UART_LCRH_OFFSET );
	volatile uint* line_ctrl_medium = ( uint* )HW_ADDR( base, UART_LCRM_OFFSET );
	volatile uint* line_ctrl_low = ( uint* )HW_ADDR( base, UART_LCRL_OFFSET );
	volatile uint* uart_ctrl = ( uint* )HW_ADDR( base, UART_CTRL_OFFSET );

	if( fifo ){
		*line_ctrl_high |= FEN_MASK;
	} else {
		*line_ctrl_high &= ( ~FEN_MASK );
	}
	
	if( dbl_stop ){
		*line_ctrl_high |= STP2_MASK;
	}

	switch( speed ) {
	case 115200:
		*line_ctrl_medium = 0x0;
		*line_ctrl_low = 0x3;
		break;
	case 2400:
		*line_ctrl_medium = 0x0;
		*line_ctrl_low = 191;
	default:
		assert( 0 );
		break;
	}

	/* Turn off interrupt */
	uart_config_interrupt( uart_ctrl, MSIEN_MASK | RIEN_MASK | TIEN_MASK | RTIEN_MASK, 0 );
	
	return ERR_NONE;
}

static int uart_request( int tid, uint type, uint* data )
{
	Uart_request request;
	Uart_reply reply;
	int status;

#ifdef IPC_MAGIC
	request.magic = UART_MAGIC;
#endif
	request.type = type;
	request.data = *data;

	status = Send( tid, ( char* )&request, sizeof( request ), ( char* )&reply, sizeof( reply ) );
	assert( status == sizeof( reply ) );
#ifdef IPC_MAGIC
	assert( request.magic == UART_MAGIC );
#endif

	*data = reply.data;

	return ERR_NONE;
}

int uart_getc( int tid, uint* data )
{
	return uart_request( tid, UART_GET, data );
}

int uart_putc( int tid, uint data )
{
	return uart_request( tid, UART_PUT, &data );
}

int uart_quit( int tid )
{
	uint data;
	return uart_request( tid, UART_QUIT, &data );
}

static int uart_txrdy( int tid )
{
	uint data;
	return uart_request( tid, UART_TXRDY, &data );
}

static int uart_rxrdy( int tid )
{
	uint data;
	return uart_request( tid, UART_RXRDY, &data );
}

static int uart_general( int tid )
{
	uint data;
	return uart_request( tid, UART_GENERAL_INT, &data );
}