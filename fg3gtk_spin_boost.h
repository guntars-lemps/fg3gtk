#ifndef FG3GTK_SPIN_BOOST_H
#define FG3GTK_SPIN_BOOST_H

typedef struct
{
    int change_count;
    double change_boost;
    guint32 prev_value;
    gboolean boosted;
    char* widget_name;
} t_spin_boost;

gboolean climb_rate_timer_handler(GtkWidget*);

void spin_boost_init();

void boost(t_spin_boost*);

void adjust_value_by_range(gdouble*, gdouble, gdouble);

extern t_spin_boost f1_delay_spin_boost;
extern t_spin_boost f1_on_spin_boost;
extern t_spin_boost f1_period_spin_boost;

extern t_spin_boost f2_delay_spin_boost;
extern t_spin_boost f2_on_spin_boost;
extern t_spin_boost f2_period_spin_boost;

extern t_spin_boost f3_delay_spin_boost;
extern t_spin_boost f3_on_spin_boost;
extern t_spin_boost f3_period_spin_boost;

#endif