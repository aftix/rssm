#ifndef _SETTING_H_
#define _SETTING_H_

#include <stdlib.h>
#include <argp.h>

//This prevents linker error, only define this in main.c
#ifdef MAIN_FILE
//program-wide definitions
const char *argp_program_version = "rssm v0.1";
const char *argp_bug_address     = "";
char doc[] = "rssm - a rss manager for pulling rss feeds into a directory structure, similar to ii.";
//Options rssm accepts
struct argp_option options[] = {
	{"verbose",   'v', 0,      0, "Produce verbose output"},
	{"feedlist",  'f', "FILE", 0, "Specify feedlist file to read from"},
	{"directory", 'd', "DIR",  0, "Specify directory to place rss data into"},
	{"logfile",   'l', "FILE", 0, "Specify path for rssm to log"},
	{"nodaemon",  'D', 0,      0, "Don't run as a daemon (logs to stdout)"},
	{"checks",    'c', "MINS", 0, "Set the number of minutes between rss feed checks (default is 5)"},
	{ 0 }
};
#endif //MAIN_FILE

//Contain all the options of rssm
struct __options {
	int verbose, daemon, mins;
	char* list;
	char* directory;
	char* log;
};
typedef struct __options rssm_options;

//A tag and url for an rss feed
struct __feed {
	char* url;
	char* tag;
	FILE *desc, *out;
};
typedef struct __feed rssm_feeditem;

//Parse an arguement
//Return is handled by argp
error_t parseArg(int key, char* arg, struct argp_state *state);

//For finding $HOME independent of getenv()
char* getConfigPath(int v);
char* getHomePath(int v);

//Get a path the log file can be written to in
//generally /var/log/rssm.$PID.log
char* getLogPath(const rssm_options *opts, int v);

//Read in feedlists from a file
rssm_feeditem** getFeeds(FILE* list, FILE* log, int v);

//Check lock file
//returns pid if it exists, 0 if it doesn't, and -1 if it can't create a new one
int checkLock(const char* path);

#ifdef MAIN_FILE
//Tells argp what is what
struct argp argp = {options, parseArg, "", doc};
#endif //MAIN_FILE

#endif //_SETTING_H_
