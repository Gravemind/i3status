// vim:ts=4:sw=4:expandtab
#include <config.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <locale.h>
#include <yajl/yajl_gen.h>
#include <yajl/yajl_version.h>

#include "i3status.h"

static bool local_timezone_init = false;
static const char *local_timezone = NULL;
static const char *current_timezone = NULL;

void set_timezone(const char *tz) {
    if (!local_timezone_init) {
        /* First call, initialize. */
        local_timezone = getenv("TZ");
        local_timezone_init = true;
    }
    if (tz == NULL || tz[0] == '\0') {
        /* User wants localtime. */
        tz = local_timezone;
    }
    if (tz != current_timezone) {
        if (tz) {
            setenv("TZ", tz, 1);
        } else {
            unsetenv("TZ");
        }
        current_timezone = tz;
    }
    tzset();
}

void print_time(yajl_gen json_gen, char *buffer, const char *title, const char *format, const char *tz, const char *locale, const char *format_time, const char *progress, bool hide_if_equals_localtime, time_t t) {
    const char *walk;
    output_color_t outcolor = COLOR_DEFAULT;
    char *outwalk = buffer;
    struct tm local_tm, tm;
    char timebuf[1024];

    if (title != NULL)
        INSTANCE(title);

    set_timezone(NULL);
    localtime_r(&t, &local_tm);

    set_timezone(tz);
    localtime_r(&t, &tm);

    // When hide_if_equals_localtime is true, compare local and target time to display only if different
    time_t local_t = mktime(&local_tm);
    double diff = difftime(local_t, t);
    if (hide_if_equals_localtime && diff == 0.0) {
        goto out;
    }

    if (locale != NULL) {
        setlocale(LC_ALL, locale);
    }

    if (format_time == NULL) {
        strftime(timebuf, sizeof(timebuf), format, &tm);
        maybe_escape_markup(timebuf, &outwalk);
    } else {
        for (walk = format; *walk != '\0'; walk++) {
            if (*walk != '%') {
                *(outwalk++) = *walk;

            } else if (BEGINS_WITH(walk + 1, "time")) {
                strftime(timebuf, sizeof(timebuf), format_time, &tm);
                maybe_escape_markup(timebuf, &outwalk);
                walk += strlen("time");

            } else {
                *(outwalk++) = '%';
            }
        }
    }

    if (progress != NULL) {
        int p = 0;
        if (strcmp(progress, "minute") == 0)
            p = 100 * tm.tm_sec / 60;
        else if (strcmp(progress, "hour") == 0)
            p = 100 * (tm.tm_min * 60 + tm.tm_sec) / (60 * 60);
        else if (strcmp(progress, "12h") == 0)
            p = 100 * ((tm.tm_hour % 12) * 60 + tm.tm_min) / (12 * 60);
        else if (strcmp(progress, "day") == 0)
            p = 100 * (tm.tm_hour * 60 + tm.tm_min) / (24 * 60);
        else if (strcmp(progress, "monday_week") == 0)
            p = 100 * ((tm.tm_wday == 0 ? 6 : tm.tm_wday - 1) * 24 + tm.tm_hour) / (7 * 24);
        else if (strcmp(progress, "week") == 0)
            p = 100 * (tm.tm_wday * 24 + tm.tm_hour) / (7 * 24);
        else if (strcmp(progress, "month") == 0)
            p = 100 * (tm.tm_mday * 24 + tm.tm_hour) / (730);  // @TODO compute actual number for current month
        else if (strcmp(progress, "year") == 0)
            p = 100 * (tm.tm_yday) / 365;  // @TODO compute actual number for current year
        else if (strcmp(progress, "100") == 0)
            p = 100;
        SET_PROGRESS(p);
    }

    if (locale != NULL) {
        setlocale(LC_ALL, "");
    }

out:
    *outwalk = '\0';
    OUTPUT_FULL_TEXT(buffer);
}
