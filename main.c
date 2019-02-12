#include <gtk/gtk.h>
#include "fg3glade.h"

//make clean && make && ./fg3gtk


GtkBuilder  *builder;

// todo

/*

1. GTK widgets
    1.1.
         mode enumeration type
         status struktūra
         on/off switches - disable
         function to enable disable widgets
         functions to adjust F2 / F3 values

    1.2. cuty cycle eneter mode (auto calculate period)
    1.3. frequencies info labels - on period in ms, duty cycle %, period in ms (s), frequency (Hz)
    1.4. pēdējo settingu ielāde (paņemt kodu no oscope2100)

2. rs-232 komunikācijas (paņemt kodu no RPM mērītaja)
    2.1. status label update funckcija
    2.2. CRC checksumm function
    2.3. startējot sūta ping-pong komandu kamēr saņem atbildi !!! vai arī pēc tty errora (kamēr ir diskonektēts)
    2.4. send on button, auto send
    2.5. eeprom save button

3. debugging
[x] 2 phase mode (automātiski rēķina delays, visus F2, diseiblo F3)
[x] 3 phase mode (automātiski rēķina delays, visus F2, visus F3)

4. uztaisīt normālus make un config


*/


int main(int argc, char *argv[])
{

    GtkWidget *window;

    gtk_init(&argc, &argv);

    builder = gtk_builder_new();

    gtk_builder_add_from_string(builder, (char*) __fg3_glade, __fg3_glade_len, NULL);

    window = GTK_WIDGET(gtk_builder_get_object(builder, "applicationwindow1"));
    gtk_builder_connect_signals(builder, NULL);



    gtk_widget_show(window);
    printf("start\n");

    // styling "Store" button background color
    GtkStyleContext *context;
    GtkCssProvider *provider;

    context = gtk_widget_get_style_context(GTK_WIDGET(gtk_builder_get_object(builder, "button_store")));
    provider = gtk_css_provider_new();

    gtk_css_provider_load_from_data(provider, "* {background: #dca3a3;} *:active {background: #e38a8a;}", -1, NULL);
    gtk_style_context_add_provider(context, GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(provider);

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
    GtkLabel *device_status;
    GtkEntry *f1_dc;

    printf("select_device_changed()\n");

    device_status = GTK_LABEL(gtk_builder_get_object(builder, "device_status"));
    gtk_label_set_text(device_status, "CONNECTED");

    PangoAttrList *alist = pango_attr_list_new ();
    PangoAttribute *attr;
    PangoFontDescription *df;

    attr = pango_attr_foreground_new(0x4e00, 0x9a00, 0x0000);
    pango_attr_list_insert(alist, attr);

    df = pango_font_description_new();
    pango_font_description_set_weight(df, PANGO_WEIGHT_BOLD);

    attr = pango_attr_font_desc_new(df);
    pango_attr_list_insert(alist, attr);

    gtk_label_set_attributes (GTK_LABEL(device_status), alist);

    pango_attr_list_unref(alist);


    f1_dc = GTK_ENTRY(gtk_builder_get_object(builder, "f1_dc"));

    gtk_widget_set_sensitive (GTK_WIDGET(f1_dc), FALSE);
}

// ON/OFF switches
void f1_switch_state_set()
{
    printf("f1_switch_state_set()\n");
}


void f2_switch_state_set()
{
    printf("f2_switch_state_set()\n");
}


void f3_switch_state_set()
{
    printf("f3_switch_state_set()\n");
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
    printf("rb_mode_toggle()\n");
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