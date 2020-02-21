#include <stdbool.h>

typedef struct _prefs
{
    char* cdrom;
    char* music_dir;
    int make_playlist;
    int make_albumdir; /* no longer used */
    char* format_music;
    char* format_playlist;
    char* format_albumdir;
    int rip_wav;
    int rip_mp3;
    int rip_ogg;
    int rip_flac;
    int mp3_vbr;
    int mp3_bitrate;
    int ogg_quality;
    int flac_compression;
    char* invalid_chars; /* no longer used */
    int main_window_width;
    int main_window_height;
    int eject_on_done;
    int do_musicbrainz_updates;
    int use_proxy;
    char* proxy_server_name;
    int proxy_port_number;
    int rip_wavpack;
    int wavpack_compression;
    int wavpack_hybrid;
    int wavpack_bitrate;
    int do_log;
    char* musicbrainz_server_name;
    int musicbrainz_port_number;
    int rip_monkey;
    int monkey_compression;
    int rip_aac; /* no longer used */
    int aac_quality; /* no longer used */
    int rip_musepack;
    int musepack_bitrate;
    int more_formats_expanded;
    int proprietary_formats_expanded; /* currently unused */
    int rip_opus;
    int opus_bitrate;
    int do_fast_rip;
    int allow_first_track_num_change;
    int first_track_num_offset;
    int track_num_width;
    int rip_fdkaac;
    int fdkaac_bitrate;
    char concatenated_track_separator[2];

} prefs;

#define DEFAULT_PROXY_PORT 8080
#define DEFAULT_MUSICBRAINZ_SERVER "musicbrainz.org"
#define DEFAULT_MUSICBRAINZ_SERVER_PORT 8880

extern prefs * global_prefs;

// allocate memory for a new prefs struct
// and make sure everything is set to NULL
prefs * new_prefs();

void clear_prefs(prefs * p);

// free memory allocated for prefs struct
// also frees any strings pointed to in the struct
void delete_prefs(prefs * p);

// returns a new prefs struct with all members set to nice default values
// this struct must be freed with delete_prefs()
prefs * get_default_prefs();

// sets up all of the widgets in the preferences dialog to
// match the given prefs struct
void set_widgets_from_prefs(prefs * p);

void get_prefs_from_widgets(prefs * p);

// store the given prefs struct to the config file
void save_prefs(prefs * p);

// load the prefereces from the config file into the given prefs struct
void load_prefs(prefs * p);

// use this method when reading the "music_dir" field of a prefs struct
// it will make sure it always points to a nice directory
char * prefs_get_music_dir(prefs * p);

int is_valid_port_number(int number);

bool prefs_are_valid(void);

bool string_has_slashes(const char* string);
