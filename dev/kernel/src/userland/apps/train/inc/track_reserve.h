#ifndef _TRACK_RESERVE_H_
#define _TRACK_RESERVE_H_

#include <types.h>
#include "track_node.h"
#include "train_types.h"

enum Track_reserve_return {
	RESERVE_SUCCESS,
	RESERVE_FAIL_SAME_DIR,
	RESERVE_FAIL_AGAINST_DIR,
	RESERVE_FAIL_EXIT
};

/* Server */
void track_reserve();

int track_reserve_get_range( int tid, Train* train, int dist );
int track_reserve_get( int tid, Train* train, track_node* node );
int track_reserve_may_i( int tid, Train* train, track_node* node, int direction );
int track_reserve_put( int tid, Train* train, track_node* node );
int track_reserve_free( Train* train );

#endif /* _TRACK_RESERVE_H_ */
