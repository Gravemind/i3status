// vim:ts=4:sw=4:expandtab
#include <config.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <dirent.h>

#include "i3status.h"

/*
 * Returns the correct color format for i3bar (color), dzen (^fg(color)), xmobar
 * (<fc=color>), lemonbar (%{Fcolor}) or terminal (escape-sequence)
 *
 */
const char *begin_color_str(output_color_t outcolor, bool try_cfg_section) {
    if (!enable_colors || output_format == O_NONE) {
        return NULL;
    }

    const char *color_key = NULL;
    switch (outcolor) {
        case COLOR_DEFAULT:
            color_key = "color_default";
            break;
        case COLOR_GOOD:
            color_key = "color_good";
            break;
        case COLOR_BAD:
            color_key = "color_bad";
            break;
        case COLOR_DEGRADED:
            color_key = "color_degraded";
            break;
        case COLOR_SEPARATOR:
            color_key = "color_separator";
            break;
    }
    if (!color_key)
        return NULL;

    const char *color = NULL;
    if (try_cfg_section && cfg_section)
        color = cfg_getstr(cfg_section, color_key);
    if (!color)
        color = cfg_getstr(cfg_general, color_key);
    if (!color || color[0] == '\0')
        return NULL;

    if (output_format == O_I3BAR)
        return color;

    static char colorbuf[32];
    if (output_format == O_DZEN2)
        (void)snprintf(colorbuf, sizeof(colorbuf), "^fg(%s)", color);
    else if (output_format == O_XMOBAR)
        (void)snprintf(colorbuf, sizeof(colorbuf), "<fc=%s>", color);
    else if (output_format == O_LEMONBAR)
        (void)snprintf(colorbuf, sizeof(colorbuf), "%%{F%s}", color);
    else if (output_format == O_TERM) {
        /* The escape-sequence for color is <CSI><col>;1m (bright/bold
         * output), where col is a 3-bit rgb-value with b in the
         * least-significant bit. We round the given color to the
         * nearist 3-bit-depth color and output the escape-sequence */
        int col = strtol(color + 1, NULL, 16);
        int r = (col & (0xFF << 0)) / 0x80;
        int g = (col & (0xFF << 8)) / 0x8000;
        int b = (col & (0xFF << 16)) / 0x800000;
        col = (r << 2) | (g << 1) | b;
        (void)snprintf(colorbuf, sizeof(colorbuf), "\033[3%d;1m", col);
    } else {
        return NULL;
    }
    return colorbuf;
}

const char *begin_progress_colors_str(output_color_t outcolor, const char *fallback, bool try_cfg_section) {
    if (!enable_colors || !enable_progress_bars) {
        return NULL;
    }

    if (output_format != O_I3BAR)
        return NULL;

    const char *color_key = NULL;
    switch (outcolor) {
        case COLOR_DEFAULT:
            color_key = "progress_colors_default";
            break;
        case COLOR_GOOD:
            color_key = "progress_colors_good";
            break;
        case COLOR_BAD:
            color_key = "progress_colors_bad";
            break;
        case COLOR_DEGRADED:
            color_key = "progress_colors_degraded";
            break;
        case COLOR_SEPARATOR:
            color_key = "progress_colors_separator";
            break;
    }
    if (!color_key)
        return NULL;

    const char *color = NULL;
    if (try_cfg_section && cfg_section)
        color = cfg_getstr(cfg_section, color_key);
    if (!color)
        color = cfg_getstr(cfg_general, color_key);

    if (!color) {
        /* Use `fallback + " " + progress_color_default_bg` */
        static char colorbuf[9 + 1 + 9 + 1];
        const char *bg = cfg_getstr(cfg_general, "progress_color_default_bg");
        (void)snprintf(colorbuf, sizeof(colorbuf), "%s %s", fallback ? fallback : "-", bg ? bg : "-");
        return colorbuf;
    }

    if (color[0] == '\0')
        return NULL; /* explicitly disabled, don't use fallback */

    return color;
}

/*
 * Some color formats (xmobar) require to terminate colors again
 *
 */
const char *end_color_str(void) {
    if (output_format == O_XMOBAR)
        return "</fc>";
    else if (output_format == O_TERM)
        return "\033[0m";
    else
        return "";
}

void print_separator(const char *separator) {
    if (output_format == O_I3BAR || strlen(separator) == 0)
        return;
    const char *colorstr = begin_color_str(COLOR_SEPARATOR, false);
    if (colorstr)
        printf("%s%s%s", colorstr, separator, end_color_str());
    else
        printf("%s", separator);
}

/*
 * The term-output hides the cursor. We call this on exit to reset that.
 */
void reset_cursor(void) {
    printf("\033[?25h");
}

/*
 * Escapes ampersand, less-than, greater-than, single-quote, and double-quote
 * characters with the corresponding Pango markup strings if markup is enabled.
 * See the glib implementation:
 * https://git.gnome.org/browse/glib/tree/glib/gmarkup.c?id=03db1f455b4265654e237d2ad55464b4113cba8a#n2142
 *
 */
void maybe_escape_markup(char *text, char **buffer) {
    if (markup_format == M_NONE) {
        *buffer += sprintf(*buffer, "%s", text);
        return;
    }
    for (; *text != '\0'; text++) {
        switch (*text) {
            case '&':
                *buffer += sprintf(*buffer, "%s", "&amp;");
                break;
            case '<':
                *buffer += sprintf(*buffer, "%s", "&lt;");
                break;
            case '>':
                *buffer += sprintf(*buffer, "%s", "&gt;");
                break;
            case '\'':
                *buffer += sprintf(*buffer, "%s", "&apos;");
                break;
            case '"':
                *buffer += sprintf(*buffer, "%s", "&quot;");
                break;
            default:
                if ((0x1 <= *text && *text <= 0x8) ||
                    (0xb <= *text && *text <= 0xc) ||
                    (0xe <= *text && *text <= 0x1f)) {
                    *buffer += sprintf(*buffer, "&#x%x;", *text);
                } else {
                    *(*buffer)++ = *text;
                }
                break;
        }
    }
}
