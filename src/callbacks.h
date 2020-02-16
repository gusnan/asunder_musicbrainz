#include <gtk/gtk.h>

gboolean
for_each_row_deselect                  (GtkTreeModel *model,
                                        GtkTreePath *path,
                                        GtkTreeIter *iter,
                                        gpointer data);


gboolean
for_each_row_select                    (GtkTreeModel *model,
                                        GtkTreePath *path,
                                        GtkTreeIter *iter,
                                        gpointer data);

gchar*
format_wavpack_bitrate                 (GtkScale *scale,
                                        gdouble   arg1,
                                        gpointer  user_data);

void
on_about_clicked                       (GtkToolButton   *toolbutton,
                                        gpointer         user_data);

void
on_aboutbox_response                   (GtkDialog       *dialog,
                                        gint             response_id,
                                        gpointer         user_data);
void
update_track_offsets                   (void);

gboolean
on_album_artist_focus_out_event        (GtkWidget       *widget,
                                        GdkEventFocus   *event,
                                        gpointer         user_data);

gboolean
on_year_focus_out_event        (GtkWidget       *widget,
                                        GdkEventFocus   *event,
                                        gpointer         user_data);

gboolean
on_album_title_focus_out_event         (GtkWidget       *widget,
                                        GdkEventFocus   *event,
                                        gpointer         user_data);

// lnr
gboolean
on_album_genre_focus_out_event         (GtkWidget       *widget,
                                        GdkEventFocus   *event,
                                        gpointer         user_data);

gboolean
on_tracknum_first_focus_out_event      (GtkWidget       *widget,
                                        GdkEventFocus   *event,
                                        gpointer         user_data);

void
on_tracknum_width_changed_event        (GtkComboBox     *combobox,
                                        gpointer         user_data);

void
on_artist_edited                    (GtkCellRendererText *cell,
                                     gchar               *path_string,
                                     gchar               *new_text,
                                     gpointer             user_data);

void
on_browse_clicked                      (GtkButton       *button,
                                        gpointer         user_data);

void
on_cancel_clicked                      (GtkButton       *button,
                                        gpointer         user_data);

//~ void
//~ on_cddb_update_close_clicked          (GtkButton       *button,
                                       //~ GtkWidget**      update_window);

gboolean
on_cddb_update_closed         (GtkWidget *widget,
                               GdkEvent  *event,
                               GtkWidget**      update_window);

void
on_deselect_all_click                  (GtkMenuItem *menuitem, 
                                        gpointer data);

void
on_matches_response                    (GtkDialog       *dialog,
                                        gint             response_id,
                                        gpointer         user_data);
void
on_vbr_toggled                         (GtkToggleButton *togglebutton,
                                        gpointer         user_data);
void
on_hybrid_toggled                      (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_mp3bitrate_value_changed            (GtkRange        *range,
                                        gpointer         user_data);

void
on_fdkaac_bitrate_value_changed            (GtkRange        *range,
                                        gpointer         user_data);

void
on_opusrate_value_changed               (GtkRange       *range,
                                         gpointer        user_data);

void
on_musepackbitrate_value_changed            (GtkRange        *range,
                                             gpointer         user_data);

void
on_wavpackcompression_value_changed    (GtkRange        *range,
                                        gpointer         user_data);

void
on_pick_disc_changed                   (GtkComboBox     *combobox,
                                        gpointer         user_data);

void
on_pick_match_changed                  (GtkComboBox     *combobox,
                                        gpointer         user_data);

void
on_preferences_clicked                 (GtkToolButton   *toolbutton,
                                        gpointer         user_data);

void
on_prefs_response                      (GtkDialog       *dialog,
                                        gint             response_id,
                                        gpointer         user_data);

void
on_prefs_show                          (GtkWidget       *widget,
                                        gpointer         user_data);

void 
on_press_f2                       (void);

void
on_lookup_clicked                     (GtkToolButton   *toolbutton,
                                        gpointer         user_data);

void
on_rip_button_clicked                  (GtkButton       *button,
                                        gpointer         user_data);

void
on_single_artist_toggled               (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_rip_ogg_toggled                     (GtkToggleButton *togglebutton,
                                        gpointer         user_data);
void
on_rip_opus_toggled                    (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_rip_flac_toggled                    (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_rip_mp3_toggled                     (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_rip_fdkaac_toggled                     (GtkToggleButton *togglebutton,
                                        gpointer         user_data);
                                        
void
on_rip_wavpack_toggled                 (GtkToggleButton *togglebutton,
                                        gpointer         user_data);
void
on_rip_monkey_toggled                  (GtkToggleButton *togglebutton,
                                     gpointer         user_data);
void
on_rip_musepack_toggled                  (GtkToggleButton *togglebutton,
                                          gpointer         user_data);
void
on_rip_toggled                       (GtkCellRendererToggle *cell,
                                      gchar                 *path_string,
                                      gpointer               user_data);

void
on_select_all_click                    (GtkMenuItem *menuitem, 
                                        gpointer data);

void
on_title_edited                     (GtkCellRendererText *cell,
                                     gchar               *path_string,
                                     gchar               *new_text,
                                     gpointer             user_data);

gboolean
on_tracklist_mouse_click               (GtkWidget* treeView, 
                                        GdkEventButton* event, 
                                        gpointer user_data);

void
on_rip_header_click                 (GtkTreeViewColumn *treeviewcolumn,
                                     gpointer           user_data);

void
on_window_close	                       (GtkWidget       *widget,
                                        GdkEventFocus   *event,
                                        gpointer         user_data);
