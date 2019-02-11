#include <gtk/gtk.h>
#include "fg3glade.h"

//make clean && make && ./fg3gtk


GtkBuilder  *builder;


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

    gtk_main();

    return 0;
}


void button_send_clicked()
{
    printf("button_send_clicked()\n");
}


void cb_auto_send_toggled()
{
    printf("cb_auto_send_toggled()\n");
}


void select_device_changed()
{
    GtkLabel *device_status;
    GtkEntry f1_dc;

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


}


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


//2. disable element ???


/*
// called when button is clicked
void on_btn_hello_clicked()
{
    static unsigned int count = 0;
    char str_count[30] = {0};

    gtk_label_set_text(GTK_LABEL(g_lbl_hello), "Hello, world!");
    count++;
    sprintf(str_count, "%d", count);
    gtk_label_set_text(GTK_LABEL(g_lbl_count), str_count);
}
*/


// called when window is closed
void on_window_main_destroy()
{
    g_object_unref(builder);
    gtk_main_quit();
}
