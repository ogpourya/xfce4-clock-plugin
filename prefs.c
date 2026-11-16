// SPDX-License-Identifier: MIT
#include "clock.h"
#include <libxfce4ui/libxfce4ui.h>
#include <string.h>

typedef struct {
    ClockPlugin *c;
    gboolean     updating;

    /* Tab 1: Layout & Format */
    GtkWidget *combo_layout;
    GtkWidget *entry_sep;
    GtkWidget *combo_time_preset;
    GtkWidget *entry_time_fmt;
    GtkWidget *lbl_time_prev;
    GtkWidget *combo_date_preset;
    GtkWidget *entry_date_fmt;
    GtkWidget *lbl_date_prev;
    GtkWidget *entry_tz;
    GtkWidget *lbl_tz_status;
    GtkWidget *cb_tooltip;
    GtkWidget *entry_tooltip;

    /* Tab 2: Style */
    GtkWidget *combo_style;
    GtkWidget *lbl_fuzzy;
    GtkWidget *combo_fuzzy;
    GtkWidget *spin_time_size;
    GtkWidget *cb_time_bold;
    GtkWidget *spin_date_size;
    GtkWidget *cb_date_bold;
    GtkWidget *spin_spacing;
    GtkWidget *cb_icon;

    /* Tab 3: Colors */
    GtkWidget *col_time;
    GtkWidget *col_date;
    GtkWidget *col_sep;
    GtkWidget *col_bg;
    GtkWidget *col_accent;
} PrefsUI;

static const struct { const char *label; const char *fmt; } TIME_PRESETS[] = {
    { "24-hour (14:35)",           "%H:%M" },
    { "24-hour + sec (14:35:09)",  "%H:%M:%S" },
    { "12-hour (02:35 PM)",        "%I:%M %p" },
    { "12-hour + sec (02:35:09)",  "%I:%M:%S %p" },
    { "Custom strftime",           NULL },
};

static const struct { const char *label; const char *fmt; } DATE_PRESETS[] = {
    { "Short (Tue, Nov 17)",       "%a, %b %d" },
    { "Day first (Tue 17 Nov)",    "%a %d %b" },
    { "Numeric (17/11/2026)",      "%d/%m/%Y" },
    { "ISO (2026-11-17)",          "%Y-%m-%d" },
    { "Compact (Nov 17)",          "%b %d" },
    { "Full weekday (Tuesday 17)", "%A %d" },
    { "Custom strftime",           NULL },
};

static void set_color_btn(GtkColorButton *btn, const char *hex) {
    GdkRGBA rgba;
    if (hex && *hex && gdk_rgba_parse(&rgba, hex)) {
        gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(btn), &rgba);
    } else {
        gdk_rgba_parse(&rgba, "rgba(0,0,0,0)");
        gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(btn), &rgba);
    }
}

static gchar *get_color_btn_hex(GtkColorButton *btn) {
    GdkRGBA rgba;
    gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(btn), &rgba);
    if (rgba.alpha < 0.02) return g_strdup("");
    if (rgba.alpha >= 0.995) {
        return g_strdup_printf("#%02x%02x%02x",
            (int)(rgba.red * 255.0 + 0.5),
            (int)(rgba.green * 255.0 + 0.5),
            (int)(rgba.blue * 255.0 + 0.5));
    }
    return g_strdup_printf("rgba(%d,%d,%d,%.2f)",
        (int)(rgba.red * 255.0 + 0.5),
        (int)(rgba.green * 255.0 + 0.5),
        (int)(rgba.blue * 255.0 + 0.5),
        rgba.alpha);
}

static void refresh_sensitivity_and_previews(PrefsUI *ui) {
    ClockPlugin *c = ui->c;
    gboolean has_t = clock_has_time(c->layout);
    gboolean has_d = clock_has_date(c->layout);
    gboolean is_fuzzy = (c->style == STYLE_FUZZY);
    gboolean is_text_time = (c->style == STYLE_DIGITAL || c->style == STYLE_FUZZY);

    gtk_widget_set_sensitive(ui->entry_sep, clock_is_inline(c->layout));
    gtk_widget_set_sensitive(ui->entry_time_fmt, has_t && !is_fuzzy);
    gtk_widget_set_sensitive(ui->combo_time_preset, has_t && !is_fuzzy);

    gtk_widget_set_sensitive(ui->spin_time_size, has_t);
    gtk_widget_set_sensitive(ui->cb_time_bold, has_t && is_text_time);

    gtk_widget_set_visible(ui->lbl_fuzzy, has_t && is_fuzzy);
    gtk_widget_set_visible(ui->combo_fuzzy, has_t && is_fuzzy);

    gtk_widget_set_sensitive(ui->entry_date_fmt, has_d);
    gtk_widget_set_sensitive(ui->combo_date_preset, has_d);
    gtk_widget_set_sensitive(ui->spin_date_size, has_d);
    gtk_widget_set_sensitive(ui->cb_date_bold, has_d);

    gtk_widget_set_sensitive(ui->entry_tooltip, c->show_tooltip);

    g_autofree gchar *tp = clock_format_preview(c, c->time_format);
    g_autofree gchar *dp = clock_format_preview(c, c->date_format);
    gtk_label_set_text(GTK_LABEL(ui->lbl_time_prev), tp);
    gtk_label_set_text(GTK_LABEL(ui->lbl_date_prev), dp);

    if (!c->timezone_id || !*c->timezone_id)
        gtk_label_set_text(GTK_LABEL(ui->lbl_tz_status), "Using system local timezone");
    else if (c->tz_valid)
        gtk_label_set_text(GTK_LABEL(ui->lbl_tz_status), "Active");
    else
        gtk_label_set_text(GTK_LABEL(ui->lbl_tz_status), "Unknown ID (falling back to local)");
}

static void on_prefs_changed(GtkWidget *w, gpointer user_data) {
    (void)w;
    PrefsUI *ui = user_data;
    if (ui->updating) return;
    ClockPlugin *c = ui->c;

    c->layout      = CLAMP(gtk_combo_box_get_active(GTK_COMBO_BOX(ui->combo_layout)), 0, LAYOUT_COUNT - 1);
    c->style       = CLAMP(gtk_combo_box_get_active(GTK_COMBO_BOX(ui->combo_style)), 0, STYLE_COUNT - 1);
    c->fuzzy_level = CLAMP(gtk_combo_box_get_active(GTK_COMBO_BOX(ui->combo_fuzzy)), 0, FUZZY_COUNT - 1);

    g_free(c->separator);
    c->separator = g_strdup(gtk_entry_get_text(GTK_ENTRY(ui->entry_sep)));

    g_free(c->time_format);
    c->time_format = g_strdup(gtk_entry_get_text(GTK_ENTRY(ui->entry_time_fmt)));

    g_free(c->date_format);
    c->date_format = g_strdup(gtk_entry_get_text(GTK_ENTRY(ui->entry_date_fmt)));

    const char *new_tz = gtk_entry_get_text(GTK_ENTRY(ui->entry_tz));
    if (g_strcmp0(c->timezone_id, new_tz) != 0) {
        g_free(c->timezone_id);
        c->timezone_id = g_strdup(new_tz);
        clock_reload_timezone(c);
    }

    c->show_tooltip = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ui->cb_tooltip));
    g_free(c->tooltip_format);
    c->tooltip_format = g_strdup(gtk_entry_get_text(GTK_ENTRY(ui->entry_tooltip)));

    c->time_size = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(ui->spin_time_size));
    c->time_bold = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ui->cb_time_bold));
    c->date_size = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(ui->spin_date_size));
    c->date_bold = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ui->cb_date_bold));
    c->spacing   = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(ui->spin_spacing));
    c->show_icon = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ui->cb_icon));

    g_free(c->time_color);   c->time_color   = get_color_btn_hex(GTK_COLOR_BUTTON(ui->col_time));
    g_free(c->date_color);   c->date_color   = get_color_btn_hex(GTK_COLOR_BUTTON(ui->col_date));
    g_free(c->sep_color);    c->sep_color    = get_color_btn_hex(GTK_COLOR_BUTTON(ui->col_sep));
    g_free(c->bg_color);     c->bg_color     = get_color_btn_hex(GTK_COLOR_BUTTON(ui->col_bg));
    g_free(c->accent_color); c->accent_color = get_color_btn_hex(GTK_COLOR_BUTTON(ui->col_accent));

    refresh_sensitivity_and_previews(ui);
    clock_apply_css(c);
    clock_rebuild_ui(c);
    clock_schedule_save(c);
}

static void on_time_preset_changed(GtkComboBox *combo, gpointer user_data) {
    PrefsUI *ui = user_data;
    if (ui->updating) return;
    int idx = gtk_combo_box_get_active(combo);
    if (idx >= 0 && idx < (int)G_N_ELEMENTS(TIME_PRESETS) && TIME_PRESETS[idx].fmt) {
        ui->updating = TRUE;
        gtk_entry_set_text(GTK_ENTRY(ui->entry_time_fmt), TIME_PRESETS[idx].fmt);
        ui->updating = FALSE;
        on_prefs_changed(GTK_WIDGET(combo), ui);
    }
}

static void on_date_preset_changed(GtkComboBox *combo, gpointer user_data) {
    PrefsUI *ui = user_data;
    if (ui->updating) return;
    int idx = gtk_combo_box_get_active(combo);
    if (idx >= 0 && idx < (int)G_N_ELEMENTS(DATE_PRESETS) && DATE_PRESETS[idx].fmt) {
        ui->updating = TRUE;
        gtk_entry_set_text(GTK_ENTRY(ui->entry_date_fmt), DATE_PRESETS[idx].fmt);
        ui->updating = FALSE;
        on_prefs_changed(GTK_WIDGET(combo), ui);
    }
}

static void on_reset_color_clicked(GtkButton *b, gpointer user_data) {
    (void)user_data;
    GtkColorButton *btn = g_object_get_data(G_OBJECT(b), "target-btn");
    PrefsUI *ui = g_object_get_data(G_OBJECT(b), "prefs-ui");
    set_color_btn(btn, "");
    on_prefs_changed(GTK_WIDGET(btn), ui);
}

static GtkWidget *make_color_row(PrefsUI *ui, GtkGrid *grid, int row, const char *label, const char *hex) {
    GtkWidget *lbl = gtk_label_new(label);
    gtk_widget_set_halign(lbl, GTK_ALIGN_START);
    GtkWidget *btn = gtk_color_button_new();
    gtk_color_chooser_set_use_alpha(GTK_COLOR_CHOOSER(btn), TRUE);
    set_color_btn(GTK_COLOR_BUTTON(btn), hex);
    g_signal_connect(btn, "color-set", G_CALLBACK(on_prefs_changed), ui);

    GtkWidget *rst = gtk_button_new_with_label("Default");
    gtk_widget_set_tooltip_text(rst, "Use style / panel theme default");
    g_object_set_data(G_OBJECT(rst), "target-btn", btn);
    g_object_set_data(G_OBJECT(rst), "prefs-ui", ui);
    g_signal_connect(rst, "clicked", G_CALLBACK(on_reset_color_clicked), NULL);

    gtk_grid_attach(grid, lbl, 0, row, 1, 1);
    gtk_grid_attach(grid, btn, 1, row, 1, 1);
    gtk_grid_attach(grid, rst, 2, row, 1, 1);
    return btn;
}

static GtkWidget *new_grid(void) {
    GtkWidget *g = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(g), 8);
    gtk_grid_set_column_spacing(GTK_GRID(g), 12);
    gtk_container_set_border_width(GTK_CONTAINER(g), 12);
    return g;
}

static GtkWidget *make_row_label(const char *text) {
    GtkWidget *lbl = gtk_label_new(text);
    gtk_widget_set_halign(lbl, GTK_ALIGN_START);
    return lbl;
}

static void on_prefs_destroy(GtkWidget *dlg, gpointer user_data) {
    (void)dlg;
    PrefsUI *ui = user_data;
    if (ui->c) {
        ui->c->prefs_dlg = NULL;
        clock_flush_save(ui->c);
        xfce_panel_plugin_unblock_menu(ui->c->plugin);
    }
    g_free(ui);
}

void prefs_open_dialog(XfcePanelPlugin *plugin, gpointer user_data) {
    ClockPlugin *c = user_data;
    if (c->prefs_dlg) {
        gtk_window_present(GTK_WINDOW(c->prefs_dlg));
        return;
    }
    xfce_panel_plugin_block_menu(plugin);

    GtkWidget *dlg = xfce_titled_dialog_new_with_mixed_buttons(
        "Clock Settings",
        GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(plugin))),
        GTK_DIALOG_DESTROY_WITH_PARENT,
        "window-close-symbolic", "_Close", GTK_RESPONSE_OK,
        NULL);
    gtk_window_set_default_size(GTK_WINDOW(dlg), 440, -1);
    c->prefs_dlg = dlg;

    PrefsUI *ui = g_new0(PrefsUI, 1);
    ui->c = c;
    ui->updating = TRUE;

    GtkWidget *nb = gtk_notebook_new();
    gtk_container_set_border_width(GTK_CONTAINER(nb), 8);
    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dlg))), nb, TRUE, TRUE, 0);

    /* ==================== Tab 1: Layout & Format ==================== */
    GtkWidget *g1 = new_grid();
    int r = 0;

    ui->combo_layout = gtk_combo_box_text_new();
    for (int i = 0; i < LAYOUT_COUNT; i++)
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(ui->combo_layout), LAYOUT_NAMES[i]);
    gtk_combo_box_set_active(GTK_COMBO_BOX(ui->combo_layout), c->layout);
    gtk_grid_attach(GTK_GRID(g1), make_row_label("Layout & order:"), 0, r, 1, 1);
    gtk_grid_attach(GTK_GRID(g1), ui->combo_layout, 1, r++, 2, 1);
    g_signal_connect(ui->combo_layout, "changed", G_CALLBACK(on_prefs_changed), ui);

    ui->entry_sep = gtk_entry_new();
    gtk_entry_set_max_length(GTK_ENTRY(ui->entry_sep), 16);
    gtk_entry_set_text(GTK_ENTRY(ui->entry_sep), c->separator);
    gtk_widget_set_tooltip_text(ui->entry_sep, "Separator shown between time and date in Inline layouts");
    gtk_grid_attach(GTK_GRID(g1), make_row_label("Inline separator:"), 0, r, 1, 1);
    gtk_grid_attach(GTK_GRID(g1), ui->entry_sep, 1, r++, 2, 1);
    g_signal_connect(ui->entry_sep, "changed", G_CALLBACK(on_prefs_changed), ui);

    ui->combo_time_preset = gtk_combo_box_text_new();
    int active_tp = (int)G_N_ELEMENTS(TIME_PRESETS) - 1;
    for (int i = 0; i < (int)G_N_ELEMENTS(TIME_PRESETS); i++) {
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(ui->combo_time_preset), TIME_PRESETS[i].label);
        if (TIME_PRESETS[i].fmt && g_strcmp0(c->time_format, TIME_PRESETS[i].fmt) == 0)
            active_tp = i;
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(ui->combo_time_preset), active_tp);
    gtk_grid_attach(GTK_GRID(g1), make_row_label("Time preset:"), 0, r, 1, 1);
    gtk_grid_attach(GTK_GRID(g1), ui->combo_time_preset, 1, r++, 2, 1);
    g_signal_connect(ui->combo_time_preset, "changed", G_CALLBACK(on_time_preset_changed), ui);

    ui->entry_time_fmt = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(ui->entry_time_fmt), c->time_format);
    ui->lbl_time_prev = make_row_label("");
    gtk_grid_attach(GTK_GRID(g1), make_row_label("Time format:"), 0, r, 1, 1);
    gtk_grid_attach(GTK_GRID(g1), ui->entry_time_fmt, 1, r, 1, 1);
    gtk_grid_attach(GTK_GRID(g1), ui->lbl_time_prev, 2, r++, 1, 1);
    g_signal_connect(ui->entry_time_fmt, "changed", G_CALLBACK(on_prefs_changed), ui);

    ui->combo_date_preset = gtk_combo_box_text_new();
    int active_dp = (int)G_N_ELEMENTS(DATE_PRESETS) - 1;
    for (int i = 0; i < (int)G_N_ELEMENTS(DATE_PRESETS); i++) {
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(ui->combo_date_preset), DATE_PRESETS[i].label);
        if (DATE_PRESETS[i].fmt && g_strcmp0(c->date_format, DATE_PRESETS[i].fmt) == 0)
            active_dp = i;
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(ui->combo_date_preset), active_dp);
    gtk_grid_attach(GTK_GRID(g1), make_row_label("Date preset:"), 0, r, 1, 1);
    gtk_grid_attach(GTK_GRID(g1), ui->combo_date_preset, 1, r++, 2, 1);
    g_signal_connect(ui->combo_date_preset, "changed", G_CALLBACK(on_date_preset_changed), ui);

    ui->entry_date_fmt = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(ui->entry_date_fmt), c->date_format);
    ui->lbl_date_prev = make_row_label("");
    gtk_grid_attach(GTK_GRID(g1), make_row_label("Date format:"), 0, r, 1, 1);
    gtk_grid_attach(GTK_GRID(g1), ui->entry_date_fmt, 1, r, 1, 1);
    gtk_grid_attach(GTK_GRID(g1), ui->lbl_date_prev, 2, r++, 1, 1);
    g_signal_connect(ui->entry_date_fmt, "changed", G_CALLBACK(on_prefs_changed), ui);

    ui->entry_tz = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(ui->entry_tz), "Local (or e.g. Asia/Tehran, UTC)");
    gtk_entry_set_text(GTK_ENTRY(ui->entry_tz), c->timezone_id);
    ui->lbl_tz_status = make_row_label("");
    gtk_grid_attach(GTK_GRID(g1), make_row_label("Timezone:"), 0, r, 1, 1);
    gtk_grid_attach(GTK_GRID(g1), ui->entry_tz, 1, r, 1, 1);
    gtk_grid_attach(GTK_GRID(g1), ui->lbl_tz_status, 2, r++, 1, 1);
    g_signal_connect(ui->entry_tz, "changed", G_CALLBACK(on_prefs_changed), ui);

    ui->cb_tooltip = gtk_check_button_new_with_label("Show hover tooltip");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ui->cb_tooltip), c->show_tooltip);
    gtk_grid_attach(GTK_GRID(g1), ui->cb_tooltip, 0, r++, 3, 1);
    g_signal_connect(ui->cb_tooltip, "toggled", G_CALLBACK(on_prefs_changed), ui);

    ui->entry_tooltip = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(ui->entry_tooltip), c->tooltip_format);
    gtk_grid_attach(GTK_GRID(g1), make_row_label("Tooltip format:"), 0, r, 1, 1);
    gtk_grid_attach(GTK_GRID(g1), ui->entry_tooltip, 1, r++, 2, 1);
    g_signal_connect(ui->entry_tooltip, "changed", G_CALLBACK(on_prefs_changed), ui);

    gtk_notebook_append_page(GTK_NOTEBOOK(nb), g1, gtk_label_new("Layout & Format"));

    /* ==================== Tab 2: Style ==================== */
    GtkWidget *g2 = new_grid();
    r = 0;

    ui->combo_style = gtk_combo_box_text_new();
    for (int i = 0; i < STYLE_COUNT; i++)
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(ui->combo_style), STYLE_NAMES[i]);
    gtk_combo_box_set_active(GTK_COMBO_BOX(ui->combo_style), c->style);
    gtk_grid_attach(GTK_GRID(g2), make_row_label("Visual style:"), 0, r, 1, 1);
    gtk_grid_attach(GTK_GRID(g2), ui->combo_style, 1, r++, 2, 1);
    g_signal_connect(ui->combo_style, "changed", G_CALLBACK(on_prefs_changed), ui);

    ui->lbl_fuzzy = make_row_label("Fuzzy precision:");
    ui->combo_fuzzy = gtk_combo_box_text_new();
    for (int i = 0; i < FUZZY_COUNT; i++)
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(ui->combo_fuzzy), FUZZY_NAMES[i]);
    gtk_combo_box_set_active(GTK_COMBO_BOX(ui->combo_fuzzy), c->fuzzy_level);
    gtk_grid_attach(GTK_GRID(g2), ui->lbl_fuzzy, 0, r, 1, 1);
    gtk_grid_attach(GTK_GRID(g2), ui->combo_fuzzy, 1, r++, 2, 1);
    g_signal_connect(ui->combo_fuzzy, "changed", G_CALLBACK(on_prefs_changed), ui);

    ui->spin_time_size = gtk_spin_button_new_with_range(7, 36, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(ui->spin_time_size), c->time_size);
    ui->cb_time_bold = gtk_check_button_new_with_label("Bold");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ui->cb_time_bold), c->time_bold);
    gtk_grid_attach(GTK_GRID(g2), make_row_label("Time size (px):"), 0, r, 1, 1);
    gtk_grid_attach(GTK_GRID(g2), ui->spin_time_size, 1, r, 1, 1);
    gtk_grid_attach(GTK_GRID(g2), ui->cb_time_bold, 2, r++, 1, 1);
    g_signal_connect(ui->spin_time_size, "value-changed", G_CALLBACK(on_prefs_changed), ui);
    g_signal_connect(ui->cb_time_bold, "toggled", G_CALLBACK(on_prefs_changed), ui);

    ui->spin_date_size = gtk_spin_button_new_with_range(6, 28, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(ui->spin_date_size), c->date_size);
    ui->cb_date_bold = gtk_check_button_new_with_label("Bold");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ui->cb_date_bold), c->date_bold);
    gtk_grid_attach(GTK_GRID(g2), make_row_label("Date size (px):"), 0, r, 1, 1);
    gtk_grid_attach(GTK_GRID(g2), ui->spin_date_size, 1, r, 1, 1);
    gtk_grid_attach(GTK_GRID(g2), ui->cb_date_bold, 2, r++, 1, 1);
    g_signal_connect(ui->spin_date_size, "value-changed", G_CALLBACK(on_prefs_changed), ui);
    g_signal_connect(ui->cb_date_bold, "toggled", G_CALLBACK(on_prefs_changed), ui);

    ui->spin_spacing = gtk_spin_button_new_with_range(0, 20, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(ui->spin_spacing), c->spacing);
    gtk_grid_attach(GTK_GRID(g2), make_row_label("Spacing (px):"), 0, r, 1, 1);
    gtk_grid_attach(GTK_GRID(g2), ui->spin_spacing, 1, r++, 1, 1);
    g_signal_connect(ui->spin_spacing, "value-changed", G_CALLBACK(on_prefs_changed), ui);

    ui->cb_icon = gtk_check_button_new_with_label("Show clock icon");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ui->cb_icon), c->show_icon);
    gtk_grid_attach(GTK_GRID(g2), ui->cb_icon, 0, r++, 3, 1);
    g_signal_connect(ui->cb_icon, "toggled", G_CALLBACK(on_prefs_changed), ui);

    gtk_notebook_append_page(GTK_NOTEBOOK(nb), g2, gtk_label_new("Style"));

    /* ==================== Tab 3: Colors ==================== */
    GtkWidget *g3 = new_grid();
    r = 0;

    ui->col_time   = make_color_row(ui, GTK_GRID(g3), r++, "Time color:", c->time_color);
    ui->col_date   = make_color_row(ui, GTK_GRID(g3), r++, "Date color:", c->date_color);
    ui->col_sep    = make_color_row(ui, GTK_GRID(g3), r++, "Separator color:", c->sep_color);
    ui->col_bg     = make_color_row(ui, GTK_GRID(g3), r++, "Background fill:", c->bg_color);
    ui->col_accent = make_color_row(ui, GTK_GRID(g3), r++, "Second hand / accent:", c->accent_color);

    gtk_notebook_append_page(GTK_NOTEBOOK(nb), g3, gtk_label_new("Colors"));

    ui->updating = FALSE;
    gtk_widget_show_all(dlg);
    refresh_sensitivity_and_previews(ui);

    g_signal_connect(dlg, "response", G_CALLBACK(gtk_widget_destroy), NULL);
    g_signal_connect(dlg, "destroy", G_CALLBACK(on_prefs_destroy), ui);
}
