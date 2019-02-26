#ifndef FG3GTK_H
#define FG3GTK_H

#define MCU_FREQUENCY        16000000
#define MCU_CLOCKS_PER_LOOP        21


typedef enum {MODE_CUSTOM, MODE_2_PHASES, MODE_3_PHASES} t_mode;

typedef enum {ST_ERROR, ST_INFO} t_status_type;

typedef enum {NOT_SELECTED, DEVICE_ERROR, DISCONNECTED, CONNECTED} t_device_status;

typedef enum {CMD_PING, CMD_SET_FREQUENCIES, CMD_STORE} t_cmd;

typedef struct
{
    gboolean enabled;
    guint16 delay;
    guint16 on;
    guint16 period;
    gboolean dc_mode;
    gdouble dc_value;
} tfrequencies;


void setup_default_config();
void load_stored_config();
void save_config();
void refresh_ui();
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
void set_values_for_phase_mode();
char *val2str(gdouble x, const char *unit);
void update_info_labels();
void update_devices_list();
void send_cmd(t_cmd cmd);


#endif