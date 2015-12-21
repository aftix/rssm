#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#define MAIN_FILE
#include "setting.h"
#include "control.h"
#include "rssmio.h"

#ifndef VERBOSE
#define VERBOSE 0
#endif

int main(int argc, char** argv) {
	//default configuration
	rssm_options opts;
	//default is set at compile time
	opts.verbose = VERBOSE;
	//Get the config path of $HOME/.config/ through all means avaliable
	char* configPath = getConfigPath(opts.verbose);
	//Allocate enough memory for defConf to be the length of the configPath + the length of "rssm.conf"
	char* defConf = malloc(sizeof(char) * (strlen(configPath) + 10));
	strcpy(defConf, configPath);
	strcat(defConf, "rssm.conf");
	opts.list = defConf;
	//Get the $HOME
	char* homePath = getHomePath(opts.verbose);
	//length of homePath + "rss"
	char* defDir   = malloc(sizeof(char) * (strlen(homePath) + 4));
	strcpy(defDir, homePath);
	strcat(defDir, "rss");
	opts.directory = defDir;
	//default log file is $HOME/.rssmlog
	char* defLog = malloc(sizeof(char) * (strlen(homePath) + 9));
	strcpy(defLog, homePath);
	strcat(defLog, ".rssmlog");
	opts.log = defLog;
	
	//free the home path, we don't need it anymore
	free(homePath);
	
	//Parse arguements
	argp_parse(&argp, argc, argv, 0, 0, &opts);
	
	//Verbose messaging
	if (opts.verbose)
		printf("Parsed command-line arguments, going to open the log file...\n");
	
	//Set up the log before we become a daemon so verbose messages can still be sent on the parent
	char* logPath = getLogPath(&opts, opts.verbose);
	//This log file will be used for fprintf the rest of the program - the log char* in rssm_options is no longer needed, which is why we don't set it to the new correct one
	FILE* log     = fopen(logPath, "w");
	
	if (opts.verbose)
		printf("Log file opened at %s, daemonizing now...\n", logPath);
	else
		printf("Log file opened at %s\n", logPath);
	
	//Free logPath if it is different from opt's log - this is done to prevent memory leaks in the case the fallback log path was used, check to avoid double free later
	if (strcmp(logPath, opts.log) != 0) {
		if (opts.verbose)
			printf("Freeing temporary logPath variable...\n");
		free(logPath);
	}
	
	//Daemonize!
	//We don't need the comm pipes - the child will set those up later
	pid_t thisPid = makeChild(NULL, NULL, NULL, opts.verbose);
	//If we are not the child, exit
	if (thisPid != 0) {
		if (opts.verbose)
			printf("Parent process exiting, child to continue in log\n");
		return 0;
	}
	
	//Now we are in daemon mode. 
	//Change to "/", the only directory that a distro WILL have
	if (opts.verbose) {
		printtime(log);
		fprintf(log, "Changing to /\n");
	}
	chdir("/");
	
	//Read the feedlist - the default file was already taken care of. If we can't access what's in opts.list we just log and exit
	//Since an empty feedlist file means rssm will do nothing, no check for writability on the path is needed. If the file isn't there, there is nothing to do so rssm exits, regardless of if the path is writable.
	if (opts.verbose) {
		printtime(log);
		fprintf(log, "Reading in the feedlist...\n");
	}
	if (access(opts.list, R_OK) != 0) {
		printtime(log);
		fprintf(log, "Can not read the feed configuration file of %s. Exiting.", opts.list);
		fclose(log);
		if (strcmp(opts.list, defConf) != 0)
			free(opts.list);
		if (strcmp(opts.directory, defDir) != 0)
			free(opts.directory);
		if (strcmp(opts.log, defLog) !=0)
			free(opts.log);
		free(defConf);
		free(defDir);
		free(defLog);
		return 0;
	}
	if (opts.verbose) {
		printtime(log);
		fprintf(log, "Reading in feedlists...\n");
	}
	//We already made sure we can read the file
	FILE* feedfile = fopen(opts.list, "r");
	//List of feed items, currently NULL
	//this list will always end with a NULL pointer
	rssm_feeditem** feeds = getFeeds(feedfile, opts.verbose, log);
	
	//Free the feeditems
	size_t i = 0;
	while (feeds[i] != NULL) {
		free(feeds[i]->tag);
		free(feeds[i]->url);
		free(feeds[i]);
		i++;
	}
	
	//Close the log file, we're done
	fclose(log);
	
	//If opt's char*'s are different, free them
	if (strcmp(opts.list, defConf) != 0)
		free(opts.list);
	if (strcmp(opts.directory, defDir) != 0)
		free(opts.directory);
	if (strcmp(opts.log, defLog) != 0)
		free(opts.log);
	free(defConf);
	free(defDir);
	free(defLog);
	return 0;
}
