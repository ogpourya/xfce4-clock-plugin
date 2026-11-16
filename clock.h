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
    STYLE_DIGITAL = 0,
    STYLE_LCD,
    STYLE_FUZZY,
    STYLE_ANALOG,
    STYLE_BINARY,
    STYLE_NIXIE,
    STYLE_DOTMATRIX,
    STYLE_FLIP,
    STYLE_MORSE,
    STYLE_ROMAN,
    STYLE_NEON,
    STYLE_COUNT
} StylePreset;

typedef enum {
    FUZZY_5MIN = 0,
    FUZZY_15MIN,
    FUZZY_DAYPART,
    FUZZY_COUNT
} FuzzyLevel;

typedef struct {
    XfcePanelPlugin *plugin;
    GtkCssProvider  *css;
    GtkWidget       *prefs_dlg;
    GtkWidget       *popup_win;
    GtkWidget       *calendar;
    gint64           popup_closed_us;

    /* Configuration */
    LayoutMode   layout;
    StylePreset  time_style;
    StylePreset  date_style;
    StylePreset  sep_style;
    FuzzyLevel   fuzzy_level;
    gchar       *timezone_id;     /* empty = local system timezone */
    gchar       *time_format;     /* strftime format */
    gchar       *date_format;     /* strftime format */
    gboolean     show_tooltip;
    gchar       *tooltip_format;  /* strftime format */
    gchar       *separator;       /* inline separator, default "·" */

    gboolean     show_icon;
    gint         time_size;       /* 7..36, default 12 */
    gboolean     time_bold;
    gint         date_size;       /* 6..28, default 9 */
    gboolean     date_bold;
    gint         spacing;         /* 0..20, default 1 */

    gchar       *time_color;
    gchar       *date_color;
    gchar       *sep_color;
    gchar       *bg_color;
    gchar       *accent_color;

    /* Runtime state */
    GTimeZone   *tz;
    gboolean     tz_valid;

    GtkWidget   *event_box;
    GtkWidget   *root_box;
    GtkWidget   *time_label;
    GtkWidget   *time_area;
    GtkWidget   *time_tiles;
    GtkWidget   *date_label;
    GtkWidget   *date_tiles;
    GtkWidget   *sep_label;

    guint        timer_id;
    guint        save_timeout_id;

    gchar        disp_time[128];
    gchar        disp_date[128];
} ClockPlugin;

extern const char *const LAYOUT_NAMES[LAYOUT_COUNT];
extern const char *const STYLE_NAMES[STYLE_COUNT];
extern const char *const FUZZY_NAMES[FUZZY_COUNT];

/* Core lifecycle & rendering (clock.c) */
void     clock_set_defaults(ClockPlugin *c);
void     clock_reset_defaults(ClockPlugin *c);
void     clock_reload_timezone(ClockPlugin *c);
gboolean clock_has_time(LayoutMode mode);
gboolean clock_has_date(LayoutMode mode);
gboolean clock_is_stacked(LayoutMode mode);
gboolean clock_is_inline(LayoutMode mode);
gboolean clock_style_uses_canvas(StylePreset style);
gboolean clock_style_uses_tiles(StylePreset style);
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
