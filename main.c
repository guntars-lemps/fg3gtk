#include <gtk/gtk.h>
#include "fg3glade.h"

//make clean && make && ./fg3gtk

#define MCU_FREQUENCY        16000000
#define MCU_CLOCKS_PER_LOOP        21


GtkBuilder  *builder;


typedef enum {MODE_CUSTOM, MODE_2_PHASES, MODE_3_PHASES} t_mode;

typedef enum {ST_ERROR, ST_INFO} t_status_type;

t_mode mode = MODE_CUSTOM;

//guint8;
//guint16;

typedef struct
{
    gboolean f1_enabled;
    guint16 f1_delay;
    guint16 f1_on;
    guint16 f1_period;

    gboolean f2_enabled;
    guint16 f2_delay;
    guint16 f2_on;
    guint16 f2_period;

    gboolean f3_enabled;
    guint16 f3_delay;
    guint16 f3_on;
    guint16 f3_period;
} tfrequencies;


tfrequencies f;


void setup_default_config();

guint16 crc16_modbus(guint8*, guint16);

void f1_control_enable(gboolean);

void f2_control_enable(gboolean);

void f3_control_enable(gboolean);

void set_mode(t_mode);

void set_status_label(t_status_type, char*);

gboolean time_handler(GtkWidget *widget);

// todo
/*

1. GTK widgets
    1.1. spinbuttons, dc entries actions -> check values -> copy to f
    1.2. duty cycle entering mode(calculate on time from period)
    1.3  functions to adjust F2 / F3 values FROM F1
    1.4. frequencies info labels - on period in ms, duty cycle %, period in ms (s), frequency (Hz)
    1.5. pēdējo settingu ielāde (paņemt kodu no oscope2100)

2. rs-232 komunikācijas (paņemt kodu no RPM mērītaja)
    2.1. device pong ping when disconnected - timer_handle() funkcijā
         *** startējot sūta ping-pong komandu kamēr saņem atbildi !!! vai arī pēc tty errora (kamēr ir diskonektēts)
    2.2. send on button, auto send
    2.3. eeprom save button

3. debugging
[x] 2 phase mode (automātiski rēķina delays, visus F2, diseiblo F3)
[x] 3 phase mode (automātiski rēķina delays, visus F2, visus F3)

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
            f.f1_enabled = active;
            f1_control_enable(active);
            break;

        case MODE_2_PHASES:
            f.f1_enabled = active;
            f1_control_enable(active);
            f.f2_enabled = active;
            gtk_switch_set_active(GTK_SWITCH(gtk_builder_get_object(builder, "f2_switch")), active);
            break;

        case MODE_3_PHASES:
          printf("test 1111\n");
            f.f1_enabled = active;
            f1_control_enable(active);
            f.f2_enabled = active;
            gtk_switch_set_active(GTK_SWITCH(gtk_builder_get_object(builder, "f2_switch")), active);
            f.f3_enabled = active;
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
        f.f2_enabled = TRUE;
        if (mode == MODE_CUSTOM) {
            f2_control_enable(TRUE);
        }
    } else {
        printf("not active\n");
        f.f2_enabled = FALSE;
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
        f.f3_enabled = TRUE;
        if (mode == MODE_CUSTOM) {
            f3_control_enable(TRUE);
        }
    } else {
        printf("not active\n");
        f.f3_enabled = FALSE;
        if (mode == MODE_CUSTOM) {
            f3_control_enable(FALSE);
        }
    }

}



// delay value
void f1_delay_value_change()
{
    printf("f1_delay_value_change()\n");
}


void f2_delay_value_change()
{
    printf("f2_delay_value_change()\n");
}


void f3_delay_value_change()
{
    printf("f3_delay_value_change()\n");
}


// on value
void f1_on_time_change()
{
    printf("f1_on_time_change()\n");
}


void f2_on_time_change()
{
    printf("f2_on_time_change()\n");
}


void f3_on_time_change()
{
    printf("f3_on_time_change()\n");
}


// duty cycle change
void f1_dc_change()
{
    printf("f1_dc_change()\n");
}


void f2_dc_change()
{
    printf("f2_dc_change()\n");
}


void f3_dc_change()
{
    printf("f3_dc_change()\n");
}


// period change
void f1_period_change()
{
    printf("f1_period_change()\n");
}


void f2_period_change()
{
    printf("f2_period_change()\n");
}


void f3_period_change()
{
    printf("f3_period_change()\n");
}


// radio button ON T vs DC change
void rb_f1_toggle()
{
    printf("rb_f1_toggle()\n");
}


void rb_f2_toggle()
{
    printf("rb_f2_toggle()\n");
}


void rb_f3_toggle()
{
    printf("rb_f3_toggle()\n");
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

/*gint grab_int_value (GtkSpinButton *button, gpointer user_data)
{
  return gtk_spin_button_get_value_as_int (button);
}

*/

// delay adjustment change
void f1_delay_adjustment_change()
{
    printf("f1_delay_adjustment_change()\n");
}


void f2_delay_adjustment_change()
{
    printf("f2_delay_adjustment_change()\n");
}


void f3_delay_adjustment_change()
{
    printf("f3_delay_adjustment_change()\n");
}


// on adjustment change
void f1_on_adjustment_change()
{
    printf("f1_on_adjustment_change()\n");
}


void f2_on_adjustment_change()
{
    printf("f2_on_adjustment_change()\n");
}


void f3_on_adjustment_change()
{
    printf("f3_on_adjustment_change()\n");
}



// period adjustment change
void f1_period_adjustment_change()
{
    printf("f1_period_adjustment_change()\n");
}


void f2_period_adjustment_change()
{
    printf("f2_period_adjustment_change()\n");
}


void f3_period_adjustment_change()
{
    printf("f3_period_adjustment_change()\n");
}


/***************/


void f1_control_enable(gboolean enable)
{
    gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(builder, "f1_delta")), enable);
    gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(builder, "f1_on_t")), enable);
    gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(builder, "f1_dc")), enable);
    gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(builder, "f1_period")), enable);
    gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(builder, "f1_rb_period")), enable);
    gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(builder, "f1_rb_dc")), enable);
}


void f2_control_enable(gboolean enable)
{
    gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(builder, "f2_delta")), enable);
    gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(builder, "f2_on_t")), enable);
    gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(builder, "f2_dc")), enable);
    gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(builder, "f2_period")), enable);
    gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(builder, "f2_rb_period")), enable);
    gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(builder, "f2_rb_dc")), enable);
}


void f3_control_enable(gboolean enable)
{
    gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(builder, "f3_delta")), enable);
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
    // uzseto widgetus pēc f vērtībam
    // ...
    adjust_phase_values(&f);
    /*
    f1_delta GtkSpinButton
    f2_delta GtkSpinButton
    f3_delta GtkSpinButton

    f1_on_t GtkSpinButton
    f2_on_t GtkSpinButton
    f3_on_t GtkSpinButton

    f1_period  GtkSpinButton
    f2_period  GtkSpinButton
    f3_period  GtkSpinButton
    */







}


void setup_default_config()
{
    mode = MODE_CUSTOM;


    f.f1_enabled = FALSE;
    f.f1_delay = 0;
    f.f1_on = 1;
    f.f1_period = 100;

    f.f2_enabled = FALSE;
    f.f2_delay = 0;
    f.f2_on = 1;
    f.f2_period = 100;

    f.f3_enabled = FALSE;
    f.f3_delay = 0;
    f.f3_on = 1;
    f.f3_period = 100;

    set_frequencies(f);

    gtk_switch_set_active(GTK_SWITCH(gtk_builder_get_object(builder, "f1_switch")), f.f1_enabled);
    f1_control_enable(f.f1_enabled);

    gtk_switch_set_active(GTK_SWITCH(gtk_builder_get_object(builder, "f2_switch")), f.f2_enabled);
    f2_control_enable(f.f2_enabled);

    gtk_switch_set_active(GTK_SWITCH(gtk_builder_get_object(builder, "f3_switch")), f.f3_enabled);
    f3_control_enable(f.f3_enabled);

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
    printf("Tick\n");

    // gtk_widget_queue_draw(widget);

    return TRUE;
}
