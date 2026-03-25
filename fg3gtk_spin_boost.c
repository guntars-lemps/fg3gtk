#include <gtk/gtk.h>
#include <string.h>
#include <stdlib.h>
#include "fg3gtk_main.h"
#include "fg3gtk_spin_boost.h"

t_spin_boost f1_delay_spin_boost;
t_spin_boost f1_on_spin_boost;
t_spin_boost f1_period_spin_boost;

t_spin_boost f2_delay_spin_boost;
t_spin_boost f2_on_spin_boost;
t_spin_boost f2_period_spin_boost;

t_spin_boost f3_delay_spin_boost;
t_spin_boost f3_on_spin_boost;
t_spin_boost f3_period_spin_boost;


void init_spin_boost_t(t_spin_boost* sb, char* widget_name)
{
    sb->change_count = 0;
    sb->change_boost = 0;
    sb->prev_value = 0;
    sb->boosted = FALSE;
    sb->widget_name = widget_name;
}

void spin_boost_init()
{
    init_spin_boost_t(&f1_delay_spin_boost, "f1_delay");
    init_spin_boost_t(&f1_on_spin_boost, "f1_on_t");
    init_spin_boost_t(&f1_period_spin_boost, "f1_period");
    init_spin_boost_t(&f2_delay_spin_boost, "f2_delay");
    init_spin_boost_t(&f2_on_spin_boost, "f2_on_t");
    init_spin_boost_t(&f2_period_spin_boost, "f2_period");
    init_spin_boost_t(&f3_delay_spin_boost, "f3_delay");
    init_spin_boost_t(&f3_on_spin_boost, "f3_on_t");
    init_spin_boost_t(&f3_period_spin_boost, "f3_period");

    g_timeout_add(500, (GSourceFunc)climb_rate_timer_handler, (gpointer)window);
}

void adjust_change_boost(t_spin_boost* spin_boost)
{
    guint32 current_value = gtk_spin_button_get_value(GTK_SPIN_BUTTON(gtk_builder_get_object(builder, spin_boost->widget_name)));

    if (spin_boost->change_count > 5) {

        spin_boost->change_count  = 0;

        if (spin_boost->prev_value < current_value) {

            if (spin_boost->change_boost == 0) {
                spin_boost->change_boost = 10;
            } else {
                spin_boost->change_boost *= 1.2;
            }

            return;
        }
        if (spin_boost->prev_value > current_value) {

            if (spin_boost->change_boost == 0) {
                spin_boost->change_boost = -10;
            } else {
                spin_boost->change_boost *= 1.5;
            }

            if (-spin_boost->change_boost > (current_value / 100.0)) {
                spin_boost->change_boost = (-1.0 * current_value) / 100;
            }

            if (spin_boost->change_boost > -1) {
                spin_boost->change_boost = -1;
            }

            return;
        }
    }
    spin_boost->change_count = 0;
    spin_boost->prev_value = current_value;
    spin_boost->change_boost = 0;
}

gboolean climb_rate_timer_handler(GtkWidget *widget)
{
    if (widget == NULL) {
        return FALSE;
    }

    adjust_change_boost(&f1_delay_spin_boost);
    adjust_change_boost(&f1_on_spin_boost);
    adjust_change_boost(&f1_period_spin_boost);
    adjust_change_boost(&f2_delay_spin_boost);
    adjust_change_boost(&f2_on_spin_boost);
    adjust_change_boost(&f2_period_spin_boost);
    adjust_change_boost(&f3_delay_spin_boost);
    adjust_change_boost(&f3_on_spin_boost);
    adjust_change_boost(&f3_period_spin_boost);

    return TRUE;
}

void adjust_value_by_range(gdouble* value, gdouble min_value, gdouble max_value)
{
    if (*value > max_value) {
        *value = max_value;
    }
    if (*value < min_value) {
        *value = min_value;
    }
}

void boost(t_spin_boost* spin_boost)
{
    // skip boosting if change signal is from boost itself
    if (!spin_boost->boosted && (spin_boost->change_boost != 0)) {

        gdouble value = gtk_spin_button_get_value(GTK_SPIN_BUTTON(gtk_builder_get_object(builder, spin_boost->widget_name)));

        gdouble min_value;
        gdouble max_value;
        gtk_spin_button_get_range(GTK_SPIN_BUTTON(gtk_builder_get_object(builder, spin_boost->widget_name)), &min_value, &max_value);

        value += spin_boost->change_boost;
        adjust_value_by_range(&value, min_value, max_value);

        gtk_spin_button_set_value(GTK_SPIN_BUTTON(gtk_builder_get_object(builder, spin_boost->widget_name)), value);
        spin_boost->boosted = TRUE;
    } else {
        spin_boost->boosted = FALSE;
    }
    spin_boost->change_count++;
}