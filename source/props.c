#include "props.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *names[P_COUNT] = {
    [P_GAME_RUNNING] = "dcp.GameRunning",
    [P_GEAR] = "dcp.gd.Gear",
    [P_SPEED] = "dcp.gd.SpeedLocal",
    [P_SPEED_UNIT] = "dcp.gd.SpeedLocalUnit",
    [P_RPM] = "dcp.gd.Rpms",
    [P_MAX_RPM] = "dcp.gd.CarSettings_MaxRPM",
    [P_REDLINE_RPM] = "dcp.gd.CarSettings_RedLineRPM",
    [P_RPM_PCT] = "dcp.gd.CarSettings_CurrentDisplayedRPMPercent",
    [P_CUR_LAP_TIME] = "dcp.gd.CurrentLapTime",
    [P_LAST_LAP_TIME] = "dcp.gd.LastLapTime",
    [P_BEST_LAP_TIME] = "dcp.gd.BestLapTime",
    [P_POSITION] = "dcp.gd.Position",
    [P_OPPONENTS] = "dcp.gd.OpponentsCount",
    [P_CUR_LAP] = "dcp.gd.CurrentLap",
    [P_TOTAL_LAPS] = "dcp.gd.TotalLaps",
    [P_FUEL] = "dcp.gd.Fuel",
    [P_FUEL_PCT] = "dcp.gd.FuelPercent",
    [P_FUEL_UNIT] = "dcp.gd.FuelUnit",
    [P_TC] = "dcp.gd.TCLevel",
    [P_ABS] = "dcp.gd.ABSLevel",
    [P_BRAKE_BIAS] = "dcp.gd.BrakeBias",
    [P_PIT_LIMITER] = "dcp.gd.PitLimiterOn",
    [P_FLAG] = "dcp.gd.Flag_Name",
    [P_THROTTLE] = "dcp.gd.Throttle",
    [P_BRAKE] = "dcp.gd.Brake",
    [P_WATER_TEMP] = "dcp.gd.WaterTemperature",
    [P_OIL_TEMP] = "dcp.gd.OilTemperature",
    [P_TYRE_TEMP_FL] = "dcp.gd.TyreTemperatureFrontLeft",
    [P_TYRE_TEMP_FR] = "dcp.gd.TyreTemperatureFrontRight",
    [P_TYRE_TEMP_RL] = "dcp.gd.TyreTemperatureRearLeft",
    [P_TYRE_TEMP_RR] = "dcp.gd.TyreTemperatureRearRight",
    [P_TYRE_PRES_FL] = "dcp.gd.TyrePressureFrontLeft",
    [P_TYRE_PRES_FR] = "dcp.gd.TyrePressureFrontRight",
    [P_TYRE_PRES_RL] = "dcp.gd.TyrePressureRearLeft",
    [P_TYRE_PRES_RR] = "dcp.gd.TyrePressureRearRight",
    [P_TYRE_PRES_UNIT] = "dcp.gd.TyrePressureUnit",
    [P_SESSION_LEFT] = "dcp.gd.SessionTimeLeft",
};

const char *prop_name(PropId id) { return names[id]; }

// .NET TimeSpan text: [-][d.]hh:mm:ss[.fffffff]
static bool parse_timespan(const char *s, double *out) {
    double sign = 1;
    if (*s == '-') { sign = -1; s++; }
    if (!strchr(s, ':')) {
        char *end;
        double v = strtod(s, &end);
        if (end == s) return false;
        *out = sign * v;
        return true;
    }
    double days = 0, hours, mins, secs;
    char *end;
    const char *dot = strchr(s, '.');
    const char *colon = strchr(s, ':');
    if (dot && dot < colon) {
        days = strtod(s, &end);
        s = dot + 1;
    }
    hours = strtod(s, &end);
    if (*end != ':') return false;
    mins = strtod(end + 1, &end);
    if (*end != ':') return false;
    secs = strtod(end + 1, &end);
    *out = sign * (days * 86400 + hours * 3600 + mins * 60 + secs);
    return true;
}

bool prop_parse_line(const char *line, PropValue *vals) {
    if (strncmp(line, "Property ", 9) != 0) return false;
    const char *name = line + 9;
    const char *sp = strchr(name, ' ');
    if (!sp) return false;
    int name_len = (int)(sp - name);
    const char *type = sp + 1;
    sp = strchr(type, ' ');
    int type_len = sp ? (int)(sp - type) : (int)strlen(type);
    const char *value = sp ? sp + 1 : "";

    int id;
    for (id = 0; id < P_COUNT; id++) {
        if ((int)strlen(names[id]) == name_len && strncmp(names[id], name, name_len) == 0) break;
    }
    if (id == P_COUNT) return false;

    PropValue *v = &vals[id];
    snprintf(v->str, sizeof(v->str), "%s", value);
    if (strcmp(value, "(null)") == 0) {
        v->valid = false;
        v->num = 0;
        v->str[0] = 0;
        return true;
    }
    v->valid = true;
    if (strncmp(type, "boolean", type_len) == 0) {
        v->num = (strcmp(value, "True") == 0 || strcmp(value, "true") == 0) ? 1 : 0;
    } else if (strncmp(type, "timespan", type_len) == 0) {
        if (!parse_timespan(value, &v->num)) v->num = 0;
    } else if (strncmp(type, "string", type_len) == 0) {
        v->num = strtod(value, NULL);
    } else {
        v->num = strtod(value, NULL);  // integer, long, double
    }
    return true;
}

void prop_format_time(const PropValue *v, char *out, int size) {
    if (!v->valid || v->num <= 0) {
        snprintf(out, size, "-:--.---");
        return;
    }
    long ms = (long)(v->num * 1000.0 + 0.5);
    long h = ms / 3600000, m = (ms / 60000) % 60, s = (ms / 1000) % 60;
    if (h > 0)
        snprintf(out, size, "%ld:%02ld:%02ld", h, m, s);
    else
        snprintf(out, size, "%ld:%02ld.%03ld", m, s, ms % 1000);
}
