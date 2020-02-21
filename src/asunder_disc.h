#ifndef __HEADER_ASUNDER_DISC_
#define __HEADER_ASUNDER_DISC_


typedef struct asunder_disc
{
    
    GList *track_list;
    GList *temp_list;

    char *artist;
    char *title;
    char *genre;
    char *release_date;
    
    unsigned int length;
    
    char *disc_id;

} asunder_disc;


typedef struct asunder_track
{
    int number;
    char *artist;
    char *title;
    int length;
    int offset;
    
} asunder_track;


asunder_disc *asunder_disc_new(char *cdrom);
asunder_disc *asunder_disc_clone(asunder_disc *disc);
void asunder_disc_destroy(asunder_disc *disc);

asunder_track *asunder_disc_get_track_first(asunder_disc *disc);
asunder_track *asunder_disc_get_track_next(asunder_disc *disc);

void asunder_disc_add_track(asunder_disc *disc, asunder_track *track);

char *asunder_disc_get_artist(asunder_disc *disc);
char *asunder_disc_get_title(asunder_disc *disc);
char *asunder_disc_get_genre(asunder_disc *disc);
char *asunder_disc_get_release_date(asunder_disc *disc);

void asunder_disc_set_artist(asunder_disc *disc, char *artist);
void asunder_disc_set_title(asunder_disc *disc, char *title);

void asunder_disc_set_length(asunder_disc *disc, unsigned int length);

void asunder_disc_set_release_date(asunder_disc *disc, char *release_date);

char *asunder_disc_get_discid(asunder_disc *disc);


asunder_track *asunder_track_new();

int asunder_track_get_length(asunder_track *track);
char *asunder_track_get_title(asunder_track *track);
char *asunder_track_get_artist(asunder_track *track);
int asunder_track_get_number(asunder_track *track);

void asunder_track_set_title(asunder_track *track, const char *title);
void asunder_track_set_artist(asunder_track *track, const char *artist);
void asunder_track_set_number(asunder_track *track, int number);
void asunder_track_set_length(asunder_track *track, int length);

void asunder_track_set_offset(asunder_track *track, int offset);

#endif /*__HEADER_ASUNDER_DISC_*/
