/* Train_auto receives sensor and control data to make judgements */
/* It is not implemented strictly as a administrator because incoming
   requests should be rare relative to its speed (sensor every 60 ms,
   control command every second, such and such) */

#include <types.h>
#include <err.h>
#include <lib/str.h>
#include <user/syscall.h>
#include <user/assert.h>
#include <user/time.h>
#include <user/name_server.h>
#include <config.h>
#include "inc/train.h"
#include "inc/config.h"
#include "inc/sensor_data.h"
#include "inc/track_data.h"
#include "inc/track_node.h"
#include "inc/warning.h"
#include "inc/train_location.h"
#include "inc/train_types.h"


enum Train_state {
	TRAIN_STATE_INIT,             /* Init */
	TRAIN_STATE_STOP,             /* Stopped */
	TRAIN_STATE_TRACKING,         /* Normal state */
	TRAIN_STATE_REVERSE,          /* Just reversed direction */
	TRAIN_STATE_SPEED_CHANGE,     /* Just changed speed */
	TRAIN_STATE_SPEED_ERROR,      /* Speed prediction out of bound error */
	TRAIN_STATE_SWITCH_ERROR,     /* Switch prediction error */
	TRAIN_STATE_UNKNOW            /* Unknown state */
};

enum Train_pickup {
	TRAIN_PICKUP_FRONT,
	TRAIN_PICKUP_BACK,
	TRAIN_PICKUP_UNKONWN
};

enum Train_auto_request_type {
	TRAIN_AUTO_INIT,
	TRAIN_AUTO_WAKEUP,
	TRAIN_AUTO_NEW_SENSOR_DATA,
	TRAIN_AUTO_NEW_TRAIN,
	TRAIN_AUTO_SET_TRAIN_SPEED,
	TRAIN_AUTO_SET_TRAIN_REVERSE,
	TRAIN_AUTO_SET_SWITCH_DIR,
	TRAIN_AUTO_SET_ALL_SWITCH,
	TRAIN_AUTO_QUERY_SWITCH_DIR,
	TRAIN_AUTO_QUERY_LAST_SENSOR
};

typedef struct Train_auto_request_s {
	uint type;
	union {
		struct {
			uint track_id;
		} init;
		Sensor_data sensor_data;
		struct {
			uint train_id;
			uint pickup;
			uint next_group;  /* The sensor the pickup is heading towards */
			uint next_id;
		} new_train;
		struct {
			uint train_id;
			uint speed_level;
		} set_speed;
		struct {
			uint train_id;
		} set_reverse;
		struct {
			uint switch_id;
			char direction;
		} set_switch;
		struct {
			char direction;
		} set_switch_all;
		struct {
			uint switch_id;
		} query_switch;
		struct {
			uint dummy;
		} query_sensor;
	} data;
} Train_auto_request;

typedef struct Train_auto_reply_s {
	int group;
	int id;
} Train_auto_reply;

static int train_auto_wakeup( int tid );

static void train_auto_alarm()
{
	int ptid = MyParentTid();

	while( 1 ){
		Delay( 1 );

		train_auto_wakeup( ptid );
	}
}

void train_auto()
{
	Train_auto_request request;
	Train_auto_reply reply;
	Sensor_data sensor_data;
	int last_sensor_group = -1;
	int last_sensor_id = -1;
	int alarm_tid;
	int tid;
	int i;
	int j;
	int temp;
	track_node track_graph[ TRACK_NUM_NODES ];
	int node_map[ GROUP_COUNT ][ TRACK_GRAPH_NODES_PER_GROUP ];
	Train_data trains[ MAX_NUM_TRAINS ] = { { 0 } };
	int train_map[ MAX_TRAIN_ID ] = { 0 };
	int available_train = 1;     /* Record 0 is not used, to distinguish non-registered car */
	Train_data* current_train;
	track_node* current_sensor;
	track_node* next_sensor;
	int switch_table[ NUM_SWITCHES ] = { 0 };
	int module_tid;
	int status;

	/* Receive initialization data */
	status = Receive( &tid, ( char* )&request, sizeof( request ) );
	assert( status == sizeof( int ) + sizeof( request.data.init ) );
	status = Reply( tid, ( char* )&reply, sizeof( reply ) );
	assert( status == SYSCALL_SUCCESS );

	switch( request.data.init.track_id ){
	case TRACK_A:
		init_tracka( track_graph, node_map );
		break;
	case TRACK_B:
		init_trackb( track_graph, node_map );
		break;
	default:
		assert( 0 );
	}

	status = RegisterAs( TRAIN_AUTO_NAME );
	assert( status == REGISTER_AS_SUCCESS );

	module_tid = WhoIs( TRAIN_MODULE_NAME );
	assert( module_tid > 0 );
	
	/* Start alarm */
	alarm_tid = Create( TRAIN_AUTO_PRIROTY, train_auto_alarm );
	assert( alarm_tid > 0 );

	// test
	Train_data test_train_data;
	Train_data* test_train = &test_train_data;
	test_train->distance = 0;
	test_train->speed.numerator = 0;
	test_train->speed.denominator = 1;
	test_train->speed_level = 0;
	test_train->old_speed_level = 0;
	test_train->last_sensor_time = 0;
	for ( i = 0; i < 14; i++ ) {
		test_train->speed_table[i].numerator = 0;
		test_train->speed_table[i].denominator = 1;
		test_train->speed_count[i] = 0;
	}
	test_train->speed_level = 14;
	test_train->next_sensor = track_graph + node_map[ 0 ][ 2 ];

	//WAR_PRINT( "init: next %c-%d\n", test_train->next_sensor->group+'A', test_train->next_sensor->id+1 );
	uint sensor_test_count = 0;
	//assert( test_train->last_sensor );
	//test_train->check_point = track_graph + node_map[ 6 ][ 4 ];
	track_node* test_sensor = 0;
	
	
	while( 1 ){
		status = Receive( &tid, ( char* )&request, sizeof( request ) );

		status -= sizeof( uint );
		/* Size check */
		switch( request.type ){
		case TRAIN_AUTO_INIT:
			/* No more init should be received */
			assert( 0 );
			break;
		case TRAIN_AUTO_WAKEUP:
			assert( status == 0 );
			break;
		case TRAIN_AUTO_NEW_SENSOR_DATA:
			assert( status == sizeof( request.data.sensor_data ) );
			break;
		case TRAIN_AUTO_NEW_TRAIN:
			assert( status == sizeof( request.data.new_train ) );
			break;
		case TRAIN_AUTO_SET_TRAIN_SPEED:
			assert( status == sizeof( request.data.set_speed ) );
			break;
		case TRAIN_AUTO_SET_TRAIN_REVERSE:
			assert( status == sizeof( request.data.set_reverse ) );
			break;
		case TRAIN_AUTO_SET_SWITCH_DIR:
			assert( status == sizeof( request.data.set_switch ) );
			break;
		case TRAIN_AUTO_SET_ALL_SWITCH:
			assert( status == sizeof( request.data.set_switch_all ) );
			break;
		case TRAIN_AUTO_QUERY_SWITCH_DIR:
			assert( status == sizeof( request.data.query_switch ) );
			break;
		case TRAIN_AUTO_QUERY_LAST_SENSOR:
			assert( status == sizeof( request.data.query_sensor ) );
			break;
		}

		/* Quick reply */
		switch( request.type ){
		case TRAIN_AUTO_NEW_SENSOR_DATA:
		case TRAIN_AUTO_NEW_TRAIN:
		case TRAIN_AUTO_SET_TRAIN_SPEED:
		case TRAIN_AUTO_SET_TRAIN_REVERSE:
		case TRAIN_AUTO_SET_SWITCH_DIR:
		case TRAIN_AUTO_SET_ALL_SWITCH:
			status = Reply( tid, ( char* )&reply, sizeof( reply ) );
			assert( status == SYSCALL_SUCCESS );
		default:
			break;
		}

		/* Process request */
		switch( request.type ){
		case TRAIN_AUTO_NEW_SENSOR_DATA:
			/* Unfortuate memcpy */
			memcpy( ( uchar* )&sensor_data, ( uchar* )&request.data.sensor_data, sizeof( sensor_data ) );

			//test_sensor = 0;

			/* Update last triggered sensor *//*
			for( temp = 0; temp < SENSOR_BYTE_COUNT; temp += 1 ){
				if( sensor_data.sensor_raw[ temp ] ){
					last_sensor_group = temp;
					for( i = 0; i < BITS_IN_BYTE; i += 1 ){
						if( sensor_data.sensor_raw[ temp ] & ( ( 1 << 7 ) >> i ) ){
							last_sensor_group = temp / 2;
							last_sensor_id = i + ( temp % 2 ) * 8;
							test_sensor = track_graph + node_map[ last_sensor_group ][ last_sensor_id ];
						}
					}
				}
			}
			
			//clear_sensor_data( &sensor_data, track_graph+node_map[0][2] );
			test_sensor = parse_sensor( &sensor_data, 0, track_graph, node_map );
			

			if ( test_sensor == current_train->next_sensor ) {
				status == update_train_speed( test_train, test_sensor, sensor_data.last_sensor_time );
				assert( status == 0 );
				status == train_next_sensor( test_train, switch_table );
			}

			test_sensor = test_train->next_sensor;
			sensor_test_count += 1;
			WAR_PRINT( "%d sensor: last %c-%d, next %c-%d, speed %d\n", sensor_test_count, last_sensor_group+'A', last_sensor_id+1, test_sensor->group+'A', test_sensor->id+1, test_train->speed.numerator * 100 / test_train->speed.denominator );

			*/
			
			break;
		case TRAIN_AUTO_NEW_TRAIN:
			current_train = trains + available_train;
			train_map[ request.data.new_train.train_id ] = available_train;
			available_train += 1;
			current_train->id = request.data.new_train.train_id;
			current_train->state = TRAIN_STATE_INIT;
			current_train->pickup = request.data.new_train.pickup;
			current_train->distance = 0;
			current_train->speed.numerator = 0;
			current_train->speed.denominator = 1;
			current_train->speed_level = TRAIN_AUTO_REGISTER_SPEED;
			current_train->old_speed_level = 0;
			current_train->last_sensor_time = 0;
			current_train->last_sensor = track_graph + node_map[ request.data.new_train.next_group ][ request.data.new_train.next_id ];
			current_train->check_point = track_graph + node_map[ request.data.new_train.next_group ][ request.data.new_train.next_id ];
			current_train->next_sensor = track_next_sensor( current_train->last_sensor->reverse, switch_table );

			WAR_PRINT( "front sensor %c%d reverse sensor %c%d\n", current_train->last_sensor->group+'A', current_train->last_sensor->id+1, current_train->next_sensor->group+'A', current_train->next_sensor->id+1 );
			train_set_speed( module_tid, request.data.new_train.train_id, TRAIN_AUTO_REGISTER_SPEED );
			
			break;
		case TRAIN_AUTO_SET_TRAIN_SPEED:
			current_train = trains + train_map[ request.data.set_speed.train_id ];
			current_train->old_speed_level = current_train->speed_level;
			current_train->speed_level = request.data.set_speed.speed_level;
			current_train->state = TRAIN_STATE_SPEED_CHANGE;
			break;
		case TRAIN_AUTO_SET_TRAIN_REVERSE:
			current_train = trains + train_map[ request.data.set_speed.train_id ];
			if( current_train->pickup == TRAIN_PICKUP_FRONT ){
				current_train->pickup = TRAIN_PICKUP_BACK;
			} else if( current_train->pickup == TRAIN_PICKUP_BACK ){
				current_train->pickup = TRAIN_PICKUP_FRONT;
			}
			/* TODO: This will lose precision */
			current_train->check_point = current_train->check_point->reverse;
			current_train->state = TRAIN_STATE_REVERSE;
			break;
		case TRAIN_AUTO_SET_SWITCH_DIR:
			switch_table[ SWID_TO_ARRAYID( request.data.set_switch.switch_id ) ] = request.data.set_switch.direction;
			break;
		case TRAIN_AUTO_SET_ALL_SWITCH:
			for( i = 0; i < NUM_SWITCHES; i += 1 ){
				switch_table[ i ] = request.data.set_switch_all.direction;
			}
		case TRAIN_AUTO_QUERY_SWITCH_DIR:
			reply.group = switch_table[ SWID_TO_ARRAYID( request.data.set_switch.switch_id ) ];
			break;
		case TRAIN_AUTO_QUERY_LAST_SENSOR:
			reply.group = last_sensor_group;
			reply.id = last_sensor_id;
			break;
		}

		/* Late reply */
		switch( request.type ){
		case TRAIN_AUTO_QUERY_SWITCH_DIR:
		case TRAIN_AUTO_QUERY_LAST_SENSOR:
			status = Reply( tid, ( char* )&reply, sizeof( reply ) );
			assert( status == SYSCALL_SUCCESS );
		default:
			break;
		}

		/* Process train states *//*
		if( request.type == TRAIN_AUTO_NEW_SENSOR_DATA || request.type == TRAIN_AUTO_WAKEUP ){
			for( temp = 1; temp < available_train; temp += 1 ){
				current_train = trains + temp;
				switch( current_train->state ){
				case TRAIN_STATE_INIT:
					current_sensor = parse_sensor( &sensor_data, 0, track_graph, node_map );
										
					if (( !current_sensor ) || (( current_sensor != current_train->last_sensor ) && (current_sensor != current_train->next_sensor ))) {
						WAR_PRINT( "init sensor %c%d is not either last %c%d or next %c%d", current_sensor->group+'A', current_sensor->id+1, current_train->last_sensor->group+'A', current_train->last_sensor->id+1, current_train->next_sensor->group+'A', current_train->next_sensor->id+1 );
						break;
					}

					if ( current_train->last_sensor != current_sensor ) {
						current_train->pickup = TRAIN_PICKUP_BACK;
					}
					current_train->last_sensor = current_sensor;
					current_train->last_sensor_time = sensor_data.last_sensor_time;
					status = train_next_sensor( current_train, switch_table );
					if ( status ) {
						// heading to an exit, deal with it
					}
					
					train_set_speed( module_tid, current_train->id, 0 );
					WAR_PRINT( "new reg train %d: pickup %d, next sensor: %c%d ", current_train->id, current_train->pickup, current_train->next_sensor->group+'A', current_train->next_sensor->id+1 );
					current_train->state = TRAIN_STATE_SPEED_CHANGE;
					break;
				case TRAIN_STATE_STOP:
					break;
				case TRAIN_STATE_TRACKING:
					current_sensor = 0;
					while ( 1 ) {
						current_sensor = parse_sensor( &sensor_data, current_sensor, track_graph, node_map );
						if ( current_sensor == 0 ) {
							break;
						}
						if ( current_sensor == current_train->next_sensor ) {
							status == update_train_speed( current_train, current_sensor, sensor_data.last_sensor_time );
							assert( status == 0 );
							status == train_next_sensor( current_train, switch_table );
							if ( status ) {
								// heading to an exit;
							}

							next_sensor = current_train->next_sensor;
							//WAR_PRINT( "sensor hit: last %c%d, next %c%d, speed %d\n", current_sensor->group+'A', current_sensor->id+1, next_sensor->group+'A', next_sensor->id+1, current_train->speed.numerator * 100 / current_train->speed.denominator );
	
							clear_sensor_data( &sensor_data, current_sensor );
						}
					}						

					break;
				case TRAIN_STATE_REVERSE:
					break;
				case TRAIN_STATE_SPEED_CHANGE:
					// TODO: deal with speed change curve and time/location
					current_train->state = TRAIN_STATE_TRACKING;
					break;
				case TRAIN_STATE_SPEED_ERROR:
					break;
				case TRAIN_STATE_SWITCH_ERROR:
					break;
				case TRAIN_STATE_UNKNOW:
					break;
				}
			}
		}
		*/
		/* Really late reply */
		switch( request.type ){
		case TRAIN_AUTO_WAKEUP:
			status = Reply( tid, ( char* )&reply, sizeof( reply ) );
			assert( status == SYSCALL_SUCCESS );
		default:
			break;
		}			
	}
}

static int train_auto_request( int tid, Train_auto_request* data, uint size, int* group, int* id )
{
	int status;
	Train_auto_reply reply;

	status = Send( tid, ( char* )data, size, ( char* )&reply, sizeof( reply ) );
	assert( status == sizeof( reply ) );

	if( group ){
		*group = reply.group;
	}
	if( id ){
		*id = reply.id;
	}

	return ERR_NONE;
}

int train_auto_new_sensor_data( int tid, Sensor_data* data )
{
	Train_auto_request request;

	request.type = TRAIN_AUTO_NEW_SENSOR_DATA;

	/* Very unfortunate */
	memcpy( ( uchar* )&request.data.sensor_data, ( uchar* )data, sizeof( Sensor_data ) );

	return train_auto_request( tid, &request, sizeof( uint ) + sizeof( request.data.sensor_data ), 0, 0 );
}

static int train_auto_wakeup( int tid )
{
	Train_auto_request request;

	request.type = TRAIN_AUTO_WAKEUP;

	return train_auto_request( tid, &request, sizeof( uint ), 0, 0 );
}

int train_auto_init( int tid, uint track )
{
	Train_auto_request request;
	
	request.type = TRAIN_AUTO_INIT;
	request.data.init.track_id = track;

	return train_auto_request( tid, &request, sizeof( uint ) + sizeof( request.data.init ), 0, 0 );
}

int train_auto_new_train( int tid, uint id, uint pre_grp, uint pre_id )
{
	Train_auto_request request;

	request.type = TRAIN_AUTO_NEW_TRAIN;
	request.data.new_train.train_id = id;
	request.data.new_train.pickup = TRAIN_PICKUP_FRONT;
	request.data.new_train.next_group = pre_grp;
	request.data.new_train.next_id = pre_id;

	return train_auto_request( tid, &request, sizeof( uint ) + sizeof( request.data.new_train ), 0, 0 );
}

int train_auto_set_speed( int tid, uint id, uint speed_level )
{
	Train_auto_request request;

	request.type = TRAIN_AUTO_SET_TRAIN_SPEED;
	request.data.set_speed.train_id = id;
	request.data.set_speed.speed_level = speed_level;

	return train_auto_request( tid, &request, sizeof( uint ) + sizeof( request.data.set_speed ), 0, 0 );
}

int train_auto_set_reverse( int tid, uint id )
{
	Train_auto_request request;

	request.type = TRAIN_AUTO_QUERY_SWITCH_DIR;

	request.data.set_reverse.train_id = id;

	return train_auto_request( tid, &request, sizeof( uint ) + sizeof( request.data.set_reverse ), 0, 0 );
}

int train_auto_set_switch( int tid, uint id, char direction )
{
	Train_auto_request request;

	request.type = TRAIN_AUTO_SET_SWITCH_DIR;
	request.data.set_switch.switch_id = id;
	request.data.set_switch.direction = direction;

	return train_auto_request( tid, &request, sizeof( uint ) + sizeof( request.data.set_switch ), 0, 0 );
}

int train_auto_set_switch_all( int tid, char direction )
{
	Train_auto_request request;

	request.type = TRAIN_AUTO_SET_ALL_SWITCH;
	request.data.set_switch_all.direction = direction;

	return train_auto_request( tid, &request, sizeof( uint ) + sizeof( request.data.set_switch_all ), 0, 0 );
}

int train_auto_query_switch( int tid, uint id, int* direction )
{
	Train_auto_request request;

	request.type = TRAIN_AUTO_QUERY_SWITCH_DIR;
	request.data.query_switch.switch_id = id;

	return train_auto_request( tid, &request, sizeof( uint ) + sizeof( request.data.query_switch ), direction, 0 );
}

int train_auto_query_sensor( int tid, int* group, int* id )
{
	Train_auto_request request;

	request.type = TRAIN_AUTO_QUERY_LAST_SENSOR;
	
	return train_auto_request( tid, &request, sizeof( uint ) + sizeof( request.data.query_sensor ), group, id );
}


