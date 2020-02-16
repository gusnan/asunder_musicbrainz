#include <stdbool.h>

GtkWidget* create_main (void);
GtkWidget* create_prefs (void);
GtkWidget* create_ripping (void);
void disable_all_main_widgets(void);
void enable_all_main_widgets(void);
void disable_flac_widgets(void);
void enable_flac_widgets(void);
void disable_mp3_widgets(void);
void enable_mp3_widgets(void);
void disable_ogg_widgets(void);
void enable_ogg_widgets(void);
void disable_opus_widgets(void);
void enable_opus_widgets(void);
void disable_wavpack_widgets(void);
void enable_wavpack_widgets(void);
void disable_monkey_widgets(void);
void enable_monkey_widgets(void);
void disable_musepack_widgets(void);
void enable_musepack_widgets(void);
void disable_fdkaac_widgets(void);
void enable_fdkaac_widgets(void);
void show_aboutbox (void);
void show_completed_dialog(int numOk, int numFailed);
void toggle_allow_tracknum(void);
