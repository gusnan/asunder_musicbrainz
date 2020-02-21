#ifndef __MUSICBRAINZ_HEADER_
#define __MUSICBRAINZ_HEADER_


// SECONDS_TO_FRAMES and FRAMES_PER_SECOND taken from cddb
// (LGPL-licensed)
//
// http://libcddb.sourceforge.net/API/cddb__disc_8h.html
//
#define FRAMES_PER_SECOND 75
#define SECONDS_TO_FRAMES(s) ((s) * FRAMES_PER_SECOND)


typedef struct musicbrainz_conn
{
    int number_of_matches;
    
    char error_message[256];
    
    int query_counter;
    
    Mb5Query mb5_query;
    
    Mb5ReleaseList release_list;
    
    int current_release;

} musicbrainz_conn;


musicbrainz_conn *musicbrainz_connection_new(char *server, int port);

void musicbrainz_set_proxy(musicbrainz_conn *conn, char *proxy_server, int proxy_port);

void musicbrainz_connection_destroy(musicbrainz_conn *conn);

int musicbrainz_query(musicbrainz_conn *conn, asunder_disc *disc);

int musicbrainz_query_next(musicbrainz_conn *conn, asunder_disc *disc);

char *musicbrainz_get_error_message(musicbrainz_conn *conn);

int musicbrainz_read(musicbrainz_conn *conn, asunder_disc *possible_match);

char *get_discid(char *cdrom);



#endif /*__MUSICBRAINZ_HEADER_*/
