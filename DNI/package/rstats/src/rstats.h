
#ifndef __RSTATS_H_
#define __RSTATS_H_

#include "list.h"

#define RSTATS_INTERVAL 5
#define RSTATS_PID_FILE "/var/run/rstats.pid"

#define RSTATS_DATA_QUERY "/tmp/rstats.query"
#define RSTATS_DATA_FILE "/tmp/rstats.json"

#define MAX_PANNEL_PORT 5
#define MAX_PHY_PORT 7

#define SHOUR (60 * 60)
#define MHOUR (60)

#define MAX_NSTATS ((5 * SHOUR) / (RSTATS_INTERVAL))

#define MAX_COUNTER 2
#define STATS_RX 0
#define STATS_TX 1

typedef struct stats_item {
    struct list_head list;
    char ifdesc[20];
    int head;
    int tail;
    int count;
    uint64_t stats[MAX_NSTATS][MAX_COUNTER];
} stats_item_t;

typedef struct stats_item_list {
    int count;
    int tail;
    struct list_head head;
} stats_item_list_t;

#endif

