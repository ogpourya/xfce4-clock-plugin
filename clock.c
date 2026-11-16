// SPDX-License-Identifier: MIT
#include "clock.h"
#include <libxfce4util/libxfce4util.h>
#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

const char *const LAYOUT_NAMES[LAYOUT_COUNT] = {
    [LAYOUT_TIME_ONLY]        = "Time Only",
    [LAYOUT_DATE_ONLY]        = "Date Only",
    [LAYOUT_STACK_TIME_DATE]  = "Stacked (Time / Date)",
    [LAYOUT_STACK_DATE_TIME]  = "Stacked (Date / Time)",
    [LAYOUT_INLINE_TIME_DATE] = "Inline (Time · Date)",
    [LAYOUT_INLINE_DATE_TIME] = "Inline (Date · Time)",
};

const char *const STYLE_NAMES[STYLE_COUNT] = {
    [STYLE_DIGITAL] = "Digital",
    [STYLE_LCD]     = "LCD (7-Segment)",
    [STYLE_FUZZY]   = "Fuzzy",
    [STYLE_ANALOG]  = "Analog",
    [STYLE_BINARY]  = "Binary (BCD)",
    [STYLE_NIXIE]   = "Nixie Tube",
};

const char *const FUZZY_NAMES[FUZZY_COUNT] = {
    [FUZZY_5MIN]    = "5 minutes (nearly quarter past twelve)",
    [FUZZY_15MIN]   = "15 minutes (around half past twelve)",
    [FUZZY_DAYPART] = "Part of day (early afternoon)",
};

static const char *const BUILTIN_CLOCK_SVG =
    "<svg xmlns='http://www.w3.org/2000/svg' width='16' height='16' viewBox='0 0 16 16'>"
    "<path fill='#2e3436' d='M8 1a7 7 0 1 0 0 14A7 7 0 0 0 8 1zm0 1.6a5.4 5.4 0 1 1 0 10.8A5.4 5.4 0 0 1 8 2.6zM7.2 4v4.4l3.3 2 .8-1.3-2.5-1.5V4H7.2z'/>"
    "</svg>";

gboolean clock_has_time(LayoutMode mode)         { return mode != LAYOUT_DATE_ONLY; }
gboolean clock_has_date(LayoutMode mode)         { return mode != LAYOUT_TIME_ONLY; }
gboolean clock_is_stacked(LayoutMode mode)       { return mode == LAYOUT_STACK_TIME_DATE || mode == LAYOUT_STACK_DATE_TIME; }
gboolean clock_is_inline(LayoutMode mode)        { return mode == LAYOUT_INLINE_TIME_DATE || mode == LAYOUT_INLINE_DATE_TIME; }
gboolean clock_style_uses_canvas(StylePreset st) { return st == STYLE_LCD || st == STYLE_ANALOG || st == STYLE_BINARY; }

static gboolean format_has_seconds(const char *fmt) {
    return fmt && (strstr(fmt, "%S") != NULL || strstr(fmt, "%T") != NULL || strstr(fmt, "%r") != NULL || strstr(fmt, "%X") != NULL);
}

static gboolean format_is_12h(const char *fmt) {
    return fmt && (strstr(fmt, "%I") != NULL || strstr(fmt, "%l") != NULL || strstr(fmt, "%r") != NULL);
}

void clock_reload_timezone(ClockPlugin *c) {
    if (c->tz) {
        g_time_zone_unref(c->tz);
        c->tz = NULL;
    }
    if (c->timezone_id && *c->timezone_id) {
        GTimeZone *tz = g_time_zone_new_identifier(c->timezone_id);
        if (tz) {
            c->tz = tz;
            c->tz_valid = TRUE;
            return;
        }
        c->tz_valid = FALSE;
    } else {
        c->tz_valid = TRUE;
    }
    c->tz = g_time_zone_new_local();
}

gchar *clock_format_preview(ClockPlugin *c, const char *fmt) {
    if (!fmt || !*fmt) return g_strdup("");
    g_autoptr(GDateTime) now = g_date_time_new_now(c->tz);
    if (!now) return g_strdup("");
    gchar *out = g_date_time_format(now, fmt);
    return out ? out : g_strdup("(invalid format)");
}

static void format_fuzzy_time(GDateTime *now, FuzzyLevel level, char *out, size_t len) {
    int h = g_date_time_get_hour(now);
    int m = g_date_time_get_minute(now);

    if (level == FUZZY_DAYPART) {
        const char *part = "night";
        if (h == 0 && m < 30) part = "midnight";
        else if (h < 5)       part = "late night";
        else if (h < 8)       part = "early morning";
        else if (h < 12)      part = "morning";
        else if (h == 12 && m < 30) part = "noon";
        else if (h < 15)      part = "early afternoon";
        else if (h < 18)      part = "late afternoon";
        else if (h < 21)      part = "evening";
        else                  part = "night";
        g_strlcpy(out, part, len);
        return;
    }

    static const char *const HOURS[12] = {
        "twelve", "one", "two", "three", "four", "five",
        "six", "seven", "eight", "nine", "ten", "eleven"
    };

    if (level == FUZZY_15MIN) {
        int sector = (m + 7) / 15;
        int disp_h = h;
        if (sector >= 3) disp_h = (h + 1) % 24;
        const char *hword = (disp_h == 0) ? "midnight" : (disp_h == 12) ? "noon" : HOURS[disp_h % 12];
        switch (sector) {
            case 0:
            case 4:
                if (disp_h == 0 || disp_h == 12) g_snprintf(out, len, "around %s", hword);
                else g_snprintf(out, len, "around %s o'clock", hword);
                break;
            case 1:
                g_snprintf(out, len, "quarter past %s", hword);
                break;
            case 2:
                g_snprintf(out, len, "half past %s", hword);
                break;
            default:
                g_snprintf(out, len, "quarter to %s", hword);
                break;
        }
        return;
    }

    /* FUZZY_5MIN */
    int bucket = (m + 2) / 5; /* 0..12 */
    int disp_h = (bucket >= 7) ? ((h + 1) % 24) : h;
    const char *hword = (disp_h == 0) ? "midnight" : (disp_h == 12) ? "noon" : HOURS[disp_h % 12];

    switch (bucket) {
        case 0:
        case 12:
            if (disp_h == 0 || disp_h == 12) g_strlcpy(out, hword, len);
            else g_snprintf(out, len, "%s o'clock", hword);
            break;
        case 1:  g_snprintf(out, len, "five past %s", hword); break;
        case 2:  g_snprintf(out, len, "ten past %s", hword); break;
        case 3:  g_snprintf(out, len, "quarter past %s", hword); break;
        case 4:  g_snprintf(out, len, "twenty past %s", hword); break;
        case 5:  g_snprintf(out, len, "twenty-five past %s", hword); break;
        case 6:  g_snprintf(out, len, "half past %s", hword); break;
        case 7:  g_snprintf(out, len, "twenty-five to %s", hword); break;
        case 8:  g_snprintf(out, len, "twenty to %s", hword); break;
        case 9:  g_snprintf(out, len, "quarter to %s", hword); break;
        case 10: g_snprintf(out, len, "ten to %s", hword); break;
        default: g_snprintf(out, len, "five to %s", hword); break;
    }
}

static void get_time_fg_rgba(ClockPlugin *c, GdkRGBA *out) {
    if (c->time_color && *c->time_color && gdk_rgba_parse(out, c->time_color))
        return;
    if (c->style == STYLE_NIXIE) {
        gdk_rgba_parse(out, "#ff7b1c");
        return;
    }
    gtk_style_context_get_color(gtk_widget_get_style_context(c->root_box), GTK_STATE_FLAG_NORMAL, out);
}

static void get_accent_rgba(ClockPlugin *c, const GdkRGBA *fg, GdkRGBA *out) {
    if (c->accent_color && *c->accent_color && gdk_rgba_parse(out, c->accent_color))
        return;
    if (c->time_color && *c->time_color && gdk_rgba_parse(out, c->time_color))
        return;
    *out = *fg;
    out->red   = MIN(1.0, out->red * 0.6 + 0.35);
    out->green = MIN(1.0, out->green * 0.6 + 0.15);
    out->blue  = MAX(0.1, out->blue * 0.5);
}

/* ==================== 7-Segment LCD Renderer ==================== */

static void draw_7seg_digit(cairo_t *cr, double x, double y, double w, double h, int digit, const GdkRGBA *fg) {
    /* Bitmask: bit0=top(a), bit1=top-right(b), bit2=bot-right(c), bit3=bot(d), bit4=bot-left(e), bit5=top-left(f), bit6=mid(g) */
    static const unsigned char MASKS[11] = {
        0x3F, /* 0 */
        0x06, /* 1 */
        0x5B, /* 2 */
        0x4F, /* 3 */
        0x66, /* 4 */
        0x6D, /* 5 */
        0x7D, /* 6 */
        0x07, /* 7 */
        0x7F, /* 8 */
        0x6F, /* 9 */
        0x00, /* blank */
    };
    unsigned char mask = (digit >= 0 && digit <= 9) ? MASKS[digit] : 0x00;

    double t = MAX(1.5, MIN(w * 0.20, h * 0.105));
    double g = MAX(0.7, t * 0.32);
    double ym = y + h * 0.5;
    double skew = w * 0.08;

    for (int seg = 0; seg < 7; seg++) {
        gboolean on = (mask & (1 << seg)) != 0;
        cairo_set_source_rgba(cr, fg->red, fg->green, fg->blue, on ? 0.96 : 0.11);

        double sx_top = skew;
        double sx_mid = skew * 0.5;
        cairo_new_path(cr);
        switch (seg) {
            case 0: /* top */
                cairo_move_to(cr, x + g + sx_top, y);
                cairo_line_to(cr, x + w - g + sx_top, y);
                cairo_line_to(cr, x + w - g - t * 0.6 + sx_top, y + t);
                cairo_line_to(cr, x + g + t * 0.6 + sx_top, y + t);
                break;
            case 1: /* top-right */
                cairo_move_to(cr, x + w + sx_top, y + g);
                cairo_line_to(cr, x + w + sx_mid, ym - g * 0.6);
                cairo_line_to(cr, x + w - t + sx_mid, ym - g * 0.6 - t * 0.45);
                cairo_line_to(cr, x + w - t + sx_top, y + g + t * 0.6);
                break;
            case 2: /* bottom-right */
                cairo_move_to(cr, x + w + sx_mid, ym + g * 0.6);
                cairo_line_to(cr, x + w, y + h - g);
                cairo_line_to(cr, x + w - t, y + h - g - t * 0.6);
                cairo_line_to(cr, x + w - t + sx_mid, ym + g * 0.6 + t * 0.45);
                break;
            case 3: /* bottom */
                cairo_move_to(cr, x + g + t * 0.6, y + h - t);
                cairo_line_to(cr, x + w - g - t * 0.6, y + h - t);
                cairo_line_to(cr, x + w - g, y + h);
                cairo_line_to(cr, x + g, y + h);
                break;
            case 4: /* bottom-left */
                cairo_move_to(cr, x + sx_mid, ym + g * 0.6);
                cairo_line_to(cr, x + t + sx_mid, ym + g * 0.6 + t * 0.45);
                cairo_line_to(cr, x + t, y + h - g - t * 0.6);
                cairo_line_to(cr, x, y + h - g);
                break;
            case 5: /* top-left */
                cairo_move_to(cr, x + sx_top, y + g);
                cairo_line_to(cr, x + t + sx_top, y + g + t * 0.6);
                cairo_line_to(cr, x + t + sx_mid, ym - g * 0.6 - t * 0.45);
                cairo_line_to(cr, x + sx_mid, ym - g * 0.6);
                break;
            default: /* middle */
                cairo_move_to(cr, x + g + t * 0.45 + sx_mid, ym - t * 0.5);
                cairo_line_to(cr, x + w - g - t * 0.45 + sx_mid, ym - t * 0.5);
                cairo_line_to(cr, x + w - g + sx_mid, ym);
                cairo_line_to(cr, x + w - g - t * 0.45 + sx_mid, ym + t * 0.5);
                cairo_line_to(cr, x + g + t * 0.45 + sx_mid, ym + t * 0.5);
                cairo_line_to(cr, x + g + sx_mid, ym);
                break;
        }
        cairo_close_path(cr);
        cairo_fill(cr);
    }
}

static void draw_lcd_clock(ClockPlugin *c, cairo_t *cr, int width, int height, GDateTime *now) {
    GdkRGBA fg;
    get_time_fg_rgba(c, &fg);

    gboolean show_sec = format_has_seconds(c->time_format);
    gboolean is_12h   = format_is_12h(c->time_format);

    int h = g_date_time_get_hour(now);
    int m = g_date_time_get_minute(now);
    int s = g_date_time_get_second(now);
    if (is_12h) {
        h = h % 12;
        if (h == 0) h = 12;
    }

    int digits[6];
    int n_groups = show_sec ? 3 : 2;
    digits[0] = (is_12h && h < 10) ? 10 : (h / 10);
    digits[1] = h % 10;
    digits[2] = m / 10;
    digits[3] = m % 10;
    digits[4] = s / 10;
    digits[5] = s % 10;

    double pad = 2.0;
    double dh = MAX(10.0, height - pad * 2.0);
    double dw = dh * 0.56;
    double dgap = MAX(2.0, dw * 0.22);
    double cgap = MAX(4.0, dw * 0.45);
    double total_w = (n_groups * 2) * dw + (n_groups * 2 - 1) * dgap + (n_groups - 1) * (cgap - dgap);

    double cx = MAX(pad, (width - total_w) * 0.5);
    double cy = (height - dh) * 0.5;
    double dot_r = MAX(1.1, dh * 0.065);

    for (int g = 0; g < n_groups; g++) {
        draw_7seg_digit(cr, cx, cy, dw, dh, digits[g * 2], &fg);
        cx += dw + dgap;
        draw_7seg_digit(cr, cx, cy, dw, dh, digits[g * 2 + 1], &fg);
        cx += dw;

        if (g < n_groups - 1) {
            double col_x = cx + cgap * 0.52;
            cairo_set_source_rgba(cr, fg.red, fg.green, fg.blue, 0.90);
            cairo_arc(cr, col_x + dw * 0.05, cy + dh * 0.33, dot_r, 0, 2.0 * M_PI);
            cairo_fill(cr);
            cairo_arc(cr, col_x + dw * 0.03, cy + dh * 0.67, dot_r, 0, 2.0 * M_PI);
            cairo_fill(cr);
            cx += cgap;
        }
    }
}

/* ==================== Analog Watch Renderer ==================== */

static void draw_analog_clock(ClockPlugin *c, cairo_t *cr, int width, int height, GDateTime *now) {
    GdkRGBA fg, accent;
    get_time_fg_rgba(c, &fg);
    get_accent_rgba(c, &fg, &accent);

    double cx = width * 0.5;
    double cy = height * 0.5;
    double r = MIN(width, height) * 0.5 - 1.5;
    if (r < 6.0) return;

    /* Outer dial circle */
    cairo_set_source_rgba(cr, fg.red, fg.green, fg.blue, 0.10);
    cairo_arc(cr, cx, cy, r, 0, 2.0 * M_PI);
    cairo_fill(cr);

    cairo_set_line_width(cr, MAX(1.1, r * 0.08));
    cairo_set_source_rgba(cr, fg.red, fg.green, fg.blue, 0.78);
    cairo_arc(cr, cx, cy, r, 0, 2.0 * M_PI);
    cairo_stroke(cr);

    /* 12 hour markers */
    for (int i = 0; i < 12; i++) {
        double ang = i * (M_PI / 6.0) - M_PI * 0.5;
        gboolean cardinal = (i % 3) == 0;
        double r1 = r * (cardinal ? 0.74 : 0.83);
        double r2 = r * 0.92;
        cairo_set_line_width(cr, cardinal ? MAX(1.2, r * 0.09) : MAX(0.8, r * 0.05));
        cairo_set_source_rgba(cr, fg.red, fg.green, fg.blue, cardinal ? 0.85 : 0.45);
        cairo_move_to(cr, cx + cos(ang) * r1, cy + sin(ang) * r1);
        cairo_line_to(cr, cx + cos(ang) * r2, cy + sin(ang) * r2);
        cairo_stroke(cr);
    }

    int h = g_date_time_get_hour(now) % 12;
    int m = g_date_time_get_minute(now);
    int s = g_date_time_get_second(now);

    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);

    /* Hour hand */
    double h_ang = ((h + m / 60.0) / 12.0) * 2.0 * M_PI - M_PI * 0.5;
    cairo_set_line_width(cr, MAX(1.6, r * 0.13));
    cairo_set_source_rgba(cr, fg.red, fg.green, fg.blue, 0.96);
    cairo_move_to(cr, cx - cos(h_ang) * r * 0.12, cy - sin(h_ang) * r * 0.12);
    cairo_line_to(cr, cx + cos(h_ang) * r * 0.54, cy + sin(h_ang) * r * 0.54);
    cairo_stroke(cr);

    /* Minute hand */
    double m_ang = ((m + s / 60.0) / 60.0) * 2.0 * M_PI - M_PI * 0.5;
    cairo_set_line_width(cr, MAX(1.2, r * 0.09));
    cairo_set_source_rgba(cr, fg.red, fg.green, fg.blue, 0.92);
    cairo_move_to(cr, cx - cos(m_ang) * r * 0.15, cy - sin(m_ang) * r * 0.15);
    cairo_line_to(cr, cx + cos(m_ang) * r * 0.78, cy + sin(m_ang) * r * 0.78);
    cairo_stroke(cr);

    /* Second hand (when %S is enabled) */
    if (format_has_seconds(c->time_format)) {
        double s_ang = (s / 60.0) * 2.0 * M_PI - M_PI * 0.5;
        cairo_set_line_width(cr, MAX(0.9, r * 0.05));
        cairo_set_source_rgba(cr, accent.red, accent.green, accent.blue, 0.95);
        cairo_move_to(cr, cx - cos(s_ang) * r * 0.18, cy - sin(s_ang) * r * 0.18);
        cairo_line_to(cr, cx + cos(s_ang) * r * 0.84, cy + sin(s_ang) * r * 0.84);
        cairo_stroke(cr);
    }

    /* Center pivot */
    cairo_set_source_rgba(cr, fg.red, fg.green, fg.blue, 1.0);
    cairo_arc(cr, cx, cy, MAX(1.3, r * 0.10), 0, 2.0 * M_PI);
    cairo_fill(cr);
}

/* ==================== Binary (BCD) Matrix Renderer ==================== */

static void draw_binary_clock(ClockPlugin *c, cairo_t *cr, int width, int height, GDateTime *now) {
    GdkRGBA fg;
    get_time_fg_rgba(c, &fg);

    gboolean show_sec = format_has_seconds(c->time_format);
    gboolean is_12h   = format_is_12h(c->time_format);
    int h = g_date_time_get_hour(now);
    int m = g_date_time_get_minute(now);
    int s = g_date_time_get_second(now);
    if (is_12h) {
        h = h % 12;
        if (h == 0) h = 12;
    }

    int cols = show_sec ? 6 : 4;
    int vals[6]     = { h / 10, h % 10, m / 10, m % 10, s / 10, s % 10 };
    int max_bits[6] = { is_12h ? 1 : 2, 4, 3, 4, 3, 4 };

    double pad = 2.0;
    double usable_h = MAX(10.0, height - pad * 2.0);
    double cell = usable_h / 4.0;
    double r = MAX(1.3, cell * 0.36);

    double total_w = cols * cell + (cols / 2 - 1) * (cell * 0.35);
    double x = MAX(pad, (width - total_w) * 0.5);
    double y0 = (height - usable_h) * 0.5;

    for (int col = 0; col < cols; col++) {
        if (col > 0 && (col % 2) == 0)
            x += cell * 0.35;
        double cx = x + cell * 0.5;
        for (int bit = 0; bit < 4; bit++) {
            int row = 3 - bit; /* bit 0 at bottom */
            double cy = y0 + (row + 0.5) * cell;
            if (bit >= max_bits[col])
                continue;
            gboolean on = ((vals[col] >> bit) & 1) != 0;
            cairo_set_source_rgba(cr, fg.red, fg.green, fg.blue, on ? 0.96 : 0.16);
            cairo_arc(cr, cx, cy, r, 0, 2.0 * M_PI);
            cairo_fill(cr);
        }
        x += cell;
    }
}

static gboolean on_canvas_draw(GtkWidget *widget, cairo_t *cr, gpointer user_data) {
    ClockPlugin *c = user_data;
    int w = gtk_widget_get_allocated_width(widget);
    int h = gtk_widget_get_allocated_height(widget);
    g_autoptr(GDateTime) now = g_date_time_new_now(c->tz);
    if (!now) return FALSE;

    switch (c->style) {
        case STYLE_LCD:
            draw_lcd_clock(c, cr, w, h, now);
            break;
        case STYLE_ANALOG:
            draw_analog_clock(c, cr, w, h, now);
            break;
        case STYLE_BINARY:
            draw_binary_clock(c, cr, w, h, now);
            break;
        default:
            break;
    }
    return FALSE;
}

/* ==================== Config Load / Save ==================== */

static void load_config(ClockPlugin *c) {
    c->layout            = LAYOUT_STACK_TIME_DATE;
    c->style             = STYLE_DIGITAL;
    c->fuzzy_level       = FUZZY_5MIN;
    c->timezone_id       = g_strdup("");
    c->time_format       = g_strdup("%H:%M");
    c->date_format       = g_strdup("%a, %b %d");
    c->show_tooltip      = TRUE;
    c->tooltip_format    = g_strdup("%A, %d %B %Y — %T (%Z)");
    c->separator         = g_strdup("·");

    c->show_icon         = FALSE;
    c->time_size         = 12;
    c->time_bold         = TRUE;
    c->date_size         = 9;
    c->date_bold         = FALSE;
    c->spacing           = 1;

    c->time_color        = g_strdup("");
    c->date_color        = g_strdup("");
    c->sep_color         = g_strdup("#888888");
    c->bg_color          = g_strdup("");
    c->accent_color      = g_strdup("");

    g_autofree gchar *file = xfce_panel_plugin_lookup_rc_file(c->plugin);
    if (!file) return;
    XfceRc *rc = xfce_rc_simple_open(file, TRUE);
    if (!rc) return;

    c->layout            = CLAMP(xfce_rc_read_int_entry(rc, "layout", LAYOUT_STACK_TIME_DATE), 0, LAYOUT_COUNT - 1);
    c->style             = CLAMP(xfce_rc_read_int_entry(rc, "style", STYLE_DIGITAL), 0, STYLE_COUNT - 1);
    c->fuzzy_level       = CLAMP(xfce_rc_read_int_entry(rc, "fuzzy_level", FUZZY_5MIN), 0, FUZZY_COUNT - 1);

    g_free(c->timezone_id);    c->timezone_id    = g_strdup(xfce_rc_read_entry(rc, "timezone_id", ""));
    g_free(c->time_format);    c->time_format    = g_strdup(xfce_rc_read_entry(rc, "time_format", "%H:%M"));
    g_free(c->date_format);    c->date_format    = g_strdup(xfce_rc_read_entry(rc, "date_format", "%a, %b %d"));
    c->show_tooltip      = xfce_rc_read_bool_entry(rc, "show_tooltip", TRUE);
    g_free(c->tooltip_format); c->tooltip_format = g_strdup(xfce_rc_read_entry(rc, "tooltip_format", "%A, %d %B %Y — %T (%Z)"));
    g_free(c->separator);      c->separator      = g_strdup(xfce_rc_read_entry(rc, "separator", "·"));

    c->show_icon         = xfce_rc_read_bool_entry(rc, "show_icon", FALSE);
    c->time_size         = CLAMP(xfce_rc_read_int_entry(rc, "time_size", 12), 7, 36);
    c->time_bold         = xfce_rc_read_bool_entry(rc, "time_bold", TRUE);
    c->date_size         = CLAMP(xfce_rc_read_int_entry(rc, "date_size", 9), 6, 28);
    c->date_bold         = xfce_rc_read_bool_entry(rc, "date_bold", FALSE);
    c->spacing           = CLAMP(xfce_rc_read_int_entry(rc, "spacing", 1), 0, 20);

    g_free(c->time_color);     c->time_color     = g_strdup(xfce_rc_read_entry(rc, "time_color", ""));
    g_free(c->date_color);     c->date_color     = g_strdup(xfce_rc_read_entry(rc, "date_color", ""));
    g_free(c->sep_color);      c->sep_color      = g_strdup(xfce_rc_read_entry(rc, "sep_color", "#888888"));
    g_free(c->bg_color);       c->bg_color       = g_strdup(xfce_rc_read_entry(rc, "bg_color", ""));
    g_free(c->accent_color);   c->accent_color   = g_strdup(xfce_rc_read_entry(rc, "accent_color", ""));

    xfce_rc_close(rc);
}

static void save_config_now(ClockPlugin *c) {
    g_autofree gchar *file = xfce_panel_plugin_save_location(c->plugin, TRUE);
    if (!file) return;
    XfceRc *rc = xfce_rc_simple_open(file, FALSE);
    if (!rc) return;

    xfce_rc_write_int_entry(rc, "layout", c->layout);
    xfce_rc_write_int_entry(rc, "style", c->style);
    xfce_rc_write_int_entry(rc, "fuzzy_level", c->fuzzy_level);
    xfce_rc_write_entry(rc, "timezone_id", c->timezone_id ? c->timezone_id : "");
    xfce_rc_write_entry(rc, "time_format", c->time_format ? c->time_format : "");
    xfce_rc_write_entry(rc, "date_format", c->date_format ? c->date_format : "");
    xfce_rc_write_bool_entry(rc, "show_tooltip", c->show_tooltip);
    xfce_rc_write_entry(rc, "tooltip_format", c->tooltip_format ? c->tooltip_format : "");
    xfce_rc_write_entry(rc, "separator", c->separator ? c->separator : "");

    xfce_rc_write_bool_entry(rc, "show_icon", c->show_icon);
    xfce_rc_write_int_entry(rc, "time_size", c->time_size);
    xfce_rc_write_bool_entry(rc, "time_bold", c->time_bold);
    xfce_rc_write_int_entry(rc, "date_size", c->date_size);
    xfce_rc_write_bool_entry(rc, "date_bold", c->date_bold);
    xfce_rc_write_int_entry(rc, "spacing", c->spacing);

    xfce_rc_write_entry(rc, "time_color", c->time_color ? c->time_color : "");
    xfce_rc_write_entry(rc, "date_color", c->date_color ? c->date_color : "");
    xfce_rc_write_entry(rc, "sep_color", c->sep_color ? c->sep_color : "");
    xfce_rc_write_entry(rc, "bg_color", c->bg_color ? c->bg_color : "");
    xfce_rc_write_entry(rc, "accent_color", c->accent_color ? c->accent_color : "");

    xfce_rc_close(rc);
}

static gboolean on_save_timeout(gpointer user_data) {
    ClockPlugin *c = user_data;
    c->save_timeout_id = 0;
    save_config_now(c);
    return G_SOURCE_REMOVE;
}

void clock_schedule_save(ClockPlugin *c) {
    if (c->save_timeout_id) g_source_remove(c->save_timeout_id);
    c->save_timeout_id = g_timeout_add(300, on_save_timeout, c);
}

void clock_flush_save(ClockPlugin *c) {
    if (c->save_timeout_id) {
        g_source_remove(c->save_timeout_id);
        c->save_timeout_id = 0;
        save_config_now(c);
    }
}

/* ==================== CSS & UI Construction ==================== */

void clock_apply_css(ClockPlugin *c) {
    if (!c->css) {
        c->css = gtk_css_provider_new();
        gtk_style_context_add_provider_for_screen(
            gdk_screen_get_default(),
            GTK_STYLE_PROVIDER(c->css),
            GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    }

    const char *bg = (c->bg_color && *c->bg_color) ? c->bg_color : NULL;
    const char *nixie_fg = (c->time_color && *c->time_color) ? c->time_color : "#ff7b1c";

    GString *s = g_string_sized_new(640);
    g_string_append_printf(s,
        ".xclk-box { padding: 2px 7px; border-radius: 5px; transition: background-color 120ms ease; background-color: %s; }\n"
        ".xclk-box:hover { background-color: alpha(currentColor, 0.08); }\n",
        bg ? bg : "transparent");

    g_string_append_printf(s,
        ".xclk-time { font-size: %dpx; font-weight: %s; font-variant-numeric: tabular-nums; line-height: 1.05; ",
        c->time_size, c->time_bold ? "bold" : "normal");
    if (c->time_color && *c->time_color)
        g_string_append_printf(s, "color: %s; ", c->time_color);
    g_string_append(s, "}\n");

    g_string_append_printf(s,
        ".xclk-fuzzy { font-size: %dpx; font-weight: %s; font-style: italic; line-height: 1.08; ",
        c->time_size, c->time_bold ? "bold" : "normal");
    if (c->time_color && *c->time_color)
        g_string_append_printf(s, "color: %s; ", c->time_color);
    g_string_append(s, "}\n");

    g_string_append_printf(s,
        ".xclk-nixie-digit { font-family: monospace; font-size: %dpx; font-weight: bold; "
        "color: %s; background-color: rgba(18, 12, 8, 0.88); "
        "border: 1px solid alpha(%s, 0.45); border-radius: 4px; "
        "padding: 1px 4px; text-shadow: 0 0 6px %s; line-height: 1.0; }\n"
        ".xclk-nixie-colon { font-family: monospace; font-size: %dpx; font-weight: bold; "
        "color: %s; text-shadow: 0 0 5px %s; padding: 0 1px; line-height: 1.0; }\n",
        c->time_size, nixie_fg, nixie_fg, nixie_fg,
        c->time_size, nixie_fg, nixie_fg);

    g_string_append_printf(s,
        ".xclk-date { font-size: %dpx; font-weight: %s; font-variant-numeric: tabular-nums; line-height: 1.05; ",
        c->date_size, c->date_bold ? "bold" : "normal");
    if (c->date_color && *c->date_color)
        g_string_append_printf(s, "color: %s; ", c->date_color);
    else
        g_string_append(s, "opacity: 0.82; ");
    g_string_append(s, "}\n");

    g_string_append_printf(s,
        ".xclk-sep { font-size: %dpx; font-weight: 600; margin: 0 %dpx; ",
        MAX(c->time_size, c->date_size), MAX(2, c->spacing + 2));
    if (c->sep_color && *c->sep_color)
        g_string_append_printf(s, "color: %s; ", c->sep_color);
    else
        g_string_append(s, "opacity: 0.65; ");
    g_string_append(s, "}\n");

    g_string_append(s,
        ".xclk-popup-frame { padding: 8px; border: 1px solid alpha(currentColor, 0.22); border-radius: 6px; }\n");

    gtk_css_provider_load_from_data(c->css, s->str, (gssize)s->len, NULL);
    g_string_free(s, TRUE);
}

static GtkWidget *make_clock_icon(ClockPlugin *c) {
    GdkRGBA fg;
    get_time_fg_rgba(c, &fg);

    char hex[8];
    g_snprintf(hex, sizeof(hex), "#%02x%02x%02x",
        (int)(fg.red * 255.0 + 0.5),
        (int)(fg.green * 255.0 + 0.5),
        (int)(fg.blue * 255.0 + 0.5));

    g_autofree gchar *tinted = g_strdup(BUILTIN_CLOCK_SVG);
    char *pos = strstr(tinted, "#2e3436");
    if (pos) memcpy(pos, hex, 7);

    int sz = CLAMP(c->time_size + 2, 12, 22);
    g_autoptr(GInputStream) stream = g_memory_input_stream_new_from_data(tinted, -1, NULL);
    g_autoptr(GdkPixbuf) pb = gdk_pixbuf_new_from_stream_at_scale(stream, sz, sz, TRUE, NULL, NULL);
    return pb ? gtk_image_new_from_pixbuf(pb) : gtk_image_new();
}

static GtkWidget *make_label(const char *css_class, gboolean stacked) {
    GtkWidget *lbl = gtk_label_new("");
    gtk_label_set_xalign(GTK_LABEL(lbl), 0.5f);
    gtk_widget_set_valign(lbl, GTK_ALIGN_CENTER);
    if (stacked)
        gtk_widget_set_halign(lbl, GTK_ALIGN_CENTER);
    gtk_style_context_add_class(gtk_widget_get_style_context(lbl), css_class);
    return lbl;
}

static GtkWidget *create_time_widget(ClockPlugin *c, gboolean stacked) {
    if (clock_style_uses_canvas(c->style)) {
        GtkWidget *da = gtk_drawing_area_new();
        gboolean show_sec = format_has_seconds(c->time_format);
        int h = MAX(14, c->time_size + 5);
        int w = h;

        if (c->style == STYLE_LCD) {
            int n_digits = show_sec ? 6 : 4;
            int n_colons = show_sec ? 2 : 1;
            w = (int)((h - 4) * 0.58 * n_digits + (h - 4) * 0.28 * n_colons + 8);
        } else if (c->style == STYLE_ANALOG) {
            h = MAX(18, c->time_size + 10);
            w = h;
        } else if (c->style == STYLE_BINARY) {
            int cols = show_sec ? 6 : 4;
            int cell = MAX(4, (h - 4) / 4);
            h = cell * 4 + 4;
            w = cell * cols + (cols / 2 - 1) * (cell / 2) + 4;
        }

        gtk_widget_set_size_request(da, w, h);
        gtk_widget_set_valign(da, GTK_ALIGN_CENTER);
        if (stacked)
            gtk_widget_set_halign(da, GTK_ALIGN_CENTER);
        g_signal_connect(da, "draw", G_CALLBACK(on_canvas_draw), c);
        c->time_area = da;
        return da;
    }

    if (c->style == STYLE_NIXIE) {
        GtkWidget *hb = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
        gtk_widget_set_valign(hb, GTK_ALIGN_CENTER);
        if (stacked)
            gtk_widget_set_halign(hb, GTK_ALIGN_CENTER);
        c->nixie_box = hb;
        return hb;
    }

    c->time_label = make_label(c->style == STYLE_FUZZY ? "xclk-fuzzy" : "xclk-time", stacked);
    return c->time_label;
}

static void rebuild_nixie_tubes(ClockPlugin *c, const char *str) {
    if (!c->nixie_box) return;
    GList *children = gtk_container_get_children(GTK_CONTAINER(c->nixie_box));
    for (GList *l = children; l != NULL; l = l->next)
        gtk_widget_destroy(GTK_WIDGET(l->data));
    g_list_free(children);

    for (const char *p = str; *p; p++) {
        char ch[2] = { *p, '\0' };
        if (*p == ' ') {
            GtkWidget *sp = gtk_label_new(" ");
            gtk_box_pack_start(GTK_BOX(c->nixie_box), sp, FALSE, FALSE, 1);
        } else if (*p == ':') {
            GtkWidget *lbl = gtk_label_new(":");
            gtk_style_context_add_class(gtk_widget_get_style_context(lbl), "xclk-nixie-colon");
            gtk_box_pack_start(GTK_BOX(c->nixie_box), lbl, FALSE, FALSE, 0);
        } else {
            GtkWidget *lbl = gtk_label_new(ch);
            gtk_style_context_add_class(gtk_widget_get_style_context(lbl), "xclk-nixie-digit");
            gtk_box_pack_start(GTK_BOX(c->nixie_box), lbl, FALSE, FALSE, 0);
        }
    }
    gtk_widget_show_all(c->nixie_box);
}

void clock_rebuild_ui(ClockPlugin *c) {
    if (c->event_box)
        gtk_widget_set_has_tooltip(c->event_box, c->show_tooltip);

    GList *children = gtk_container_get_children(GTK_CONTAINER(c->root_box));
    for (GList *l = children; l != NULL; l = l->next)
        gtk_widget_destroy(GTK_WIDGET(l->data));
    g_list_free(children);

    c->time_label = NULL;
    c->time_area  = NULL;
    c->nixie_box  = NULL;
    c->date_label = NULL;
    c->sep_label  = NULL;
    c->disp_time[0] = '\0';
    c->disp_date[0] = '\0';

    if (c->show_icon) {
        GtkWidget *img = make_clock_icon(c);
        gtk_box_pack_start(GTK_BOX(c->root_box), img, FALSE, FALSE, 0);
    }

    gboolean stacked = clock_is_stacked(c->layout);
    GtkWidget *content_box = gtk_box_new(
        stacked ? GTK_ORIENTATION_VERTICAL : GTK_ORIENTATION_HORIZONTAL,
        stacked ? c->spacing : 0);
    gtk_widget_set_valign(content_box, GTK_ALIGN_CENTER);

    GtkWidget *time_w = clock_has_time(c->layout) ? create_time_widget(c, stacked) : NULL;
    if (clock_has_date(c->layout))
        c->date_label = make_label("xclk-date", stacked);

    switch (c->layout) {
        case LAYOUT_TIME_ONLY:
            gtk_box_pack_start(GTK_BOX(content_box), time_w, FALSE, FALSE, 0);
            break;
        case LAYOUT_DATE_ONLY:
            gtk_box_pack_start(GTK_BOX(content_box), c->date_label, FALSE, FALSE, 0);
            break;
        case LAYOUT_STACK_TIME_DATE:
            gtk_box_pack_start(GTK_BOX(content_box), time_w, FALSE, FALSE, 0);
            gtk_box_pack_start(GTK_BOX(content_box), c->date_label, FALSE, FALSE, 0);
            break;
        case LAYOUT_STACK_DATE_TIME:
            gtk_box_pack_start(GTK_BOX(content_box), c->date_label, FALSE, FALSE, 0);
            gtk_box_pack_start(GTK_BOX(content_box), time_w, FALSE, FALSE, 0);
            break;
        case LAYOUT_INLINE_TIME_DATE:
            gtk_box_pack_start(GTK_BOX(content_box), time_w, FALSE, FALSE, 0);
            if (c->separator && *c->separator) {
                c->sep_label = make_label("xclk-sep", FALSE);
                gtk_label_set_text(GTK_LABEL(c->sep_label), c->separator);
                gtk_box_pack_start(GTK_BOX(content_box), c->sep_label, FALSE, FALSE, 0);
            }
            gtk_box_pack_start(GTK_BOX(content_box), c->date_label, FALSE, FALSE, 0);
            break;
        case LAYOUT_INLINE_DATE_TIME:
            gtk_box_pack_start(GTK_BOX(content_box), c->date_label, FALSE, FALSE, 0);
            if (c->separator && *c->separator) {
                c->sep_label = make_label("xclk-sep", FALSE);
                gtk_label_set_text(GTK_LABEL(c->sep_label), c->separator);
                gtk_box_pack_start(GTK_BOX(content_box), c->sep_label, FALSE, FALSE, 0);
            }
            gtk_box_pack_start(GTK_BOX(content_box), time_w, FALSE, FALSE, 0);
            break;
        default:
            break;
    }

    gtk_box_pack_start(GTK_BOX(c->root_box), content_box, FALSE, FALSE, 0);
    gtk_widget_show_all(c->root_box);
    clock_update_now(c);
}

void clock_update_now(ClockPlugin *c) {
    g_autoptr(GDateTime) now = g_date_time_new_now(c->tz);
    if (!now) return;

    if (c->time_area) {
        gtk_widget_queue_draw(c->time_area);
    } else if (c->nixie_box) {
        g_autofree gchar *t = g_date_time_format(now, (c->time_format && *c->time_format) ? c->time_format : "%H:%M");
        const char *str = t ? t : "00:00";
        if (strcmp(str, c->disp_time) != 0) {
            g_strlcpy(c->disp_time, str, sizeof(c->disp_time));
            rebuild_nixie_tubes(c, str);
        }
    } else if (c->time_label) {
        char fuzzy_buf[96];
        g_autofree gchar *t = NULL;
        const char *str = "";
        if (c->style == STYLE_FUZZY) {
            format_fuzzy_time(now, c->fuzzy_level, fuzzy_buf, sizeof(fuzzy_buf));
            str = fuzzy_buf;
        } else {
            t = g_date_time_format(now, (c->time_format && *c->time_format) ? c->time_format : "%H:%M");
            str = t ? t : "ERR";
        }
        if (strcmp(str, c->disp_time) != 0) {
            g_strlcpy(c->disp_time, str, sizeof(c->disp_time));
            gtk_label_set_text(GTK_LABEL(c->time_label), str);
        }
    }

    if (c->date_label) {
        g_autofree gchar *d = g_date_time_format(now, (c->date_format && *c->date_format) ? c->date_format : "%a, %b %d");
        const char *str = d ? d : "ERR";
        if (strcmp(str, c->disp_date) != 0) {
            g_strlcpy(c->disp_date, str, sizeof(c->disp_date));
            gtk_label_set_text(GTK_LABEL(c->date_label), str);
        }
    }
}

static gboolean on_timer_tick(gpointer user_data) {
    ClockPlugin *c = user_data;
    clock_update_now(c);
    clock_restart_timer(c);
    return G_SOURCE_REMOVE;
}

void clock_restart_timer(ClockPlugin *c) {
    if (c->timer_id) {
        g_source_remove(c->timer_id);
        c->timer_id = 0;
    }
    gint64 real_us = g_get_real_time();
    guint rem_ms = (guint)((1000000 - (real_us % 1000000)) / 1000) + 5;
    c->timer_id = g_timeout_add(rem_ms, on_timer_tick, c);
}

static void sync_calendar_today(ClockPlugin *c) {
    if (!c->calendar) return;
    g_autoptr(GDateTime) now = g_date_time_new_now(c->tz);
    if (!now) return;

    GtkCalendarDisplayOptions opts = GTK_CALENDAR_SHOW_HEADING | GTK_CALENDAR_SHOW_DAY_NAMES;
    gtk_calendar_set_display_options(GTK_CALENDAR(c->calendar), opts);
    gtk_calendar_select_month(GTK_CALENDAR(c->calendar),
        (guint)(g_date_time_get_month(now) - 1),
        (guint)g_date_time_get_year(now));
    gtk_calendar_select_day(GTK_CALENDAR(c->calendar),
        (guint)g_date_time_get_day_of_month(now));
}

static void hide_calendar_popup(ClockPlugin *c) {
    if (c->popup_win && gtk_widget_get_visible(c->popup_win)) {
        gtk_widget_hide(c->popup_win);
        c->popup_closed_us = g_get_monotonic_time();
        xfce_panel_plugin_block_autohide(c->plugin, FALSE);
    }
}

static void on_today_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    sync_calendar_today((ClockPlugin *)user_data);
}

static gboolean on_popup_focus_out(GtkWidget *w, GdkEventFocus *ev, gpointer user_data) {
    (void)w; (void)ev;
    hide_calendar_popup((ClockPlugin *)user_data);
    return FALSE;
}

static gboolean on_popup_key_press(GtkWidget *w, GdkEventKey *ev, gpointer user_data) {
    (void)w;
    if (ev->keyval == GDK_KEY_Escape) {
        hide_calendar_popup((ClockPlugin *)user_data);
        return TRUE;
    }
    return FALSE;
}

static void toggle_calendar_popup(ClockPlugin *c) {
    if (!c->popup_win) {
        c->popup_win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        gtk_window_set_decorated(GTK_WINDOW(c->popup_win), FALSE);
        gtk_window_set_resizable(GTK_WINDOW(c->popup_win), FALSE);
        gtk_window_set_skip_taskbar_hint(GTK_WINDOW(c->popup_win), TRUE);
        gtk_window_set_skip_pager_hint(GTK_WINDOW(c->popup_win), TRUE);
        gtk_window_set_type_hint(GTK_WINDOW(c->popup_win), GDK_WINDOW_TYPE_HINT_POPUP_MENU);
        gtk_window_set_keep_above(GTK_WINDOW(c->popup_win), TRUE);
        gtk_window_stick(GTK_WINDOW(c->popup_win));

        GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
        gtk_style_context_add_class(gtk_widget_get_style_context(vbox), "xclk-popup-frame");

        c->calendar = gtk_calendar_new();
        gtk_box_pack_start(GTK_BOX(vbox), c->calendar, FALSE, FALSE, 0);

        GtkWidget *btn_today = gtk_button_new_with_label("Today");
        g_signal_connect(btn_today, "clicked", G_CALLBACK(on_today_clicked), c);
        gtk_box_pack_start(GTK_BOX(vbox), btn_today, FALSE, FALSE, 0);

        gtk_container_add(GTK_CONTAINER(c->popup_win), vbox);
        gtk_widget_show_all(vbox);

        g_signal_connect(c->popup_win, "delete-event", G_CALLBACK(gtk_widget_hide_on_delete), NULL);
        g_signal_connect(c->popup_win, "focus-out-event", G_CALLBACK(on_popup_focus_out), c);
        g_signal_connect(c->popup_win, "key-press-event", G_CALLBACK(on_popup_key_press), c);
    }

    if (gtk_widget_get_visible(c->popup_win)) {
        hide_calendar_popup(c);
        return;
    }

    if (g_get_monotonic_time() - c->popup_closed_us < 180000)
        return;

    sync_calendar_today(c);
    xfce_panel_plugin_block_autohide(c->plugin, TRUE);
    xfce_panel_plugin_popup_window(c->plugin, GTK_WINDOW(c->popup_win), c->event_box);
}

static gboolean on_button_press(GtkWidget *w, GdkEventButton *ev, gpointer user_data) {
    (void)w;
    ClockPlugin *c = user_data;
    if (ev->type == GDK_BUTTON_PRESS && ev->button == 1) {
        toggle_calendar_popup(c);
        return TRUE;
    }
    return FALSE;
}

static gboolean on_query_tooltip(GtkWidget *w, gint x, gint y, gboolean kb, GtkTooltip *tip, gpointer user_data) {
    (void)w; (void)x; (void)y; (void)kb;
    ClockPlugin *c = user_data;
    if (!c->show_tooltip) return FALSE;
    g_autoptr(GDateTime) now = g_date_time_new_now(c->tz);
    if (!now) return FALSE;

    const char *fmt = (c->tooltip_format && *c->tooltip_format) ? c->tooltip_format : "%A, %d %B %Y — %T (%Z)";
    g_autofree gchar *formatted = g_date_time_format(now, fmt);
    const char *tz_abbr = g_date_time_get_timezone_abbreviation(now);
    gint week = g_date_time_get_week_of_year(now);

    g_autofree gchar *txt = g_strdup_printf("%s\nWeek %d  •  %s",
        formatted ? formatted : "",
        week,
        (c->timezone_id && *c->timezone_id && c->tz_valid) ? c->timezone_id : (tz_abbr ? tz_abbr : "Local"));
    gtk_tooltip_set_text(tip, txt);
    return TRUE;
}

static void on_style_updated(GtkWidget *w, gpointer user_data) {
    (void)w;
    ClockPlugin *c = user_data;
    if (c->show_icon && !(c->time_color && *c->time_color))
        clock_rebuild_ui(c);
    else if (c->time_area)
        gtk_widget_queue_draw(c->time_area);
}

static gboolean on_size_changed(XfcePanelPlugin *plugin, gint size, gpointer user_data) {
    (void)user_data;
    if (xfce_panel_plugin_get_orientation(plugin) == GTK_ORIENTATION_HORIZONTAL)
        gtk_widget_set_size_request(GTK_WIDGET(plugin), -1, size);
    else
        gtk_widget_set_size_request(GTK_WIDGET(plugin), size, -1);
    return TRUE;
}

static void on_free_data(XfcePanelPlugin *plugin, gpointer user_data) {
    (void)plugin;
    ClockPlugin *c = user_data;
    if (c->prefs_dlg) gtk_widget_destroy(c->prefs_dlg);
    if (c->popup_win) gtk_widget_destroy(c->popup_win);
    if (c->timer_id) g_source_remove(c->timer_id);
    clock_flush_save(c);

    if (c->css) {
        gtk_style_context_remove_provider_for_screen(
            gdk_screen_get_default(), GTK_STYLE_PROVIDER(c->css));
        g_object_unref(c->css);
    }
    if (c->tz) g_time_zone_unref(c->tz);

    g_free(c->timezone_id);
    g_free(c->time_format);
    g_free(c->date_format);
    g_free(c->tooltip_format);
    g_free(c->separator);
    g_free(c->time_color);
    g_free(c->date_color);
    g_free(c->sep_color);
    g_free(c->bg_color);
    g_free(c->accent_color);
    g_free(c);
}

static void clock_construct(XfcePanelPlugin *plugin) {
    ClockPlugin *c = g_new0(ClockPlugin, 1);
    c->plugin = plugin;

    load_config(c);
    clock_reload_timezone(c);
    clock_apply_css(c);

    c->event_box = gtk_event_box_new();
    gtk_event_box_set_visible_window(GTK_EVENT_BOX(c->event_box), FALSE);
    gtk_widget_set_has_tooltip(c->event_box, c->show_tooltip);

    c->root_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_set_valign(c->root_box, GTK_ALIGN_CENTER);
    gtk_style_context_add_class(gtk_widget_get_style_context(c->root_box), "xclk-box");

    gtk_container_add(GTK_CONTAINER(c->event_box), c->root_box);
    gtk_container_add(GTK_CONTAINER(plugin), c->event_box);
    xfce_panel_plugin_add_action_widget(plugin, c->event_box);

    xfce_panel_plugin_menu_show_configure(plugin);
    g_signal_connect(plugin, "configure-plugin", G_CALLBACK(prefs_open_dialog), c);
    g_signal_connect(plugin, "style-updated", G_CALLBACK(on_style_updated), c);
    g_signal_connect(c->event_box, "button-press-event", G_CALLBACK(on_button_press), c);
    g_signal_connect(c->event_box, "query-tooltip", G_CALLBACK(on_query_tooltip), c);
    g_signal_connect(plugin, "size-changed", G_CALLBACK(on_size_changed), c);
    g_signal_connect(plugin, "free-data", G_CALLBACK(on_free_data), c);

    clock_rebuild_ui(c);
    gtk_widget_show_all(c->event_box);
    clock_restart_timer(c);
}

XFCE_PANEL_PLUGIN_REGISTER(clock_construct)
