#ifndef _TRAIN_CONFIG_H_
#define _TRAIN_CONFIG_H_

#define SENSOR_BYTE_COUNT    10
#define SENSOR_NAME_LENGTH    3
#define NUM_SWITCHES         22
#define MAX_NUM_TRAINS       10
#define MAX_TRAIN_ID         128
#define PATH_BUFFER_SIZE     128
#define TARGET_BUFFER_SIZE   20     /* First for current chk pnt, others for future */
#define PATH_LOOK_AHEAD_TIME    200
#define PATH_LOOK_AHEAD_LOW     6
#define PATH_LOOK_AHEAD_HIGH    10
#define PATH_LOOK_AHEAD_BUFFER  32
#define PATH_LOOK_AHEAD_DIST    4000
#define PATH_LOOK_AHEAD_ADJUST_DIST 2000
#define PATH_LOOK_AHEAD_ADJUST_TIME 200
#define PATH_LOOK_AHEAD_DEFAULT_STOP 100

#define TRAIN_AUTO_REG_SPEED_1  8
#define TRAIN_AUTO_REG_SPEED_2  12

#define TRAIN_TRAVEL_SPEED        10
#define TRAIN_CLOSE_TRAVEL_SPEED  6

#define TRAIN_AUTO_REG_RETRY      3
#define TRAIN_AUTO_REG_SPEED_CALIB_TIME   1000

#define SENSOR_GROUP_COUNT      5
#define SENSOR_COUNT_PER_GROUP  16

/* Error bounds */
#define DIST_ERROR           40       /* in mm */
#define TIME_ERROR           0        /* TODO: Undefined yet */

/* Perf measurements */
#define SENSOR_AVERAGE_DELAY  8       /* in tickes */

/* UI */
#define SENSOR_UI_NAME      "sensor_ui"
#define TRAIN_AUTO_NAME     "train_auto"
#define TRAIN_SWITCH_NAME   "switch_ui"
#define TRAIN_MODULE_NAME   "train_mod"
#define SENSOR_QUERY_NAME   "sensor_qu"
#define TRACKING_UI_NAME    "tracking_ui"
#define TRAIN_COMMAND_MAX_TOKEN  16

/* Timing, in ticks */
#define SENSOR_XMIT_TIME    8
#define TRAIN_AUTO_WAKEUP_PERIOD 2

#define WARNING_REGION       { 14, 23, 1, 78 - 14, 1, 0 }
#define SWID_TO_ARRAYID( i ) ( i < 19 ? i - 1 : i - 135 )
#define ARRAYID_TO_SWID( i ) ( i < 18 ? i + 1 : i + 135 )

#endif /* _TRAIN_CONFIG_H_ */
