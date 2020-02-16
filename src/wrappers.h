#include <sys/types.h>
#include <stdbool.h>

extern pid_t cdparanoia_pid;
extern pid_t lame_pid;
extern pid_t oggenc_pid;
extern pid_t opusenc_pid;
extern pid_t flac_pid;
extern pid_t wavpack_pid;
extern pid_t monkey_pid;
extern pid_t musepack_pid;
extern pid_t fdkaac_pid;

extern int numCdparanoiaFailed;
extern int numLameFailed;
extern int numOggFailed;
extern int numOpusFailed;
extern int numFlacFailed;
extern int numWavpackFailed;
extern int numMonkeyFailed;
extern int numMusepackFailed;
extern int numAacFailed;

extern int numCdparanoiaOk;
extern int numLameOk;
extern int numOggOk;
extern int numOpusOk;
extern int numFlacOk;
extern int numWavpackOk;
extern int numMonkeyOk;
extern int numMusepackOk;
extern int numAacOk;

// signal handler to find out when out child has exited
void sigchld(int signum);

// fork() and exec() the file listed in "args"
//
// args - a valid array for execvp()
// toread - the file descriptor to pipe back to the parent
// p - a place to write the PID of the exec'ed process
// dir - directory to run program in
// 
// returns - a file descriptor that reads whatever the program outputs on "toread"
int exec_with_output(const char * args[], int toread, pid_t * p, const char * dir);

// uses cdparanoia to rip a WAV track from a cdrom
//
// cdrom    - the path to the cdrom device
// tracknum - the track to rip
// filename - the name of the output WAV file
// progress - the percent done
void cdparanoia(const char * cdrom,
                int tracknum,
                const char * filename,
                double * progress);

// uses LAME to encode a WAV file into a MP# and tag it
//
// tracknum - the track number
// artist - the artist's name
// album - the album the song came from
// title - the name of the song
// wavfilename - the path to the WAV file to encode
// mp3filename - the path to the output MP3 file
// vbr - (boolean) wether or not to encode with Variable Bit Rate
// bitrate - the bitrate to encode at (or maximum bitrate if using VBR)
// progress - the percent done
void lame(int tracknum,
          const char * artist,
          const char * album,
          const char * title,
          const char * genre,
          const char * year,
          const char * wavfilename,
          const char * mp3filename,
          int vbr,
          int bitrate,
          double * progress);

// uses oggenc to encode a WAV file into a OGG and tag it
//
// tracknum - the track number
// artist - the artist's name
// album - the album the song came from
// title - the name of the song
// wavfilename - the path to the WAV file to encode
// oggfilename - the path to the output OGG file
// quality_level - how hard to compress the file (0-10)
// progress - the percent done
void oggenc(int tracknum,
            const char * artist,
            const char * album,
            const char * title,
            const char * year,
            const char * genre,
            const char * wavfilename,
            const char * oggfilename,
            int quality_level,
            double * progress);

// uses opusenc to encode a WAV file into a OGG and tag it
//
// tracknum - the track number
// artist - the artist's name
// album - the album the song came from
// title - the name of the song
// wavfilename - the path to the WAV file to encode
// opusfilename - the path to the output OPUS file
// bitrate - the bitrate to encode at
// progress - the percent done
void opusenc(int tracknum,
            const char * artist,
            const char * album,
            const char * title,
            const char * year,
            const char * genre,
            const char * wavfilename,
            const char * opusfilename,
            int bitrate,
            double * progress);

// uses the FLAC reference encoder to encode a WAV file into a FLAC and tag it
//
// tracknum - the track number
// artist - the artist's name
// albumartist - we want to tag this if it is a compilation
// album - the album the song came from
// title - the name of the song
// wavfilename - the path to the WAV file to encode
// flacfilename - the path to the output FLAC file
// compression_level - how hard to compress the file (0-8) see flac man page
// progress - the percent done
void flac(int tracknum,
          const char * artist,
          const char * albumartist, //mw
          gboolean single_artist, //mw
          const char * album,
          const char * title,
          const char * genre,
          const char * year,
          const char * wavfilename,
          const char * flacfilename,
          int compression_level,
          double * progress);

// uses wavpack to encode a WAV file into a .wv/.wvc and tag it
//
// tracknum - the track number
// artist - the artist's name
// album - the album the song came from
// title - the name of the song
// wavfilename - the path to the WAV file to encode
// wavpackfilename - the path to the output .wv/.wvc file
// compression - compression quantity variable
// hybrid - flag to indicate creating the lossless hybrid .wvc file.
// bitrate - the bitrate to encode at
// progress - the percent done
void wavpack(int tracknum,
             const char * artist,
             const char * album,
             const char * title,
             const char * year,
             const char * genre,
             const char * wavfilename,
             const char * wavpackfilename_wv,
             const char * wavpackfilename_wvc,
             int compression,
             bool hybrid,
             int bitrate,
             double * progress);

void mac(const char* wavfilename,
         const char* monkeyfilename,
         int compression,
         double* progress);

// uses mpcenc to encode a WAV file into a .mpc and tag it
//
// tracknum - the track number
// artist - the artist's name
// album - the album the song came from
// title - the name of the song
// wavfilename - the path to the WAV file to encode
// musepackfilename - the path to the output .mpc file
// quality - encode value
// progress - the percent done
void musepack(int tracknum,
              const char * artist,
              const char * album,
              const char * title,
              const char * year,
              const char * genre,
              const char * wavfilename,
              const char * musepackfilename,
              int quality,
              double* progress);

void aac(int tracknum,
         const char * artist,
         const char * album,
         const char * title,
         const char * genre,
         const char * year,
         const char* wavfilename,
         const char* aacfilename,
         int quality,
         double* progress);

void fdkaac(int tracknum,
         const char * artist,
         const char * album,
         const char * title,
         const char * genre,
         const char * year,
         const char* wavfilename,
         const char* aacfilename,
         int bitrate,
         double* progress);
