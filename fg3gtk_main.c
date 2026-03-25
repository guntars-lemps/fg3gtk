#include <gtk/gtk.h>
#include <string.h>
#include <stdlib.h>
#include <locale.h>
#include <math.h>
#include <sys/stat.h>
#include "fg3glade.h"
#include "fg3gtk_main.h"
#include "fg3gtk_spin_boost.h"
#include "ini.h"
#include "serial_lib.h"


GtkBuilder* builder;

GtkWidget* window;

t_mode mode = MODE_CUSTOM;

tfrequencies f1, f2, f3;

t_device_status device_status = NOT_SELECTED;

gboolean ignore_device_change = FALSE;

gchar *serial_device = NULL;

PSerLibHandle_t serial_device_handle = NULL;

guint16 serial_cmd_timeout;

guint16 auto_send_timeot;

gboolean auto_send;

guint32 ticks_per_sec, ticks_per_unit, min_units, max_units;


int main(int argc, char *argv[])
{
    gtk_init(&argc, &argv);

    builder = gtk_builder_new();

    GError *err = NULL;

    if (gtk_builder_add_from_string(builder, (char*) __fg3_glade, __fg3_glade_len, &err) == 0) {
        fprintf(stderr, "Error: %s\n", err->message);
        return 1;
    }

    window = GTK_WIDGET(gtk_builder_get_object(builder, "application_window"));

    gtk_builder_connect_signals(builder, NULL);

    gtk_widget_show(window);

    // styling "Store" button background color
    GtkStyleContext *context;
    GtkCssProvider *provider;

    context = gtk_widget_get_style_context(GTK_WIDGET(gtk_builder_get_object(builder, "button_store")));
    provider = gtk_css_provider_new();

    gtk_css_provider_load_from_data(provider, "* {background: #dca3a3;} *:active {background: #e38a8a;}", -1, NULL);
    gtk_style_context_add_provider(context, GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(provider);

    setlocale(LC_NUMERIC, "C");

    setup_default_config();

    device_status = NOT_SELECTED;

    load_stored_config();

    set_frequencies();

    set_spin_buttons_range();

    update_devices_list();

    refresh_ui();

    g_timeout_add(1000, (GSourceFunc)timer_handler, (gpointer)window);

    g_timeout_add(100, (GSourceFunc)receiver_timer_handler, (gpointer)window);

    g_timeout_add(100, (GSourceFunc)auto_send_timer_handler, (gpointer)window);

    spin_boost_init();

    enable_sending_widgets(FALSE);

    gtk_main();

    save_config();

    PSerLib_close(&serial_device_handle);

    return 0;
}

// for debugging
/*void print_buffer(uint8_t* buf, int buflen)
{
    for (int i = 0; i < buflen; i++) {
        printf(" %#04x", buf[i]);
    }
}*/

// return integer rounded by module, round up
guint32 next_mod_number(guint32 num, guint32 mod)
{
    if (num < (UINT32_MAX - 2)) {
        num += 2;
    } else {
        num = UINT32_MAX;
    }
    return mod * (num / mod);
}

// return integer rounded by module, round down
guint32 prev_mod_number(guint32 num, guint32 mod)
{
    return mod * (num / mod);
}


void button_send_click()
{
    send_cmd(CMD_SET_FREQUENCIES);
    enable_sending_widgets(FALSE);
    set_status_label(ST_INFO, "SENDING");
}


void button_store_click()
{
    send_cmd(CMD_STORE);
    enable_sending_widgets(FALSE);
    set_status_label(ST_INFO, "SENDING");
}


void cb_auto_send_toggle()
{
    auto_send = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "cb_auto_send")));
}


void serial_device_change()
{
    if (!ignore_device_change) {
        enable_sending_widgets(FALSE);

        gchar *new_serial_device = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(gtk_builder_get_object(builder, "device_list")));

        if (serial_device != NULL) {
            g_free(serial_device);
        }
        serial_device = new_serial_device;

        if (serial_device == NULL) {
            device_status = NOT_SELECTED;
            set_status_label(ST_ERROR, "NOT SELECTED");
        } else {
            device_status = DISCONNECTED;
            set_status_label(ST_ERROR, "DISCONNECTED");

            PSerLib_close(&serial_device_handle);
            sleep(1); // give time to device to initialize
            PSL_ErrorCodes_e error = PSerLib_open(serial_device, &serial_device_handle);
            if (error) {
                fprintf(stderr, "Serial device error: %s\n", PSerLib_getErrorMessage(error));
                device_status = DEVICE_ERROR;
                set_status_label(ST_ERROR, "DEVICE ERROR");
            } else {
                PSerLib_setParams(serial_device_handle, PSL_BR_19200, PSL_DB_8, PSL_P_none, PSL_SB_1, PSL_FC_none);
            }
        }
    }
}


// ON/OFF switches
void f1_switch_activate()
{
    gboolean active = gtk_switch_get_active(GTK_SWITCH(gtk_builder_get_object(builder, "f1_switch")));

    switch (mode) {

        case MODE_CUSTOM:
            f1.enabled = active;
            f1_control_enable(active);
            break;

        case MODE_2_PHASES:
            f1.enabled = active;
            f1_control_enable(active);
            f2.enabled = active;
            gtk_switch_set_active(GTK_SWITCH(gtk_builder_get_object(builder, "f2_switch")), active);
            break;

        case MODE_3_PHASES:
            f1.enabled = active;
            f1_control_enable(active);
            f2.enabled = active;
            gtk_switch_set_active(GTK_SWITCH(gtk_builder_get_object(builder, "f2_switch")), active);
            f3.enabled = active;
            gtk_switch_set_active(GTK_SWITCH(gtk_builder_get_object(builder, "f3_switch")), active);
            break;
    }
}


void f2_switch_activate()
{
    gboolean active = gtk_switch_get_active(GTK_SWITCH(gtk_builder_get_object(builder, "f2_switch")));

    f2.enabled = active;
    if (mode == MODE_CUSTOM) {
        f2_control_enable(active);
    }
}


void f3_switch_activate()
{
    gboolean active = gtk_switch_get_active(GTK_SWITCH(gtk_builder_get_object(builder, "f3_switch")));

    f3.enabled = active;
    if (mode == MODE_CUSTOM) {
        f3_control_enable(active);
    }
}


// radio button ON T vs DC change
void rb_f1_toggle()
{
    gboolean active;

    active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "f1_rb_dc")));

    f1.dc_mode = active;

    set_values_for_phase_mode();
    set_dc_values_and_widgets();
    update_info_labels();
}


void rb_f2_toggle()
{
    gboolean active;

    active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "f2_rb_dc")));

    f2.dc_mode = active;

    set_dc_values_and_widgets();
    update_info_labels();
}


void rb_f3_toggle()
{
    gboolean active;

    active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "f3_rb_dc")));

    f3.dc_mode = active;

    set_dc_values_and_widgets();
    update_info_labels();
}


// radio button frequencies mode change
void rb_mode_toggle()
{
    gboolean active;

    active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "rb_custom_mode")));
    if (active) {
        set_mode(MODE_CUSTOM);
        return;
    }

    active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "rb_2_phase_mode")));
    if (active) {
        set_mode(MODE_2_PHASES);
        return;
    }

    active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "rb_3_phase_mode")));
    if (active) {
        set_mode(MODE_3_PHASES);
        return;
    }
}


// called when window is closed
void on_window_main_destroy()
{
    g_object_unref(builder);
    gtk_main_quit();
}

void set_spin_buttons_range()
{
    gtk_spin_button_set_range(GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "f1_delay")), 0, max_units - min_units);
    gtk_spin_button_set_range(GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "f2_delay")), 0, max_units - min_units);
    gtk_spin_button_set_range(GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "f3_delay")), 0, max_units - min_units);

    gtk_spin_button_set_range(GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "f1_on_t")), min_units, max_units - min_units);
    gtk_spin_button_set_range(GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "f2_on_t")), min_units, max_units - min_units);
    gtk_spin_button_set_range(GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "f3_on_t")), min_units, max_units - min_units);

    // Call signal functions, that will adjust "period" spin buttons ranges
    f1_period_adjustment_change();
    f2_period_adjustment_change();
    f3_period_adjustment_change();
}

// delay adjustment change
void f1_delay_adjustment_change()
{
    boost(&f1_delay_spin_boost);
    f1.delay = gtk_spin_button_get_value(GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "f1_delay")));
}


void f2_delay_adjustment_change()
{
    boost(&f2_delay_spin_boost);
    f2.delay = gtk_spin_button_get_value(GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "f2_delay")));
}


void f3_delay_adjustment_change()
{
    boost(&f3_delay_spin_boost);
    f3.delay = gtk_spin_button_get_value(GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "f3_delay")));
}


// on adjustment change
void f1_on_adjustment_change()
{
    boost(&f1_on_spin_boost);

    guint32 f1_on = gtk_spin_button_get_value(GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "f1_on_t")));

    if (f1_on < min_units) {
        f1_on = min_units;
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "f1_on_t")), f1.on);
    }
    if (f1_on > (f1.period - min_units)) {
        f1_on = f1.period - min_units;
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "f1_on_t")), f1.on);
    }
    f1.on = f1_on;
    set_values_for_phase_mode();
    set_dc_values_and_widgets();
    update_info_labels();
}


void f2_on_adjustment_change()
{
    boost(&f2_on_spin_boost);

    guint32 f2_on = gtk_spin_button_get_value(GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "f2_on_t")));

    if (f2_on < min_units) {
        f2_on = min_units;
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "f2_on_t")), f2.on);
    }
    if (f2_on > (f2.period - min_units)) {
        f2_on = f2.period - min_units;
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "f2_on_t")), f2.on);
    }
    f2.on = f2_on;
    set_dc_values_and_widgets();
    update_info_labels();
}


void f3_on_adjustment_change()
{
    boost(&f3_on_spin_boost);

    guint32 f3_on = gtk_spin_button_get_value(GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "f3_on_t")));

    if (f3_on < min_units) {
        f3_on = min_units;
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "f3_on_t")), f3.on);
    }
    if (f3_on > (f3.period - min_units)) {
        f3_on = f3.period - min_units;
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "f3_on_t")), f3.on);
    }
    f3.on = f3_on;
    set_dc_values_and_widgets();
    update_info_labels();
}


// duty cycle change
void f1_dc_change()
{
    GtkEntry *dc_entry = GTK_ENTRY(gtk_builder_get_object(builder, "f1_dc"));

    const char *str_value = gtk_entry_get_text(dc_entry);
    gdouble dc_value;
    if (validate_and_convert_dc_value(str_value, &dc_value, dc_entry)) {
        f1.dc_value = dc_value;
        set_values_for_phase_mode();
        set_dc_values_and_widgets();
        update_info_labels();
    }
}


void f2_dc_change()
{
    GtkEntry *dc_entry = GTK_ENTRY(gtk_builder_get_object(builder, "f2_dc"));

    const char *str_value = gtk_entry_get_text(dc_entry);
    gdouble dc_value;
    if (validate_and_convert_dc_value(str_value, &dc_value, dc_entry)) {
        f2.dc_value = dc_value;
        set_dc_values_and_widgets();
        update_info_labels();
    }
}


void f3_dc_change()
{
    GtkEntry *dc_entry = GTK_ENTRY(gtk_builder_get_object(builder, "f3_dc"));

    const char *str_value = gtk_entry_get_text(dc_entry);
    gdouble dc_value;
    if (validate_and_convert_dc_value(str_value, &dc_value, dc_entry)) {
        f3.dc_value = dc_value;
        set_dc_values_and_widgets();
        update_info_labels();
    }
}

// ducty cycle focus out events
gboolean f1_dc_focus_out(GtkEntry *dc_entry)
{
    char str_value[20];
    gdouble dc_value;

    snprintf(str_value, 10, "%.3f", f1.dc_value);
    fix_float(str_value);

    if (validate_and_convert_dc_value(str_value, &dc_value, dc_entry)) {
        f1.dc_value = dc_value;
        gtk_entry_set_text(dc_entry, str_value);
        set_dc_values_and_widgets();
        update_info_labels();
    }
    return FALSE;
}


gboolean f2_dc_focus_out(GtkEntry *dc_entry)
{
    char str_value[20];
    gdouble dc_value;

    snprintf(str_value, 10, "%.3f", f2.dc_value);
    fix_float(str_value);

    if (validate_and_convert_dc_value(str_value, &dc_value, dc_entry)) {
        f2.dc_value = dc_value;
        gtk_entry_set_text(dc_entry, str_value);
        set_dc_values_and_widgets();
        update_info_labels();
    }
    return FALSE;
}


gboolean f3_dc_focus_out(GtkEntry *dc_entry)
{
    char str_value[20];
    gdouble dc_value;

    snprintf(str_value, 10, "%.3f", f3.dc_value);
    fix_float(str_value);

    if (validate_and_convert_dc_value(str_value, &dc_value, dc_entry)) {
        f3.dc_value = dc_value;
        gtk_entry_set_text(dc_entry, str_value);
        set_dc_values_and_widgets();
        update_info_labels();
    }
    return FALSE;
}


// period adjustment change
void f1_period_adjustment_change()
{
    boost(&f1_period_spin_boost);

    gdouble f1_period = gtk_spin_button_get_value(GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "f1_period")));

    gdouble min_value, max_value;

    switch (mode) {
        case MODE_CUSTOM:
            min_value = 2 * min_units;
            max_value = max_units;
            break;

        case MODE_2_PHASES:
            f1_period = prev_mod_number(f1_period, 2);
            min_value = prev_mod_number(2 * min_units, 2);
            max_value = prev_mod_number(max_units, 2);
            break;

       case MODE_3_PHASES:
            f1_period = prev_mod_number(f1_period, 3);
            min_value = prev_mod_number(2 * min_units, 3);
            max_value = prev_mod_number(max_units, 3);
            break;
    }

    adjust_value_by_range(&f1_period, min_value, max_value);

    gtk_spin_button_set_range(GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "f1_period")), min_value, max_value);

    gtk_spin_button_set_value(GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "f1_period")), f1_period);
    set_values_for_phase_mode();
    f1.period = f1_period;

    if (f1.on > (f1.period - min_units)) {
        f1.on = f1.period - min_units;
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "f1_on_t")), f1.on);
    }
    set_dc_values_and_widgets();
    update_info_labels();
}


void f2_period_adjustment_change()
{
    boost(&f2_period_spin_boost);

    gdouble f2_period = gtk_spin_button_get_value(GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "f2_period")));

    gdouble min_value, max_value;

    min_value = 2 * min_units;
    max_value = max_units;

    adjust_value_by_range(&f2_period, min_value, max_value);

    f2.period = f2_period;

    gtk_spin_button_set_range(GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "f2_period")), min_value, max_value);

    if (f2.on > (f2.period - min_units)) {
        f2.on = f2.period - min_units;
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "f2_on_t")), f2.on);
    }
    set_dc_values_and_widgets();
    update_info_labels();
}


void f3_period_adjustment_change()
{
    boost(&f3_period_spin_boost);

    gdouble f3_period = gtk_spin_button_get_value(GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "f3_period")));

    gdouble min_value, max_value;

    min_value = 2 * min_units;
    max_value = max_units;

    adjust_value_by_range(&f3_period, min_value, max_value);

    f3.period = f3_period;

    gtk_spin_button_set_range(GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "f3_period")), min_value, max_value);

    if (f3.on > (f3.period - min_units)) {
        f3.on = f3.period - min_units;
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "f3_on_t")), f3.on);
    }
    set_dc_values_and_widgets();
    update_info_labels();
}


void f1_control_enable(gboolean enable)
{
    gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(builder, "f1_delay")), enable);
    gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(builder, "f1_on_t")), enable);
    gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(builder, "f1_dc")), enable);
    gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(builder, "f1_period")), enable);
    gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(builder, "f1_rb_period")), enable);
    gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(builder, "f1_rb_dc")), enable);
}


void f2_control_enable(gboolean enable)
{
    gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(builder, "f2_delay")), enable);
    gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(builder, "f2_on_t")), enable);
    gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(builder, "f2_dc")), enable);
    gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(builder, "f2_period")), enable);
    gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(builder, "f2_rb_period")), enable);
    gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(builder, "f2_rb_dc")), enable);
}


void f3_control_enable(gboolean enable)
{
    gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(builder, "f3_delay")), enable);
    gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(builder, "f3_on_t")), enable);
    gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(builder, "f3_dc")), enable);
    gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(builder, "f3_period")), enable);
    gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(builder, "f3_rb_period")), enable);
    gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(builder, "f3_rb_dc")), enable);
}


void set_mode(t_mode set_mode)
{
    gboolean f1_active = gtk_switch_get_active(GTK_SWITCH(gtk_builder_get_object(builder, "f1_switch")));
    gboolean f2_active = gtk_switch_get_active(GTK_SWITCH(gtk_builder_get_object(builder, "f2_switch")));
    gboolean f3_active = gtk_switch_get_active(GTK_SWITCH(gtk_builder_get_object(builder, "f3_switch")));

    mode = set_mode;
    guint32 f1_period;

    switch (mode) {

        case MODE_CUSTOM:
            f1_control_enable(TRUE);
            f2_control_enable(TRUE);
            f3_control_enable(TRUE);
            gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(builder, "f1_switch")), TRUE);
            gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(builder, "f2_switch")), TRUE);
            gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(builder, "f3_switch")), TRUE);
            f2_control_enable(f2_active);
            f3_control_enable(f3_active);
            // restore F1 delay upper limit to max_units
            gtk_adjustment_set_upper(GTK_ADJUSTMENT(gtk_builder_get_object(builder, "f1_delay_adjustment")), max_units - min_units);
            // restore F1 period step to 1
            gtk_adjustment_set_step_increment(GTK_ADJUSTMENT(gtk_builder_get_object(builder, "f1_period_adjustment")), 1);
            // restore F1 period range [2 * min_units .. max_units]
            gtk_adjustment_set_lower(GTK_ADJUSTMENT(gtk_builder_get_object(builder, "f1_period_adjustment")), 2 * min_units);
            gtk_adjustment_set_upper(GTK_ADJUSTMENT(gtk_builder_get_object(builder, "f1_period_adjustment")), max_units);
            break;

        case MODE_2_PHASES:
            f1_control_enable(TRUE);
            f2_control_enable(FALSE);
            f3_control_enable(FALSE);
            gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(builder, "f1_switch")), TRUE);
            gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(builder, "f2_switch")), FALSE);
            gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(builder, "f3_switch")), FALSE);
            // copy F1 switch state to F2, turn off F3
            gtk_switch_set_active(GTK_SWITCH(gtk_builder_get_object(builder, "f2_switch")), f1_active);
            gtk_switch_set_active(GTK_SWITCH(gtk_builder_get_object(builder, "f3_switch")), FALSE);
            // set F1 delay to 0
            gtk_spin_button_set_value(GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "f1_delay")), 0);
            gtk_adjustment_set_upper(GTK_ADJUSTMENT(gtk_builder_get_object(builder, "f1_delay_adjustment")), 0);
            // set F1 period step to 2
            gtk_adjustment_set_step_increment(GTK_ADJUSTMENT(gtk_builder_get_object(builder, "f1_period_adjustment")), 2);
            f1_period = gtk_spin_button_get_value(GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "f1_period")));
            f1_period = prev_mod_number(f1_period, 2);
            if (f1_period < (2 * min_units)) {
                f1_period = 2 * min_units;
            }
            gtk_spin_button_set_value(GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "f1_period")), f1_period);
            // set F1 period range
            gtk_adjustment_set_lower(GTK_ADJUSTMENT(gtk_builder_get_object(builder, "f1_period_adjustment")), prev_mod_number(2 * min_units, 2));
            gtk_adjustment_set_upper(GTK_ADJUSTMENT(gtk_builder_get_object(builder, "f1_period_adjustment")), prev_mod_number(max_units, 2));
            // set F2 delay
            set_values_for_phase_mode();
            update_info_labels();
            break;

       case MODE_3_PHASES:
            f1_control_enable(TRUE);
            f2_control_enable(FALSE);
            f3_control_enable(FALSE);
            gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(builder, "f1_switch")), TRUE);
            gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(builder, "f2_switch")), FALSE);
            gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(builder, "f3_switch")), FALSE);
            // copy F1 switch state to F2 and F3
            gtk_switch_set_active(GTK_SWITCH(gtk_builder_get_object(builder, "f2_switch")), f1_active);
            gtk_switch_set_active(GTK_SWITCH(gtk_builder_get_object(builder, "f3_switch")), f1_active);
            // set F1 delay to 0
            gtk_spin_button_set_value(GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "f1_delay")), 0);
            gtk_adjustment_set_upper(GTK_ADJUSTMENT(gtk_builder_get_object(builder, "f1_delay_adjustment")), 0);
            // set F1 period step to 3
            gtk_adjustment_set_step_increment(GTK_ADJUSTMENT(gtk_builder_get_object(builder, "f1_period_adjustment")), 3);
            f1_period = gtk_spin_button_get_value(GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "f1_period")));
            f1_period = prev_mod_number(f1_period, 3);
            if (f1_period < (min_units + 1)) {
                f1_period = min_units + 1;
            }
            gtk_spin_button_set_value(GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "f1_period")), f1_period);
            // set F1 period range
            gtk_adjustment_set_lower(GTK_ADJUSTMENT(gtk_builder_get_object(builder, "f1_period_adjustment")), prev_mod_number(2 * min_units, 3));
            gtk_adjustment_set_upper(GTK_ADJUSTMENT(gtk_builder_get_object(builder, "f1_period_adjustment")), prev_mod_number(max_units, 3));
            // set F2 and F3 delays
            set_values_for_phase_mode();
            update_info_labels();
            break;
    }
}


void set_frequencies()
{
    set_values_for_phase_mode(); // adjust F2 and F3 delay for phase mode

    gtk_spin_button_set_value(GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "f1_delay")), f1.delay);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "f2_delay")), f2.delay);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "f3_delay")), f3.delay);

    gtk_spin_button_set_value(GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "f1_on_t")), f1.on);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "f2_on_t")), f2.on);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "f3_on_t")), f3.on);

    gtk_spin_button_set_value(GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "f1_period")), f1.period);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "f2_period")), f2.period);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "f3_period")), f3.period);

    update_info_labels();
}


void set_dc_values_and_widgets()
{
    char dc_value_string[20];

    // F1
    if (f1.dc_mode) {
        gtk_editable_set_editable(GTK_EDITABLE(gtk_builder_get_object(builder, "f1_on_t")), FALSE);
        gtk_editable_set_editable(GTK_EDITABLE(gtk_builder_get_object(builder, "f1_dc")), TRUE);
        // calculate ON_T from dc value + period
        f1.on = round(0.01 * f1.period * f1.dc_value);
        if (f1.on < min_units) {
            f1.on = min_units;
        }
        if (f1.on > (f1.period - min_units)) {
            f1.on = f1.period - min_units;
        }
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "f1_on_t")), f1.on);
    } else {
        gtk_editable_set_editable(GTK_EDITABLE(gtk_builder_get_object(builder, "f1_on_t")), TRUE);
        gtk_editable_set_editable(GTK_EDITABLE(gtk_builder_get_object(builder, "f1_dc")), FALSE);

        // calculate dc value from ON_T + period
        f1.dc_value = (100.00 * f1.on) / f1.period;
        snprintf(dc_value_string, 10, "%.3f", f1.dc_value);
        fix_float(dc_value_string);
        gtk_entry_set_text(GTK_ENTRY(gtk_builder_get_object(builder, "f1_dc")), dc_value_string);
    }

    // F2
    if (f2.dc_mode) {
        gtk_editable_set_editable(GTK_EDITABLE(gtk_builder_get_object(builder, "f2_on_t")), FALSE);
        gtk_editable_set_editable(GTK_EDITABLE(gtk_builder_get_object(builder, "f2_dc")), TRUE);
        // calculate ON_T from dc value + period
        f2.on = round(0.01 * f2.period * f2.dc_value);
        if (f2.on < min_units) {
            f2.on = min_units;
        }
        if (f2.on > (f2.period - min_units)) {
            f2.on = f2.period - min_units;
        }
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "f2_on_t")), f2.on);
    } else {
        gtk_editable_set_editable(GTK_EDITABLE(gtk_builder_get_object(builder, "f2_on_t")), TRUE);
        gtk_editable_set_editable(GTK_EDITABLE(gtk_builder_get_object(builder, "f2_dc")), FALSE);

        // calculate dc value from ON_T + period
        f2.dc_value = (100.00 * f2.on) / f2.period;
        snprintf(dc_value_string, 10, "%.3f", f2.dc_value);
        fix_float(dc_value_string);
        gtk_entry_set_text(GTK_ENTRY(gtk_builder_get_object(builder, "f2_dc")), dc_value_string);
    }

    // F3
    if (f3.dc_mode) {
        gtk_editable_set_editable(GTK_EDITABLE(gtk_builder_get_object(builder, "f3_on_t")), FALSE);
        gtk_editable_set_editable(GTK_EDITABLE(gtk_builder_get_object(builder, "f3_dc")), TRUE);
        // calculate ON_T from dc value + period
        f3.on = round(0.01 * f3.period * f3.dc_value);
        if (f3.on < min_units) {
            f3.on = min_units;
        }
        if (f3.on > (f3.period - min_units)) {
            f3.on = f3.period - min_units;
        }
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "f3_on_t")), f3.on);
    } else {
        gtk_editable_set_editable(GTK_EDITABLE(gtk_builder_get_object(builder, "f3_on_t")), TRUE);
        gtk_editable_set_editable(GTK_EDITABLE(gtk_builder_get_object(builder, "f3_dc")), FALSE);

        // calculate dc value from ON_T + period
        f3.dc_value = (100.00 * f3.on) / f3.period;
        snprintf(dc_value_string, 10, "%.3f", f3.dc_value);
        fix_float(dc_value_string);
        gtk_entry_set_text(GTK_ENTRY(gtk_builder_get_object(builder, "f3_dc")), dc_value_string);
    }
}


void setup_default_config()
{
    mode = MODE_CUSTOM;

    auto_send = FALSE;

    // Use default values of AVR board
    ticks_per_sec = 200000000; //// DEBUG !!!!
    ticks_per_unit = 1;        //// DEBUG !!!!
    min_units = 3;             //// DEBUG !!!!
    max_units = 0xffffffff;    //// DEBUG !!!!

    f1.enabled = FALSE;
    f1.delay = 0;
    f1.on = 1;
    f1.period = 100;
    f1.dc_mode = FALSE;

    f2.enabled = FALSE;
    f2.delay = 0;
    f2.on = 1;
    f2.period = 100;
    f2.dc_mode = FALSE;

    f3.enabled = FALSE;
    f3.delay = 0;
    f3.on = 1;
    f3.period = 100;
    f3.dc_mode = FALSE;
}


void refresh_ui()
{
    char dc_value_str[20];

    set_frequencies();
    set_dc_values_and_widgets();

    // switches
    gtk_switch_set_active(GTK_SWITCH(gtk_builder_get_object(builder, "f1_switch")), f1.enabled);
    f1_control_enable(f1.enabled);

    gtk_switch_set_active(GTK_SWITCH(gtk_builder_get_object(builder, "f2_switch")), f2.enabled);
    f2_control_enable(f2.enabled);

    gtk_switch_set_active(GTK_SWITCH(gtk_builder_get_object(builder, "f3_switch")), f3.enabled);
    f3_control_enable(f3.enabled);

    // f1 dc mode and value
    if (f1.dc_mode) {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "f1_rb_dc")), TRUE);
        snprintf(dc_value_str, 20, "%.3f", f1.dc_value);
        fix_float(dc_value_str);
        gtk_entry_set_text(GTK_ENTRY(gtk_builder_get_object(builder, "f1_dc")), dc_value_str);
    } else {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "f1_rb_period")), TRUE);
    }

    // f2 dc mode and value
    if (f2.dc_mode) {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "f2_rb_dc")), TRUE);
        snprintf(dc_value_str, 20, "%.3f", f2.dc_value);
        fix_float(dc_value_str);
        gtk_entry_set_text(GTK_ENTRY(gtk_builder_get_object(builder, "f2_dc")), dc_value_str);
    } else {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "f2_rb_period")), TRUE);
    }

    // f3 dc mode and value
    if (f3.dc_mode) {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "f3_rb_dc")), TRUE);
        snprintf(dc_value_str, 20, "%.3f", f3.dc_value);
        fix_float(dc_value_str);
        gtk_entry_set_text(GTK_ENTRY(gtk_builder_get_object(builder, "f3_dc")), dc_value_str);
    } else {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "f3_rb_period")), TRUE);
    }

    // mode
    if (mode == MODE_CUSTOM) {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "rb_custom_mode")), TRUE);
    }
    if (mode == MODE_2_PHASES) {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "rb_2_phase_mode")), TRUE);
    }
    if (mode == MODE_3_PHASES) {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "rb_3_phase_mode")), TRUE);
    }

    set_mode(mode);

    update_info_labels();
}


guint16 crc16_modbus(guint8 *buf, guint16 len)
{
    guint16 crc = 0xffff;
    int pos;
    for (pos = 0; pos < len; pos++) {
        crc ^= (guint16)buf[pos];
        int i;
        for (i = 0; i < 8; i++) {
            if ((crc & 0x0001) != 0) {
                crc >>= 1;
                crc ^= 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}


void set_status_label(t_status_type type, char *label_text)
{
    PangoAttrList *alist = pango_attr_list_new ();
    PangoAttribute *attr;
    PangoFontDescription *df;
    GtkLabel *device_status;

    switch (type) {
        case ST_ERROR:
            attr = pango_attr_foreground_new(0xcccc, 0x0000, 0x0000);
            break;
        case ST_INFO:
            attr = pango_attr_foreground_new(0x4e00, 0x9a00, 0x0000);
            break;
    }

    device_status = GTK_LABEL(gtk_builder_get_object(builder, "device_status"));

    gtk_label_set_text(device_status, label_text);

    pango_attr_list_insert(alist, attr);

    df = pango_font_description_new();
    pango_font_description_set_weight(df, PANGO_WEIGHT_BOLD);

    attr = pango_attr_font_desc_new(df);
    pango_attr_list_insert(alist, attr);

    gtk_label_set_attributes (GTK_LABEL(device_status), alist);

    pango_attr_list_unref(alist);

}


// timer for device connection/disconnection detection, update device status
gboolean timer_handler(GtkWidget *widget)
{
    if (widget == NULL) {
        return FALSE;
    }

    update_devices_list();

    if (device_status == DISCONNECTED) {
        send_cmd(CMD_CAPABILITIES);
    }

    return TRUE;
}

guint32 read_uint32(guint8 *buf)
{
    uint8_t highest_byte = buf[0];
    uint8_t high_byte = buf[1];
    uint8_t low_byte = buf[2];
    uint8_t lowest_byte = buf[3];
    return (highest_byte << 24) | (high_byte << 16) | (low_byte << 8) | lowest_byte;
}

// timer handler function to read received responses from device
gboolean receiver_timer_handler(GtkWidget *widget)
{
    static uint8_t buf[256];
    static int buflen = 0;
    static gboolean prev_received = FALSE;

    if (widget == NULL) {
        return FALSE;
    }

    // Possible responses to receive:

    // 00 + CRC16 = OK, COMMAND EXECUTED
    // 01 + CRC16 = BAD COMMAND (CRC ERROR)
    // 02 + CRC16 = BAD DATA IN EEPROM (CRC ERROR)
    //
    // 04 Capabilities response
    //    byte  0     : Command 0x04
    //    bytes 1-4   : Minimal frequency value, uint32
    //    bytes 5-8   : Maximal frequency value, uint32
    //    bytes 9-12  : CPU ticks per 1 frequency unit, unit32
    //    bytes 13-16 : CPU ticks per 1 second, uint32
    //    bytes 17-18 : CRC16

    int bytes_read;

    if (PSerLib_readBinaryData(serial_device_handle, buf + buflen, 256 - buflen, &bytes_read) == PSL_ERROR_none) {

        buflen += bytes_read;

        if (bytes_read == 0) {
            if (!prev_received) {
                // nothing received previous time and now, reset input buffer
                buflen = 0;
            }
            if (serial_cmd_timeout) {
                if (--serial_cmd_timeout == 0) {
                    device_status = DISCONNECTED;
                    set_status_label(ST_ERROR, "DISCONNECTED");
                }
            }
            prev_received = FALSE;
            return TRUE;
        }

        prev_received = TRUE;

        // chekc if there are enough bytes received
        uint8_t cmd = buf[0];
        if ((cmd != 0) && (cmd != 1) && (cmd != 2) && (cmd != 4)) {
            device_status = DEVICE_ERROR;
            set_status_label(ST_ERROR, "DEVICE ERROR");
            return TRUE;
        }
        int bytes_required = ((cmd == 4) ? 19 : 3);
        if (buflen < bytes_required) {
            // need more bytes, exit now
            return TRUE;
        }
        // check crc
        guint16 crc = crc16_modbus(buf, ((cmd == 4) ? 17 : 1));

        if ((buf[(cmd == 4) ? 17 : 1] != (crc >> 8)) || (buf[(cmd == 4) ? 18 : 2] != (crc & 0xff))) {
            device_status = DEVICE_ERROR;
            set_status_label(ST_ERROR, "DEVICE ERROR");
        } else {
            device_status = CONNECTED;
            enable_sending_widgets(TRUE);
            switch (buf[0]) {
                case 0x00:
                    set_status_label(ST_INFO, "OK");
                    break;
                case 0x01:
                    set_status_label(ST_INFO, "BAD CRC");
                    break;
                case 0x02:
                    set_status_label(ST_INFO, "EEPROM ERROR");
                    break;
                case 0x04:
                    set_status_label(ST_INFO, "OK");
                    min_units = read_uint32(buf + 1);
                    max_units = read_uint32(buf + 5);
                    ticks_per_unit = read_uint32(buf + 9);
                    ticks_per_sec = read_uint32(buf + 13);
                    break;
                default:
                    set_status_label(ST_INFO, "CONNECTED");
            }
        }
        serial_cmd_timeout = 0;
        buflen -= bytes_required;
        memcpy(buf, buf + bytes_required, buflen);

    } else {
        device_status = DEVICE_ERROR;
        set_status_label(ST_ERROR, "DEVICE ERROR");
        prev_received = FALSE;
    }
    return TRUE;
}


char *fix_float(char* str)
{
    char *p = str + strlen(str);
    char *point_p = p;

    while (--p >= str) {
        if (*p == ',') {
            *p = '.';
        }
        if (*p == '.'){
            point_p = p;
        }
    }

    p = str + strlen(str);

    while (--p >= point_p) {
        if ((*p == '0') || (*p == '.')) {
            *p = 0;
        } else {
            break;
        }
    }
    return str;
}


gboolean validate_and_convert_dc_value(const char *str_value, gdouble *dc_value, GtkEntry *dc_entry)
{

    char *str = malloc(strlen(str_value) + 1);
    strcpy(str, str_value);
    fix_float(str);

    char *endptr;

    *dc_value = strtod(str, &endptr);

    gboolean valid_value = ((*dc_value > 0) && (*dc_value < 100) && (*endptr == 0));

    free(str);

    if (!valid_value && strlen(str_value)) {
        char *css = ".invalid_bg {background: #ffcccc;} .invalid_bg:selected {background: #cc4444;}";
        // set red background color
        GtkStyleContext *context;
        GtkCssProvider *provider;

        context = gtk_widget_get_style_context(GTK_WIDGET(dc_entry));
        provider = gtk_css_provider_new();

        gtk_css_provider_load_from_data(provider, css, -1, NULL);
        gtk_style_context_add_class(context, "invalid_bg");
        gtk_style_context_add_provider(context, GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
        g_object_unref(provider);
    } else {
        // set default background color
        GtkStyleContext *context;
        GtkCssProvider *provider;

        context = gtk_widget_get_style_context(GTK_WIDGET(dc_entry));
        provider = gtk_css_provider_new();

        gtk_style_context_remove_class(context, "invalid_bg");
        g_object_unref(provider);
    }

    return valid_value;
}


void set_values_for_phase_mode()
{
    const char *dc_value;

    switch (mode) {

        case MODE_CUSTOM:
            return;

        case MODE_2_PHASES:
            f2.delay = f1.period / 2;
            f2.on = f1.on;
            f2.period = f1.period;
            f2.dc_mode = f1.dc_mode;
            f2.dc_value = f1.dc_value;
            gtk_spin_button_set_value(GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "f2_on_t")), f2.on);
            gtk_spin_button_set_value(GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "f2_delay")), f2.delay);
            gtk_spin_button_set_value(GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "f2_period")), f2.period);
            dc_value = gtk_entry_get_text(GTK_ENTRY(gtk_builder_get_object(builder, "f1_dc")));
            gtk_entry_set_text(GTK_ENTRY(gtk_builder_get_object(builder, "f2_dc")), dc_value);
            break;

        case MODE_3_PHASES:
            f2.delay = f1.period / 3;
            f2.on = f1.on;
            f2.period = f1.period;
            f2.dc_mode = f1.dc_mode;
            f2.dc_value = f1.dc_value;

            gtk_spin_button_set_value(GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "f2_on_t")), f2.on);

            gtk_spin_button_set_value(GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "f2_delay")), f2.delay);

            gtk_spin_button_set_value(GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "f2_period")), f2.period);

            dc_value = gtk_entry_get_text(GTK_ENTRY(gtk_builder_get_object(builder, "f1_dc")));

            gtk_entry_set_text(GTK_ENTRY(gtk_builder_get_object(builder, "f2_dc")), dc_value);

            f3.delay = 2 * (f1.period / 3);
            f3.on = f1.on;
            f3.period = f1.period;
            f3.dc_mode = f1.dc_mode;
            f3.dc_value = f1.dc_value;
            gtk_spin_button_set_value(GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "f3_on_t")), f3.on);
            gtk_spin_button_set_value(GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "f3_delay")), f3.delay);
            gtk_spin_button_set_value(GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "f3_period")), f3.period);
            dc_value = gtk_entry_get_text(GTK_ENTRY(gtk_builder_get_object(builder, "f1_dc")));
            gtk_entry_set_text(GTK_ENTRY(gtk_builder_get_object(builder, "f3_dc")), dc_value);
            break;
    }
}


char *val2str(gdouble x, const char *unit, gboolean hertz)
{
    gboolean allow_decimals = TRUE;
    int p1 = 0;

    while ((fabs(x) < 100) && (p1 > -12)) {
        x *= 10;
        p1--;
    }
    while ((fabs(x) >= 1000) && (p1 < 12)) {
        x /= 10;
        p1++;
    }
    if ((p1 >= 12) || (p1 <= -12)) {
        x = 0;
        p1 = 0;
    }
    x = round(x);
    gdouble intpart;
    int p2 = 0;
    while ((fabs(x) > 0) && (fabs(modf(x / 10, &intpart)) < 0.01)) {
        x = round(x / 10);
        p2++;
    }

    char float_str[20];
    char *strs = malloc(100);

    if (((p1 + p2) >= 6) || (allow_decimals && (p1 > 3))) {
        snprintf(float_str, 20, "%.3f", x * pow(10, p1 + p2 - 6));
        fix_float(float_str);
        snprintf(strs, 100, "%s%s%s", float_str, "M", unit);
        return strs;
    }
    if (((p1 + p2) >= 3) || (allow_decimals && (p1 > 0))) {
        snprintf(float_str, 20, "%.3f", x * pow(10, p1 + p2 - 3));
        fix_float(float_str);
        snprintf(strs, 100, "%s%s%s", float_str, "K", unit);
        return strs;
    }
    if (!hertz) {
        if (p1 <= -12) {
            snprintf(float_str, 20, "%.3f", x * pow(10, p1 + p2 + 12));
            fix_float(float_str);
            snprintf(strs, 100, "%s%s%s", float_str, "p", unit);
            return strs;
        }
        if (p1 <= -9) {
            snprintf(float_str, 20, "%.3f", x * pow(10, p1 + p2 + 9));
            fix_float(float_str);
            snprintf(strs, 100, "%s%s%s", float_str, "n", unit);
            return strs;
        }
        if (p1 <= -6) {
            snprintf(float_str, 20, "%.3f", x * pow(10, p1 + p2 + 6));
            fix_float(float_str);
            snprintf(strs, 100, "%s%s%s", float_str, "u", unit);
            return strs;
        }
        if (p1 <= -3) {
            snprintf(float_str, 20, "%.3f", x * pow(10, p1 + p2 + 3));
            fix_float(float_str);
            snprintf(strs, 100, "%s%s%s", float_str, "m", unit);
            return strs;
        }
    }
    snprintf(float_str, 20, "%.3f", x * pow(10, p1 + p2));
    fix_float(float_str);
    snprintf(strs, 100, "%s%s", float_str, unit);
    return strs;
}


void update_info_labels()
{
    char text[1000];

    char *f1_pulse_width_text = val2str(((1.0 * ticks_per_unit * f1.on) / ticks_per_sec), "s", FALSE);
    char *f1_period_text = val2str(((1.0 * ticks_per_unit * f1.period) / ticks_per_sec), "s", FALSE);
    char *f1_frequency_text = val2str(((1.0 * ticks_per_sec) / (f1.period * ticks_per_unit)), "Hz", TRUE);
    snprintf(text, 1000, "PW = %s     T = %s     F = %s", f1_pulse_width_text, f1_period_text, f1_frequency_text);
    gtk_label_set_text(GTK_LABEL(gtk_builder_get_object(builder, "f1_info")), text);
    free(f1_pulse_width_text);
    free(f1_period_text);
    free(f1_frequency_text);

    char *f2_pulse_width_text = val2str(((1.0 * ticks_per_unit * f2.on) / ticks_per_sec), "s", FALSE);
    char *f2_period_text = val2str(((1.0 * ticks_per_unit * f2.period) / ticks_per_sec), "s", FALSE);
    char *f2_frequency_text = val2str(((1.0 * ticks_per_sec) / (f2.period * ticks_per_unit)), "Hz", TRUE);
    snprintf(text, 1000, "PW = %s     T = %s     F = %s", f2_pulse_width_text, f2_period_text, f2_frequency_text);
    gtk_label_set_text(GTK_LABEL(gtk_builder_get_object(builder, "f2_info")), text);
    free(f2_pulse_width_text);
    free(f2_period_text);
    free(f2_frequency_text);

    char *f3_pulse_width_text = val2str(((1.0 * ticks_per_unit * f3.on) / ticks_per_sec), "s", FALSE);
    char *f3_period_text = val2str(((1.0 * ticks_per_unit * f3.period) / ticks_per_sec), "s", FALSE);
    char *f3_frequency_text = val2str(((1.0 * ticks_per_sec) / (f3.period * ticks_per_unit)), "Hz", TRUE);
    snprintf(text, 1000, "PW = %s     T = %s     F = %s", f3_pulse_width_text, f3_period_text, f3_frequency_text);
    gtk_label_set_text(GTK_LABEL(gtk_builder_get_object(builder, "f3_info")), text);
    free(f3_pulse_width_text);
    free(f3_period_text);
    free(f3_frequency_text);
}


void load_stored_config()
{
    char *home_dir = getenv("HOME");
    char *conf_dir = "/.fg3gtk";
    char *file_name = "/fg3gtk.cfg";
    char *path = malloc(strlen(home_dir) + strlen(conf_dir) + strlen(file_name) + 1);
    sprintf(path,"%s%s%s", home_dir, conf_dir, file_name);

    ini_t *config = ini_load(path);

    if (config == NULL) {
        return;
    } else {
        // mode
        const char *mode_str = ini_get(config, NULL, "mode");
        if (mode_str) {
            if (!strcmp(mode_str, "custom")) {
                mode = MODE_CUSTOM;
            }
            if (!strcmp(mode_str, "2phase")) {
                mode = MODE_2_PHASES;
            }
            if (!strcmp(mode_str, "3phase")) {
                mode = MODE_3_PHASES;
            }
        }
        // serial device
        const char *device_str = ini_get(config, NULL, "device");
        if (device_str) {
            serial_device = strdup(device_str);
        }
        // auto send
        const char *auto_send_str = ini_get(config, NULL, "auto_send");
        if (auto_send_str) {
            auto_send = strcmp(auto_send_str, "true") ? FALSE : TRUE;
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "cb_auto_send")), auto_send);
        }
        // Device capabilities
        ini_sget(config, NULL, "ticks_per_sec", "%u", &ticks_per_sec);
        ini_sget(config, NULL, "ticks_per_unit", "%u", &ticks_per_unit);
        ini_sget(config, NULL, "min_units", "%u", &min_units);
        ini_sget(config, NULL, "max_units", "%u", &max_units);

        // f1
        const char *f1_enabled_str = ini_get(config, "f1", "enabled");
        if (f1_enabled_str) {
            if (!strcmp(f1_enabled_str, "true")) {
                f1.enabled = TRUE;
            } else {
                f1.enabled = FALSE;
            }
        }
        ini_sget(config, "f1", "delay", "%u", &f1.delay);
        ini_sget(config, "f1", "on", "%u", &f1.on);
        ini_sget(config, "f1", "period", "%u", &f1.period);

        const char *f1_dc_mdoe_str = ini_get(config, "f1", "dc_mode");
        if (f1_dc_mdoe_str) {
            if (!strcmp(f1_dc_mdoe_str, "true")) {
                f1.dc_mode = TRUE;
            } else {
                f1.dc_mode = FALSE;
            }
        }
        double f1_dc_value;
        ini_sget(config, "f1", "dc_value", "%lf", &f1_dc_value);
        f1.dc_value = f1_dc_value;
        // f2
        const char *f2_enabled_str = ini_get(config, "f2", "enabled");
        if (f2_enabled_str) {
            if (!strcmp(f2_enabled_str, "true")) {
                f2.enabled = TRUE;
            } else {
                f2.enabled = FALSE;
            }
        }
        ini_sget(config, "f2", "delay", "%u", &f2.delay);
        ini_sget(config, "f2", "on", "%u", &f2.on);
        ini_sget(config, "f2", "period", "%u", &f2.period);
        const char *f2_dc_mdoe_str = ini_get(config, "f2", "dc_mode");
        if (f2_dc_mdoe_str) {
            if (!strcmp(f2_dc_mdoe_str, "true")) {
                f2.dc_mode = TRUE;
            } else {
                f2.dc_mode = FALSE;
            }
        }
        double f2_dc_value;
        ini_sget(config, "f2", "dc_value", "%lf", &f2_dc_value);
        f2.dc_value = f2_dc_value;
        // f3
        const char *f3_enabled_str = ini_get(config, "f3", "enabled");
        if (f3_enabled_str) {
            if (!strcmp(f3_enabled_str, "true")) {
                f3.enabled = TRUE;
            } else {
                f3.enabled = FALSE;
            }
        }
        ini_sget(config, "f3", "delay", "%u", &f3.delay);
        ini_sget(config, "f3", "on", "%u", &f3.on);
        ini_sget(config, "f3", "period", "%u", &f3.period);
        const char *f3_dc_mdoe_str = ini_get(config, "f3", "dc_mode");
        if (f3_dc_mdoe_str) {
            if (!strcmp(f3_dc_mdoe_str, "true")) {
                f3.dc_mode = TRUE;
            } else {
                f3.dc_mode = FALSE;
            }
        }
        double f3_dc_value;
        ini_sget(config, "f3", "dc_value", "%lf", &f3_dc_value);
        f3.dc_value = f3_dc_value;

        ini_free(config);
    }
}


void save_config()
{
    struct stat st;

    char *home_dir = getenv("HOME");
    char *conf_dir = "/.fg3gtk";
    char *file_name = "/fg3gtk.cfg";

    char *path;

    path = malloc(strlen(home_dir) + strlen(conf_dir) + 1);
    sprintf(path,"%s%s", home_dir, conf_dir);

    if (stat(path, &st) != 0) {
        // create dir if doesn't exist
        mkdir(path, 0755);
    }

    path = realloc(path, strlen(home_dir) + strlen(conf_dir) + strlen(file_name) + 1);
    sprintf(path,"%s%s%s", home_dir, conf_dir, file_name);

    FILE *f = fopen(path, "w");
    free(path);

    if (f != NULL) {
        fprintf(f, "# Do not edit, file automatically generated by 'fg3gtk'\n\n");
        // mode
        fprintf(f, "mode = %s\n\n", ((mode == MODE_CUSTOM) ? "custom" : ((mode == MODE_2_PHASES) ? "2phase" : "3phase")));
        // device
        fprintf(f, "device = %s\n\n", ((serial_device == NULL) ? "" : serial_device));
        // auto send
        fprintf(f, "auto_send = %s\n\n", (auto_send ? "true" : "false"));
        //
        fprintf(f, "# Device capabilities\n");
        // ticks per second
        fprintf(f, "ticks_per_sec = %u\n", ticks_per_sec);
        // ticks per unit
        fprintf(f, "ticks_per_unit = %u\n", ticks_per_unit);
        // min units value
        fprintf(f, "min_units = %u\n", min_units);
        // max units value
        fprintf(f, "max_units = %u\n\n", max_units);
        // f1
        fprintf(f, "[f1]\n");
        fprintf(f, "enabled = %s\n", (f1.enabled ? "true" : "false"));
        fprintf(f, "delay = %u\n", f1.delay);
        fprintf(f, "on = %u\n", f1.on);
        fprintf(f, "period = %u\n", f1.period);
        fprintf(f, "dc_mode = %s\n", (f1.dc_mode ? "true" : "false"));
        fprintf(f, "dc_value = %f\n", f1.dc_value);
        // f2
        fprintf(f, "\n[f2]\n");
        fprintf(f, "enabled = %s\n", (f2.enabled ? "true" : "false"));
        fprintf(f, "delay = %u\n", f2.delay);
        fprintf(f, "on = %u\n", f2.on);
        fprintf(f, "period = %u\n", f2.period);
        fprintf(f, "dc_mode = %s\n", (f2.dc_mode ? "true" : "false"));
        fprintf(f, "dc_value = %f\n", f2.dc_value);
        // f3
        fprintf(f, "\n[f3]\n");
        fprintf(f, "enabled = %s\n", (f3.enabled ? "true" : "false"));
        fprintf(f, "delay = %u\n", f3.delay);
        fprintf(f, "on = %u\n", f3.on);
        fprintf(f, "period = %u\n", f3.period);
        fprintf(f, "dc_mode = %s\n", (f3.dc_mode ? "true" : "false"));
        fprintf(f, "dc_value = %f\n", f3.dc_value);

        fclose (f);
    }
}


void update_devices_list()
{
    char **devices;
    int n;

    PSerLib_getAvailablePorts(&devices, &n);

    // check if device list is changed

    gboolean changed = FALSE;

    GtkComboBoxText *cb = GTK_COMBO_BOX_TEXT(gtk_builder_get_object(builder, "device_list"));

    GtkTreeModel *m = gtk_combo_box_get_model(GTK_COMBO_BOX(cb));

    gint items_count = gtk_tree_model_iter_n_children(m, NULL);

    if (items_count != n) {
        changed = TRUE;
    } else {
        // compare each device

        GtkTreeIter iter;
        gtk_tree_model_get_iter_first(m, &iter);
        int i;
        for (i = 0; i < n; i++) {

            gchar *item_text;
            gtk_tree_model_get(m, &iter, 0, &item_text, -1);

            if (strcmp(devices[i], item_text)) {
                changed = TRUE;
                break;
            }

            if (!gtk_tree_model_iter_next(m, &iter)) {
                break; // no more items
            }
        }
    }
    // if changed then remove all items and add new ones
    if (changed) {

        ignore_device_change = TRUE;
        gtk_combo_box_text_remove_all(cb);
        ignore_device_change = FALSE;

        gboolean device_found = FALSE;
        int i;
        for (i = 0; i < n; i++) {

            gtk_combo_box_text_append_text(cb, devices[i]);
            if ((serial_device != NULL) && !strcmp(devices[i], serial_device)) {
                gtk_combo_box_set_active(GTK_COMBO_BOX(cb), i);
                device_found = TRUE;
            }
        }
        if (!device_found) {
            device_status = NOT_SELECTED;
            set_status_label(ST_ERROR, "NOT SELECTED");
            enable_sending_widgets(FALSE);
            PSerLib_close(&serial_device_handle);
        }
    }
}

// Put guint32 to the buffer
void put_uint32(uint8_t* buffer, uint8_t* buflen, guint32 value)
{
    uint8_t highest_byte = (uint8_t)((value & 0xff000000) >> 24);
    uint8_t high_byte = (uint8_t)((value & 0x00ff0000) >> 16);
    uint8_t low_byte = (uint8_t)((value & 0x0000ff00) >> 8);
    uint8_t lowest_byte = (uint8_t)value;
    buffer[(*buflen)++] = highest_byte;
    buffer[(*buflen)++] = high_byte;
    buffer[(*buflen)++] = low_byte;
    buffer[(*buflen)++] = lowest_byte;
}

void send_cmd(t_cmd cmd)
{
    uint8_t buffer[256];
    uint8_t len = 0;

    switch (cmd) {

        case CMD_SET_FREQUENCIES:
            buffer[len++] = 0x01;
            // DELAY F1
            put_uint32(buffer, &len, f1.delay + min_units);
            // ON F1, if ON == 0 then frequency is muted (off)
            put_uint32(buffer, &len, f1.enabled ? f1.on : 0);
            // OFF F1
            put_uint32(buffer, &len, f1.period - f1.on);
            // DELAY F2
            put_uint32(buffer, &len, f2.delay + min_units);
            // ON F2, if ON == 0 then frequency is muted (off)
            put_uint32(buffer, &len, f2.enabled ? f2.on : 0);
            // OFF F2
            put_uint32(buffer, &len, f2.period - f2.on);
            // DELAY F3
            put_uint32(buffer, &len, f3.delay + min_units);
            // ON F3, if ON == 0 then frequency is muted (off)
            put_uint32(buffer, &len, f3.enabled ? f3.on : 0);
            // OFF F3
            put_uint32(buffer, &len, f3.period - f3.on);
            break;

        case CMD_STORE:
            buffer[len++] = 0x02;
            break;

        case CMD_CAPABILITIES:
            buffer[len++] = 0x04;
            break;

        default:
            return;
    }

    // Add CRC16
    guint16 crc = crc16_modbus(buffer, len);
    buffer[len++] = crc >> 8;
    buffer[len++] = crc & 0xff;

    // Send
    int written;
    PSerLib_writeBinaryData(serial_device_handle, buffer, len, &written);

    if (len != written) {
        // TODO: Continue sending in loop?
    }

    serial_cmd_timeout = 3;
}


void enable_sending_widgets(gboolean enable)
{
    gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(builder, "cb_auto_send")), enable);
    gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(builder, "button_store")), enable);
    gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(builder, "button_send")), enable);
}


gboolean auto_send_timer_handler(GtkWidget *widget)
{
    static tfrequencies tf1, tf2, tf3;

    if (widget == NULL) {
        return FALSE;
    }

    if ((device_status == CONNECTED) &&
        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "cb_auto_send"))) &&
        ((f1.enabled != tf1.enabled) || (f1.delay != tf1.delay) || (f1.on != tf1.on) || (f1.period != tf1.period) ||
         (f2.enabled != tf2.enabled) || (f2.delay != tf2.delay) || (f2.on != tf2.on) || (f2.period != tf2.period) ||
         (f3.enabled != tf3.enabled) || (f3.delay != tf3.delay) || (f3.on != tf3.on) || (f3.period != tf3.period))) {

        send_cmd(CMD_SET_FREQUENCIES);

        tf1.enabled = f1.enabled;
        tf1.delay = f1.delay;
        tf1.on = f1.on;
        tf1.period = f1.period;

        tf2.enabled = f2.enabled;
        tf2.delay = f2.delay;
        tf2.on = f2.on;
        tf2.period = f2.period;

        tf3.enabled = f3.enabled;
        tf3.delay = f3.delay;
        tf3.on = f3.on;
        tf3.period = f3.period;
    }
    return TRUE;
}
