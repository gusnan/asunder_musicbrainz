#include <stdio.h>
#include <stdbool.h>

#include <glib.h>
#include <glib/gprintf.h>

#include "musicbrainz5/mb5_c.h"

#include "asunder_disc.h"
#include "musicbrainz.h"
#include "discid/discid.h"



gchar *time_string(int value)
{
    int h, m, s;
    gchar *result = NULL;

    h = (value / 3600);
    m = (value / 60) % 60;
    s = value % 60;

    result = g_strdup_printf("%02d:%02d:%02d", h, m, s);

    return result;
}

char *get_discid(char *cdrom)
{
    char *result = malloc(256 * sizeof(char));

    // get the disc id
    DiscId *discid = discid_new();

    if ( discid_read_sparse(discid, cdrom, 0) == 0 ) {
        fprintf(stderr, "Error: %s\n", discid_get_error_msg(discid));

        discid_free(discid);
        free(result);
        return NULL;
    }

    sprintf(result, "%s", discid_get_id(discid));

    discid_free(discid);

    return result;
}

musicbrainz_conn *musicbrainz_connection_new(char *server, int port)
{
    musicbrainz_conn *conn = malloc(sizeof(musicbrainz_conn));

    conn->number_of_matches = 0;
    conn->query_counter = 0;

    conn->mb5_query = mb5_query_new("asunder-2.9.6", server, port);

    // Empty the error message
    g_sprintf(conn->error_message, "%s", "");

    tQueryResult result = mb5_query_get_lastresult(conn->mb5_query);
    if (result != eQuery_Success) {

        mb5_query_get_lasterrormessage(conn->mb5_query, conn->error_message, sizeof(conn->error_message));
    }

    return conn;
}

void musicbrainz_set_proxy(musicbrainz_conn *conn, char *proxy_server, int proxy_port)
{
    if (conn != NULL) {

        mb5_query_set_proxyhost(conn->mb5_query, proxy_server);
        mb5_query_set_proxyport(conn->mb5_query, proxy_port);
    }
}

void musicbrainz_connection_destroy(musicbrainz_conn *conn)
{
    if (conn != NULL) {
        
        mb5_query_delete(conn->mb5_query);

        free(conn);
    }
}

int musicbrainz_get_data(musicbrainz_conn *conn, Mb5Release release, asunder_disc *in_disc)
{
    Mb5Metadata metadata2 = 0;
    Mb5ArtistCredit artist_credit;
    Mb5NameCreditList name_credit_list;
    int required_size = 0;
    char *artist_name = NULL;

    if (release) {
    /* The releases returned from LookupDiscID don't contain full information */

        char **ParamNames;
        char **ParamValues;
        char release_ID[256];

        char release_date[256];

        ParamNames = malloc(sizeof(char *));
        ParamNames[0] = malloc(256);
        ParamValues = malloc(sizeof(char *));
        ParamValues[0] = malloc(256);

        strcpy(ParamNames[0], "inc");
        strcpy(ParamValues[0], "artists labels recordings release-groups url-rels discids artist-credits");

        mb5_release_get_id(release, release_ID, sizeof(release_ID));

        metadata2 = mb5_query_query(conn->mb5_query, "release", release_ID, "", 1, ParamNames, ParamValues);

        if (metadata2) {
            Mb5Release full_release = mb5_metadata_get_release(metadata2);

            mb5_release_get_date(full_release, release_date, sizeof(release_date));

            printf("Relase date: %s\n", release_date);

            asunder_disc_set_release_date(in_disc, release_date);

            // Get the album artist
            artist_credit = mb5_release_get_artistcredit(full_release);
            name_credit_list = mb5_artistcredit_get_namecreditlist(artist_credit);

            for (int i = 0; i < mb5_namecredit_list_size (name_credit_list); i++) {
                Mb5NameCredit  name_credit = mb5_namecredit_list_item (name_credit_list, i);
                Mb5Artist      artist;
                // char          *artist_name = NULL;
                // char          *artist_id = NULL;

                artist = mb5_namecredit_get_artist (name_credit);

                required_size = mb5_artist_get_name (artist, artist_name, 0);
                artist_name = g_new (char, required_size + 1);
                mb5_artist_get_name (artist, artist_name, required_size + 1);
                //debug (DEBUG_INFO, "==> [MB] ARTIST NAME: %s\n", artist_name);
                // printf("Release artist: %s\n", artist_name);

                asunder_disc_set_artist(in_disc, artist_name);

                if (artist_name != NULL) {
                    g_free(artist_name);
                    artist_name = NULL;
                }

            }

            if (full_release) {
                /*
                 * However, these releases will include information for all media in the release
                 * So we need to filter out the only the media we want.
                 */

                Mb5MediumList MediumList = mb5_release_media_matching_discid(full_release, in_disc->disc_id);
                if (MediumList)
                {
                    int number_of_matches2 = mb5_medium_list_size(MediumList);

                    // printf("Number of matches 2: %d\n", number_of_matches2);

                    if (number_of_matches2 != 0)
                    {
                        Mb5ReleaseGroup ReleaseGroup = mb5_release_get_releasegroup(full_release);
                        if (ReleaseGroup)
                        {
                            /* One way of getting a string, just use a buffer that
                             * you're pretty sure will accomodate the whole string
                             */

                            char title[256];

                            mb5_releasegroup_get_title(ReleaseGroup, title, sizeof(title));

                            asunder_disc_set_title(in_disc, title);
                        }
                        else
                            printf("No release group for this release\n");

                        for (int current_medium = 0; current_medium < mb5_medium_list_size(MediumList); current_medium++)
                        {
                            Mb5Medium Medium = mb5_medium_list_item(MediumList, current_medium);
                            if (Medium)
                            {
                                char medium_title[256];

                                Mb5TrackList track_list = mb5_medium_get_tracklist(Medium);

                                mb5_medium_get_title(Medium, medium_title, sizeof(medium_title));

                                printf("Found media: '%s', position %d\n", medium_title, mb5_medium_get_position(Medium));

                                GSList *artist_list = NULL;
                                GSList *temp_list = NULL;

                                gchar *temp_string = NULL;

                                gboolean compilation = FALSE;

                                if (track_list)
                                {
                                    int current_track=0;
                                    temp_list = artist_list;

                                    for (current_track = 0; current_track < mb5_track_list_size(track_list); current_track++)
                                    {
                                        Mb5Track track = mb5_track_list_item(track_list, current_track);
                                        Mb5Recording recording = mb5_track_get_recording(track);

                                        // Get the artist from track
                                        if (recording)
                                        {
                                            Mb5ArtistCredit track_artist_credit = mb5_artistcredit_clone(mb5_recording_get_artistcredit(recording));
                                            Mb5NameCreditList temp_name_credit_list = mb5_artistcredit_get_namecreditlist(track_artist_credit);

                                            Mb5NameCreditList new_name_credit_list = mb5_namecredit_list_clone(temp_name_credit_list);

                                            // Get per track artist
                                            int size = mb5_namecredit_list_size(new_name_credit_list);

                                            for (int i = 0; i < size; i++) {
                                                Mb5NameCredit new_name_credit = mb5_namecredit_list_item (new_name_credit_list, i);
                                                Mb5Artist artist;
                                                
                                                artist = mb5_namecredit_get_artist (new_name_credit);

                                                required_size = mb5_artist_get_name (artist, artist_name, 0);
                                                artist_name = g_new (char, required_size + 1);
                                                mb5_artist_get_name (artist, artist_name, required_size + 1);

                                                gchar *temp_artist = g_strdup(artist_name);

                                                g_free(artist_name);

                                                temp_list = g_slist_append(temp_list, temp_artist);
                                            }
                                            
                                            artist_list = temp_list;
                                            
                                            mb5_namecredit_list_delete(new_name_credit_list);
                                        }
                                    }

                                    // Check if it is a Compilation CD (This might be doable using the MusicBrainz API,
                                    // I haven't checked carefully. I assume it is, if there's several different artists, and
                                    // not one and the same for all the tracks.
                                    //
                                    if (artist_list != NULL) {

                                        for (temp_list = artist_list; temp_list != NULL; temp_list = temp_list -> next) {

                                            if (temp_string == NULL) {
                                                temp_string = g_strdup(temp_list->data);
                                            } else {

                                                if (g_strcmp0(temp_list->data, temp_string) != 0) {
                                                    compilation = TRUE;
                                                }
                                            }
                                        }
                                        if (temp_string != NULL) {
                                            g_free(temp_string);
                                        }
                                    }
                                }

                                if (compilation) asunder_disc_set_compilation(in_disc, TRUE);

                                if (track_list)
                                {
                                    int current_track = 0;
                                    char *track_title = NULL;
                                    GSList *list = artist_list;

                                    int tracklist_offset = mb5_track_list_get_offset(track_list);

                                    printf("Tracklist offset: %d\n", tracklist_offset);

                                    for (current_track = 0; current_track < mb5_track_list_size(track_list); current_track++)
                                    {
                                        int required_length = 0;

                                        asunder_track *new_track = asunder_track_new();

                                        Mb5Track track = mb5_track_list_item(track_list, current_track);
                                        Mb5Recording recording = mb5_track_get_recording(track);

                                        /* Yet another way of getting string. Call it once to
                                         * find out how long the buffer needs to be, allocate
                                         * enough space and then call again.
                                         */

                                        if (recording)
                                        {
                                            required_length = mb5_recording_get_title(recording, track_title, 0);
                                            track_title = malloc(required_length + 1);
                                            mb5_recording_get_title(recording, track_title, required_length + 1);
                                        }
                                        else
                                        {
                                            required_length = mb5_track_get_title(track, track_title, 0);
                                            track_title = malloc(required_length + 1);
                                            mb5_track_get_title(track, track_title, required_length + 1);
                                        }

                                        int track_length = mb5_track_get_length(track);

                                        gchar *temp_time_string = time_string(track_length / 1000);

                                        // If a compilation, print artist for each track.
                                        if (compilation) {
                                            asunder_track_set_title(new_track, track_title);
                                            asunder_track_set_number(new_track, mb5_track_get_position(track));
                                            asunder_track_set_length(new_track, track_length);
                                            asunder_track_set_artist(new_track, (gchar*)(list->data));

                                        } else {
                                            asunder_track_set_title(new_track, track_title);
                                            asunder_track_set_number(new_track, mb5_track_get_position(track));
                                            asunder_track_set_length(new_track, track_length);
                                            asunder_track_set_artist(new_track, artist_name);
                                        }

                                        asunder_disc_add_track(in_disc, new_track);

                                        g_free(temp_time_string);

                                        list = list -> next;

                                        if (track_title != NULL) {
                                            g_free(track_title);
                                            track_title = NULL;
                                        }
                                    }
                                }

                                g_slist_free_full(artist_list, g_free);
                            }
                        }
                    }

                    /* We must delete the result of 'media_matching_discid' */
                    mb5_medium_list_delete(MediumList);
                }
            }
            /* We must delete anything returned from the query methods */
            mb5_metadata_delete(metadata2);
        }

        free(ParamValues[0]);
        free(ParamValues);
        free(ParamNames[0]);
        free(ParamNames);

    }
    return 0;
}


int musicbrainz_query(musicbrainz_conn *conn, asunder_disc *in_disc)
{
    conn->query_counter = 0;
    int query_result = 0;

    Mb5Metadata metadata1 = mb5_query_query(conn->mb5_query, "discid", in_disc->disc_id, "", 0, NULL, NULL);
    char errormessage[256];

    tQueryResult result = mb5_query_get_lastresult(conn->mb5_query);
    int httpcode = mb5_query_get_lasthttpcode(conn->mb5_query);

    mb5_query_get_lasterrormessage(conn->mb5_query, errormessage, sizeof(errormessage));
    printf("Result: %d\nHTTPCode: %d\nErrorMessage: '%s'\n", result, httpcode, errormessage);

    if (metadata1) {
        Mb5Disc disc = mb5_metadata_get_disc(metadata1);
        if (disc) {
            Mb5ReleaseList release_list = mb5_disc_get_releaselist(disc);
            if (release_list) {
                conn->number_of_matches = mb5_release_list_size(release_list);
                conn->release_list = release_list;

                query_result = conn->number_of_matches;

                conn->current_release = 0;
            }
        }
    }

    return query_result;
}

int musicbrainz_query_next(musicbrainz_conn *conn, asunder_disc *disc)
{
    if (conn->current_release < conn->number_of_matches) {
        conn->current_release++;
        return 1;
    }
    return 0;
}

char *musicbrainz_get_error_message(musicbrainz_conn *conn)
{
    return conn->error_message;
}

int musicbrainz_read(musicbrainz_conn *conn, asunder_disc *possible_match)
{
    Mb5Release release = mb5_release_list_item(conn->release_list, conn->current_release);

    if (release) {
        musicbrainz_get_data(conn, release, possible_match);
    }

    return TRUE;
}
