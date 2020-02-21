/*
Asunder

Copyright(C) 2005 Eric Lathrop <eric@ericlathrop.com>
Copyright(C) 2007 Andrew Smith <http://littlesvr.ca/contact.php>

Any code in this file may be redistributed or modified under the terms of
the GNU General Public Licence as published by the Free Software 
Foundation; version 2 of the licence.

*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <glib.h>
#include <glib/gstdio.h>

#include "asunder_disc.h"
#include "prefs.h"
#include "main.h"
#include "util.h"
#include "support.h"
#include "interface.h"

#define CONFIG_FILENAME "asunder"

prefs * global_prefs = NULL;

// allocate memory for a new prefs struct
// and make sure everything is set to NULL
prefs * new_prefs()
{
    prefs * p = malloc(sizeof(prefs));
    if (p == NULL)
        fatalError("malloc(sizeof(prefs)) failed. Out of memory.");
    memset(p, 0, sizeof(prefs));
    
    return p;
}

void clear_prefs(prefs * p)
{
    if (p->cdrom != NULL) 
        free(p->cdrom);
    p->cdrom = NULL;

    if (p->music_dir != NULL) 
        free(p->music_dir);
    p->music_dir = NULL;

    if (p->format_music != NULL) 
        free(p->format_music);
    p->format_music = NULL;

    if (p->format_playlist != NULL) 
        free(p->format_playlist);
    p->format_playlist = NULL;

    if (p->format_albumdir != NULL) 
        free(p->format_albumdir);
    p->format_albumdir = NULL;
    
    if (p->proxy_server_name != NULL) 
        free(p->proxy_server_name);
    p->proxy_server_name = NULL;
    
    if (p->musicbrainz_server_name != NULL) 
        free(p->musicbrainz_server_name);
    p->musicbrainz_server_name = NULL;
}

// free memory allocated for prefs struct
// also frees any strings pointed to in the struct
void delete_prefs(prefs * p)
{
    clear_prefs(p);
    
    free(p);
}

// returns a new prefs struct with all members set to nice default values
// this struct must be freed with delete_prefs()
prefs * get_default_prefs()
{
    prefs * p = new_prefs();
    
    p->cdrom = malloc(sizeof(char) * 11);
    if (p->cdrom == NULL)
        fatalError("malloc(sizeof(char) * 11) failed. Out of memory.");
    strncpy(p->cdrom, "/dev/cdrom", 11);
    
    p->music_dir = strdup(getenv("HOME"));
    p->make_playlist = 1;
    
    p->format_music = malloc(sizeof(char) * 13);
    if (p->format_music == NULL)
        fatalError("malloc(sizeof(char) * 8) failed. Out of memory.");
    strncpy(p->format_music, "%N - %A - %T", 13);
    
    p->format_playlist = malloc(sizeof(char) * 8);
    if (p->format_playlist == NULL)
        fatalError("malloc(sizeof(char) * 8) failed. Out of memory.");
    strncpy(p->format_playlist, "%A - %L", 8);
    
    p->format_albumdir = malloc(sizeof(char) * 8);
    if (p->format_albumdir == NULL)
        fatalError("malloc(sizeof(char) * 8) failed. Out of memory.");
    strncpy(p->format_albumdir, "%A - %L", 8);
    
    p->rip_wav = 0;
    p->do_fast_rip = 0;
    p->rip_mp3 = 0;
    p->rip_ogg = 1;
    p->rip_flac = 0;
    p->rip_wavpack = 0;
    p->mp3_vbr = 1;
    p->mp3_bitrate = 10;
    p->ogg_quality = 6;
    p->flac_compression = 5;
    p->wavpack_compression = 1;
    p->wavpack_hybrid = 1;
    p->wavpack_bitrate = 3;
    p->rip_monkey = 0;
    p->monkey_compression = 2;
    p->rip_aac = 0;
    p->aac_quality = 60;
    p->rip_musepack = 0;
    p->musepack_bitrate = 2;
    p->rip_opus = 0;
    p->opus_bitrate = 9;
    
    p->main_window_width = 600;
    p->main_window_height = 450;
    
    p->eject_on_done = 0;
    
    p->do_musicbrainz_updates = 1;
    
    p->use_proxy = 0;
    
    p->do_log = 0;
    
    p->proxy_server_name = malloc(sizeof(char) * (strlen("10.0.0.1") + 1));
    if (p->proxy_server_name == NULL)
        fatalError("malloc(sizeof(char) * (strlen(\"10.0.0.1\") + 1)) failed. Out of memory.");
    strcpy(p->proxy_server_name, "10.0.0.1");
    
    p->proxy_port_number = DEFAULT_PROXY_PORT;
    
    p->musicbrainz_server_name = malloc(sizeof(char) * (strlen(DEFAULT_MUSICBRAINZ_SERVER) + 1));
    if (p->musicbrainz_server_name == NULL)
        fatalError("malloc(sizeof(char) * (strlen(DEFAULT_MUSICBRAINZ_SERVER) + 1)) failed. Out of memory.");
    strcpy(p->musicbrainz_server_name, DEFAULT_MUSICBRAINZ_SERVER);
    
    p->musicbrainz_port_number = DEFAULT_MUSICBRAINZ_SERVER_PORT;
    
    p->more_formats_expanded = 0;
    p->proprietary_formats_expanded = 0;
    p->do_fast_rip = 1;
    
    p->allow_first_track_num_change = 0;
    p->first_track_num_offset = 0;
    p->track_num_width = 2;
    
    p->rip_fdkaac = 0;
    p->fdkaac_bitrate = 10;
    
    p->concatenated_track_separator[0] = '/';
    p->concatenated_track_separator[1] = '\0';

    return p;
}

// sets up all of the widgets in the preferences dialog to
// match the given prefs struct
void set_widgets_from_prefs(prefs * p)
{
    char tempStr[10];
    
    gtk_entry_set_text(GTK_ENTRY(lookup_widget(win_prefs, "cdrom")), p->cdrom);
    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(lookup_widget(win_prefs, "music_dir")), prefs_get_music_dir(p));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(lookup_widget(win_prefs, "make_playlist")), p->make_playlist);
    gtk_entry_set_text(GTK_ENTRY(lookup_widget(win_prefs, "format_music")), p->format_music);
    gtk_entry_set_text(GTK_ENTRY(lookup_widget(win_prefs, "format_playlist")), p->format_playlist);
    gtk_entry_set_text(GTK_ENTRY(lookup_widget(win_prefs, "format_albumdir")), p->format_albumdir);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(lookup_widget(win_prefs, "rip_wav")), p->rip_wav);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(lookup_widget(win_prefs, "do_fast_rip")), p->do_fast_rip);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(lookup_widget(win_prefs, "rip_mp3")), p->rip_mp3);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(lookup_widget(win_prefs, "rip_ogg")), p->rip_ogg);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(lookup_widget(win_prefs, "rip_flac")), p->rip_flac);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(lookup_widget(win_prefs, "rip_wavpack")), p->rip_wavpack);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(lookup_widget(win_prefs, "mp3_vbr")), p->mp3_vbr);
    gtk_range_set_value(GTK_RANGE(lookup_widget(win_prefs, "mp3bitrate")), p->mp3_bitrate);
    gtk_range_set_value(GTK_RANGE(lookup_widget(win_prefs, "oggquality")), p->ogg_quality);
    gtk_range_set_value(GTK_RANGE(lookup_widget(win_prefs, "flaccompression")), p->flac_compression);
    gtk_range_set_value(GTK_RANGE(lookup_widget(win_prefs, "wavpack_compression")), p->wavpack_compression);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(lookup_widget(win_prefs, "wavpack_hybrid")), p->wavpack_hybrid);
    gtk_range_set_value(GTK_RANGE(lookup_widget(win_prefs, "wavpack_bitrate_slider")), p->wavpack_bitrate);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(lookup_widget(win_prefs, "rip_monkey")), p->rip_monkey);
    gtk_range_set_value(GTK_RANGE(lookup_widget(win_prefs, "monkey_compression_slider")), p->monkey_compression);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(lookup_widget(win_prefs, "rip_fdkaac")), p->rip_fdkaac);
    gtk_range_set_value(GTK_RANGE(lookup_widget(win_prefs, "fdkaac_bitrate")), p->fdkaac_bitrate);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(lookup_widget(win_prefs, "rip_musepack")), p->rip_musepack);
    gtk_range_set_value(GTK_RANGE(lookup_widget(win_prefs, "musepack_bitrate_slider")), p->musepack_bitrate);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(lookup_widget(win_prefs, "rip_opus")), p->rip_opus);
    gtk_range_set_value(GTK_RANGE(lookup_widget(win_prefs, "opusrate")), p->opus_bitrate);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(lookup_widget(win_prefs, "eject_on_done")), p->eject_on_done);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(lookup_widget(win_prefs, "do_musicbrainz_updates")), p->do_musicbrainz_updates);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(lookup_widget(win_prefs, "use_proxy")), p->use_proxy);
    gtk_entry_set_text(GTK_ENTRY(lookup_widget(win_prefs, "proxy_server_name")), p->proxy_server_name);
    snprintf(tempStr, 10, "%d", p->proxy_port_number);
    gtk_entry_set_text(GTK_ENTRY(lookup_widget(win_prefs, "proxy_port_number")), tempStr);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(lookup_widget(win_prefs, "do_log")), p->do_log);
    gtk_entry_set_text(GTK_ENTRY(lookup_widget(win_prefs, "musicbrainz_server_name")), p->musicbrainz_server_name);
    snprintf(tempStr, 10, "%d", p->musicbrainz_port_number);
    gtk_entry_set_text(GTK_ENTRY(lookup_widget(win_prefs, "musicbrainz_port_number")), tempStr);
    if(global_prefs->more_formats_expanded)
        gtk_expander_set_expanded (GTK_EXPANDER(lookup_widget(win_prefs, "more_formats_expander")), TRUE);
    //~ if(global_prefs->proprietary_formats_expanded)
        //~ gtk_expander_set_expanded (GTK_EXPANDER(lookup_widget(win_prefs, "proprietary_formats_expander")), TRUE);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(lookup_widget(win_prefs, "allow_tracknum")), p->allow_first_track_num_change);
    toggle_allow_tracknum(); // This will set up the rest of the related widgets

    gtk_entry_set_text(GTK_ENTRY(lookup_widget(win_prefs,
					"concatenated_track_separator")), p->concatenated_track_separator);
    
    /* disable widgets if needed */
    if ( !(p->rip_mp3) )
        disable_mp3_widgets();
    if ( !(p->rip_ogg) )
        disable_ogg_widgets();
    if ( !(p->rip_flac) )
        disable_flac_widgets();
    if ( !(p->rip_wavpack) )
        disable_wavpack_widgets();
    else
        enable_wavpack_widgets(); /* need this to potentially disable hybrid widgets */
    if( !(p->rip_monkey) )
        disable_monkey_widgets();
    if( !(p->rip_fdkaac) )
        disable_fdkaac_widgets();
    if( !(p->rip_musepack) )
        disable_musepack_widgets();
    if (!(p->rip_opus))
        disable_opus_widgets();
}

// populates a prefs struct from the current state of the widgets
void get_prefs_from_widgets(prefs * p)
{
    gchar * tocopy = NULL;
    const gchar * tocopyc = NULL;

    clear_prefs(p);
    
    tocopyc = gtk_entry_get_text(GTK_ENTRY(lookup_widget(win_prefs, "cdrom")));
    p->cdrom = malloc(sizeof(char) * (strlen(tocopyc) + 1));
    if (p->cdrom == NULL)
        fatalError("malloc(sizeof(char) * (strlen(tocopyc) + 1)) failed. Out of memory.");
    strncpy(p->cdrom, tocopyc, strlen(tocopyc)+1);
    
    tocopy = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(lookup_widget(win_prefs, "music_dir")));
    if ((p->music_dir = malloc(sizeof(char) * (strlen(tocopy) + 1))) == NULL)
        fatalError("malloc(sizeof(char) * (strlen(tocopy) + 1)) failed. Out of memory.");
    strncpy(p->music_dir, tocopy, strlen(tocopy)+1);
    g_free(tocopy);
    
    p->make_playlist = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(lookup_widget(win_prefs, "make_playlist")));
    
    tocopyc = gtk_entry_get_text(GTK_ENTRY(lookup_widget(win_prefs, "format_music")));
    p->format_music = malloc(sizeof(char) * (strlen(tocopyc) + 1));
    if (p->format_music == NULL)
        fatalError("malloc(sizeof(char) * (strlen(tocopyc) + 1)) failed. Out of memory.");
    strncpy(p->format_music, tocopyc, strlen(tocopyc)+1);
    
    tocopyc = gtk_entry_get_text(GTK_ENTRY(lookup_widget(win_prefs, "format_playlist")));
    p->format_playlist = malloc(sizeof(char) * (strlen(tocopyc) + 1));
    if (p->format_playlist == NULL)
        fatalError("malloc(sizeof(char) * (strlen(tocopyc) + 1)) failed. Out of memory.");
    strncpy(p->format_playlist, tocopyc, strlen(tocopyc)+1);
    
    tocopyc = gtk_entry_get_text(GTK_ENTRY(lookup_widget(win_prefs, "format_albumdir")));
    if ((p->format_albumdir = malloc(sizeof(char) * (strlen(tocopyc) + 1))) == NULL)
        fatalError("malloc(sizeof(char) * (strlen(tocopyc) + 1)) failed. Out of memory.");
    strncpy(p->format_albumdir, tocopyc, strlen(tocopyc)+1);

    p->rip_wav = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(lookup_widget(win_prefs, "rip_wav")));
    p->do_fast_rip = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(lookup_widget(win_prefs, "do_fast_rip")));
    p->rip_mp3 = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(lookup_widget(win_prefs, "rip_mp3")));
    p->rip_ogg = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(lookup_widget(win_prefs, "rip_ogg")));
    p->rip_flac = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(lookup_widget(win_prefs, "rip_flac")));
    p->rip_wavpack = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(lookup_widget(win_prefs, "rip_wavpack")));
    p->mp3_vbr = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(lookup_widget(win_prefs, "mp3_vbr")));
    p->mp3_bitrate = (int)gtk_range_get_value(GTK_RANGE(lookup_widget(win_prefs, "mp3bitrate")));
    p->ogg_quality = (int)gtk_range_get_value(GTK_RANGE(lookup_widget(win_prefs, "oggquality")));
    p->flac_compression = (int)gtk_range_get_value(GTK_RANGE(lookup_widget(win_prefs, "flaccompression")));
    p->wavpack_compression = (int)gtk_range_get_value(GTK_RANGE(lookup_widget(win_prefs, "wavpack_compression")));
    p->wavpack_hybrid = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(lookup_widget(win_prefs, "wavpack_hybrid")));
    p->wavpack_bitrate = (int)gtk_range_get_value(GTK_RANGE(lookup_widget(win_prefs, "wavpack_bitrate_slider")));
    p->rip_monkey = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(lookup_widget(win_prefs, "rip_monkey")));
    p->monkey_compression = (int)gtk_range_get_value(GTK_RANGE(lookup_widget(win_prefs, "monkey_compression_slider")));
    p->rip_fdkaac = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(lookup_widget(win_prefs, "rip_fdkaac")));
    p->fdkaac_bitrate = (int)gtk_range_get_value(GTK_RANGE(lookup_widget(win_prefs, "fdkaac_bitrate")));
    p->rip_musepack = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(lookup_widget(win_prefs, "rip_musepack")));
    p->musepack_bitrate = (int)gtk_range_get_value(GTK_RANGE(lookup_widget(win_prefs, "musepack_bitrate_slider")));
    p->rip_opus = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(lookup_widget(win_prefs, "rip_opus")));
    p->opus_bitrate = (int)gtk_range_get_value(GTK_RANGE(lookup_widget(win_prefs, "opusrate")));
    
    p->eject_on_done = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(lookup_widget(win_prefs, "eject_on_done")));
    
    p->do_musicbrainz_updates = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(lookup_widget(win_prefs, "do_musicbrainz_updates")));
    
    p->use_proxy = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(lookup_widget(win_prefs, "use_proxy")));
    
    tocopyc = gtk_entry_get_text(GTK_ENTRY(lookup_widget(win_prefs, "proxy_server_name")));
    p->proxy_server_name = malloc(sizeof(char) * (strlen(tocopyc) + 1));
    if (p->proxy_server_name == NULL)
        fatalError("malloc(sizeof(char) * (strlen(tocopyc) + 1)) failed. Out of memory.");
    strncpy(p->proxy_server_name, tocopyc, strlen(tocopyc) + 1);
    
    p->proxy_port_number = atoi(gtk_entry_get_text(GTK_ENTRY(lookup_widget(win_prefs, "proxy_port_number"))));
    
    tocopyc = gtk_entry_get_text(GTK_ENTRY(lookup_widget(win_prefs, "musicbrainz_server_name")));
    p->musicbrainz_server_name = malloc(sizeof(char) * (strlen(tocopyc) + 1));
    if (p->musicbrainz_server_name == NULL)
        fatalError("malloc(sizeof(char) * (strlen(tocopyc) + 1)) failed. Out of memory.");
    strncpy(p->musicbrainz_server_name, tocopyc, strlen(tocopyc) + 1);
    
    p->musicbrainz_port_number = atoi(gtk_entry_get_text(GTK_ENTRY(lookup_widget(win_prefs, "musicbrainz_port_number"))));
    
    p->do_log = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(lookup_widget(win_prefs, "do_log")));

    tocopyc = gtk_entry_get_text(GTK_ENTRY(lookup_widget(win_prefs,
                    "concatenated_track_separator")));
    p->concatenated_track_separator[0] = tocopyc[0];
    
    p->more_formats_expanded = gtk_expander_get_expanded (GTK_EXPANDER(lookup_widget(win_prefs, "more_formats_expander")));
    //~ p->proprietary_formats_expanded = gtk_expander_get_expanded (GTK_EXPANDER(lookup_widget(win_prefs, "proprietary_formats_expander")));
    
    p->allow_first_track_num_change = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(lookup_widget(win_prefs, "allow_tracknum")));
    p->first_track_num_offset = atoi(gtk_entry_get_text(GTK_ENTRY (lookup_widget(win_main, "tn_first")))) - 1;
    p->track_num_width = gtk_combo_box_get_active(GTK_COMBO_BOX(lookup_widget(win_main, "tn_width"))) + 1;
}

// get a config file path to load from, using XDG_CONFIG_HOME if available
//
// Priority:
//    1: Use the file got from g_get_user_config_dir + "asunder" + CONFIG_FILENAME
//    2: XDG_CONFIG_HOME + /asunder/CONFIG_FILENAME
//    3: getenv(HOME) + dot_filename
// 
static gchar *get_prefs_load_config_path(void)
{
    gchar *result_filename = NULL;
    
    const gchar *xdg_config_home;
    gchar *user_config_home_filename;
    gchar *getenv_home_filename;
    gchar *xdg_config_home_filename;
    
    user_config_home_filename = g_build_filename(g_get_user_config_dir(), "asunder", CONFIG_FILENAME, NULL);
    
    gchar *dot_filename = g_strdup_printf(".%s", CONFIG_FILENAME);
    getenv_home_filename = g_build_filename(getenv("HOME"), dot_filename, NULL);
    g_free(dot_filename);
    
    xdg_config_home = getenv("XDG_CONFIG_HOME");
    xdg_config_home_filename = g_build_filename(xdg_config_home, "asunder", CONFIG_FILENAME, NULL);
    
    if (g_file_test(user_config_home_filename, G_FILE_TEST_EXISTS)) {

        // Use the user_config_home_filename
        result_filename = g_strdup(user_config_home_filename);
        
    } else if (xdg_config_home != NULL && *xdg_config_home != '\0') {
        // use the xdg one
        result_filename = g_strdup(xdg_config_home_filename);
    }
    else {
        // use the getenv(HOME) one
        result_filename = g_strdup(getenv_home_filename);
    }
    
    g_free(xdg_config_home_filename);
    g_free(getenv_home_filename);
    g_free(user_config_home_filename);
    
    return result_filename;
}

// get a config file path for saving prefs - Should always save
// to the get_user_config_dir location.
static gchar *get_prefs_save_config_path(void)
{
    gchar *result_filename = NULL;
    
    // Check if path exists, otherwise, create it.
    const gchar *config_path = g_get_user_config_dir();
    
    // ~/.config/asunder/
    gchar *path = g_build_filename(config_path, "asunder", NULL);
    
    // and then filename CONFIG_FILENAME which is "asunder".
    result_filename = g_build_filename(path, CONFIG_FILENAME, NULL);
    
    if (!g_file_test(path, G_FILE_TEST_IS_DIR)) {
        
        // Folder doesn't exist, create it.
        if (g_mkdir(path, 0755) != 0) {
            fatalError("g_mkdir() failed. Couldn't create folder.");
        }
    }
    
    g_free(path);
    
    return result_filename;
}

// store the given prefs struct to the config file
void save_prefs(prefs * p)
{
    gchar * file = get_prefs_save_config_path();
    debugLog("Saving configuration\n");
    
    FILE * config = fopen(file, "w");
    if (config != NULL)
    {
        fprintf(config, "%s\n", p->cdrom);
        fprintf(config, "%s\n", p->music_dir);
        fprintf(config, "%d\n", p->make_playlist);
        fprintf(config, "%d\n", 1); /* used to be p->make_albumdir */
        fprintf(config, "%s\n", p->format_music);
        fprintf(config, "%s\n", p->format_playlist);
        fprintf(config, "%s\n", p->format_albumdir);
        fprintf(config, "%d\n", p->rip_wav);
        fprintf(config, "%d\n", p->rip_mp3);
        fprintf(config, "%d\n", p->rip_ogg);
        fprintf(config, "%d\n", p->rip_flac);
        fprintf(config, "%d\n", p->mp3_vbr);
        fprintf(config, "%d\n", p->mp3_bitrate);
        fprintf(config, "%d\n", p->ogg_quality);
        fprintf(config, "%d\n", p->flac_compression);
        fprintf(config, "%s\n", "unused"); /* used to be p->invalid_chars */
        fprintf(config, "%d\n", p->main_window_width);
        fprintf(config, "%d\n", p->main_window_height);
        fprintf(config, "%d\n", p->eject_on_done);
        fprintf(config, "%d\n", p->do_musicbrainz_updates);
        fprintf(config, "%d\n", p->use_proxy);
        fprintf(config, "%s\n", p->proxy_server_name);
        fprintf(config, "%d\n", p->proxy_port_number);
        fprintf(config, "%d\n", p->rip_wavpack);
        fprintf(config, "%d\n", p->wavpack_compression);
        fprintf(config, "%d\n", p->wavpack_hybrid);
        fprintf(config, "%d\n", p->wavpack_bitrate);
        fprintf(config, "%d\n", p->do_log);
        fprintf(config, "%s\n", p->musicbrainz_server_name);
        fprintf(config, "%d\n", p->musicbrainz_port_number);
        fprintf(config, "%d\n", p->rip_monkey);
        fprintf(config, "%d\n", p->monkey_compression);
        fprintf(config, "%s\n", "unused"); /* used to be p->rip_aac */
        fprintf(config, "%s\n", "unused"); /* used to be p->aac_quality */
        fprintf(config, "%d\n", p->rip_musepack);
        fprintf(config, "%d\n", p->musepack_bitrate);
        fprintf(config, "%d\n", p->more_formats_expanded);
        fprintf(config, "%d\n", p->proprietary_formats_expanded);
        fprintf(config, "%d\n", p->rip_opus);
        fprintf(config, "%d\n", p->opus_bitrate);
        fprintf(config, "%d\n", p->do_fast_rip);
        fprintf(config, "%d\n", p->allow_first_track_num_change);
        fprintf(config, "%d\n", p->first_track_num_offset);
        fprintf(config, "%d\n", p->track_num_width);
        fprintf(config, "%d\n", p->rip_fdkaac);
        fprintf(config, "%d\n", p->fdkaac_bitrate);
        fprintf(config, "%s\n", p->concatenated_track_separator);
        
        fclose(config);
    } else {
        fprintf(stderr, "Warning: could not save config file: %s\n", strerror(errno));
    }
    g_free(file);
}

// load the prefereces from the config file into the given prefs struct
void load_prefs(prefs * p)
{
    gchar * file = get_prefs_load_config_path();
    debugLog("Loading configuration\n");
    
    int fd = open(file, O_RDONLY);
    if (fd > -1)
    {
        int anInt;
        char* aCharPtr;
        
        aCharPtr = read_line(fd);
        if (aCharPtr != NULL)
        {
            if (p->cdrom != NULL)
                free(p->cdrom);
            p->cdrom = aCharPtr;
        }
        
        aCharPtr = read_line(fd);
        if (aCharPtr != NULL)
        {
            if (p->music_dir != NULL)
                free(p->music_dir);
            p->music_dir = aCharPtr;
        }
        
        // this one can be 0
        p->make_playlist = read_line_num(fd);
        
        // used to be p->make_albumdir, but no longer used
        p->make_albumdir = 1;
        read_line_num(fd);
        
        aCharPtr = read_line(fd);
        if (aCharPtr != NULL)
        {
            if (p->format_music != NULL)
                free(p->format_music);
            p->format_music = aCharPtr;
        }
        
        aCharPtr = read_line(fd);
        if (aCharPtr != NULL)
        {
            if (p->format_playlist != NULL)
                free(p->format_playlist);
            p->format_playlist = aCharPtr;
        }
        
        aCharPtr = read_line(fd);
        if (aCharPtr != NULL)
        {
            if (p->format_albumdir != NULL)
                free(p->format_albumdir);
            p->format_albumdir = aCharPtr;
        }
        
        // this one can be 0
        p->rip_wav = read_line_num(fd);
        
        // this one can be 0
        p->rip_mp3 = read_line_num(fd);
        
        // this one can be 0
        p->rip_ogg = read_line_num(fd);

        // this one can be 0
        p->rip_flac = read_line_num(fd);
        
        // this one can be 0
        p->mp3_vbr = read_line_num(fd);
        
        anInt = read_line_num(fd);
        if (anInt != 0)
            p->mp3_bitrate = anInt;
        
        // this one can be 0
        p->ogg_quality = read_line_num(fd);
        
        // this one can be 0
        p->flac_compression = read_line_num(fd);
        
        /* used to be p->invalid_chars, but no longer used */
        aCharPtr = read_line(fd);
        if (aCharPtr != NULL)
            free(aCharPtr);
        
        anInt = read_line_num(fd);
        if (anInt != 0)
            p->main_window_width = anInt;
        
        anInt = read_line_num(fd);
        if (anInt != 0)
            p->main_window_height = anInt;
        
        // this one can be 0
        p->eject_on_done = read_line_num(fd);
        
        // this one can be 0
        p->do_musicbrainz_updates = read_line_num(fd);
        
        // this one can be 0
        p->use_proxy = read_line_num(fd);
        
        aCharPtr = read_line(fd);
        if (aCharPtr != NULL)
        {
            if (p->proxy_server_name != NULL)
                free(p->proxy_server_name);
            p->proxy_server_name = aCharPtr;
        }
        
        p->proxy_port_number = read_line_num(fd);
        if (p->proxy_port_number == 0 || !is_valid_port_number(p->proxy_port_number))
        {
            printf("bad port number read from config file, using %d instead\n", DEFAULT_PROXY_PORT);
            p->proxy_port_number = DEFAULT_PROXY_PORT;
        }
        
        // this one can be 0
        p->rip_wavpack = read_line_num(fd);
        
        // this one can be 0
        p->wavpack_compression = read_line_num(fd);
        
        // this one can be 0
        p->wavpack_hybrid = read_line_num(fd);
        
        // this one can be 0
        p->wavpack_bitrate = read_line_num(fd);
        
        // this one can be 0
        p->do_log = read_line_num(fd);
        
        aCharPtr = read_line(fd);
        if (aCharPtr != NULL)
        {
            if (p->musicbrainz_server_name != NULL)
                free(p->musicbrainz_server_name);
            p->musicbrainz_server_name = aCharPtr;
        }
        
        p->musicbrainz_port_number = read_line_num(fd);
        if (p->musicbrainz_port_number == 0 || !is_valid_port_number(p->musicbrainz_port_number))
        {
            printf("bad port number read from config file, using 888 instead\n");
            p->musicbrainz_port_number = DEFAULT_MUSICBRAINZ_SERVER_PORT;
        }
        
        // this one can be 0
        p->rip_monkey = read_line_num(fd);
        
        // this one can be 0
        p->monkey_compression = read_line_num(fd);
        
        /* used to be p->rip_aac, but no longer used */
        aCharPtr = read_line(fd);
        if (aCharPtr != NULL)
            free(aCharPtr);
        
        /* used to be p->aac_quality, but no longer used */
        aCharPtr = read_line(fd);
        if (aCharPtr != NULL)
            free(aCharPtr);
        
        // this one can be 0
        p->rip_musepack = read_line_num(fd);
        
        // this one can be 0
        p->musepack_bitrate = read_line_num(fd);
        
        // this one can be 0
        p->more_formats_expanded = read_line_num(fd);
        
        // this one can be 0
        p->proprietary_formats_expanded = read_line_num(fd);
        
        // this one can be 0
        p->rip_opus = read_line_num(fd);
        
        anInt = read_line_num(fd);
        if (anInt != 0)
            p->opus_bitrate = anInt;
        
        // this one can be 0
        p->do_fast_rip = read_line_num(fd);

        // this one can be 0
        p->allow_first_track_num_change = read_line_num(fd);
        
        // this one can be 0
        p->first_track_num_offset = read_line_num(fd);
        
        p->track_num_width = read_line_num(fd);
        if (p->track_num_width < 1 || p->track_num_width > 4)
            p->track_num_width = 2;
        
        // this one can be 0
        p->rip_fdkaac = read_line_num(fd);
        
        // this one can be 0
        p->fdkaac_bitrate = read_line_num(fd);
        
        aCharPtr = read_line(fd);
        if (aCharPtr != NULL)
        {
            p->concatenated_track_separator[0] = aCharPtr[0];
        }

        close(fd);
    } else {
        fprintf(stderr, "Warning: could not load config file: %s\n", strerror(errno));
    }
    g_free(file);
    
    
    // Check if we have a file (and not a folder) called ~/.config/asunder
    // This causes the save config later to fail, since it tries to make
    // a folder with exactly that name - We can safely remove it now, since
    // we have already loaded any config it contains, if any.
    gchar *test_filename = g_build_filename(g_get_user_config_dir(),
                                            CONFIG_FILENAME, NULL);
    
    // if it is a file (and not a directory, remove it)
    if (!g_file_test(test_filename, G_FILE_TEST_IS_DIR)) {
    
        // Remove the file - we save the config later, to the correct location
        if (g_remove(test_filename) == -1) {
            fprintf(stderr, "Couldn't remove config file in wrong location...\n");
        }
    }
    
    g_free(test_filename);
}

// use this method when reading the "music_dir" field of a prefs struct
// it will make sure it always points to a nice directory
char * prefs_get_music_dir(prefs * p)
{
    struct stat s;
    char * home;
    GtkWidget * dialog;
    
    if (stat(p->music_dir, &s) != 0)
    {
        home = getenv("HOME");
        
        dialog = gtk_message_dialog_new(GTK_WINDOW(win_main), 
                                        GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, 
                                        "The music directory '%s' does not exist.\n\n"
                                        "The music directory will be reset to '%s'.", 
                                        p->music_dir, home);
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        
        free(p->music_dir);
        p->music_dir = malloc(sizeof(char) * (strlen(home)+1));
        if (p->music_dir == NULL)
            fatalError("malloc(sizeof(char) * (strlen(home)+1)) failed. Out of memory.");
        
        strncpy(p->music_dir, home, strlen(home)+1);
        
        save_prefs(p);
    }
    return p->music_dir;
}

int is_valid_port_number(int number)
{
    if(number >= 0 && number <= 65535)
        return 1;
    else
        return 0;
}

bool prefs_are_valid(void)
{
    GtkWidget * warningDialog;
    bool somethingWrong = false;
    
    // playlistfile
    if(string_has_slashes(gtk_entry_get_text(GTK_ENTRY(lookup_widget(win_prefs, "format_playlist")))))
    {
        warningDialog = gtk_message_dialog_new(GTK_WINDOW(win_main), GTK_DIALOG_DESTROY_WITH_PARENT,
                                               GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, 
                                               _("Invalid characters in the '%s' field"),
                                               _("Playlist file: "));
        gtk_dialog_run(GTK_DIALOG(warningDialog));
        gtk_widget_destroy(warningDialog);
        somethingWrong = true;
    }
    
    // musicfile
    if(string_has_slashes(gtk_entry_get_text(GTK_ENTRY(lookup_widget(win_prefs, "format_music")))))
    {
        warningDialog = gtk_message_dialog_new(GTK_WINDOW(win_main), GTK_DIALOG_DESTROY_WITH_PARENT,
                                               GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, 
                                               _("Invalid characters in the '%s' field"),
                                               _("Music file: "));
        gtk_dialog_run(GTK_DIALOG(warningDialog));
        gtk_widget_destroy(warningDialog);
        somethingWrong = true;
    }
    if(strlen(gtk_entry_get_text(GTK_ENTRY(lookup_widget(win_prefs, "format_music")))) == 0)
    {
        warningDialog = gtk_message_dialog_new(GTK_WINDOW(win_main), GTK_DIALOG_DESTROY_WITH_PARENT,
                                               GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, 
                                               _("'%s' cannot be empty"),
                                               _("Music file: "));
        gtk_dialog_run(GTK_DIALOG(warningDialog));
        gtk_widget_destroy(warningDialog);
        somethingWrong = true;
    }
    
    // proxy port
    int rc;
    int proxy_port_number;
    rc = sscanf(gtk_entry_get_text(GTK_ENTRY(lookup_widget(win_prefs, "proxy_port_number"))), "%d", &proxy_port_number);
    if (rc != 1 || !is_valid_port_number(proxy_port_number))
    {
        warningDialog = gtk_message_dialog_new(GTK_WINDOW(win_main), GTK_DIALOG_DESTROY_WITH_PARENT,
                                               GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, 
                                               _("Invalid proxy port number"));
        gtk_dialog_run(GTK_DIALOG(warningDialog));
        gtk_widget_destroy(warningDialog);
        somethingWrong = true;
    }
    
    // musicbrainz server port
    int musicbrainz_port_number;
    rc = sscanf(gtk_entry_get_text(GTK_ENTRY(lookup_widget(win_prefs, "musicbrainz_port_number"))), "%d", &musicbrainz_port_number);
    if (rc != 1 || !is_valid_port_number(musicbrainz_port_number))
    {
        warningDialog = gtk_message_dialog_new(GTK_WINDOW(win_main), GTK_DIALOG_DESTROY_WITH_PARENT,
                                               GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, 
                                               _("Invalid musicbrainz server port number"));
        gtk_dialog_run(GTK_DIALOG(warningDialog));
        gtk_widget_destroy(warningDialog);
        somethingWrong = true;
    }
	
    if(somethingWrong)
        return false;
    else
        return true;
}

bool string_has_slashes(const char* string)
{
    int count;
    
    for(count = strlen(string) - 1; count >= 0; count--)
    {
        if(string[count] == '/')
            return true;
    }
    
    return false;
}
