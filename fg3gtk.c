#include <gtk/gtk.h>
#include <string.h>
#include <stdlib.h>
#include <locale.h>
#include <math.h>
#include "fg3glade.h"
#include "fg3gtk.h"
#include "ini.h"

//make clean && make && ./fg3gtk

GtkBuilder  *builder;

tfrequencies f1, f2, f3;

// todo
/*

1. config store on close, load on startup

2. rs-232 komunikācijas (paņemt kodu no RPM mērītaja)
    2.1. device pong ping when disconnected - timer_handle() funkcijā
         *** startējot sūta ping-pong komandu kamēr saņem atbildi !!! vai arī pēc tty errora (kamēr ir diskonektēts)
    2.2. send on button, auto send
    2.3. eeprom save button

3. debugging

4. uztaisīt normālus make un config

*/


int main(int argc, char *argv[])
{
    GtkWidget *window;

    gtk_init(&argc, &argv);

    builder = gtk_builder_new();

    gtk_builder_add_from_string(builder, (char*) __fg3_glade, __fg3_glade_len, NULL);

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

    setup_default_config();

    load_stored_config();

    refresh_ui();

    set_status_label(ST_ERROR, "DISCONNECTED");

    g_timeout_add(1000, (GSourceFunc)time_handler, (gpointer)window);

    setlocale(LC_NUMERIC,"C");

    gtk_main();

    save_config();

    return 0;
}


void button_send_click()
{
    printf("button_send_click()\n");
}


void button_store_click()
{
    printf("button_store_click()\n");
}


void cb_auto_send_toggle()
{
    printf("cb_auto_send_toggle()\n");
}


void select_device_change()
{
    printf("select_device_changed()\n");
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
          printf("test 1111\n");
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


// delay adjustment change
void f1_delay_adjustment_change()
{
    gint32 f1_delay = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "f1_delay")));

    if (f1_delay < 0) {
        f1_delay = 0;
    }
    if (f1_delay > 65535) {
        f1_delay = 65535;
    }
    f1.delay = f1_delay;
}


void f2_delay_adjustment_change()
{
    gint32 f2_delay = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "f2_delay")));

    if (f2_delay < 0) {
        f2_delay = 0;
    }
    if (f2_delay > 65535) {
        f2_delay = 65535;
    }
    f2.delay = f2_delay;
}


void f3_delay_adjustment_change()
{
    gint32 f3_delay = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "f3_delay")));

    if (f3_delay < 0) {
        f3_delay = 0;
    }
    if (f3_delay > 65535) {
        f3_delay = 65535;
    }
    f3.delay = f3_delay;
}


// on adjustment change
void f1_on_adjustment_change()
{
    gint32 f1_on = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "f1_on_t")));

    if (f1_on < 1) {
        f1_on = 1;
    }
    if (f1_on > (f1.period - 1)) {
        f1_on = f1.period - 1;
    }
    f1.on = f1_on;
    set_values_for_phase_mode();
    set_dc_values_and_widgets();
    update_info_labels();
}


void f2_on_adjustment_change()
{
    gint32 f2_on = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "f2_on_t")));

    if (f2_on < 1) {
        f2_on = 1;
    }
    if (f2_on > (f2.period - 1)) {
        f2_on = f2.period - 1;
    }
    f2.on = f2_on;
    set_dc_values_and_widgets();
    update_info_labels();
}


void f3_on_adjustment_change()
{
    gint32 f3_on = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "f3_on_t")));

    if (f3_on < 1) {
        f3_on = 1;
    }
    if (f3_on > (f3.period - 1)) {
        f3_on = f3.period - 1;
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
    gint32 f1_period = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "f1_period")));

    switch (mode) {
        case MODE_CUSTOM:
            if (f1_period < 2) {
                f1_period = 2;
            }
            if (f1_period > 65535) {
                f1_period = 65535;
            }
            break;

        case MODE_2_PHASES:
            f1_period = 2 * (f1_period / 2);

            if (f1_period < 2) {
                f1_period = 2;
            }
            if (f1_period > 65534) {
                f1_period = 65534;
            }
            break;

       case MODE_3_PHASES:
            f1_period = 3 * (f1_period / 3);

            if (f1_period < 3) {
                f1_period = 3;
            }
            if (f1_period > 65535) {
                f1_period = 65535;
            }
            break;
    }

    gtk_spin_button_set_value(GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "f1_period")), f1_period);
    set_values_for_phase_mode();
    f1.period = f1_period;

    // set F1 ON upper limit
    gtk_adjustment_set_upper(GTK_ADJUSTMENT(gtk_builder_get_object(builder, "f1_on_adjustment")), (f1.period - 1));
    if (f1.on > (f1.period - 1)) {
        f1.on = f1.period - 1;
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "f1_on_t")), f1.on);
    }
    set_dc_values_and_widgets();
    update_info_labels();
}


void f2_period_adjustment_change()
{
    gint32 f2_period = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "f2_period")));

    if (f2_period < 2) {
        f2_period = 2;
    }
    if (f2_period > 65535) {
        f2_period = 65535;
    }
    f2.period = f2_period;

    gtk_adjustment_set_upper(GTK_ADJUSTMENT(gtk_builder_get_object(builder, "f2_on_adjustment")), (f2.period - 1));
    if (f2.on > (f2.period - 1)) {
        f2.on = f2.period - 1;
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "f2_on_t")), f2.on);
    }
    set_dc_values_and_widgets();
    update_info_labels();
}


void f3_period_adjustment_change()
{
    gint32 f3_period = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "f3_period")));

    if (f3_period < 2) {
        f3_period = 2;
    }
    if (f3_period > 65535) {
        f3_period = 65535;
    }
    f3.period = f3_period;

    gtk_adjustment_set_upper(GTK_ADJUSTMENT(gtk_builder_get_object(builder, "f3_on_adjustment")), (f3.period - 1));
    if (f3.on > (f3.period - 1)) {
        f3.on = f3.period - 1;
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
    gint32 f1_period;

    switch (mode) {

        case MODE_CUSTOM:
            gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(builder, "f1_switch")), TRUE);
            gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(builder, "f2_switch")), TRUE);
            gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(builder, "f3_switch")), TRUE);
            f2_control_enable(f2_active);
            f3_control_enable(f3_active);
            // restore F1 delay upper limit to 65535
            gtk_adjustment_set_upper(GTK_ADJUSTMENT(gtk_builder_get_object(builder, "f1_delay_adjustment")), 65535);
            // restore F1 period step to 1
            gtk_adjustment_set_step_increment(GTK_ADJUSTMENT(gtk_builder_get_object(builder, "f1_period_adjustment")), 1);
            // restore F1 period range [2..65535]
            gtk_adjustment_set_lower(GTK_ADJUSTMENT(gtk_builder_get_object(builder, "f1_period_adjustment")), 2);
            gtk_adjustment_set_upper(GTK_ADJUSTMENT(gtk_builder_get_object(builder, "f1_period_adjustment")), 65535);
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
            f1_period = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "f1_period")));
            f1_period = 2 * (f1_period / 2);
            if (f1_period < 2) {
                f1_period = 2;
            }
            gtk_spin_button_set_value(GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "f1_period")), f1_period);
            // set F1 period range [2..65534]
            gtk_adjustment_set_lower(GTK_ADJUSTMENT(gtk_builder_get_object(builder, "f1_period_adjustment")), 2);
            gtk_adjustment_set_upper(GTK_ADJUSTMENT(gtk_builder_get_object(builder, "f1_period_adjustment")), 65534);
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
            f1_period = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "f1_period")));
            f1_period = 3 * (f1_period / 3);
            if (f1_period < 3) {
                f1_period = 3;
            }
            gtk_spin_button_set_value(GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "f1_period")), f1_period);
            // set F1 period range [3..65535]
            gtk_adjustment_set_lower(GTK_ADJUSTMENT(gtk_builder_get_object(builder, "f1_period_adjustment")), 3);
            gtk_adjustment_set_upper(GTK_ADJUSTMENT(gtk_builder_get_object(builder, "f1_period_adjustment")), 65535);
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

    gtk_adjustment_set_upper(GTK_ADJUSTMENT(gtk_builder_get_object(builder, "f1_on_adjustment")), (f1.period - 1));
    gtk_adjustment_set_upper(GTK_ADJUSTMENT(gtk_builder_get_object(builder, "f2_on_adjustment")), (f2.period - 1));
    gtk_adjustment_set_upper(GTK_ADJUSTMENT(gtk_builder_get_object(builder, "f3_on_adjustment")), (f3.period - 1));

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
        if (f1.on < 1) {
            f1.on = 1;
        }
        if (f1.on > (f1.period - 1)) {
            f1.on = f1.period - 1;
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
        if (f2.on < 1) {
            f2.on = 1;
        }
        if (f2.on > (f2.period - 1)) {
            f2.on = f2.period - 1;
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
        if (f3.on < 1) {
            f3.on = 1;
        }
        if (f3.on > (f3.period - 1)) {
            f3.on = f3.period - 1;
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
    set_mode(mode);

    set_frequencies();
    set_dc_values_and_widgets();

    gtk_switch_set_active(GTK_SWITCH(gtk_builder_get_object(builder, "f1_switch")), f1.enabled);
    f1_control_enable(f1.enabled);

    gtk_switch_set_active(GTK_SWITCH(gtk_builder_get_object(builder, "f2_switch")), f2.enabled);
    f2_control_enable(f2.enabled);

    gtk_switch_set_active(GTK_SWITCH(gtk_builder_get_object(builder, "f3_switch")), f3.enabled);
    f3_control_enable(f3.enabled);

    update_info_labels();
}


// 01 20 34 56 ab ce 00 55
// 0x996D
/*
guint8 test_crc[] = {0x01, 0x20, 0x34, 0x56, 0xab, 0xce, 0x00, 0x55};
guint8 buflen = 8;
guint16 crc;

crc = crc16_modbus(test_crc, buflen);
printf("CRC16 = %04x\n", crc);
*/
guint16 crc16_modbus(guint8 *buf, guint16 len)
{
    guint16 crc = 0xffff;

    for (int pos = 0; pos < len; pos++) {
        crc ^= (guint16)buf[pos];

        for (int i = 0; i < 8; i++) {
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


gboolean time_handler(GtkWidget *widget)
{
    if (widget == NULL) {
        return FALSE;
    }
    // printf("Tick\n");

    // gtk_widget_queue_draw(widget);

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


char *val2str(gdouble x, const char *unit)
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
    snprintf(float_str, 20, "%.3f", x * pow(10, p1 + p2));
    fix_float(float_str);
    snprintf(strs, 100, "%s%s", float_str, unit);
    return strs;
}


void update_info_labels()
{
    char text[1000];

    char *f1_pulse_width_text = val2str(((1.0 * MCU_CLOCKS_PER_LOOP * f1.on) / MCU_FREQUENCY), "s");
    char *f1_period_text = val2str(((1.0 * MCU_CLOCKS_PER_LOOP * f1.period) / MCU_FREQUENCY), "s");
    char *f1_frequency_text = val2str(((1.0 * MCU_FREQUENCY) / (f1.period * MCU_CLOCKS_PER_LOOP)), "Hz");
    snprintf(text, 1000, "PW = %s     T = %s     F = %s", f1_pulse_width_text, f1_period_text, f1_frequency_text);
    gtk_label_set_text(GTK_LABEL(gtk_builder_get_object(builder, "f1_info")), text);
    free(f1_pulse_width_text);
    free(f1_period_text);
    free(f1_frequency_text);

    char *f2_pulse_width_text = val2str(((1.0 * MCU_CLOCKS_PER_LOOP * f2.on) / MCU_FREQUENCY), "s");
    char *f2_period_text = val2str(((1.0 * MCU_CLOCKS_PER_LOOP * f2.period) / MCU_FREQUENCY), "s");
    char *f2_frequency_text = val2str(((1.0 * MCU_FREQUENCY) / (f2.period * MCU_CLOCKS_PER_LOOP)), "Hz");
    snprintf(text, 1000, "PW = %s     T = %s     F = %s", f2_pulse_width_text, f2_period_text, f2_frequency_text);
    gtk_label_set_text(GTK_LABEL(gtk_builder_get_object(builder, "f2_info")), text);
    free(f2_pulse_width_text);
    free(f2_period_text);
    free(f2_frequency_text);

    char *f3_pulse_width_text = val2str(((1.0 * MCU_CLOCKS_PER_LOOP * f3.on) / MCU_FREQUENCY), "s");
    char *f3_period_text = val2str(((1.0 * MCU_CLOCKS_PER_LOOP * f3.period) / MCU_FREQUENCY), "s");
    char *f3_frequency_text = val2str(((1.0 * MCU_FREQUENCY) / (f3.period * MCU_CLOCKS_PER_LOOP)), "Hz");
    snprintf(text, 1000, "PW = %s     T = %s     F = %s", f3_pulse_width_text, f3_period_text, f3_frequency_text);
    gtk_label_set_text(GTK_LABEL(gtk_builder_get_object(builder, "f3_info")), text);
    free(f3_pulse_width_text);
    free(f3_period_text);
    free(f3_frequency_text);
}


void load_stored_config()
{
/*  char *home = getenv("HOME");
  defdir+="/.oscope2100";
  struct stat st;
  if (stat(defdir.c_str(),&st)!=0)
  {
    //std::cout << "Dir not exists, creating "<<std::endl;
    mkdir (defdir.c_str(),0755);
  }
  std::string filename=defdir+"/last.cfg";
  FILE *file;
  if ((file=fopen(filename.c_str(),"a"))==NULL)
  {
    std::cout <<"Cannot open or create file "<<filename<<std::endl;
    exit(1);
  }
  fclose(file);
  ConfigFile config(filename);
  //
  int tmpint;
  config.readInto(tmpint,"mode",int(MODE_NORMAL));
  switch (tmpint)
  {
    case MODE_NORMAL:ui_mode=MODE_NORMAL;break;
    case MODE_FZOOM:ui_mode=MODE_FZOOM;break;
    case MODE_XY:ui_mode=MODE_XY;break;
    default:ui_mode=MODE_NORMAL;
  }
  config.readInto(ui_time,"time",9);
  if (ui_time<0) ui_time=0;
  if (ui_time>=TCOUNT) ui_time=TCOUNT-1;
  //
  config.readInto(tmpint,"ch1_on",1);
  ui_ch1_on=(tmpint!=0);
  config.readInto(tmpint,"ch1_flip",0);
  ui_ch1_flip=(tmpint!=0);
  config.readInto(ui_ch1_voltage,"ch1_voltage",5);
  if (ui_ch1_voltage<0) ui_ch1_voltage=0;
  if (ui_ch1_voltage>=VCOUNT) ui_ch1_voltage=VCOUNT-1;
  config.readInto(tmpint,"ch1_mode",int(CHMODE_DC));
  switch (tmpint)
  {
    case CHMODE_DC:ui_ch1_mode=CHMODE_DC;break;
    case CHMODE_AC:ui_ch1_mode=CHMODE_AC;break;
    case CHMODE_GND:ui_ch1_mode=CHMODE_GND;break;
    default:ui_ch1_mode=CHMODE_DC;
  }
  config.readInto(tmpint,"ch1_mul",int(X1));
  switch (tmpint)
  {
    case X1:ui_ch1_mul=X1;break;
    case X10:ui_ch1_mul=X10;break;
    case X100:ui_ch1_mul=X100;break;
    default:ui_ch1_mul=X1;
  }
  //
  config.readInto(tmpint,"ch2_on",1);
  ui_ch2_on=(tmpint!=0);
  config.readInto(tmpint,"ch2_flip",0);
  ui_ch2_flip=(tmpint!=0);
  config.readInto(ui_ch2_voltage,"ch2_voltage",5);
  if (ui_ch2_voltage<0) ui_ch2_voltage=0;
  if (ui_ch2_voltage>=VCOUNT) ui_ch2_voltage=VCOUNT-1;
  config.readInto(tmpint,"ch2_mode",int(CHMODE_DC));
  switch (tmpint)
  {
    case CHMODE_DC:ui_ch2_mode=CHMODE_DC;break;
    case CHMODE_AC:ui_ch2_mode=CHMODE_AC;break;
    case CHMODE_GND:ui_ch2_mode=CHMODE_GND;break;
    default:ui_ch2_mode=CHMODE_DC;
  }
  config.readInto(tmpint,"ch2_mul",int(X1));
  switch (tmpint)
  {
    case X1:ui_ch2_mul=X1;break;
    case X10:ui_ch2_mul=X10;break;
    case X100:ui_ch2_mul=X100;break;
    default:ui_ch2_mul=X1;
  }
  //
  config.readInto(tmpint,"trigger",int(TR_OFF));
  switch (tmpint)
  {
    case 0:ui_trigger=TR_OFF;break;
    case 1:ui_trigger=TR_CH1;break;
    case 2:ui_trigger=TR_CH2;break;
    case 3:ui_trigger=TR_EXT;break;
    default:ui_trigger=TR_OFF;
  }
  config.readInto(ui_edge,"edge",0);
  //
  config.readInto(tmpint,"headers",1);
  ui_export_headers=(tmpint!=0);
  config.readInto(tmpint,"separator",0x09);
  ui_separator=tmpint;
  */

    //[owner]
    //name = John Doe
    //organization = Acme Widgets Inc.

    //[database]
    //; use IP address in case network name resolution is not working
    //server = 192.0.2.62
    //port = 143
    //file = "payroll.dat"


    // NULL is returned if the file cannot be loaded.
    ini_t *config = ini_load("config.ini");


    // Given a section and a key the corresponding value is returned if it exists.
    // If the section argument is NULL then all sections are searched.
    const char *name = ini_get(config, "owner", "name");
    if (name) {
        printf("name: %s\n", name);
    }


    // takes the same arguments as ini_get() with the addition of a
    // scanf format string and a pointer for where to store the value.
    const char *server = "default";
    int port = 80;

    ini_sget(config, "database", "server", NULL, &server);
    ini_sget(config, "database", "port", "%d", &port);

    // destroy config and invalidates all string pointers returned by the library.
    ini_free(config);

}


void save_config()
{
    struct stat st;

mode = custom / 2phase / 3phase


    char *home = getenv("HOME");
  defdir+="/.oscope2100";
  if (stat(defdir.c_str(),&st)!=0)
  {
    //std::cout << "Dir not exists, creating "<<std::endl;
    mkdir (defdir.c_str(),0755);
  }
  //
  std::string filename=defdir+"/last.cfg";
  std::ofstream out(filename.c_str(), std::ios::out | std::ios::binary);
  if (!out)
  {
    std::cout << "Cannot open file "<<filename<<std::endl;
  }
  else
  {
    out<<"# Do not edit, file automatically generated by 'Oscope2100' !!!"<<std::endl;
    out<<std::endl;
    out<<"# Time and mode settings"<<std::endl;
    out<<"mode="<<ui_mode<<std::endl;
    out<<"time="<<ui_time<<std::endl;
    out<<std::endl;
    //
    out<<"# CH1 settings"<<std::endl;
    out<<"ch1_on="<<ui_ch1_on<<std::endl;
    out<<"ch1_flip="<<ui_ch1_flip<<std::endl;
    out<<"ch1_voltage="<<ui_ch1_voltage<<std::endl;
    out<<"ch1_mode="<<ui_ch1_mode<<std::endl;
    out<<"ch1_mul="<<ui_ch1_mul<<std::endl;
    out<<std::endl;
    //
    out<<"# CH2 settings"<<std::endl;
    out<<"ch2_on="<<ui_ch2_on<<std::endl;
    out<<"ch2_flip="<<ui_ch2_flip<<std::endl;
    out<<"ch2_voltage="<<ui_ch2_voltage<<std::endl;
    out<<"ch2_mode="<<ui_ch2_mode<<std::endl;
    out<<"ch2_mul="<<ui_ch2_mul<<std::endl;
    out<<std::endl;
    //
    out<<"# Trigger settings"<<std::endl;
    out<<"trigger="<<ui_trigger<<std::endl;
    out<<"edge="<<ui_edge<<std::endl;
    out<<std::endl;
    //
    out<<"# Exporting settings"<<std::endl;
    out<<"headers="<<ui_export_headers<<std::endl;
    out<<"separator="<<int(ui_separator)<<std::endl;
    out<<std::endl;
    //
    out.close();
  }
}