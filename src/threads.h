#include <stdbool.h>

extern bool working;
extern bool aborted;

// aborts ripping- stops all the threads and return to normal execution
void abort_threads();

// spawn needed threads and begin ripping
void dorip();

// the thread that handles ripping tracks to WAV files
gpointer rip(gpointer data);

// the thread that handles encoding WAV files to all other formats
gpointer encode(gpointer data);

// the thread that calculates the progress of the other threads and updates the progress bars
gpointer track(gpointer data);
