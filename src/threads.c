/*
Asunder

Copyright(C) 2005 Eric Lathrop <eric@ericlathrop.com>
Copyright(C) 2007 Andrew Smith <http://littlesvr.ca/contact.php>

Any code in this file may be redistributed or modified under the terms of
the GNU General Public Licence as published by the Free Software 
Foundation; version 2 of the licence.

*/

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <gtk/gtk.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>

#include "threads.h"
#include "main.h"
#include "prefs.h"
#include "util.h"
#include "wrappers.h"
#include "support.h"
#include "interface.h"
#include "completion.h"

static GMutex * barrier = NULL;
static GCond * available = NULL;
static int counter;

static FILE * playlist_wav = NULL;
static FILE * playlist_mp3 = NULL;
static FILE * playlist_ogg = NULL;
static FILE * playlist_opus = NULL;
static FILE * playlist_flac = NULL;
static FILE * playlist_wavpack = NULL;
static FILE * playlist_monkey = NULL;
static FILE * playlist_musepack = NULL;
static FILE * playlist_aac = NULL;

/* ripping or encoding, so that can know not to clear the tracklist on eject */
bool working;
/* for canceling */
bool aborted;
/* for stopping the tracking thread */
bool allDone;

static GThread * ripper;
static GThread * encoder;
static GThread * tracker;

static int tracks_to_rip;
static double rip_percent;
static double mp3_percent;
static double ogg_percent;
static double opus_percent;
static double flac_percent;
static double wavpack_percent;
static double monkey_percent;
static double musepack_percent;
static double aac_percent;
static int rip_tracks_completed;
static int encode_tracks_completed;

extern bool overwriteAll;
extern bool overwriteNone;

// aborts ripping- stops all the threads and return to normal execution
void abort_threads()
{
    aborted = true;

    pid_t pid;  /* Avoid unlikely race condition. */
    if ((pid = cdparanoia_pid) != 0)
        kill(pid, SIGKILL);
    if ((pid = lame_pid)       != 0)
        kill(pid, SIGKILL);
    if ((pid = oggenc_pid)     != 0)
        kill(pid, SIGKILL);
    if ((pid = opusenc_pid)    != 0)
        kill(pid, SIGKILL);
    if ((pid = flac_pid)       != 0)
        kill(pid, SIGKILL);
    if ((pid = wavpack_pid)    != 0)
        kill(pid, SIGKILL);
    if ((pid = monkey_pid)     != 0)
        kill(pid, SIGKILL);
    if ((pid = musepack_pid)   != 0)
        kill(pid, SIGKILL);
    if ((pid = fdkaac_pid)     != 0)
        kill(pid, SIGKILL);
    
    /* wait until all the worker threads are done */
    while (cdparanoia_pid != 0 || lame_pid != 0 || oggenc_pid != 0 || 
           opusenc_pid != 0 || flac_pid != 0 || wavpack_pid != 0 || monkey_pid != 0 ||
           musepack_pid != 0 || fdkaac_pid != 0)
    {
        debugLog("w1");
        usleep(100000);
    }
    
    g_cond_signal(available);
    
    debugLog("Aborting: 1\n");
    g_thread_join(ripper);
    debugLog("Aborting: 2\n");
    g_thread_join(encoder);
    debugLog("Aborting: 3\n");
    g_thread_join(tracker);
    debugLog("Aborting: 4 (All threads joined)\n");
    
    gtk_window_set_title(GTK_WINDOW(win_main), "Asunder");
    
    gtk_widget_hide(win_ripping);
    gdk_flush();
    working = false;
    
    show_completed_dialog(numCdparanoiaOk + numLameOk + numOggOk + numOpusOk + numFlacOk + numWavpackOk + numMonkeyOk + numMusepackOk + numAacOk,
                          numCdparanoiaFailed + numLameFailed + numOggFailed + numOpusFailed + numFlacFailed + numWavpackFailed + numMonkeyFailed + numMusepackFailed + numAacFailed);
}

// spawn needed threads and begin ripping
void dorip()
{
    char logStr[1024];
    working = true;
    aborted = false;
    allDone = false;
    counter = 0;
    if (barrier) 
        g_mutex_free(barrier);
    if (available) 
        g_cond_free(available);
    barrier = g_mutex_new();
    available = g_cond_new();
    rip_percent = 0.0;
    mp3_percent = 0.0;
    ogg_percent = 0.0;
    opus_percent = 0.0;
    flac_percent = 0.0;
    wavpack_percent = 0.0;
    monkey_percent = 0.0;
    musepack_percent = 0.0;
    aac_percent = 0.0;
    rip_tracks_completed = 0;
    encode_tracks_completed = 0;
    
    GtkTreeIter iter;
    GtkListStore * store = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(lookup_widget(win_main, "tracklist"))));
    gboolean rowsleft = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(store), &iter);

    const char * albumartist = gtk_entry_get_text(GTK_ENTRY(lookup_widget(win_main, "album_artist")));
    const char * albumtitle = gtk_entry_get_text(GTK_ENTRY(lookup_widget(win_main, "album_title")));
    const char * albumyear = gtk_entry_get_text(GTK_ENTRY(lookup_widget(win_main, "album_year")));
    const char * albumgenre = gtk_entry_get_text(GTK_ENTRY(lookup_widget(win_main, "album_genre")));
    
    //Trimmed for use in filenames	//mrpl
    char * albumartist_trimmed = strdup(albumartist);
    trim_chars(albumartist_trimmed, BADCHARS);
    char * albumtitle_trimmed = strdup(albumtitle);
    trim_chars(albumtitle_trimmed, BADCHARS);
    char * albumgenre_trimmed = strdup(albumgenre);
    trim_chars(albumgenre_trimmed, BADCHARS);

    //char * albumdir = parse_format(global_prefs->format_albumdir, 0, albumyear, albumartist, albumtitle, albumgenre, NULL);
    //char * playlist = parse_format(global_prefs->format_playlist, 0, albumyear, albumartist, albumtitle, albumgenre, NULL);
    char * albumdir = parse_format(global_prefs->format_albumdir, 0, albumyear, albumartist_trimmed, albumtitle_trimmed, albumgenre_trimmed, NULL);
    char * playlist = parse_format(global_prefs->format_playlist, 0, albumyear, albumartist_trimmed, albumtitle_trimmed, albumgenre_trimmed, NULL);
    
    overwriteAll = false;
    overwriteNone = false;
    
    // make sure there's at least one format to rip to
    if (!global_prefs->rip_wav && !global_prefs->rip_mp3 && !global_prefs->rip_ogg && !global_prefs->rip_opus &&
        !global_prefs->rip_flac && !global_prefs->rip_wavpack && !global_prefs->rip_monkey &&
        !global_prefs->rip_musepack && !global_prefs->rip_fdkaac)
    {
        GtkWidget * dialog;
        dialog = gtk_message_dialog_new(GTK_WINDOW(win_main), 
                                        GTK_DIALOG_DESTROY_WITH_PARENT, 
                                        GTK_MESSAGE_ERROR, 
                                        GTK_BUTTONS_OK, 
                                        _("No ripping/encoding method selected. Please enable one from the "
                                        "'Preferences' menu."));
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        free(albumartist_trimmed);
        free(albumtitle_trimmed);
        free(albumgenre_trimmed);
        free(albumdir);
        free(playlist);
        return;
    }
    
    // make sure there's some tracks to rip
    rowsleft = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(store), &iter);
    tracks_to_rip = 0;
    int riptrack;
    while(rowsleft)
    {
        gtk_tree_model_get(GTK_TREE_MODEL(store), &iter,
                           COL_RIPTRACK, &riptrack,
                           -1);
        if (riptrack) 
            tracks_to_rip++;
        rowsleft = gtk_tree_model_iter_next(GTK_TREE_MODEL(store), &iter);
    }
    if (tracks_to_rip == 0)
    {
        GtkWidget * dialog;
        dialog = gtk_message_dialog_new(GTK_WINDOW(win_main), 
                                        GTK_DIALOG_DESTROY_WITH_PARENT, 
                                        GTK_MESSAGE_ERROR, 
                                        GTK_BUTTONS_OK, 
                                        _("No tracks were selected for ripping/encoding. "
                                        "Please select at least one track."));
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        free(albumdir);
        free(playlist);
        free(albumartist_trimmed);
        free(albumtitle_trimmed);
        free(albumgenre_trimmed);
        return;
    }
    
    /* CREATE the album directory */
    char * dirpath = make_filename(prefs_get_music_dir(global_prefs), albumdir, NULL, NULL);
    
    snprintf(logStr, 1024, "Making album directory '%s'\n", dirpath);
    debugLog(logStr);
    
    if ( recursive_mkdir(dirpath, S_IRWXU|S_IRWXG|S_IRWXO) != 0 && 
         errno != EEXIST )
    {
        GtkWidget * dialog;
        dialog = gtk_message_dialog_new(GTK_WINDOW(win_main), 
                                        GTK_DIALOG_DESTROY_WITH_PARENT, 
                                        GTK_MESSAGE_ERROR, 
                                        GTK_BUTTONS_OK, 
                                        "Unable to create directory '%s': %s", 
                                        dirpath, strerror(errno));
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        free(dirpath);
        free(albumdir);
        free(playlist);
        free(albumartist_trimmed);
        free(albumtitle_trimmed);
        free(albumgenre_trimmed);
        return;
    }
    
    free(dirpath);
    /* END CREATE the album directory */
    
    if (global_prefs->make_playlist)
    {
        debugLog("Creating playlists\n");
        
        if (global_prefs->rip_wav)
        {
            char * filename = make_filename(prefs_get_music_dir(global_prefs), albumdir, playlist, "wav.m3u");
            
            make_playlist(filename, &playlist_wav);
            
            free(filename);
        }
        if (global_prefs->rip_mp3)
        {
            char * filename = make_filename(prefs_get_music_dir(global_prefs), albumdir, playlist, "mp3.m3u");
            
            make_playlist(filename, &playlist_mp3);
            
            free(filename);
        }
        if (global_prefs->rip_ogg)
        {
            char * filename = make_filename(prefs_get_music_dir(global_prefs), albumdir, playlist, "ogg.m3u");
            
            make_playlist(filename, &playlist_ogg);
            
            free(filename);
        }
        if (global_prefs->rip_opus)
        {
            char * filename = make_filename(prefs_get_music_dir(global_prefs), albumdir, playlist, "opus.m3u");
            
            make_playlist(filename, &playlist_opus);
            
            free(filename);
        }
        if (global_prefs->rip_flac)
        {
            char * filename = make_filename(prefs_get_music_dir(global_prefs), albumdir, playlist, "flac.m3u");
            
            make_playlist(filename, &playlist_flac);
            
            free(filename);
        }
        if (global_prefs->rip_wavpack)
        {
            char * filename = make_filename(prefs_get_music_dir(global_prefs), albumdir, playlist, "wv.m3u");
            
            make_playlist(filename, &playlist_wavpack);
            
            free(filename);
        }
        if (global_prefs->rip_monkey)
        {
            char * filename = make_filename(prefs_get_music_dir(global_prefs), albumdir, playlist, "ape.m3u");
            
            make_playlist(filename, &playlist_monkey);
            
            free(filename);
        }
        if (global_prefs->rip_musepack)
        {
            char * filename = make_filename(prefs_get_music_dir(global_prefs), albumdir, playlist, "mpc.m3u");
            
            make_playlist(filename, &playlist_musepack);
            
            free(filename);
        }
        if (global_prefs->rip_fdkaac)
        {
            char * filename = make_filename(prefs_get_music_dir(global_prefs), albumdir, playlist, "m4a.m3u");
            
            make_playlist(filename, &playlist_aac);
            
            free(filename);
        }
    }
    
    free(albumdir);
    free(playlist);
    free(albumartist_trimmed);
    free(albumtitle_trimmed);
    free(albumgenre_trimmed);
    
    gtk_widget_show(win_ripping);
    
    numCdparanoiaFailed = 0;
    numLameFailed = 0;
    numOggFailed = 0;
    numOpusFailed = 0;
    numFlacFailed = 0;
    numWavpackFailed = 0;
    numMonkeyFailed = 0;
    numMusepackFailed = 0;
    numAacFailed = 0;
    
    numCdparanoiaOk = 0;
    numLameOk = 0;
    numOggOk = 0;
    numOpusOk = 0;
    numFlacOk = 0;
    numWavpackOk = 0;
    numMonkeyOk = 0;
    numMusepackOk = 0;
    numAacOk = 0;
    
    ripper = g_thread_create(rip, NULL, TRUE, NULL);
    encoder = g_thread_create(encode, NULL, TRUE, NULL);
    tracker = g_thread_create(track, NULL, TRUE, NULL);
}

// the thread that handles ripping tracks to WAV files
gpointer rip(gpointer data)
{
    char logStr[1024];
    GtkTreeIter iter;
    
    gdk_threads_enter();
        GtkListStore * store = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(lookup_widget(win_main, "tracklist"))));
        gboolean single_artist = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(lookup_widget(win_main, "single_artist")));
        const char * albumartist = gtk_entry_get_text(GTK_ENTRY(lookup_widget(win_main, "album_artist")));
        const char * albumtitle = gtk_entry_get_text(GTK_ENTRY(lookup_widget(win_main, "album_title")));
        const char * albumyear = gtk_entry_get_text(GTK_ENTRY(lookup_widget(win_main, "album_year")));
        const char * albumgenre = gtk_entry_get_text(GTK_ENTRY(lookup_widget(win_main, "album_genre")));
        
        gboolean rowsleft = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(store), &iter);
    gdk_threads_leave();
    
    //Trimmed for use in filenames	//mrpl
    char * albumartist_trimmed = strdup(albumartist);
    trim_chars(albumartist_trimmed, BADCHARS);
    char * albumtitle_trimmed = strdup(albumtitle);
    trim_chars(albumtitle_trimmed, BADCHARS);
    char * albumgenre_trimmed = strdup(albumgenre);
    trim_chars(albumgenre_trimmed, BADCHARS);

    int titleColumn = COL_TRACKTITLE;
    
    if(single_artist && join_artist_title == JOIN_ON)
    {
        titleColumn = COL_TRACKARTISTTITLE;
    }
    
    while(rowsleft)
    {
        int riptrack;
        int tracknum;
        int tracknum_vis;
        char * trackartist;
        char * tracktitle;

        gdk_threads_enter();
            gtk_tree_model_get(GTK_TREE_MODEL(store), &iter,
                COL_RIPTRACK, &riptrack,
                COL_TRACKNUM, &tracknum,
                COL_TRACKNUM_VIS, &tracknum_vis,
                COL_TRACKARTIST, &trackartist,
                titleColumn, &tracktitle,
                -1);
        gdk_threads_leave();
        
        if (single_artist)
        {
            free(trackartist);
            trackartist = strdup(albumartist);
        }
        
        if (riptrack)
        {
            char * albumdir;
            char * musicfilename;
            char * wavfilename;
            char * trackartist_trimmed;
            char * tracktitle_trimmed;

            //Trimmed for use in filenames	//mrpl
            trackartist_trimmed = strdup(trackartist);
            trim_chars(trackartist_trimmed, BADCHARS);
            tracktitle_trimmed = strdup(tracktitle);
            trim_chars(tracktitle_trimmed, BADCHARS);
            
            //~albumdir = parse_format(global_prefs->format_albumdir, 0, albumyear, albumartist, albumtitle, albumgenre, NULL);
            //~musicfilename = parse_format(global_prefs->format_music, tracknum, trackyear, trackartist, albumtitle, tracktitle);
            //~musicfilename = parse_format(global_prefs->format_music, tracknum, albumyear, trackartist, albumtitle, albumgenre, tracktitle);            
            albumdir = parse_format(global_prefs->format_albumdir, 0, albumyear, albumartist_trimmed, albumtitle_trimmed, albumgenre_trimmed, NULL);
            musicfilename = parse_format(global_prefs->format_music, tracknum_vis, albumyear, trackartist_trimmed, albumtitle_trimmed, albumgenre_trimmed, tracktitle_trimmed);
            wavfilename = make_filename(prefs_get_music_dir(global_prefs), albumdir, musicfilename, "wav");
            
            snprintf(logStr, 1024, "Ripping track %d to \"%s\"\n", tracknum, wavfilename);
            debugLog(logStr);
            
            if (aborted)
            {
                free(albumdir);
                free(musicfilename);
                free(wavfilename);
                free(trackartist_trimmed);
                free(tracktitle_trimmed);

                free(trackartist);
                free(tracktitle);

                free(albumartist_trimmed);
                free(albumtitle_trimmed);
                free(albumgenre_trimmed);
                return NULL;
            }
            
            struct stat statStruct;
            int rc;
            bool doRip;
            
            rc = stat(wavfilename, &statStruct);
            if(rc == 0)
            {
                gdk_threads_enter();
                    if(confirmOverwrite(wavfilename))
                        doRip = true;
                    else
                        doRip = false;
                gdk_threads_leave();
            }
            else
                doRip = true;
            
            if(doRip)
                cdparanoia(global_prefs->cdrom, tracknum, wavfilename, &rip_percent);
            
            free(albumdir);
            free(musicfilename);
            free(wavfilename);
            free(trackartist_trimmed);
            free(tracktitle_trimmed);
            
            rip_percent = 0.0;
            rip_tracks_completed++;
        }

        free(trackartist);
        free(tracktitle);
        
        debugLog("rip() waiting for barrier\n");
        g_mutex_lock(barrier);
        counter++;
        debugLog("rip() done waiting\n");
        g_mutex_unlock(barrier);
        debugLog("rip() signaling available\n");
        g_cond_signal(available);
        
        if (aborted)
        {
            free(albumartist_trimmed);
            free(albumtitle_trimmed);
            free(albumgenre_trimmed);
            return NULL;
        }
        
        gdk_threads_enter();
            rowsleft = gtk_tree_model_iter_next(GTK_TREE_MODEL(store), &iter);
        gdk_threads_leave();
    }
    
    free(albumartist_trimmed);
    free(albumtitle_trimmed);
    free(albumgenre_trimmed);
    
    return NULL;
}

void close_playlists(void)
{
    if (playlist_wav) 
        fclose(playlist_wav);
    playlist_wav = NULL;
    if (playlist_mp3) 
        fclose(playlist_mp3);
    playlist_mp3 = NULL;
    if (playlist_ogg) 
        fclose(playlist_ogg);
    playlist_ogg = NULL;
    if (playlist_opus) 
        fclose (playlist_opus);
    playlist_opus = NULL;
    if (playlist_flac) 
        fclose(playlist_flac);
    playlist_flac = NULL;
    if (playlist_wavpack) 
        fclose(playlist_wavpack);
    playlist_wavpack = NULL;
    if (playlist_monkey) 
        fclose(playlist_monkey);
    playlist_monkey = NULL;
    if (playlist_musepack) 
        fclose(playlist_musepack);
    playlist_musepack = NULL;
    if (playlist_aac) 
        fclose(playlist_aac);
    playlist_aac = NULL;
}

// the thread that handles encoding WAV files to all other formats
gpointer encode(gpointer data)
{
    char logStr[1024];
    GtkTreeIter iter;
    int rc;
    
    char* album_artist = NULL;
    char* album_title = NULL;
    char* album_genre = NULL;		// lnr
    char* album_year = NULL;
    
    char* album_artist_trimmed = NULL;
    char* album_title_trimmed = NULL;
    char* album_genre_trimmed = NULL;
    
    struct stat statStruct;
    bool doEncode;
    
    gdk_threads_enter();
        GtkListStore * store = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(lookup_widget(win_main, "tracklist"))));
        gboolean single_artist = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(lookup_widget(win_main, "single_artist")));

        GtkWidget * album_artist_widget = lookup_widget(win_main, "album_artist");
        album_artist = strdup(gtk_entry_get_text(GTK_ENTRY(album_artist_widget)));
        add_completion(album_artist_widget);
        save_completion(album_artist_widget);
        
        album_year = strdup(gtk_entry_get_text(GTK_ENTRY(lookup_widget(win_main, "album_year"))));
        
        GtkWidget * album_title_widget = lookup_widget(win_main, "album_title");
        album_title = strdup(gtk_entry_get_text(GTK_ENTRY(album_title_widget)));
        add_completion(album_title_widget);
        save_completion(album_title_widget);

        GtkWidget * album_genre_widget = lookup_widget(win_main, "album_genre");
        album_genre = strdup(gtk_entry_get_text(GTK_ENTRY(album_genre_widget)));      // lnr
        add_completion(album_genre_widget);
        save_completion(album_genre_widget);
        
        gboolean rowsleft = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(store), &iter);
    gdk_threads_leave();
    
    //Trimmed for use in filenames	//mrpl
    album_artist_trimmed = strdup(album_artist);
    trim_chars(album_artist_trimmed, BADCHARS);
    album_title_trimmed = strdup(album_title);
    trim_chars(album_title_trimmed, BADCHARS);
    album_genre_trimmed = strdup(album_genre);
    trim_chars(album_genre_trimmed, BADCHARS);
 
    int titleColumn = COL_TRACKTITLE;
    if(single_artist && join_artist_title == JOIN_ON)
    {
        titleColumn = COL_TRACKARTISTTITLE;
    }
    
    while(rowsleft)
    {
        int riptrack;
        int tracknum;
        int tracknum_vis;
        char* trackartist;
        char* tracktitle;
        char* tracktime;
        int min;
        int sec;

        debugLog("encode() waiting for 'barrier'\n");
        g_mutex_lock(barrier);
        while ((counter < 1) && (!aborted))
        {
            debugLog("encode() waiting for 'available'\n");
            g_cond_wait(available, barrier);
        }
        counter--;
        snprintf(logStr, 1024, "encode() done waiting, counter is now %d\n", counter);
        debugLog(logStr);
        g_mutex_unlock(barrier);

        if (aborted)
        {
            free(album_artist);
            free(album_title);
            free(album_genre);
            free(album_year);
            free(album_artist_trimmed);
            free(album_title_trimmed);
            free(album_genre_trimmed);

            close_playlists();
            return NULL;
        }
        
        gdk_threads_enter();
            gtk_tree_model_get(GTK_TREE_MODEL(store), &iter,
                COL_RIPTRACK, &riptrack,
                COL_TRACKNUM, &tracknum,
                COL_TRACKNUM_VIS, &tracknum_vis,
                COL_TRACKARTIST, &trackartist,
                titleColumn, &tracktitle,
                COL_TRACKTIME, &tracktime,
                -1);
        gdk_threads_leave();
        sscanf(tracktime, "%d:%d", &min, &sec);
        
        if (single_artist)
        {
            free(trackartist);
            trackartist = strdup(album_artist);
        }

        if (riptrack)
        {
            char* albumdir;
            char* musicfilename;
            char* wavfilename;
            char* trackartist_trimmed;
            char* tracktitle_trimmed;

            //Trimmed for use in filenames	//mrpl
            trackartist_trimmed = strdup(trackartist);
            trim_chars(trackartist_trimmed, BADCHARS);
            tracktitle_trimmed = strdup(tracktitle);
            trim_chars(tracktitle_trimmed, BADCHARS);
            
            //~albumdir = parse_format(global_prefs->format_albumdir, 0, album_year, album_artist, album_title, genre, NULL);
            //~musicfilename = parse_format(global_prefs->format_music, tracknum, trackyear, trackartist, album_title, tracktitle);
            //~musicfilename = parse_format(global_prefs->format_music, tracknum, album_year, trackartist, album_title, genre, tracktitle);
            albumdir = parse_format(global_prefs->format_albumdir, 0, album_year, album_artist_trimmed, album_title_trimmed, album_genre_trimmed, NULL);
            musicfilename = parse_format(global_prefs->format_music, tracknum_vis, album_year, trackartist_trimmed, album_title_trimmed, album_genre_trimmed, tracktitle_trimmed);
            
            wavfilename = make_filename(prefs_get_music_dir(global_prefs), albumdir, musicfilename, "wav");

            if (global_prefs->rip_mp3)
            {
                if (aborted)
                {
                    free(albumdir);
                    free(musicfilename);
                    free(wavfilename);
                    free(trackartist_trimmed);
                    free(tracktitle_trimmed);

                    free(trackartist);
                    free(tracktitle);
                    free(tracktime);

                    free(album_artist);
                    free(album_title);
                    free(album_genre);
                    free(album_year);
                    free(album_artist_trimmed);
                    free(album_title_trimmed);
                    free(album_genre_trimmed);

                    close_playlists();
                    return NULL;
                }

                char * mp3filename = make_filename(prefs_get_music_dir(global_prefs), albumdir, musicfilename, "mp3");
                snprintf(logStr, 1024, "Encoding track %d to \"%s\"\n", tracknum, mp3filename);
                debugLog(logStr);
                
                rc = stat(mp3filename, &statStruct);
                if(rc == 0)
                {
                    gdk_threads_enter();
                        if(confirmOverwrite(mp3filename))
                            doEncode = true;
                        else
                            doEncode = false;
                    gdk_threads_leave();
                }
                else
                    doEncode = true;

                if(doEncode)
                    //~ lame(tracknum, trackartist, album_title, tracktitle, genre, trackyear, wavfilename, mp3filename, 
                         //~ global_prefs->mp3_vbr, global_prefs->mp3_bitrate, &mp3_percent);
                    lame(tracknum_vis, trackartist, album_title, tracktitle, album_genre, album_year, wavfilename, mp3filename,
                         global_prefs->mp3_vbr, global_prefs->mp3_bitrate, &mp3_percent);

                if (playlist_mp3)
                {
                    fprintf(playlist_mp3, "#EXTINF:%d,%s - %s\n", (min*60)+sec, trackartist, tracktitle);
                    fprintf(playlist_mp3, "%s\n", basename(mp3filename));
                    fflush(playlist_mp3);
                }
                free(mp3filename);
            }
            if (global_prefs->rip_ogg)
            {
                if (aborted)
                {
                    free(albumdir);
                    free(musicfilename);
                    free(wavfilename);
                    free(trackartist_trimmed);
                    free(tracktitle_trimmed);

                    free(trackartist);
                    free(tracktitle);
                    free(tracktime);

                    free(album_artist);
                    free(album_title);
                    free(album_genre);
                    free(album_year);
                    free(album_artist_trimmed);
                    free(album_title_trimmed);
                    free(album_genre_trimmed);

                    close_playlists();
                    return NULL;
                }

                char * oggfilename = make_filename(prefs_get_music_dir(global_prefs), albumdir, musicfilename, "ogg");
                snprintf(logStr, 1024, "Encoding track %d to \"%s\"\n", tracknum, oggfilename);
                debugLog(logStr);
                
                rc = stat(oggfilename, &statStruct);
                if(rc == 0)
                {
                    gdk_threads_enter();
                        if(confirmOverwrite(oggfilename))
                            doEncode = true;
                        else
                            doEncode = false;
                    gdk_threads_leave();
                }
                else
                    doEncode = true;
                
                if(doEncode)
                {
                    oggenc(tracknum_vis, trackartist, album_title, tracktitle, album_year, album_genre, wavfilename,
                           oggfilename, global_prefs->ogg_quality, &ogg_percent);
                }

                if (playlist_ogg)
                {
                    fprintf(playlist_ogg, "#EXTINF:%d,%s - %s\n", (min*60)+sec, trackartist, tracktitle);
                    fprintf(playlist_ogg, "%s\n", basename(oggfilename));
                    fflush(playlist_ogg);
                }
                free(oggfilename);
            }
            if (global_prefs->rip_opus)
            {
                if (aborted)
                {
                    free(albumdir);
                    free(musicfilename);
                    free(wavfilename);
                    free(trackartist_trimmed);
                    free(tracktitle_trimmed);

                    free(trackartist);
                    free(tracktitle);
                    free(tracktime);

                    free(album_artist);
                    free(album_title);
                    free(album_genre);
                    free(album_year);
                    free(album_artist_trimmed);
                    free(album_title_trimmed);
                    free(album_genre_trimmed);

                    close_playlists();
                    return NULL;
                }

                char * opusfilename = make_filename(prefs_get_music_dir(global_prefs), albumdir, musicfilename, "opus");
                snprintf(logStr, 1024, "Encoding track %d to \"%s\"\n", tracknum, opusfilename);
                debugLog(logStr);

                rc = stat(opusfilename, &statStruct);
                if(rc == 0)
                {
                    gdk_threads_enter();
                        if(confirmOverwrite(opusfilename))
                            doEncode = true;
                        else
                            doEncode = false;
                    gdk_threads_leave();
                }
                else
                    doEncode = true;

                if(doEncode)
                    opusenc(tracknum_vis, trackartist, album_title, tracktitle, album_year, album_genre, wavfilename,
                           opusfilename, global_prefs->opus_bitrate, &opus_percent);

                if (playlist_opus)
                {
                    fprintf(playlist_opus, "#EXTINF:%d,%s - %s\n", (min*60)+sec, trackartist, tracktitle);
                    fprintf(playlist_opus, "%s\n", basename(opusfilename));
                    fflush(playlist_opus);
                }
                free(opusfilename);
            }
            if (global_prefs->rip_flac)
            {
                if (aborted)
                {
                    free(albumdir);
                    free(musicfilename);
                    free(wavfilename);
                    free(trackartist_trimmed);
                    free(tracktitle_trimmed);

                    free(trackartist);
                    free(tracktitle);
                    free(tracktime);

                    free(album_artist);
                    free(album_title);
                    free(album_genre);
                    free(album_year);
                    free(album_artist_trimmed);
                    free(album_title_trimmed);
                    free(album_genre_trimmed);

                    close_playlists();
                    return NULL;
                }

                char * flacfilename = make_filename(prefs_get_music_dir(global_prefs), albumdir, musicfilename, "flac");
                snprintf(logStr, 1024, "Encoding track %d to \"%s\"\n", tracknum, flacfilename);
                debugLog(logStr);
                
                rc = stat(flacfilename, &statStruct);
                if(rc == 0)
                {
                    gdk_threads_enter();
                        if(confirmOverwrite(flacfilename))
                            doEncode = true;
                        else
                            doEncode = false;
                    gdk_threads_leave();
                }
                else
                    doEncode = true;
                
                if(doEncode)
                    //~ flac(tracknum, trackartist, album_title, tracktitle, genre, trackyear, wavfilename, 
                         //~ flacfilename, global_prefs->flac_compression, &flac_percent);
                    flac(tracknum_vis, trackartist, album_artist, single_artist, album_title, tracktitle, album_genre, album_year, wavfilename,
                         flacfilename, global_prefs->flac_compression, &flac_percent); //mw

                
                if (playlist_flac)
                {
                    fprintf(playlist_flac, "#EXTINF:%d,%s - %s\n", (min*60)+sec, trackartist, tracktitle);
                    fprintf(playlist_flac, "%s\n", basename(flacfilename));
                    fflush(playlist_flac);
                }
                free(flacfilename);
            }
            if (global_prefs->rip_wavpack)
            {
                if (aborted)
                {
                    free(albumdir);
                    free(musicfilename);
                    free(wavfilename);
                    free(trackartist_trimmed);
                    free(tracktitle_trimmed);

                    free(trackartist);
                    free(tracktitle);
                    free(tracktime);

                    free(album_artist);
                    free(album_title);
                    free(album_genre);
                    free(album_year);
                    free(album_artist_trimmed);
                    free(album_title_trimmed);
                    free(album_genre_trimmed);

                    close_playlists();
                    return NULL;
                }

                // Create the longer filename first, then derive the shorter filename.
                // If the .wvc filename was shortened by make_filename(), then
                // the .wv filename will have the same base as the .wvc filename.
                // If hybrid is false, we could squeeze one more character into the .wv
                // filename, but why bother?
                char * wavpackfilename2 = make_filename(prefs_get_music_dir(global_prefs), albumdir, musicfilename, "wvc");
                char * wavpackfilename = strdup(wavpackfilename2);
                wavpackfilename[strlen(wavpackfilename) - 1] = '\0';    // Change .wvc to .wv
                snprintf(logStr, 1024, "Encoding track %d to \"%s\"\n", tracknum, wavpackfilename);
                debugLog(logStr);
                
                rc = stat(wavpackfilename, &statStruct);
                int rc2 = stat(wavpackfilename2, &statStruct);
                if(rc == 0 || rc2 == 0)
                {
                    gdk_threads_enter();
                        if(confirmOverwrite(wavpackfilename))
                            doEncode = true;
                        else
                            doEncode = false;
                    gdk_threads_leave();
                }
                else
                    doEncode = true;
                
                if(doEncode)
                {
                    wavpack(tracknum_vis, trackartist, album_title, tracktitle, album_year, album_genre,
                            wavfilename, wavpackfilename, wavpackfilename2,
                            global_prefs->wavpack_compression, global_prefs->wavpack_hybrid, 
                            int_to_wavpack_bitrate(global_prefs->wavpack_bitrate), &wavpack_percent);
                }
                
                if (playlist_wavpack)
                {
                    fprintf(playlist_wavpack, "#EXTINF:%d,%s - %s\n", (min*60)+sec, trackartist, tracktitle);
                    fprintf(playlist_wavpack, "%s\n", basename(wavpackfilename));
                    fflush(playlist_wavpack);
                }
                free(wavpackfilename);
                free(wavpackfilename2);
            }
            if (global_prefs->rip_monkey)
            {
                if (aborted)
                {
                    free(albumdir);
                    free(musicfilename);
                    free(wavfilename);
                    free(trackartist_trimmed);
                    free(tracktitle_trimmed);

                    free(trackartist);
                    free(tracktitle);
                    free(tracktime);

                    free(album_artist);
                    free(album_title);
                    free(album_genre);
                    free(album_year);
                    free(album_artist_trimmed);
                    free(album_title_trimmed);
                    free(album_genre_trimmed);

                    close_playlists();
                    return NULL;
                }

                char * monkeyfilename = make_filename(prefs_get_music_dir(global_prefs), albumdir, musicfilename, "ape");
                snprintf(logStr, 1024, "Encoding track %d to \"%s\"\n", tracknum, monkeyfilename);
                debugLog(logStr);
                
                rc = stat(monkeyfilename, &statStruct);
                if(rc == 0)
                {
                    gdk_threads_enter();
                        if(confirmOverwrite(monkeyfilename))
                            doEncode = true;
                        else
                            doEncode = false;
                    gdk_threads_leave();
                }
                else
                    doEncode = true;
                
                if(doEncode)
                {
                    mac(wavfilename, monkeyfilename,
                        int_to_monkey_int(global_prefs->monkey_compression), 
                        &monkey_percent);
                }
                
                if (playlist_monkey)
                {
                    fprintf(playlist_monkey, "#EXTINF:%d,%s - %s\n", (min*60)+sec, trackartist, tracktitle);
                    fprintf(playlist_monkey, "%s\n", basename(monkeyfilename));
                    fflush(playlist_monkey);
                }
                free(monkeyfilename);
            }
            if (global_prefs->rip_musepack)
            {
                if (aborted)
                {
                    free(albumdir);
                    free(musicfilename);
                    free(wavfilename);
                    free(trackartist_trimmed);
                    free(tracktitle_trimmed);

                    free(trackartist);
                    free(tracktitle);
                    free(tracktime);

                    free(album_artist);
                    free(album_title);
                    free(album_genre);
                    free(album_year);
                    free(album_artist_trimmed);
                    free(album_title_trimmed);
                    free(album_genre_trimmed);

                    close_playlists();
                    return NULL;
                }

                char * musepackfilename = make_filename(prefs_get_music_dir(global_prefs), albumdir, musicfilename, "mpc");
                snprintf(logStr, 1024, "Encoding track %d to \"%s\"\n", tracknum, musepackfilename);
                debugLog(logStr);
                
                rc = stat(musepackfilename, &statStruct);
                if(rc == 0)
                {
                    gdk_threads_enter();
                        if(confirmOverwrite(musepackfilename))
                            doEncode = true;
                        else
                            doEncode = false;
                    gdk_threads_leave();
                }
                else
                    doEncode = true;
                
                if(doEncode)
                {
                    musepack(tracknum_vis, trackartist, album_title, tracktitle, album_year, album_genre,
                             wavfilename, musepackfilename,
                             int_to_musepack_int(global_prefs->musepack_bitrate), 
                             &musepack_percent);
                }
                
                if (playlist_musepack)
                {
                    fprintf(playlist_musepack, "#EXTINF:%d,%s - %s\n", (min*60)+sec, trackartist, tracktitle);
                    fprintf(playlist_musepack, "%s\n", basename(musepackfilename));
                    fflush(playlist_musepack);
                }
                free(musepackfilename);
            }
            if (global_prefs->rip_fdkaac)
            {
                if (aborted)
                {
                    free(albumdir);
                    free(musicfilename);
                    free(wavfilename);
                    free(trackartist_trimmed);
                    free(tracktitle_trimmed);

                    free(trackartist);
                    free(tracktitle);
                    free(tracktime);

                    free(album_artist);
                    free(album_title);
                    free(album_genre);
                    free(album_year);
                    free(album_artist_trimmed);
                    free(album_title_trimmed);
                    free(album_genre_trimmed);

                    close_playlists();
                    return NULL;
                }
				
                char * aacfilename = make_filename(prefs_get_music_dir(global_prefs), albumdir, musicfilename, "m4a");
                snprintf(logStr, 1024, "Encoding track %d to \"%s\"\n", tracknum, aacfilename);
                debugLog(logStr);
                
                rc = stat(aacfilename, &statStruct);
                if(rc == 0)
                {
                    gdk_threads_enter();
                        if(confirmOverwrite(aacfilename))
                            doEncode = true;
                        else
                            doEncode = false;
                    gdk_threads_leave();
                }
                else
                    doEncode = true;
                
                if(doEncode)
                {
                    fdkaac(tracknum, trackartist, album_title, tracktitle, album_genre, album_year,
                           wavfilename, aacfilename,
                           global_prefs->fdkaac_bitrate,
                           &aac_percent);
                }
                
                if (playlist_aac)
                {
                    fprintf(playlist_aac, "#EXTINF:%d,%s - %s\n", (min*60)+sec, trackartist, tracktitle);
                    fprintf(playlist_aac, "%s\n", basename(aacfilename));
                    fflush(playlist_aac);
                }
                free(aacfilename);
            }
            if (!global_prefs->rip_wav)
            {
                snprintf(logStr, 1024, "Removing track %d WAV file\n", tracknum);
                debugLog(logStr);
                
                if (unlink(wavfilename) != 0)
                {
                    snprintf(logStr, 1024, "Unable to delete WAV file \"%s\": %s\n", wavfilename, strerror(errno));
                    debugLog(logStr);
                }
            } else {
                if (playlist_wav)
                {
                    fprintf(playlist_wav, "#EXTINF:%d,%s - %s\n", (min*60)+sec, trackartist, tracktitle);
                    fprintf(playlist_wav, "%s\n", basename(wavfilename));
                    fflush(playlist_wav);
                }
            }
            
            free(albumdir);
            free(musicfilename);
            free(wavfilename);
            free(trackartist_trimmed);
            free(tracktitle_trimmed);
            
            mp3_percent = 0.0;
            ogg_percent = 0.0;
            opus_percent = 0.0;
            flac_percent = 0.0;
            wavpack_percent = 0.0;
            monkey_percent = 0.0;
            musepack_percent = 0.0;
            aac_percent = 0.0;
            encode_tracks_completed++;
        }

        free(trackartist);
        free(tracktitle);
        free(tracktime);
        
        if (aborted)
        {
            free(album_artist);
            free(album_title);
            free(album_genre);
            free(album_year);
            free(album_artist_trimmed);
            free(album_title_trimmed);
            free(album_genre_trimmed);

            close_playlists();
            return NULL;
        }
        
        gdk_threads_enter();
            rowsleft = gtk_tree_model_iter_next(GTK_TREE_MODEL(store), &iter);
        gdk_threads_leave();
    }
    
    free(album_artist);
    free(album_title);
    free(album_genre);
    free(album_year);
    free(album_artist_trimmed);
    free(album_title_trimmed);
    free(album_genre_trimmed);
    
    close_playlists();
    
    g_mutex_free(barrier);
    barrier = NULL;
    g_cond_free(available);
    available = NULL;
    
    /* wait until all the worker threads are done */
    while (cdparanoia_pid != 0 || lame_pid != 0 || oggenc_pid != 0 || 
           opusenc_pid != 0 || flac_pid != 0 || wavpack_pid != 0 || monkey_pid != 0 ||
           musepack_pid != 0 || fdkaac_pid != 0)
    {
        debugLog("w2");
        usleep(100000);
    }
    
    allDone = true; // so the tracker thread will exit
    working = false;
    
    gdk_threads_enter();
        gtk_widget_hide(win_ripping);
        gdk_flush();
        
    // no more tracks to encode, safe to eject
    if (global_prefs->eject_on_done)
    {
        eject_disc(global_prefs->cdrom);
    }

        show_completed_dialog(numCdparanoiaOk + numLameOk + numOggOk + numOpusOk + numFlacOk + numWavpackOk + numMonkeyOk + numMusepackOk + numAacOk,
                              numCdparanoiaFailed + numLameFailed + numOggFailed + numOpusFailed + numFlacFailed + numWavpackFailed + numMonkeyFailed + numMusepackFailed + numAacFailed);
    gdk_threads_leave();
    
    return NULL;
}

// the thread that calculates the progress of the other threads and updates the progress bars
gpointer track(gpointer data)
{
    char logStr[1024];
    int parts = 1;
    if(global_prefs->rip_mp3) 
        parts++;
    if(global_prefs->rip_ogg) 
        parts++;
    if(global_prefs->rip_opus)
        parts++;
    if(global_prefs->rip_flac) 
        parts++;
    if(global_prefs->rip_wavpack) 
        parts++;
    if(global_prefs->rip_monkey) 
        parts++;
    if(global_prefs->rip_musepack) 
        parts++;
    if(global_prefs->rip_fdkaac)
        parts++;
    
    gdk_threads_enter();
        GtkProgressBar * progress_total = GTK_PROGRESS_BAR(lookup_widget(win_ripping, "progress_total"));
        GtkProgressBar * progress_rip = GTK_PROGRESS_BAR(lookup_widget(win_ripping, "progress_rip"));
        GtkProgressBar * progress_encode = GTK_PROGRESS_BAR(lookup_widget(win_ripping, "progress_encode"));
        
        gtk_progress_bar_set_fraction(progress_total, 0.0);
        gtk_progress_bar_set_text(progress_total, _("Waiting..."));
        gtk_progress_bar_set_fraction(progress_rip, 0.0);
        gtk_progress_bar_set_text(progress_rip, _("Waiting..."));
        if (parts > 1)
        {
            gtk_progress_bar_set_fraction(progress_encode, 0.0);
            gtk_progress_bar_set_text(progress_encode, _("Waiting..."));
        } else {
            gtk_progress_bar_set_fraction(progress_encode, 1.0);
            gtk_progress_bar_set_text(progress_encode, "100% (0/0)");
        }
    gdk_threads_leave();
    
    double prip;
    char srip[13];
    double pencode = 0;
    char sencode[13];
    double ptotal;
    char stotal[5];
    char windowTitle[15]; /* "Asunder - 100%" */
    int looper = 0;
    
    while (!allDone)
    {
        if (aborted) g_thread_exit(NULL);
        
        prip = (rip_tracks_completed+rip_percent) / tracks_to_rip;
        snprintf(srip, 13, "%d%% (%d/%d)", (int)(prip*100),
                 (rip_tracks_completed < tracks_to_rip)
                     ? (rip_tracks_completed + 1)
                     : tracks_to_rip,
                 tracks_to_rip);
        if (parts > 1)
        {
            pencode = ((double)encode_tracks_completed/(double)tracks_to_rip) + 
                       ((mp3_percent+ogg_percent+flac_percent+wavpack_percent+monkey_percent
                         +opus_percent+musepack_percent+aac_percent) /
                        (parts-1) / tracks_to_rip);
            snprintf(sencode, 13, "%d%% (%d/%d)", (int)(pencode*100),
                     (encode_tracks_completed < tracks_to_rip)
                         ? (encode_tracks_completed + 1)
                         : tracks_to_rip,
                     tracks_to_rip);
            ptotal = prip/parts + pencode*(parts-1)/parts;
        } else {
            ptotal = prip;
        }
        snprintf(stotal, 5, "%d%%", (int)(ptotal*100));
        
        strcpy(windowTitle, "Asunder - ");
        strcat(windowTitle, stotal);
        
        if (aborted) g_thread_exit(NULL);
        
        gdk_threads_enter();
            gtk_progress_bar_set_fraction(progress_rip, prip);
            gtk_progress_bar_set_text(progress_rip, srip);
            if (parts > 1)
            {
                gtk_progress_bar_set_fraction(progress_encode, pencode);
                gtk_progress_bar_set_text(progress_encode, sencode);
            }
            
            gtk_progress_bar_set_fraction(progress_total, ptotal);
            gtk_progress_bar_set_text(progress_total, stotal);
            
            gtk_window_set_title(GTK_WINDOW(win_main), windowTitle);
        gdk_threads_leave();

        if ((looper % 20) == 0) {
            snprintf(logStr, 1024, "completed tracks %d, rip %.2lf%%; encoded tracks %d, "
                     "mp3 %.2lf%% ogg %.2lf%% opus %.2lf%% flac %.2lf%% wavpack %.2lf%% "
                     "monkey %.2lf%% musepack %.2lf%% aac %.2lf%%; prip %.2lf%% pencode %.2lf%%\n",
                     rip_tracks_completed, rip_percent*100, encode_tracks_completed,
                     mp3_percent*100, ogg_percent*100, opus_percent*100, flac_percent*100, wavpack_percent*100,
                     monkey_percent*100,musepack_percent*100,aac_percent*100,
                     prip*100, pencode*100);
            debugLog(logStr);
        }
        looper++;
        
        usleep(100000);
    }
    
    gdk_threads_enter();
        gtk_window_set_title(GTK_WINDOW(win_main), "Asunder");
    gdk_threads_leave();
    
    return NULL;
}
