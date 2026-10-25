#pragma once
#include <stdbool.h>

// Every SimHub property the dash subscribes to. Names live in props.c.
typedef enum {
    P_GAME_RUNNING,
    P_GEAR,
    P_SPEED,
    P_SPEED_UNIT,
    P_RPM,
    P_MAX_RPM,
    P_REDLINE_RPM,
    P_RPM_PCT,
    P_CUR_LAP_TIME,
    P_LAST_LAP_TIME,
    P_BEST_LAP_TIME,
    P_POSITION,
    P_OPPONENTS,
    P_CUR_LAP,
    P_TOTAL_LAPS,
    P_FUEL,
    P_FUEL_PCT,
    P_FUEL_UNIT,
    P_TC,
    P_ABS,
    P_BRAKE_BIAS,
    P_PIT_LIMITER,
    P_FLAG,
    P_THROTTLE,
    P_BRAKE,
    P_WATER_TEMP,
    P_OIL_TEMP,
    P_TYRE_TEMP_FL,
    P_TYRE_TEMP_FR,
    P_TYRE_TEMP_RL,
    P_TYRE_TEMP_RR,
    P_TYRE_PRES_FL,
    P_TYRE_PRES_FR,
    P_TYRE_PRES_RL,
    P_TYRE_PRES_RR,
    P_TYRE_PRES_UNIT,
    P_SESSION_LEFT,
    P_COUNT
} PropId;

typedef struct {
    bool valid;      // false when SimHub reports (null) or nothing arrived yet
    double num;      // numeric value; timespans are in seconds, booleans 0/1
    char str[48];    // raw value text
} PropValue;

const char *prop_name(PropId id);

// Parses one server line ("Property <name> <type> <value>") into vals.
// Returns true if the line updated a known property.
bool prop_parse_line(const char *line, PropValue *vals);

// Formats a lap/session time as m:ss.mmm (or h:mm:ss when long).
void prop_format_time(const PropValue *v, char *out, int size);
