// SPDX-License-Identifier: MIT
#ifndef XFCE4_CLOCK_H
#define XFCE4_CLOCK_H

#include <gtk/gtk.h>
#include <libxfce4panel/libxfce4panel.h>

typedef enum {
    LAYOUT_TIME_ONLY = 0,
    LAYOUT_DATE_ONLY,
    LAYOUT_STACK_TIME_DATE,
    LAYOUT_STACK_DATE_TIME,
    LAYOUT_INLINE_TIME_DATE,
    LAYOUT_INLINE_DATE_TIME,
    LAYOUT_COUNT
} LayoutMode;

typedef enum {
    STYLE_MINIMAL = 0,
    STYLE_PILL,
    STYLE_ACCENT,
    STYLE_OUTLINE,
    STYLE_MONOSPACE,
    STYLE_COUNT
} StylePreset;

typedef enum {
    WEIGHT_NORMAL = 0,
    WEIGHT_MEDIUM,
    WEIGHT_SEMIBOLD,
    WEIGHT_BOLD,
    WEIGHT_COUNT
} FontWeight;

typedef struct {
    XfcePanelPlugin *plugin;
    GtkCssProvider  *css;
    GtkWidget       *prefs_dlg;
    GtkWidget       *popup_win;
    GtkWidget       *calendar;
    gint64           popup_closed_us;

    /* Configuration */
    LayoutMode   layout;
    StylePreset  style;
    gchar       *timezone_id;     /* empty = local system timezone */
    gchar       *time_format;     /* strftime format */
    gchar       *date_format;     /* strftime format */
    gboolean     show_tooltip;
    gchar       *tooltip_format;  /* strftime format */
    gchar       *separator;       /* inline separator, default "·" */
    gchar       *click_cmd;       /* empty = built-in calendar popup */

    gboolean     show_icon;
    gchar       *icon_name;
    gint         icon_size;       /* 8..32, default 14 */

    gint         time_size;       /* 7..36, default 11 */
    FontWeight   time_weight;
    gchar       *time_color;

    gint         date_size;       /* 6..28, default 9 */
    FontWeight   date_weight;
    gchar       *date_color;

    gchar       *sep_color;
    gchar       *bg_color;        /* optional override */
    gchar       *accent_color;    /* optional border/accent override */
    gint         spacing;         /* 0..20, default 1 */
    gint         pad_x;           /* 0..24, default 8 */
    gint         pad_y;           /* 0..16, default 2 */
    gint         radius;          /* 0..24, default 6 */


    /* Runtime state */
    GTimeZone   *tz;
    gboolean     tz_valid;

    GtkWidget   *event_box;
    GtkWidget   *root_box;
    GtkWidget   *time_label;
    GtkWidget   *date_label;
    GtkWidget   *sep_label;

    guint        timer_id;
    guint        save_timeout_id;

    gchar        disp_time[128];
    gchar        disp_date[128];
} ClockPlugin;

extern const char *const LAYOUT_NAMES[LAYOUT_COUNT];
extern const char *const STYLE_NAMES[STYLE_COUNT];
extern const char *const WEIGHT_NAMES[WEIGHT_COUNT];
extern const int         WEIGHT_VALUES[WEIGHT_COUNT];

/* Core lifecycle & rendering (clock.c) */
void     clock_reload_timezone(ClockPlugin *c);
gboolean clock_has_time(LayoutMode mode);
gboolean clock_has_date(LayoutMode mode);
gboolean clock_is_stacked(LayoutMode mode);
gboolean clock_is_inline(LayoutMode mode);
void     clock_apply_css(ClockPlugin *c);
void     clock_rebuild_ui(ClockPlugin *c);
void     clock_update_now(ClockPlugin *c);
void     clock_restart_timer(ClockPlugin *c);
void     clock_schedule_save(ClockPlugin *c);
void     clock_flush_save(ClockPlugin *c);
gchar   *clock_format_preview(ClockPlugin *c, const char *fmt);

/* Preferences dialog (prefs.c) */
void     prefs_open_dialog(XfcePanelPlugin *plugin, gpointer user_data);

#endif
