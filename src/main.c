/*
Asunder

Copyright(C) 2005 Eric Lathrop <eric@ericlathrop.com>
Copyright(C) 2007 Andrew Smith <http://littlesvr.ca/contact.php>

Any code in this file may be redistributed or modified under the terms of
the GNU General Public Licence as published by the Free Software 
Foundation; version 2 of the licence.

*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gtk/gtk.h>
#include <sys/types.h>
#include <cddb/cddb.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <signal.h>
#include <syslog.h>

#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__) || defined(__NetBSD__)
    #include <sys/cdio.h>
#elif defined(__linux__)
    #include <linux/cdrom.h>
#endif

#include "main.h"
#include "interface.h"
#include "support.h"
#include "prefs.h"
#include "callbacks.h"
#include "util.h"
#include "wrappers.h"
#include "threads.h"

static unsigned int gbl_current_discid = 0;

GList * gbl_disc_matches = NULL;
gboolean track_format[100];

GtkWidget * win_main = NULL;
GtkWidget * win_prefs = NULL;
GtkWidget * win_ripping = NULL;
GtkWidget * win_about = NULL;

GtkWidget * album_artist;
GtkWidget * album_title;
GtkWidget * album_genre;					// lnr
GtkWidget * album_year;

GtkWidget * tracklist;
GtkWidget * pick_disc;
GtkWidget * statusLbl;

static GMutex refresh_mutex;
static GCond refresh_cond;
static int refresh_forced = 0;      /* modify only while protected by mutex */
static gboolean start_refresh_thread(gpointer data);

extern const char* GBLprogramName;

int gbl_null_fd;
JoinState join_artist_title = JOIN_UNSET;

int main(int argc, char *argv[])
{
    char logStr[1024];
    GtkCellRenderer *renderer;
    
#ifdef ENABLE_NLS
    /* initialize gettext */
    bindtextdomain("asunder", PACKAGE_LOCALE_DIR);
    bind_textdomain_codeset("asunder", "UTF-8"); /* so that gettext() returns UTF-8 strings */
    textdomain("asunder");
#endif
    
    /* SET UP signal handler for children */
    struct sigaction signalHandler;
    sigset_t blockedSignals;
    
    bzero(&signalHandler, sizeof(signalHandler));
    signalHandler.sa_handler = sigchld;
    //~ signalHandler.sa_flags = SA_RESTART;
    sigemptyset(&blockedSignals);
    sigaddset(&blockedSignals, SIGCHLD);
    signalHandler.sa_mask = blockedSignals;
    
    sigaction(SIGCHLD, &signalHandler, NULL);
    /* END SET UP signal handler for children */
    
    gbl_null_fd = open("/dev/null", O_RDWR);
    
    //gtk_set_locale();
    g_thread_init(NULL);
    gdk_threads_init();
    gtk_init(&argc, &argv);
    
    // If moving this in relation to the signal handler setup above - make sure
    // to pay attention to the check in sigchld(), see email from Ariel Faigon
    global_prefs = get_default_prefs();
    load_prefs(global_prefs);
    
    openlog("asunder", 0, LOG_USER);
    snprintf(logStr, 1024, "Starting %s", GBLprogramName);
    debugLog(logStr);
    
    add_pixmap_directory(PACKAGE_DATA_DIR "/pixmaps");
    snprintf(logStr, 1024, "Pixmap dir: "PACKAGE_DATA_DIR "/pixmaps\n");
    debugLog(logStr);
    
    win_main = create_main();
    album_artist = lookup_widget(win_main, "album_artist");
    album_title = lookup_widget(win_main, "album_title");
    album_genre	= lookup_widget(win_main, "album_genre");				// lnr
    album_year = lookup_widget(win_main, "album_year");
    tracklist = lookup_widget(win_main, "tracklist");
    pick_disc = lookup_widget(win_main, "pick_disc");
    statusLbl = lookup_widget(win_main, "statusLbl");
    
    // set up all the columns for the track listing widget
    renderer = gtk_cell_renderer_toggle_new();
    g_object_set(renderer, "activatable", TRUE, NULL);
    g_signal_connect(renderer, "toggled", (GCallback) on_rip_toggled, NULL);
    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(tracklist), -1, 
                    _("Rip"), renderer, "active", COL_RIPTRACK, NULL);
    GtkTreeViewColumn * col = gtk_tree_view_get_column(GTK_TREE_VIEW(tracklist), COL_RIPTRACK);
    gtk_tree_view_column_set_clickable(col, TRUE);
    g_signal_connect((gpointer)col, "clicked", G_CALLBACK(on_rip_header_click), NULL);

    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(tracklist), -1, 
                    "Track-physical", renderer, "text", COL_TRACKNUM, NULL);
    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(tracklist), -1,
                    _("Track"), renderer, "text", COL_TRACKNUM_VIS, NULL);
    col = gtk_tree_view_get_column(GTK_TREE_VIEW(tracklist), COL_TRACKNUM);
    gtk_tree_view_column_set_visible(col, FALSE);

    renderer = gtk_cell_renderer_text_new();
    g_object_set(renderer, "editable", TRUE, NULL);
    g_signal_connect(renderer, "edited", (GCallback) on_artist_edited, NULL);
    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(tracklist), -1, 
                    _("Artist"), renderer, "text", COL_TRACKARTIST, NULL);
    col = gtk_tree_view_get_column(GTK_TREE_VIEW(tracklist), COL_TRACKARTIST);
    gtk_tree_view_column_set_resizable(col, TRUE);

    renderer = gtk_cell_renderer_text_new();
    g_object_set(renderer, "editable", TRUE, NULL);
    g_signal_connect(renderer, "edited", (GCallback) on_title_edited, NULL);
    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(tracklist), -1, 
                    _("Title"), renderer, "text", COL_TRACKTITLE, NULL);
    col = gtk_tree_view_get_column(GTK_TREE_VIEW(tracklist), COL_TRACKTITLE);
    gtk_tree_view_column_set_resizable(col, TRUE);

    // Set up initially hidden column to hold artist + title if the user opts to
    // join the two fields together
    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(tracklist), -1, 
                    _("Title"), renderer, "text", COL_TRACKARTISTTITLE, NULL);

    GtkTreeViewColumn * concatenatedTitleCol = gtk_tree_view_get_column(
            GTK_TREE_VIEW(tracklist), COL_TRACKARTISTTITLE);
    gtk_tree_view_column_set_visible(concatenatedTitleCol, FALSE);

    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(tracklist), -1, 
                    _("Time"), renderer, "text", COL_TRACKTIME, NULL);
 
    // set up the columns for the album selecting dropdown box
    renderer = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(pick_disc), renderer, TRUE);
    gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(pick_disc), renderer,
                                                    "text", 0,
                                                    NULL);
    renderer = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(pick_disc), renderer, TRUE);
    gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(pick_disc), renderer,
                                                    "text", 1,
                                                    NULL);
	
    // disable the "rip" button
    // it will be enabled when check_disc() finds a disc in the drive
    gtk_widget_set_sensitive(lookup_widget(win_main, "rip_button"), FALSE);
    
    win_ripping = create_ripping();

    if (!program_exists("cdparanoia"))
    {
        GtkWidget * dialog;

        dialog = gtk_message_dialog_new(GTK_WINDOW(win_main), GTK_DIALOG_DESTROY_WITH_PARENT,
                GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, 
                _("'cdparanoia' was not found in your path. Asunder requires cdparanoia to rip CDs."));
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        exit(-1);
    }
    
	toggle_allow_tracknum();
	
    gtk_widget_show(win_main);
    
    // Start the refresh thread after gtk startup.
    g_idle_add(start_refresh_thread, NULL);

    gdk_threads_enter();
    gtk_main();
    gdk_threads_leave();
    return 0;
}

// scan the cdrom device for a disc
// returns True if a disc is present and
//   is different from the last time this was called
bool check_disc(char * cdrom)
{
    int fd;
    bool ret = false;
    int status;
    char msgStr[1024];
    gint64 diff_time, open_diff_time;
    gint64 start_time = g_get_monotonic_time();

    static bool nowBusy = true;  /* more feedback if check_disc is taking a long time */
    bool newBusy = false;
#define BUSY_THRESHOLD  (2*1000*1000)
    
#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__) || defined(__NetBSD__)
    struct ioc_read_subchannel cdsc;
    struct cd_sub_channel_info data;
#endif
    
    if (nowBusy)
    {
        // The big problem is if the disc has been changed.
        // The open could take a long time without a status message.
        gdk_threads_enter();
        gtk_label_set_markup(GTK_LABEL(statusLbl), _("<b>Checking disc...</b>"));
        gdk_threads_leave();
    }
    
    // open the device
    fd = open(cdrom, O_RDONLY | O_NONBLOCK);
    open_diff_time = g_get_monotonic_time() - start_time;
    diff_time = open_diff_time;
    if (fd < 0 || working || refresh_forced)
    {
        if (fd < 0)
            fprintf(stderr, "Error: Couldn't open %s\n", cdrom);
        if (nowBusy)
        {
            gdk_threads_enter();
            gtk_label_set_text(GTK_LABEL(statusLbl), "");
            gdk_threads_leave();
        }
        nowBusy = diff_time > BUSY_THRESHOLD;
        return false;
    }
    
    // Debug print add time last taken by the open() call.
    //snprintf(msgStr, 1024, "%s [open %.1lf sec]",
        //_("<b>Checking disc...</b>"), open_diff_time / 1E6);
    snprintf(msgStr, 1024, "%s", _("<b>Checking disc...</b>"));
    if (nowBusy || diff_time > BUSY_THRESHOLD)
    {
        gdk_threads_enter();
        gtk_label_set_markup(GTK_LABEL(statusLbl), msgStr);
        gdk_threads_leave();
    }
    
    static bool alreadyKnowGood = false; /* check when program just started */
    static bool alreadyCleared = true; /* no need to clear when program just started */
    
#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__) || defined(__NetBSD__)
    bzero(&cdsc, sizeof(cdsc));
    cdsc.data = &data;
    cdsc.data_len = sizeof(data);
    cdsc.data_format = CD_CURRENT_POSITION;
    cdsc.address_format = CD_MSF_FORMAT;
    status = ioctl(fd, CDIOCREADSUBCHANNEL, (char*)&cdsc);
    if (status >= 0)
#elif defined(__linux__)
    status = ioctl(fd, CDROM_DISC_STATUS, CDSL_CURRENT);
    if (status == CDS_AUDIO || status == CDS_MIXED)
#endif
    {
        if (!alreadyKnowGood)
        {
            ret = true;
            alreadyKnowGood = true; /* don't return true again for this disc */
            alreadyCleared = false; /* clear when disc is removed */
            newBusy = true;
        }
    }
    else
    {
        alreadyKnowGood = false; /* return true when good disc inserted */
        if (!alreadyCleared)
        {
            alreadyCleared = true;
            gdk_threads_enter();
            clear_widgets();
            gdk_threads_leave();
            newBusy = true;
        }
    }

    close(fd);
    
    diff_time = g_get_monotonic_time() - start_time;
    if (nowBusy || diff_time > BUSY_THRESHOLD)
    {
        // Debug print last time taken to do the ioctl()
        //int len = strlen(msgStr);
        //snprintf(&msgStr[len], 1024-len, " [status %.1lf sec]", (diff_time - open_diff_time) / 1E6);
        //gdk_threads_enter();
        //gtk_label_set_markup(GTK_LABEL(statusLbl), msgStr);
        //gdk_threads_leave();
    }
    else
    {
        gdk_threads_enter();
        gtk_label_set_text(GTK_LABEL(statusLbl), "");
        gdk_threads_leave();
    }
    nowBusy = newBusy || diff_time > BUSY_THRESHOLD;
    
    if (working || refresh_forced)
    {
        gdk_threads_enter();
        gtk_label_set_text(GTK_LABEL(statusLbl), "");
        gdk_threads_leave();
    }
    
    return ret;
}


void clear_widgets()
{
    gbl_current_discid = 0;

    // hide the widgets for multiple albums
    gtk_widget_hide(lookup_widget(win_main, "disc"));
    gtk_widget_hide(lookup_widget(win_main, "pick_disc"));
    
    // clear the textboxes
    gtk_entry_set_text(GTK_ENTRY(album_artist), "");
    gtk_entry_set_text(GTK_ENTRY(album_title), "");
    gtk_entry_set_text(GTK_ENTRY(album_genre), "");				// lnr
    gtk_entry_set_text(GTK_ENTRY(album_year), "");
    
    // clear the tracklist
    gtk_tree_view_set_model(GTK_TREE_VIEW(tracklist), NULL);
    
    // disable the "rip" button
    gtk_widget_set_sensitive(lookup_widget(win_main, "rip_button"), FALSE);
}


GtkTreeModel * create_model_from_disc(cddb_disc_t * disc)
{
    GtkListStore * store;
    GtkTreeIter iter;
    cddb_track_t * track;
    int seconds;
    char time[6];
    char * track_artist;
    char * track_title;
    
    store = gtk_list_store_new(NUM_COLS, 
                               G_TYPE_BOOLEAN, /* rip? checkbox */
                               G_TYPE_UINT, /* track number */
                               G_TYPE_UINT, /* track number, visual */
                               G_TYPE_STRING, /* track artist */
                               G_TYPE_STRING, /* track title */
                               G_TYPE_STRING, /* track time */
                               G_TYPE_STRING  /* artist + title concatenated */
                               );
    
    for (track = cddb_disc_get_track_first(disc); track != NULL; track = cddb_disc_get_track_next(disc))
    {
        seconds = cddb_track_get_length(track);
        snprintf(time, 6, "%02d:%02d", seconds/60, seconds%60);
        
        track_artist = (char*)cddb_track_get_artist(track);
        //trim_chars(track_artist, BADCHARS);		// lnr	//Commented out by mrpl
        trim_whitespace(track_artist);
        
        track_title = (char*)cddb_track_get_title(track); //!! this returns const char*
        //trim_chars(track_title, BADCHARS);		// lnr	//Commented out by mrpl
        trim_whitespace(track_title);
        
        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter,
            COL_RIPTRACK, track_format[cddb_track_get_number(track)],
            COL_TRACKNUM, cddb_track_get_number(track),
            COL_TRACKNUM_VIS, cddb_track_get_number(track) + global_prefs->first_track_num_offset,
            COL_TRACKARTIST, track_artist,
            COL_TRACKTITLE, track_title,
            COL_TRACKARTISTTITLE, "",
            COL_TRACKTIME, time,
            -1);
    }
    
    return GTK_TREE_MODEL(store);
}

// The original code only worked on Linux, commented it out to work 
// on BSD as well.
// The new code will hopefully work everywhere, but left the old two
// here for posterity's reference.
void eject_disc(char * cdrom)
{
    /*int fd;
    
    // open the device
    fd = open(cdrom, O_RDONLY | O_NONBLOCK);
    if (fd < 0)
    {
        fprintf(stderr, "Error: Couldn't open %s\n", cdrom);
        return;
    }

    //~ if (ioctl(fd, CDROM_DRIVE_STATUS, CDSL_CURRENT) == CDS_TRAY_OPEN)
    //~ {
        //~ ioctl(fd, CDROMCLOSETRAY, CDSL_CURRENT);
    //~ } else {
#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__) || defined(__NetBSD__)
            ioctl(fd, CDIOCALLOW);
            ioctl(fd, CDIOCEJECT);
#elif defined(__linux__)
            ioctl(fd, CDROMEJECT, CDSL_CURRENT);
#endif
    //~ }
    
    close(fd);*/
    
    if (!fork())
    {
        // Wonderful const magic to make compiler happy
        const char* const args[] = {"eject", cdrom, NULL};
        execvp(args[0], (char*const*)args);
        
        debugLog("Should never see this, why did the call to 'eject' fail?\n");
        exit(1);
    }
}

GList * cddb_query_run(cddb_conn_t * conn, cddb_disc_t * original_disc)
{
    char logStr[1024];
    int i;
    GList * matches = NULL;
    // Use clone of original disc since cddb_query_next() will overwrite it.
    cddb_disc_t * disc = cddb_disc_clone(original_disc);
    
    int num_matches = cddb_query(conn, disc);
    if (num_matches == -1)
    {
        cddb_error_print(cddb_errno(conn));
        num_matches = 0;
    }
    
    snprintf(logStr, 1024, "Found %d CDDB matches", num_matches);
    debugLog(logStr);
    
    // make a list of all the matches
    for (i = 0; i < num_matches; i++)
    {
        cddb_disc_t * possible_match = cddb_disc_clone(disc);
        if (cddb_read(conn, possible_match))
        {
            snprintf(logStr, 1024, "Match %d: '%08x' '%s' '%s' '%s'\n",
                i + 1,
                cddb_disc_get_discid(possible_match),
                cddb_disc_get_artist(possible_match),
                cddb_disc_get_title(possible_match),
                cddb_disc_get_genre(possible_match));
            debugLog(logStr);

            matches = g_list_append(matches, possible_match);
            
            // move to next match
            if (i < num_matches - 1)
            {
                if (!cddb_query_next(conn, disc))
                    fatalError("Query index out of bounds.");
            }
        }
        else
        {
            fprintf(stderr, "Failed to cddb_read()\n");
            cddb_error_print(cddb_errno(conn));
            cddb_disc_destroy(possible_match);
        }
    }
    
    cddb_disc_destroy(disc);
    return matches;
}

GList * lookup_disc(cddb_disc_t * disc)
{
    // set up the connection to the cddb server
    cddb_conn_t * conn = cddb_new();
    if (conn == NULL)
        fatalError("cddb_new() failed. Out of memory?");
    
    cddb_set_server_name(conn, global_prefs->cddb_server_name);
    cddb_set_server_port(conn, global_prefs->cddb_port_number);
    
    if (global_prefs->use_proxy)
    {
        cddb_set_http_proxy_server_name(conn, global_prefs->server_name);
        cddb_set_http_proxy_server_port(conn, global_prefs->port_number);
        cddb_http_proxy_enable(conn);
    }
    
    // force HTTP when port 80 (for MusicBrainz). This code by Tim.
    if (global_prefs->cddb_port_number == 80)
        cddb_http_enable(conn);

    // Disable caching of CDDB entries since libcddb bug fouls up multiple matches.
    // https://sourceforge.net/p/libcddb/bugs/9/
    cddb_cache_disable(conn);
    
    // show cddb update window
    gdk_threads_enter();
        disable_all_main_widgets();
        
        gtk_label_set_markup(GTK_LABEL(statusLbl), _("<b>Getting disc info from the internet...</b>"));
    gdk_threads_leave();

    // query cddb to find similar discs
    GList * matches = cddb_query_run(conn, disc);

    gdk_threads_enter();
        gtk_label_set_text(GTK_LABEL(statusLbl), "");
        
        enable_all_main_widgets();
    gdk_threads_leave();
    
    cddb_destroy(conn);
    
    return matches;
}

// reads the TOC of a cdrom into a CDDB struct
// returns the filled out struct
// so we can send it over the internet to lookup the disc
cddb_disc_t * read_disc(char * cdrom)
{
    char logStr[1024];
    int fd;
    int status;
    int i;
#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
    struct ioc_toc_header th;
    struct ioc_read_toc_single_entry te;
    struct ioc_read_subchannel cdsc;
    struct cd_sub_channel_info data;
#elif defined(__NetBSD__)
    struct ioc_toc_header th;
    struct ioc_read_toc_entry te;
    struct cd_toc_entry toc ;
    struct ioc_read_subchannel cdsc;
    struct cd_sub_channel_info data;
#elif defined(__linux__)
    struct cdrom_tochdr th;
    struct cdrom_tocentry te;
#endif
    
    cddb_disc_t * disc = NULL;
    cddb_track_t * track = NULL;

    char trackname[9];

    // open the device
    fd = open(cdrom, O_RDONLY | O_NONBLOCK);
    if (fd < 0)
    {
        fprintf(stderr, "Error: Couldn't open %s\n", cdrom);
        return NULL;
    }

    // Add indicator that read_disc() is occuring since it might take a long time.
    gdk_threads_enter();
    // Should we disable all widgets here, like lookup_disc() does?
    // Might it create a flicker?
    // Seems hardly noticable for "normal" discs.
    disable_all_main_widgets();
    gtk_label_set_markup(GTK_LABEL(statusLbl), _("<b>Reading disc...</b>"));
    gdk_threads_leave();
    
#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
    // read disc status info
    bzero(&cdsc,sizeof(cdsc));
    cdsc.data = &data;
    cdsc.data_len = sizeof(data);
    cdsc.data_format = CD_CURRENT_POSITION;
    cdsc.address_format = CD_MSF_FORMAT;
    status = ioctl(fd, CDIOCREADSUBCHANNEL, (char *)&cdsc);
    if (status >= 0)
    {
        // see if we can read the disc's table of contents (TOC).
        if (ioctl(fd, CDIOREADTOCHEADER, &th) == 0)
        {
            snprintf(logStr, 1024, "starting track: %d, ending track: %d\n", th.starting_track, th.ending_track);
            debugLog(logStr);
            
            disc = cddb_disc_new();
            if (disc == NULL)
                fatalError("cddb_disc_new() failed. Out of memory?");
            
            te.address_format = CD_LBA_FORMAT;
            for (i=th.starting_track; i<=th.ending_track; i++)
            {
                te.track = i;
                if (ioctl(fd, CDIOREADTOCENTRY, &te) == 0)
                {
                    if (te.entry.control & 0x04)
                    {
                        // track is a DATA track. make sure its "rip" box is not checked by default
                        track_format[i] = FALSE;
                    } else {
                        track_format[i] = TRUE;
                    }

                    track = cddb_track_new();
                    if (track == NULL)
                        fatalError("cddb_track_new() failed. Out of memory?");
                    
                    cddb_track_set_frame_offset(track, ntohl(te.entry.addr.lba)+SECONDS_TO_FRAMES(2));
                    snprintf(trackname, 9, "Track %d", i);
                    cddb_track_set_title(track, trackname);
                    cddb_track_set_artist(track, "Unknown Artist");
                    cddb_disc_add_track(disc, track);
                }
                if (working || refresh_forced)
                    break;
            }
            te.track = 0xAA;
            if (ioctl(fd, CDIOREADTOCENTRY, &te) == 0)
            {
                cddb_disc_set_length(disc, (ntohl(te.entry.addr.lba)+SECONDS_TO_FRAMES(2))/SECONDS_TO_FRAMES(1));
            }
        }
    }
#elif defined(__NetBSD__)
    // read disc status info
    bzero(&cdsc,sizeof(cdsc));
    cdsc.data = &data;
    cdsc.data_len = sizeof(data);
    cdsc.data_format = CD_CURRENT_POSITION;
    cdsc.address_format = CD_MSF_FORMAT;
    status = ioctl(fd, CDIOCREADSUBCHANNEL, (char *)&cdsc);
    if (status >= 0)
    {
        // see if we can read the disc's table of contents (TOC).
        if (ioctl(fd, CDIOREADTOCHEADER, &th) == 0)
        {
            snprintf(logStr, 1024, "starting track: %d, ending track: %d\n", th.starting_track, th.ending_track);
            debugLog(logStr);
            
            disc = cddb_disc_new();
            if (disc == NULL)
                fatalError("cddb_disc_new() failed. Out of memory?");
            
            te.address_format = CD_LBA_FORMAT;
            te.data = &toc ;
            te.data_len=sizeof(struct cd_toc_entry) ;
            for (i=th.starting_track; i<=th.ending_track; i++)
            {
                te.starting_track = i;
                if ((ioctl(fd, CDIOREADTOCENTRIES, &te)) == 0)
                {
                    if (te.data->control & 0x04)
                    {
                        // track is a DATA track. make sure its "rip" box is not checked by default
                        track_format[i] = FALSE;
                    } else {
                        track_format[i] = TRUE;
                    }

                    track = cddb_track_new();
                    if (track == NULL)
                        fatalError("cddb_track_new() failed. Out of memory?");
                    
                    cddb_track_set_frame_offset(track, te.data->addr.lba+SECONDS_TO_FRAMES(2));
                    snprintf(trackname, 9, "Track %d", i);
                    cddb_track_set_title(track, trackname);
                    cddb_track_set_artist(track, "Unknown Artist");
                    cddb_disc_add_track(disc, track);
                }
                if (working || refresh_forced)
                    break;
            }
            te.starting_track = 0xAA;
            if (ioctl(fd, CDIOREADTOCENTRIES, &te) == 0)
             {
                cddb_disc_set_length(disc, (te.data->addr.lba+SECONDS_TO_FRAMES(2))/SECONDS_TO_FRAMES(1));
            }
        }
    }
#elif defined(__linux__)
    // read disc status info
    status = ioctl(fd, CDROM_DISC_STATUS, CDSL_CURRENT);
    if ((status == CDS_AUDIO) || (status == CDS_MIXED))
    {
        // see if we can read the disc's table of contents (TOC).
        if (ioctl(fd, CDROMREADTOCHDR, &th) == 0)
        {
            snprintf(logStr, 1024, "starting track: %d, ending track: %d\n", th.cdth_trk0, th.cdth_trk1);
            debugLog(logStr);
            
            disc = cddb_disc_new();
            if (disc == NULL)
                fatalError("cddb_disc_new() failed. Out of memory?");
            
            te.cdte_format = CDROM_LBA;
            for (i=th.cdth_trk0; i<=th.cdth_trk1; i++)
            {
                te.cdte_track = i;
                if (ioctl(fd, CDROMREADTOCENTRY, &te) == 0)
                {
                    if (te.cdte_ctrl & CDROM_DATA_TRACK)
                    {
                        // track is a DATA track. make sure its "rip" box is not checked by default
                        track_format[i] = FALSE;
                    } else {
                        track_format[i] = TRUE;
                    }

                    track = cddb_track_new();
                    if (track == NULL)
                        fatalError("cddb_track_new() failed. Out of memory?");

                    cddb_track_set_frame_offset(track, te.cdte_addr.lba + SECONDS_TO_FRAMES(2));
                    snprintf(trackname, 9, "Track %d", i);
                    cddb_track_set_title(track, trackname);
                    cddb_track_set_artist(track, "Unknown Artist");
                    cddb_disc_add_track(disc, track);
                }
                if (working || refresh_forced)
                    break;
            }
            
            te.cdte_track = CDROM_LEADOUT;
            if (ioctl(fd, CDROMREADTOCENTRY, &te) == 0)
            {
                cddb_disc_set_length(disc, (te.cdte_addr.lba+SECONDS_TO_FRAMES(2))/SECONDS_TO_FRAMES(1));
            }
        }
    }
#endif
    close(fd);
    
    /* These two lines from Nicolas Léveillé
    * "let us have a discid for each read disc" */
    if (disc)
    {
        cddb_disc_calc_discid(disc);
        snprintf(logStr, 1024, "read_disc: discid=%08x\n", cddb_disc_get_discid(disc));
        debugLog(logStr);
    }

    gdk_threads_enter();
    gtk_label_set_text(GTK_LABEL(statusLbl), "");
    enable_all_main_widgets();
    gdk_threads_leave();

    if (working || refresh_forced)
    {
        cddb_disc_destroy(disc);
        disc = NULL;
    }

    return disc;
}


void update_tracklist(cddb_disc_t * disc)
{
    GtkTreeModel * model;
    char * disc_artist = g_strdup (cddb_disc_get_artist (disc));
    char * disc_title = g_strdup (cddb_disc_get_title (disc));
    char * disc_genre = g_strdup (cddb_disc_get_genre (disc));			// lnr
    unsigned disc_year = cddb_disc_get_year(disc);
    cddb_track_t * track;
    bool singleartist;
    char logStr[1024];
 
    // Reset column visibility to defaults
    join_artist_title = JOIN_UNSET;
    gtk_tree_view_column_set_visible(gtk_tree_view_get_column(GTK_TREE_VIEW(tracklist),
                COL_TRACKARTISTTITLE),
            FALSE);
    gtk_tree_view_column_set_visible(gtk_tree_view_get_column(GTK_TREE_VIEW(tracklist),
                COL_TRACKTITLE),
            TRUE);

    gbl_current_discid = cddb_disc_get_discid(disc);

    snprintf(logStr, 1024, "update_tracklist() disk '%08x' '%s' '%s' '%s'\n",
        gbl_current_discid, disc_artist, disc_title, disc_genre);
    debugLog(logStr);
    if (disc_artist != NULL)
    {
        //trim_chars(disc_artist, BADCHARS);			// lnr	//Commented out by mrpl
        trim_whitespace(disc_artist);
        gtk_entry_set_text(GTK_ENTRY(album_artist), disc_artist);
        
        singleartist = true;
        for (track = cddb_disc_get_track_first(disc); track != NULL; track = cddb_disc_get_track_next(disc))
        {
            if (strcmp(disc_artist, cddb_track_get_artist(track)) != 0)
            {
                singleartist = false;
                break;
            }
        }
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(lookup_widget(win_main, "single_artist")), singleartist);
    }
    if (disc_title != NULL)
    {
        //trim_chars(disc_title, BADCHARS);			// lnr	//Commented out by mrpl
        trim_whitespace(disc_title);
        gtk_entry_set_text(GTK_ENTRY(album_title), disc_title);
    }
    
    if ( disc_genre )								// lnr
    {
        //trim_chars( disc_genre, BADCHARS);			//Commented out by mrpl
        trim_whitespace( disc_genre );

        gtk_entry_set_text( GTK_ENTRY( album_genre ), disc_genre );
    }
    else
        gtk_entry_set_text( GTK_ENTRY( album_genre ), "Unknown" );
    
    if(disc_year == 0)
        disc_year = 1900;
    char disc_year_char[5];
    snprintf(disc_year_char, 5, "%d", disc_year);
    gtk_entry_set_text( GTK_ENTRY( album_year ), disc_year_char );
    
    model = create_model_from_disc(disc);
    gtk_tree_view_set_model(GTK_TREE_VIEW(tracklist), model);
    g_object_unref(model);

    g_free (disc_artist);
    g_free (disc_title);
    g_free (disc_genre);
}


void refresh(void)
{
    // This is to deal with the fact that the signal below will take up to
    // a second to get processed in refresh_thread() which will call check_disk()
    // which will set the label to the same thing at the start.
    gtk_label_set_markup(GTK_LABEL(statusLbl), _("<b>Checking disc...</b>"));
    
    /* Wake up the refresh thread. */
    g_mutex_lock(&refresh_mutex);
    refresh_forced = 1;
    g_cond_signal(&refresh_cond);
    g_mutex_unlock(&refresh_mutex);
}

void refresh_thread_body(char * cdrom, int force)
{
    cddb_disc_t * disc;
    
    if(working)
    /* don't do nothing */
        return;
    
    if (check_disc(cdrom) || force)
    {
        if (working || refresh_forced)
            return;

        disc = read_disc(cdrom);
        if (disc == NULL)
            return;
        
        if (gbl_current_discid != cddb_disc_get_discid(disc))
        {
            /* only trash the user's inputs when the disc is new */
            
            gdk_threads_enter();
            gtk_widget_set_sensitive(lookup_widget(win_main, "rip_button"), TRUE);
        
            // show the temporary info
            gtk_entry_set_text(GTK_ENTRY(album_artist), "Unknown Artist");
            gtk_entry_set_text(GTK_ENTRY(album_title), "Unknown Album");
            update_tracklist(disc);
            gdk_threads_leave();
        }
        
        if ((!global_prefs->do_cddb_updates && !force) || working || refresh_forced)
        {
            cddb_disc_destroy(disc);
            return;
        }
        
        GList * disc_matches = lookup_disc(disc);
        cddb_disc_destroy(disc);

        if (working || refresh_forced)
            return;
        
        // Only access/modify gbl_disc_matches inside gdk_thread lock, or else
        // another mutex is needed.
        gdk_threads_enter();

        // clear out the previous list of matches
        if (gbl_disc_matches != NULL)
        {
            for (GList * curr = g_list_first(gbl_disc_matches); curr != NULL; curr = g_list_next(curr))
                cddb_disc_destroy((cddb_disc_t *)curr->data);
            g_list_free(gbl_disc_matches);
        }
        gbl_disc_matches = disc_matches;

        if (gbl_disc_matches == NULL)
        {
            gdk_threads_leave();
            return;
        }
        
        if (g_list_length(gbl_disc_matches) > 1)
        {
            // fill in and show the album drop-down box
            GtkListStore * store = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_STRING);
            GtkTreeIter iter;
            GList * curr;
            cddb_disc_t * tempdisc;
            
            for (curr = g_list_first(gbl_disc_matches); curr != NULL; curr = g_list_next(curr))
            {
                tempdisc = (cddb_disc_t *)curr->data;
                gtk_list_store_append(store, &iter);
                gtk_list_store_set(store, &iter,
                    0, cddb_disc_get_artist(tempdisc),
                    1, cddb_disc_get_title(tempdisc),
                    -1);
            }
            gtk_combo_box_set_model(GTK_COMBO_BOX(pick_disc), GTK_TREE_MODEL(store));
            g_object_unref(store);
            gtk_combo_box_set_active(GTK_COMBO_BOX(pick_disc), 0);
            
            gtk_widget_show(lookup_widget(win_main, "disc"));
            gtk_widget_show(lookup_widget(win_main, "pick_disc"));
        }
        else
            update_tracklist((cddb_disc_t *)g_list_nth_data(gbl_disc_matches, 0));
        gdk_threads_leave();
    }
}

static gpointer refresh_thread(gpointer data)
{
    gint64 end_time;
    int force = 0;

    while (1)
    {
        refresh_thread_body(global_prefs->cdrom, force);

        /* Wait X ms before next iteration, or until awakened. */
        g_mutex_lock(&refresh_mutex);

        // 500ms is too flickery for my taste, especially if status messages are up
        //end_time = g_get_monotonic_time() + 500 * G_TIME_SPAN_MILLISECOND;
        end_time = g_get_monotonic_time() + 1000 * G_TIME_SPAN_MILLISECOND;

        while (!refresh_forced)
            if (!g_cond_wait_until(&refresh_cond, &refresh_mutex, end_time))
                break; /* timeout has passed */

        force = refresh_forced;
        refresh_forced = 0;

        g_mutex_unlock(&refresh_mutex);
    }

    return(NULL);
}

static gboolean start_refresh_thread(gpointer data)
{
    g_thread_create(refresh_thread, NULL, FALSE, NULL);
    return(FALSE);
}
