/* Primitive types definitions */

#ifndef _TYPES_H_
#define _TYPES_H_

typedef unsigned int uint;
typedef uint ptr;
typedef unsigned char uchar;
typedef uint time_t;

typedef struct Context_s Context;
typedef struct Memmgr_s Memmgr;
typedef struct Task_s Task;
typedef struct Clock_s Clock;
typedef struct Console_s Console;
typedef struct Sched_s Sched;
typedef struct List_s List;
typedef struct Syscall_s Syscall;
typedef struct Hashtable_s Hashtable;
typedef struct Interrupt_mgr_s Interrupt_mgr;

typedef int (*Interrupt_handler)( Context* ctx );

#define BITS_IN_BYTE     8

#endif /* _TYPES_H_ */
