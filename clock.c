// SPDX-License-Identifier: MIT
#include "clock.h"
#include <libxfce4util/libxfce4util.h>
#include <string.h>

const char *const LAYOUT_NAMES[LAYOUT_COUNT] = {
    [LAYOUT_TIME_ONLY]        = "Time Only",
    [LAYOUT_DATE_ONLY]        = "Date Only",
    [LAYOUT_STACK_TIME_DATE]  = "Stacked (Time / Date)",
    [LAYOUT_STACK_DATE_TIME]  = "Stacked (Date / Time)",
    [LAYOUT_INLINE_TIME_DATE] = "Inline (Time · Date)",
    [LAYOUT_INLINE_DATE_TIME] = "Inline (Date · Time)",
};

const char *const STYLE_NAMES[STYLE_COUNT] = {
    [STYLE_MINIMAL]   = "Minimal",
    [STYLE_PILL]      = "Pill Badge",
    [STYLE_ACCENT]    = "Accent Split",
    [STYLE_OUTLINE]   = "Card Outline",
    [STYLE_MONOSPACE] = "Monospace",
};

const char *const WEIGHT_NAMES[WEIGHT_COUNT] = {
    [WEIGHT_NORMAL]   = "Normal",
    [WEIGHT_MEDIUM]   = "Medium",
    [WEIGHT_SEMIBOLD] = "Semi-Bold",
    [WEIGHT_BOLD]     = "Bold",
};

const int WEIGHT_VALUES[WEIGHT_COUNT] = {
    [WEIGHT_NORMAL]   = 400,
    [WEIGHT_MEDIUM]   = 500,
    [WEIGHT_SEMIBOLD] = 600,
    [WEIGHT_BOLD]     = 700,
};

static const char *const BUILTIN_CLOCK_SVG =
    "<svg xmlns='http://www.w3.org/2000/svg' width='16' height='16' viewBox='0 0 16 16'>"
    "<path fill='#2e3436' d='M8 1a7 7 0 1 0 0 14A7 7 0 0 0 8 1zm0 1.6a5.4 5.4 0 1 1 0 10.8A5.4 5.4 0 0 1 8 2.6zM7.2 4v4.4l3.3 2 .8-1.3-2.5-1.5V4H7.2z'/>"
    "</svg>";

gboolean clock_has_time(LayoutMode mode)   { return mode != LAYOUT_DATE_ONLY; }
gboolean clock_has_date(LayoutMode mode)   { return mode != LAYOUT_TIME_ONLY; }
gboolean clock_is_stacked(LayoutMode mode) { return mode == LAYOUT_STACK_TIME_DATE || mode == LAYOUT_STACK_DATE_TIME; }
gboolean clock_is_inline(LayoutMode mode)  { return mode == LAYOUT_INLINE_TIME_DATE || mode == LAYOUT_INLINE_DATE_TIME; }

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

static void load_config(ClockPlugin *c) {
    c->layout            = LAYOUT_STACK_TIME_DATE;
    c->style             = STYLE_MINIMAL;
    c->timezone_id       = g_strdup("");
    c->time_format       = g_strdup("%H:%M");
    c->date_format       = g_strdup("%a, %b %d");
    c->tooltip_format    = g_strdup("%A, %d %B %Y — %T (%Z)");
    c->separator         = g_strdup("·");
    c->click_cmd         = g_strdup("");

    c->show_icon         = FALSE;
    c->icon_name         = g_strdup("builtin-clock");
    c->icon_size         = 14;

    c->time_size         = 11;
    c->time_weight       = WEIGHT_BOLD;
    c->time_color        = g_strdup("");

    c->date_size         = 9;
    c->date_weight       = WEIGHT_MEDIUM;
    c->date_color        = g_strdup("");

    c->sep_color         = g_strdup("#888888");
    c->bg_color          = g_strdup("");
    c->accent_color      = g_strdup("");
    c->spacing           = 1;
    c->pad_x             = 8;
    c->pad_y             = 2;
    c->radius            = 6;
    c->show_week_numbers = TRUE;

    g_autofree gchar *file = xfce_panel_plugin_lookup_rc_file(c->plugin);
    if (!file) return;
    XfceRc *rc = xfce_rc_simple_open(file, TRUE);
    if (!rc) return;

    c->layout            = CLAMP(xfce_rc_read_int_entry(rc, "layout", LAYOUT_STACK_TIME_DATE), 0, LAYOUT_COUNT - 1);
    c->style             = CLAMP(xfce_rc_read_int_entry(rc, "style", STYLE_MINIMAL), 0, STYLE_COUNT - 1);

    g_free(c->timezone_id);    c->timezone_id    = g_strdup(xfce_rc_read_entry(rc, "timezone_id", ""));
    g_free(c->time_format);    c->time_format    = g_strdup(xfce_rc_read_entry(rc, "time_format", "%H:%M"));
    g_free(c->date_format);    c->date_format    = g_strdup(xfce_rc_read_entry(rc, "date_format", "%a, %b %d"));
    g_free(c->tooltip_format); c->tooltip_format = g_strdup(xfce_rc_read_entry(rc, "tooltip_format", "%A, %d %B %Y — %T (%Z)"));
    g_free(c->separator);      c->separator      = g_strdup(xfce_rc_read_entry(rc, "separator", "·"));
    g_free(c->click_cmd);      c->click_cmd      = g_strdup(xfce_rc_read_entry(rc, "click_cmd", ""));

    c->show_icon         = xfce_rc_read_bool_entry(rc, "show_icon", FALSE);
    g_free(c->icon_name);      c->icon_name      = g_strdup(xfce_rc_read_entry(rc, "icon_name", "builtin-clock"));
    c->icon_size         = CLAMP(xfce_rc_read_int_entry(rc, "icon_size", 14), 8, 32);

    c->time_size         = CLAMP(xfce_rc_read_int_entry(rc, "time_size", 11), 7, 36);
    c->time_weight       = CLAMP(xfce_rc_read_int_entry(rc, "time_weight", WEIGHT_BOLD), 0, WEIGHT_COUNT - 1);
    g_free(c->time_color);     c->time_color     = g_strdup(xfce_rc_read_entry(rc, "time_color", ""));

    c->date_size         = CLAMP(xfce_rc_read_int_entry(rc, "date_size", 9), 6, 28);
    c->date_weight       = CLAMP(xfce_rc_read_int_entry(rc, "date_weight", WEIGHT_MEDIUM), 0, WEIGHT_COUNT - 1);
    g_free(c->date_color);     c->date_color     = g_strdup(xfce_rc_read_entry(rc, "date_color", ""));

    g_free(c->sep_color);      c->sep_color      = g_strdup(xfce_rc_read_entry(rc, "sep_color", "#888888"));
    g_free(c->bg_color);       c->bg_color       = g_strdup(xfce_rc_read_entry(rc, "bg_color", ""));
    g_free(c->accent_color);   c->accent_color   = g_strdup(xfce_rc_read_entry(rc, "accent_color", ""));
    c->spacing           = CLAMP(xfce_rc_read_int_entry(rc, "spacing", 1), 0, 20);
    c->pad_x             = CLAMP(xfce_rc_read_int_entry(rc, "pad_x", 8), 0, 24);
    c->pad_y             = CLAMP(xfce_rc_read_int_entry(rc, "pad_y", 2), 0, 16);
    c->radius            = CLAMP(xfce_rc_read_int_entry(rc, "radius", 6), 0, 24);
    c->show_week_numbers = xfce_rc_read_bool_entry(rc, "show_week_numbers", TRUE);

    xfce_rc_close(rc);
}

static void save_config_now(ClockPlugin *c) {
    g_autofree gchar *file = xfce_panel_plugin_save_location(c->plugin, TRUE);
    if (!file) return;
    XfceRc *rc = xfce_rc_simple_open(file, FALSE);
    if (!rc) return;

    xfce_rc_write_int_entry(rc, "layout", c->layout);
    xfce_rc_write_int_entry(rc, "style", c->style);
    xfce_rc_write_entry(rc, "timezone_id", c->timezone_id ? c->timezone_id : "");
    xfce_rc_write_entry(rc, "time_format", c->time_format ? c->time_format : "");
    xfce_rc_write_entry(rc, "date_format", c->date_format ? c->date_format : "");
    xfce_rc_write_entry(rc, "tooltip_format", c->tooltip_format ? c->tooltip_format : "");
    xfce_rc_write_entry(rc, "separator", c->separator ? c->separator : "");
    xfce_rc_write_entry(rc, "click_cmd", c->click_cmd ? c->click_cmd : "");

    xfce_rc_write_bool_entry(rc, "show_icon", c->show_icon);
    xfce_rc_write_entry(rc, "icon_name", c->icon_name ? c->icon_name : "");
    xfce_rc_write_int_entry(rc, "icon_size", c->icon_size);

    xfce_rc_write_int_entry(rc, "time_size", c->time_size);
    xfce_rc_write_int_entry(rc, "time_weight", c->time_weight);
    xfce_rc_write_entry(rc, "time_color", c->time_color ? c->time_color : "");

    xfce_rc_write_int_entry(rc, "date_size", c->date_size);
    xfce_rc_write_int_entry(rc, "date_weight", c->date_weight);
    xfce_rc_write_entry(rc, "date_color", c->date_color ? c->date_color : "");

    xfce_rc_write_entry(rc, "sep_color", c->sep_color ? c->sep_color : "");
    xfce_rc_write_entry(rc, "bg_color", c->bg_color ? c->bg_color : "");
    xfce_rc_write_entry(rc, "accent_color", c->accent_color ? c->accent_color : "");
    xfce_rc_write_int_entry(rc, "spacing", c->spacing);
    xfce_rc_write_int_entry(rc, "pad_x", c->pad_x);
    xfce_rc_write_int_entry(rc, "pad_y", c->pad_y);
    xfce_rc_write_int_entry(rc, "radius", c->radius);
    xfce_rc_write_bool_entry(rc, "show_week_numbers", c->show_week_numbers);

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

void clock_apply_css(ClockPlugin *c) {
    if (!c->css) {
        c->css = gtk_css_provider_new();
        gtk_style_context_add_provider_for_screen(
            gdk_screen_get_default(),
            GTK_STYLE_PROVIDER(c->css),
            GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    }

    const char *bg = (c->bg_color && *c->bg_color) ? c->bg_color : NULL;
    const char *ac = (c->accent_color && *c->accent_color) ? c->accent_color : NULL;
    const char *font_family = (c->style == STYLE_MONOSPACE) ? "monospace" : "inherit";

    GString *s = g_string_sized_new(512);
    g_string_append_printf(s,
        ".xclk-box { padding: %dpx %dpx; border-radius: %dpx; transition: background-color 120ms ease; ",
        c->pad_y, c->pad_x, c->radius);

    switch (c->style) {
        case STYLE_PILL:
            g_string_append_printf(s,
                "background-color: %s; border: 1px solid %s; }\n"
                ".xclk-box:hover { background-color: alpha(currentColor, 0.13); }\n",
                bg ? bg : "alpha(currentColor, 0.08)",
                ac ? ac : "alpha(currentColor, 0.14)");
            break;
        case STYLE_ACCENT:
            g_string_append_printf(s,
                "background-color: %s; border-left: 3px solid %s; border-radius: %dpx; }\n"
                ".xclk-box:hover { background-color: alpha(currentColor, 0.10); }\n",
                bg ? bg : "alpha(currentColor, 0.05)",
                ac ? ac : "#3584e4",
                c->radius);
            break;
        case STYLE_OUTLINE:
            g_string_append_printf(s,
                "background-color: %s; border: 1px solid %s; }\n"
                ".xclk-box:hover { background-color: alpha(currentColor, 0.08); }\n",
                bg ? bg : "transparent",
                ac ? ac : "alpha(currentColor, 0.28)");
            break;
        case STYLE_MONOSPACE:
        case STYLE_MINIMAL:
        default:
            if (bg)
                g_string_append_printf(s, "background-color: %s; }\n", bg);
            else
                g_string_append(s, "background-color: transparent; }\n");
            g_string_append(s, ".xclk-box:hover { background-color: alpha(currentColor, 0.07); }\n");
            break;
    }

    g_string_append_printf(s,
        ".xclk-time { font-family: %s; font-size: %dpx; font-weight: %d; "
        "font-variant-numeric: tabular-nums; line-height: 1.05; ",
        font_family, c->time_size, WEIGHT_VALUES[c->time_weight]);
    if (c->time_color && *c->time_color)
        g_string_append_printf(s, "color: %s; ", c->time_color);
    g_string_append(s, "}\n");

    g_string_append_printf(s,
        ".xclk-date { font-family: %s; font-size: %dpx; font-weight: %d; "
        "font-variant-numeric: tabular-nums; line-height: 1.05; ",
        font_family, c->date_size, WEIGHT_VALUES[c->date_weight]);
    if (c->date_color && *c->date_color)
        g_string_append_printf(s, "color: %s; ", c->date_color);
    else if (c->style == STYLE_ACCENT)
        g_string_append_printf(s, "color: %s; ", ac ? ac : "#3584e4");
    else
        g_string_append(s, "opacity: 0.82; ");
    g_string_append(s, "}\n");

    g_string_append_printf(s,
        ".xclk-sep { font-family: %s; font-size: %dpx; font-weight: 600; margin: 0 %dpx; ",
        font_family, MAX(c->time_size, c->date_size), MAX(2, c->spacing + 2));
    if (c->sep_color && *c->sep_color)
        g_string_append_printf(s, "color: %s; ", c->sep_color);
    else
        g_string_append(s, "opacity: 0.65; ");
    g_string_append(s, "}\n");

    g_string_append(s, ".xclk-popover { padding: 10px; }\n");

    gtk_css_provider_load_from_data(c->css, s->str, (gssize)s->len, NULL);
    g_string_free(s, TRUE);
}

static GtkWidget *make_clock_icon(ClockPlugin *c) {
    const char *name = (c->icon_name && *c->icon_name) ? c->icon_name : "builtin-clock";
    if (strcmp(name, "builtin-clock") == 0) {
        GdkRGBA fg;
        if (!(c->time_color && *c->time_color && gdk_rgba_parse(&fg, c->time_color)))
            gtk_style_context_get_color(gtk_widget_get_style_context(c->root_box), GTK_STATE_FLAG_NORMAL, &fg);

        char hex[8];
        g_snprintf(hex, sizeof(hex), "#%02x%02x%02x",
            (int)(fg.red * 255.0 + 0.5),
            (int)(fg.green * 255.0 + 0.5),
            (int)(fg.blue * 255.0 + 0.5));

        g_autofree gchar *tinted = g_strdup(BUILTIN_CLOCK_SVG);
        char *pos = strstr(tinted, "#2e3436");
        if (pos) memcpy(pos, hex, 7);

        g_autoptr(GInputStream) stream = g_memory_input_stream_new_from_data(tinted, -1, NULL);
        g_autoptr(GdkPixbuf) pb = gdk_pixbuf_new_from_stream_at_scale(stream, c->icon_size, c->icon_size, TRUE, NULL, NULL);
        return pb ? gtk_image_new_from_pixbuf(pb) : gtk_image_new();
    }
    GtkWidget *img = gtk_image_new_from_icon_name(name, GTK_ICON_SIZE_MENU);
    gtk_image_set_pixel_size(GTK_IMAGE(img), c->icon_size);
    return img;
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

void clock_rebuild_ui(ClockPlugin *c) {
    GList *children = gtk_container_get_children(GTK_CONTAINER(c->root_box));
    for (GList *l = children; l != NULL; l = l->next)
        gtk_widget_destroy(GTK_WIDGET(l->data));
    g_list_free(children);

    c->time_label = NULL;
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

    if (clock_has_time(c->layout))
        c->time_label = make_label("xclk-time", stacked);
    if (clock_has_date(c->layout))
        c->date_label = make_label("xclk-date", stacked);

    switch (c->layout) {
        case LAYOUT_TIME_ONLY:
            gtk_box_pack_start(GTK_BOX(content_box), c->time_label, FALSE, FALSE, 0);
            break;
        case LAYOUT_DATE_ONLY:
            gtk_box_pack_start(GTK_BOX(content_box), c->date_label, FALSE, FALSE, 0);
            break;
        case LAYOUT_STACK_TIME_DATE:
            gtk_box_pack_start(GTK_BOX(content_box), c->time_label, FALSE, FALSE, 0);
            gtk_box_pack_start(GTK_BOX(content_box), c->date_label, FALSE, FALSE, 0);
            break;
        case LAYOUT_STACK_DATE_TIME:
            gtk_box_pack_start(GTK_BOX(content_box), c->date_label, FALSE, FALSE, 0);
            gtk_box_pack_start(GTK_BOX(content_box), c->time_label, FALSE, FALSE, 0);
            break;
        case LAYOUT_INLINE_TIME_DATE:
            gtk_box_pack_start(GTK_BOX(content_box), c->time_label, FALSE, FALSE, 0);
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
            gtk_box_pack_start(GTK_BOX(content_box), c->time_label, FALSE, FALSE, 0);
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

    if (c->time_label) {
        g_autofree gchar *t = g_date_time_format(now, (c->time_format && *c->time_format) ? c->time_format : "%H:%M");
        const char *str = t ? t : "ERR";
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
    if (c->show_week_numbers)
        opts |= GTK_CALENDAR_SHOW_WEEK_NUMBERS;
    gtk_calendar_set_display_options(GTK_CALENDAR(c->calendar), opts);
    gtk_calendar_select_month(GTK_CALENDAR(c->calendar),
        (guint)(g_date_time_get_month(now) - 1),
        (guint)g_date_time_get_year(now));
    gtk_calendar_select_day(GTK_CALENDAR(c->calendar),
        (guint)g_date_time_get_day_of_month(now));
}

static void on_today_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    sync_calendar_today((ClockPlugin *)user_data);
}

static void toggle_calendar_popover(ClockPlugin *c) {
    if (!c->popover) {
        c->popover = gtk_popover_new(c->event_box);
        gtk_popover_set_modal(GTK_POPOVER(c->popover), TRUE);

        GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
        gtk_style_context_add_class(gtk_widget_get_style_context(vbox), "xclk-popover");

        c->calendar = gtk_calendar_new();
        gtk_box_pack_start(GTK_BOX(vbox), c->calendar, FALSE, FALSE, 0);

        GtkWidget *btn_today = gtk_button_new_with_label("Today");
        g_signal_connect(btn_today, "clicked", G_CALLBACK(on_today_clicked), c);
        gtk_box_pack_start(GTK_BOX(vbox), btn_today, FALSE, FALSE, 0);

        gtk_container_add(GTK_CONTAINER(c->popover), vbox);
        gtk_widget_show_all(vbox);
    }

    if (gtk_widget_get_visible(c->popover)) {
        gtk_popover_popdown(GTK_POPOVER(c->popover));
    } else {
        sync_calendar_today(c);
        gtk_popover_popup(GTK_POPOVER(c->popover));
    }
}

static gboolean on_button_press(GtkWidget *w, GdkEventButton *ev, gpointer user_data) {
    (void)w;
    ClockPlugin *c = user_data;
    if (ev->type == GDK_BUTTON_PRESS && ev->button == 1) {
        if (c->click_cmd && *c->click_cmd) {
            GError *err = NULL;
            if (!g_spawn_command_line_async(c->click_cmd, &err)) {
                g_warning("xfce4-clock: failed to run '%s': %s", c->click_cmd, err->message);
                g_error_free(err);
            }
        } else {
            toggle_calendar_popover(c);
        }
        return TRUE;
    }
    return FALSE;
}

static gboolean on_query_tooltip(GtkWidget *w, gint x, gint y, gboolean kb, GtkTooltip *tip, gpointer user_data) {
    (void)w; (void)x; (void)y; (void)kb;
    ClockPlugin *c = user_data;
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
    if (c->show_icon && g_strcmp0(c->icon_name, "builtin-clock") == 0 && !(c->time_color && *c->time_color))
        clock_rebuild_ui(c);
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
    if (c->popover) gtk_widget_destroy(c->popover);
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
    g_free(c->click_cmd);
    g_free(c->icon_name);
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
    gtk_widget_set_has_tooltip(c->event_box, TRUE);

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
