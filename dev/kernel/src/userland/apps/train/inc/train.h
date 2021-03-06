#ifndef _USER_TRAIN_H_
#define _USER_TRAIN_H_

#include "sensor_data.h"
#include "track_node.h"
#include "train_types.h"

/* task priorities : high - low */
#define TRAIN_SENSOR_PRIORITY       6
#define TRAIN_MODULE_PRIORITY       7
#define TRAIN_EXECUTOR_PRIORITY     7
#define TRAIN_AUTO_PRIROTY          7
#define TRAIN_SCHED_PRIORITY        8
#define TRAIN_PLAN_PRIORITY         9
#define TRAIN_UI_PRIORITY           10


#define COMMAND_BUFFER_SIZE (sizeof( Train_command ) * 128)		// 12 * 32

/* train event api */
/* Server */
void train_module();
int train_set_speed( int tid, int train, int speed );
int train_reverse( int tid, int train );
int train_check_switch( int tid, int switch_id );
int train_switch( int tid, int switch_id, int direction );
int train_all_sensor( int tid );
int train_module_suicide( int tid );
int train_pressure_test( int tid );
int train_switch_all( int tid, int d );
int train_execute( int tid );

/* executor notifier */
void train_module_executor();

/* Automation */
/* Server */
void train_auto();
/* API */
enum Track_id {
	TRACK_A,
	TRACK_B
};

int train_auto_new_sensor_data( int tid, Sensor_data* data );
int train_auto_init( int tid, uint track );
int train_auto_new_train( int tid, uint id );
int train_auto_set_speed( int tid, uint id, uint speed_level );
int train_auto_set_reverse( int tid, uint id );
int train_auto_set_switch( int tid, uint id, char direction );
int train_auto_set_switch_all( int tid, char direction );
int train_auto_query_switch( int tid, uint id, int* direction );
/* Return group < 0 if no sensor is triggered */
int train_auto_query_sensor( int tid, int* group, int* id );
int train_auto_hit_and_stop( int tid, int train_id, int group, int id, int distance );
int train_auto_plan( int tid, int train_id, int group, int id, int dist_pass );
int train_auto_set_train_sc_time( int tid, int train_id, int min, int max );
int train_auto_reset_track( int tid, char track_id );
int train_auto_set_bad_switch( int tid, uint id, char direction );
int train_auto_set_pickup( int tid, uint train_id, char direction );

/* switch event api */
int switch_ui_update_id( int tid, int id, char direction );
int switch_ui_update_all( int tid, char direction );
int switch_ui_check_id( int tid, int id );

/* Train tracking ui api */
int tracking_ui_new_train( int tid, int train_id );
int tracking_ui_chkpnt( int tid, int train_id, int group, int id, int predict, int diff );
int tracking_ui_landmrk( int tid, int train_id, int group, int id );
int tracking_ui_dist( int tid, int train_id, int dist );
int tracking_ui_nextmrk( int tid, int train_id, int group, int id, int eta );
int tracking_ui_speed( int tid, int train_id, int speed );

/* Planner ui api */
int planner_ui_new_train( int tid, int train_id );
int planner_ui_new_plan( int tid, int train_id, int group, int id, int deadline );
int planner_ui_arrival( int tid, int train_id, int deadline );
int planner_ui_cancel( int tid, int train_id );

/* sensor api */
/* Server */
void train_sensor();
/* group < 0 if no sensor has been triggered yet */
int sensor_query_recent( int tid, int* group, int* id );
int sensor_id2name( char* str, uchar group, uchar id );
int sensor_name2id( const char* str, int* group, int* id );

/* ui components */
void sensor_ui();
int sensor_ui_update( int tid, Sensor_data* data );
void clock_ui();
void switches_ui();
void tracking_ui();
void planner_ui();

/* Train planner task */
/* Server */
void train_planner();
/* API */
int train_planner_init( int tid, Train_data* train );
int train_planner_path_plan( int tid, track_node* dst, int dist_pass );
int train_planner_wakeup( int tid );
int train_planner_have_control( Train_data* train );
/* UI */
int train_ui_register( int tid, Train_data* train );

/* Scheduler UI */
/* Server */
void sched_ui();
int sched_ui_new_plan( int tid, int ticket, int dest_group, int dest_id, int dest_dist, int src_group, int src_id, int src_dist );
int sched_ui_assign( int tid, int ticket, int train_id );
int sched_ui_complete( int tid, int ticket );

#endif
