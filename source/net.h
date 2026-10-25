#pragma once
#include "props.h"

typedef enum {
    NET_IDLE,
    NET_CONNECTING,
    NET_CONNECTED,
    NET_FAILED,  // last attempt failed; retrying soon
} NetState;

// Starts the background thread that keeps a connection to the
// SimHub Property Server and keeps the property table up to date.
void net_start(const char *host, int port);
void net_stop(void);

// Points the client at a new PC; the current connection is dropped.
void net_set_target(const char *host, int port);

// Copies the latest property values and connection state.
NetState net_snapshot(PropValue *out);
