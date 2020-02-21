
#include <gtk/gtk.h>

#include "musicbrainz5/mb5_c.h"

#include "asunder_disc.h"

#include "musicbrainz.h"


asunder_disc *asunder_disc_i = NULL;

asunder_track *asunder_track_i = NULL;

asunder_disc *asunder_disc_new(char *cdrom)
{
    asunder_disc_i = malloc(sizeof(asunder_disc));
    asunder_disc_i->track_list = NULL;
    asunder_disc_i->temp_list = NULL;
    asunder_disc_i->disc_id = get_discid(cdrom);
    
    printf("DiscID: %s\n", asunder_disc_i->disc_id);
    
    asunder_disc_i->artist = (char*)g_strdup_printf("unknown");
    asunder_disc_i->title = (char*)g_strdup_printf("unknown");
    asunder_disc_i->genre = (char*)g_strdup_printf("unknown");
    
    asunder_disc_i->release_date = (char*)g_strdup_printf("1900");
    
    asunder_disc_i->length = 0;

    return asunder_disc_i;
}

void asunder_disc_destroy(asunder_disc *disc)
{
    if (disc != NULL) {
        
        if (disc->disc_id != NULL) {
            free(disc->disc_id);
            disc->disc_id = NULL;
        }
        
        if (disc->artist != NULL) {
            free(disc->artist);
            disc->artist = NULL;
        }
        if (disc->title != NULL) {
            free(disc->title);
            disc->title = NULL;
        }
        if (disc->genre != NULL) {
            free(disc->genre);
            disc->genre = NULL;
        }
        if (disc->release_date != NULL) {
            free(disc->release_date);
            disc->release_date = NULL;
        }

        free(disc);
        disc = NULL;
    }
}

asunder_track *asunder_disc_get_track_first(asunder_disc *disc)
{
    disc->temp_list = disc->track_list;
    
    if (disc->temp_list) {
        return (asunder_track *)(disc->temp_list->data);
    }

    return NULL;
}

asunder_track *asunder_disc_get_track_next(asunder_disc *disc)
{
    if (disc != NULL) {
        if (disc->temp_list != NULL) {
            disc->temp_list = disc->temp_list->next;
            
            if (disc->temp_list != NULL) 
                return (asunder_track*)(disc->temp_list)->data;
        }
    }

    return NULL;

}

void asunder_disc_add_track(asunder_disc *disc, asunder_track *track)
{
    if (disc != NULL) {
        GList *temp_list = disc->track_list;

        disc->track_list = g_list_append(temp_list, track);
    }
}

asunder_disc *asunder_disc_clone(asunder_disc *in_disc)
{
    asunder_disc *disc = malloc(sizeof(asunder_disc));
    
    //disc->disc_id = in_disc->disc_id;
    
    disc->disc_id = (char*)g_strdup_printf(in_disc->disc_id);
    
    disc->artist = (char*)g_strdup_printf("%s", in_disc->artist);
    
    disc->title = (char*)g_strdup_printf("%s", in_disc->title);
    disc->genre = (char*)g_strdup_printf("%s", in_disc->genre);
    
    disc->track_list = NULL;
    
    disc->release_date = (char*)g_strdup_printf("%s", in_disc->release_date);
    
    return disc;
}

void asunder_disc_set_artist(asunder_disc *disc, char *artist)
{
    if (disc != NULL) {
        if (disc->artist != NULL) {
            g_free(disc->artist);
        }
            
        disc->artist = g_strdup_printf("%s", artist);
            
        printf("ARTIST: %s\n", disc->artist);
        
    }
}

void asunder_disc_set_title(asunder_disc *disc, char *title)
{
    if (disc != NULL) {
        if (disc->artist != NULL) {
            g_free(disc->title);
        }
            
        disc->title = g_strdup_printf("%s", title);
            
        printf("TITLE: %s\n", disc->title);
        
    }
}

char *asunder_disc_get_artist(asunder_disc *disc)
{
    char *result = NULL;
    if (disc != NULL) {
        result = disc->artist;        
    }
    return result;
}

char *asunder_disc_get_title(asunder_disc *disc)
{
    char *result = NULL;
    if (disc != NULL) {
        result = disc->title;        
    }
    return result;
}

char *asunder_disc_get_genre(asunder_disc *disc)
{
    char *result = NULL;
    if (disc != NULL) {
        result = disc->genre;        
    }
    return result;
}

char *asunder_disc_get_release_date(asunder_disc *disc)
{
    char *result = NULL;
    if (disc != NULL) {
        result = disc->release_date;
    }
    return result;
}

void asunder_disc_set_length(asunder_disc *disc, unsigned int length)
{
    if (disc != NULL) {
        disc->length = length;
    }
}

void asunder_disc_set_release_date(asunder_disc *disc, char *release_date)
{
    if (disc != NULL) {
        if (disc->release_date != NULL) {
            free(disc->release_date);
        }
        
        disc->release_date = g_strdup_printf("%s", release_date);
    }
}


char *asunder_disc_get_discid(asunder_disc *disc)
{
    char *result = NULL;

    if (disc != NULL) result = disc->disc_id;

    return result;
}


asunder_track *asunder_track_new()
{
    asunder_track_i = malloc(sizeof(asunder_track));
    asunder_track_i->title = (char*)g_strdup_printf("unknown");
    asunder_track_i->artist = (char*)g_strdup_printf("unknown");
    asunder_track_i->length = 0;
    asunder_track_i->number = 0;

    return asunder_track_i;
}

void asunder_track_set_title(asunder_track *track, const char *in_title)
{
    if (track != NULL) {
        if (track->title != NULL) {
            free(track->title);
        }
        track->title = (char*)g_strdup_printf("%s", in_title);
    }
}

void asunder_track_set_artist(asunder_track *track, const char *in_artist)
{
    if (track != NULL) {
        if (track->artist != NULL ) {
            free (track->artist);
        }
        
        track->artist = (char*)g_strdup_printf("%s", in_artist);
    }
}

void asunder_track_set_number(asunder_track *track, int number)
{
    if (track != NULL) {
        track->number = number;
    }
}

void asunder_track_set_length(asunder_track *track, int length)
{
    if (track != NULL) {
        track->length = length / 1000;
    }
}

int asunder_track_get_length(asunder_track *track)
{
    int result = -1;
    if (track != NULL) result = track->length;

    return result;
}

char *asunder_track_get_artist(asunder_track *track)
{
    char *result = NULL;

    if (track != NULL) result = track->artist;
    return result;
   
}

char *asunder_track_get_title(asunder_track *track)
{
    char *result = NULL;

    if (track != NULL) result = track->title;
    return result;
}

int asunder_track_get_number(asunder_track *track)
{
    return track->number;
}

void asunder_track_set_offset(asunder_track *track, int offset)
{
    if (track != NULL) {
        track->offset = offset;
    }
}
