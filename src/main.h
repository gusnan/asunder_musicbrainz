#include <gtk/gtk.h>
#include <stdbool.h>

// LNR - I think the editable genre column should come before the static track time,
// but it could arguably come between artist and title so the two editable columns
// would be together. 
enum
{
    COL_RIPTRACK,
    COL_TRACKNUM,
    COL_TRACKNUM_VIS,
    COL_TRACKARTIST,
    COL_TRACKTITLE,
    COL_TRACKARTISTTITLE,
    COL_TRACKTIME,
    NUM_COLS
};

// creates a tree model that represents the data in the asunder_disc
GtkTreeModel * create_model_from_disc(asunder_disc * disc);

// open/close the drive's tray
void eject_disc(char * cdrom);

// looks up the given asunder_disc in the online database, and fills in the values
GList * lookup_disc(asunder_disc * disc);

// the main logic for scanning the discs
void refresh(void);

// updates all the necessary widgets with the data for the given asunder_disc
void update_tracklist(asunder_disc * disc);

void clear_widgets();

extern GList * gbl_disc_matches;

extern GtkWidget * win_main;
extern GtkWidget * win_prefs;
extern GtkWidget * win_ripping;
extern GtkWidget * win_about;

extern GtkWidget * tracklist;

extern int gbl_null_fd;

typedef enum JoinState
{
    JOIN_UNSET,
    JOIN_OFF,
    JOIN_ON
} JoinState;

extern JoinState join_artist_title;

//#define DEBUG
