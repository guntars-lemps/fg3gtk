#include <gtk/gtk.h>
#include <string.h>
#include <stdlib.h>
#include <locale.h>
#include <math.h>
#include "fg3glade.h"

//make clean && make && ./fg3gtk

#define MCU_FREQUENCY        16000000
#define MCU_CLOCKS_PER_LOOP        21


GtkBuilder  *builder;


typedef enum {MODE_CUSTOM, MODE_2_PHASES, MODE_3_PHASES} t_mode;

typedef enum {ST_ERROR, ST_INFO} t_status_type;

t_mode mode = MODE_CUSTOM;

typedef struct
{
    gboolean enabled;
    guint16 delay;
    guint16 on;
    guint16 period;
    gboolean dc_mode;
    gdouble dc_value;
} tfrequencies;


tfrequencies f1, f2, f3;


void setup_default_config();
guint16 crc16_modbus(guint8*, guint16);
void f1_control_enable(gboolean);
void f2_control_enable(gboolean);
void f3_control_enable(gboolean);
void set_mode(t_mode);
void set_status_label(t_status_type, char*);
gboolean time_handler(GtkWidget *widget);
void set_dc_values_and_widgets();
char* fix_float(char* str);
gboolean validate_and_convert_dc_value(const char *str_value, gdouble *dc_value, GtkEntry *dc_entry);

// todo
/*

1. GTK widgets
    1.3. 2-phase and 3-phase mode - adjust F2 / F3 values from F1
    1.4. frequencies info labels - on period in ms, duty cycle %, period in ms (s), frequency (Hz)
    1.5. pēdējo settingu ielāde (paņemt kodu no oscope2100)

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
    printf("start\n");

    GtkWidget *window;


    gtk_init(&argc, &argv);

    builder = gtk_builder_new();

    gtk_builder_add_from_string(builder, (char*) __fg3_glade, __fg3_glade_len, NULL);

    window = GTK_WIDGET(gtk_builder_get_object(builder, "applicationwindow1"));
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

    set_status_label(ST_ERROR, "DISCONNECTED");

    g_timeout_add(1000, (GSourceFunc)time_handler, (gpointer)window);

    setlocale(LC_NUMERIC,"C");

    gtk_main();

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
    printf("f1_switch_activate()\n");

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

    if (active) {
        printf("active\n");
    } else {
        printf("not active\n");
    }
}


void f2_switch_activate()
{
    printf("f2_switch_activate()\n");

    gboolean active = gtk_switch_get_active(GTK_SWITCH(gtk_builder_get_object(builder, "f2_switch")));

    if (active) {
        printf("active\n");
        f2.enabled = TRUE;
        if (mode == MODE_CUSTOM) {
            f2_control_enable(TRUE);
        }
    } else {
        printf("not active\n");
        f2.enabled = FALSE;
        if (mode == MODE_CUSTOM) {
            f2_control_enable(FALSE);
        }
    }
}


void f3_switch_activate()
{
    printf("f3_switch_activate()\n");
    // on/off switches - disable/enable
    // f1_switch - GtkSwitch

    gboolean active = gtk_switch_get_active(GTK_SWITCH(gtk_builder_get_object(builder, "f3_switch")));

    if (active) {
        f3.enabled = TRUE;
        if (mode == MODE_CUSTOM) {
            f3_control_enable(TRUE);
        }
    } else {
        printf("not active\n");
        f3.enabled = FALSE;
        if (mode == MODE_CUSTOM) {
            f3_control_enable(FALSE);
        }
    }

}


// radio button ON T vs DC change
void rb_f1_toggle()
{
    gboolean active;

    printf("rb_f1_toggle()\n");

    active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "f1_rb_dc")));
    if (active) {
        printf("F1 dc mode enabled\n");
        f1.dc_mode = TRUE;
    } else {
        printf("F1 dc mode disabled\n");
        f1.dc_mode = FALSE;
    }
    set_dc_values_and_widgets();
}


void rb_f2_toggle()
{
    gboolean active;

    printf("rb_f2_toggle()\n");

    active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "f2_rb_dc")));
    if (active) {
        printf("F2 dc mode enabled\n");
        f2.dc_mode = TRUE;
    } else {
        printf("F2 dc mode disabled\n");
        f2.dc_mode = FALSE;
    }
    set_dc_values_and_widgets();
}


void rb_f3_toggle()
{
    gboolean active;

    printf("rb_f3_toggle()\n");

    active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "f3_rb_dc")));
    if (active) {
        printf("F3 dc mode enabled\n");
        f3.dc_mode = TRUE;
    } else {
        printf("F3 dc mode disabled\n");
        f3.dc_mode = FALSE;
    }
    set_dc_values_and_widgets();
}


// radio button frequencies mode change
void rb_mode_toggle()
{
    gboolean active;

    printf("rb_mode_toggle()\n");

    active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "rb_custom_mode")));
    if (active) {
        printf("Custom mode\n");
        set_mode(MODE_CUSTOM);
        return;
    }

    active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "rb_2_phase_mode")));
    if (active) {
        printf("2 phase mode\n");
        set_mode(MODE_2_PHASES);
        return;
    }

    active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "rb_3_phase_mode")));
    if (active) {
        printf("3 phase mode\n");
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
    printf("f1_delay_adjustment_change()\n");
    gint32 f1_delay = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "f1_delay")));
    printf("f1 delay = %d\n", f1_delay);

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
    printf("f2_delay_adjustment_change()\n");
    gint32 f2_delay = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "f2_delay")));
    printf("f2 delay = %d\n", f2_delay);

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
    printf("f3_delay_adjustment_change()\n");
    gint32 f3_delay = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "f3_delay")));
    printf("f3 delay = %d\n", f3_delay);

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
    printf("f1_on_adjustment_change()\n");

    gint32 f1_on = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "f1_on_t")));
    printf("f1 on = %d\n", f1_on);

    if (f1_on < 1) {
        f1_on = 1;
    }
    if (f1_on > (f1.period - 1)) {
        f1_on = f1.period - 1;
    }
    f1.on = f1_on;
    set_dc_values_and_widgets();
}


void f2_on_adjustment_change()
{
    printf("f2_on_adjustment_change()\n");

    gint32 f2_on = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "f2_on_t")));
    printf("f2 on = %d\n", f2_on);

    if (f2_on < 1) {
        f2_on = 1;
    }
    if (f2_on > (f2.period - 1)) {
        f2_on = f2.period - 1;
    }
    f2.on = f2_on;
    set_dc_values_and_widgets();
}


void f3_on_adjustment_change()
{
    printf("f3_on_adjustment_change()\n");

    gint32 f3_on = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "f3_on_t")));
    printf("f3 on = %d\n", f3_on);

    if (f3_on < 1) {
        f3_on = 1;
    }
    if (f3_on > (f3.period - 1)) {
        f3_on = f3.period - 1;
    }
    f3.on = f3_on;
    set_dc_values_and_widgets();
}


// duty cycle change
void f1_dc_change()
{
    printf("f1_dc_change()\n");

    GtkEntry *dc_entry = GTK_ENTRY(gtk_builder_get_object(builder, "f1_dc"));

    const char *str_value = gtk_entry_get_text(dc_entry);
    gdouble dc_value;
    if (validate_and_convert_dc_value(str_value, &dc_value, dc_entry)) {
        f1.dc_value = dc_value;
        set_dc_values_and_widgets();
    }
}


void f2_dc_change()
{
    printf("f2_dc_change()\n");

    GtkEntry *dc_entry = GTK_ENTRY(gtk_builder_get_object(builder, "f2_dc"));

    const char *str_value = gtk_entry_get_text(dc_entry);
    gdouble dc_value;
    if (validate_and_convert_dc_value(str_value, &dc_value, dc_entry)) {
        f2.dc_value = dc_value;
        set_dc_values_and_widgets();
    }
}


void f3_dc_change()
{
    printf("f3_dc_change()\n");

    GtkEntry *dc_entry = GTK_ENTRY(gtk_builder_get_object(builder, "f3_dc"));

    const char *str_value = gtk_entry_get_text(dc_entry);
    gdouble dc_value;
    if (validate_and_convert_dc_value(str_value, &dc_value, dc_entry)) {
        f3.dc_value = dc_value;
        set_dc_values_and_widgets();
    }
}


// period adjustment change
void f1_period_adjustment_change()
{
    printf("f1_period_adjustment_change()\n");

    gint32 f1_period = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "f1_period")));
    printf("f1 period = %d\n", f1_period);

    if (f1_period < 2) {
        f1_period = 2;
    }
    if (f1_period > 65535) {
        f1_period = 65535;
    }
    f1.period = f1_period;

    gtk_adjustment_set_upper(GTK_ADJUSTMENT(gtk_builder_get_object(builder, "f1_on_adjustment")), (f1.period - 1));
    if (f1.on > (f1.period - 1)) {
        f1.on = f1.period - 1;
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "f1_on_t")), f1.on);
    }
    set_dc_values_and_widgets();
}


void f2_period_adjustment_change()
{
    printf("f2_period_adjustment_change()\n");

    gint32 f2_period = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "f2_period")));
    printf("f2 period = %d\n", f2_period);

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
}


void f3_period_adjustment_change()
{
    printf("f3_period_adjustment_change()\n");


    gint32 f3_period = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "f3_period")));
    printf("f3 period = %d\n", f3_period);

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
}


/***************/


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

    switch (mode) {

        case MODE_CUSTOM:
            gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(builder, "f1_switch")), TRUE);
            gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(builder, "f2_switch")), TRUE);
            gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(builder, "f3_switch")), TRUE);
            f2_control_enable(f2_active);
            f3_control_enable(f3_active);
            // restore F1 delay upper limit to 65535
            gtk_adjustment_set_upper(GTK_ADJUSTMENT(gtk_builder_get_object(builder, "f1_delay_adjustment")), 65535);
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
            break;
    }
}


void adjust_phase_values()
{
     switch (mode) {

        case MODE_CUSTOM:
            break;

        case MODE_2_PHASES:
            // copy f1 period and pulse, set f1 delay to 0, calculate f2 delay
            break;

        case MODE_3_PHASES:
            // copy f1 period and pulse to f2 and f3,
            // set f1 delay to 0, calculate f2 and f3 delay
            break;
    }
}


void set_frequencies()
{
    adjust_phase_values(); // adjust values if 2 or 3 phase mode

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
}


void set_dc_values_and_widgets()
{

    char dc_value_string[20];
    // F1

    if (f1.dc_mode) {
        printf("enable dc entry\n");
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
        printf("disable dc entry\n");
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
        printf("enable dc entry\n");
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
        printf("disable dc entry\n");
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
        printf("enable dc entry\n");
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
        printf("disable dc entry\n");
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

    set_frequencies();
    set_dc_values_and_widgets();

    gtk_switch_set_active(GTK_SWITCH(gtk_builder_get_object(builder, "f1_switch")), f1.enabled);
    f1_control_enable(f1.enabled);

    gtk_switch_set_active(GTK_SWITCH(gtk_builder_get_object(builder, "f2_switch")), f2.enabled);
    f2_control_enable(f2.enabled);

    gtk_switch_set_active(GTK_SWITCH(gtk_builder_get_object(builder, "f3_switch")), f3.enabled);
    f3_control_enable(f3.enabled);

    set_mode(mode);
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
    // find last char
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