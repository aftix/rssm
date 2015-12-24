#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>

#include <curl/curl.h>

#define MAIN_FILE
#include "setting.h"
#include "control.h"
#include "rssmio.h"

#ifndef VERBOSE
#define VERBOSE 0
#endif

int loop = 1;

//Free up the memory and close the log
static void freeMem(rssm_options *opts, rssm_feeditem** feeds, FILE* log) {
	if (opts->directory != NULL)
		free(opts->directory);
	if (opts->list != NULL)
		free(opts->list);
	if (opts->log != NULL)
		free(opts->log);
	
	if (feeds != NULL) {
		size_t i = 0;
		while (feeds[i] != NULL) {
			if (feeds[i]->tag != NULL)
				free(feeds[i]->tag);
			if (feeds[i]->url != NULL)
				free(feeds[i]->url);
			if (feeds[i]->desc != NULL)
				fclose(feeds[i]->desc);
			if (feeds[i]->out != NULL)
				fclose(feeds[i]->out);
			free(feeds[i]);
			i++;
		}
		free(feeds);
	}
	
	if (log != NULL)
		fclose(log);
	
	curl_global_cleanup();
}

static void smartSleep(int secs) {
	if (secs == 0)
		secs = 300;
	;
	size_t i = 0;
	while (loop && i < secs) {
		sleep(1);
		i++;
	}
}

void handleTerm(int signo, siginfo_t *sinfo, void *context);

int main(int argc, char** argv) {
	curl_global_init(CURL_GLOBAL_DEFAULT);
	
	//default configuration
	rssm_options opts;
	//default is set at compile time
	opts.verbose = VERBOSE;
	opts.daemon  = 1;
	opts.mins    = 5;
	opts.force   = 0;
	
	//Get the config path of $HOME/.config/ through all means avaliable
	char* configPath = getConfigPath(opts.verbose);
	
	//Allocate enough memory for defConf to be the length of the configPath + the length of "rssm.conf"
	opts.list = malloc(sizeof(char) * (strlen(configPath) + 10));
	strcpy(opts.list, configPath);
	strcat(opts.list, "rssm.conf");
	
	//Get the $HOME
	char* homePath = getHomePath(opts.verbose);
	//length of homePath + "rss"
	opts.directory = malloc(sizeof(char) * (strlen(homePath) + 4));
	strcpy(opts.directory, homePath);
	strcat(opts.directory, "rss");
	
	//default log file is $HOME/.rssmlog
	opts.log = malloc(sizeof(char) * (strlen(homePath) + 9));
	strcpy(opts.log, homePath);
	strcat(opts.log, ".rssmlog");
	
	//free the home path, we don't need it anymore
	free(homePath);
	free(configPath);
	
	//Parse arguements
	argp_parse(&argp, argc, argv, 0, 0, &opts);
	
	//Check the lockfile
	int pid = checkLock("/tmp/rssm.lock");
	if (pid > 0 && !opts.force) {
		printf("Error! There is another instance running with pid %d . Only one rssm can be run at a time.\n", pid);
		return -1;
	} else if (opts.force && pid > 0) {
		if (opts.verbose)
			printf("Killing current daemon...\n");
		kill(pid, SIGTERM);
		remove("/tmp/rssm.lock");
		checkLock("/tmp/rssm.lock");
	} else if (pid == -1) {
		printf("Error! Lock file can not be created. Exiting.\n");
		return -1;
	}
	
	//setup the sigterm handler
	struct sigaction act;
	memset(&act, 0, sizeof(struct sigaction));
	act.sa_sigaction = handleTerm;
	act.sa_flags = SA_SIGINFO;
	
	if (sigaction(SIGTERM, &act, NULL) == -1) {
		printf("Error on sigaction!\n");
		return -1;
	}
	
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
	
	if (opts.daemon) {
		//Daemonize!
		//We don't need the comm pipes - the child will set those up later
		pid_t thisPid = makeChild(NULL, NULL, NULL, opts.verbose);
		//If we are not the child, exit
		if (thisPid != 0) {
			if (opts.verbose)
				printf("Parent process exiting, child to continue in log\n");
			return 0;
		}
	} else {
		fclose(log);
		log = stdout;
	}
	
	//Now we are in daemon mode. 
	//Change to "/", the only directory that a distro WILL have
	if (opts.verbose) {
		printtime(log);
		fprintf(log, "Changing to /\n");
	}
	if (chdir("/") < 0) {
		printtime(log);
		fprintf(log, "Error changing directory to / . This should never happen.\n");
		
		freeMem(&opts, NULL, log);
		return 0;
	}

	
	//Read the feedlist - the default file was already taken care of. If we can't access what's in opts.list we just log and exit
	//Since an empty feedlist file means rssm will do nothing, no check for writability on the path is needed. If the file isn't there, there is nothing to do so rssm exits, regardless of if the path is writable.
	if (opts.verbose) {
		printtime(log);
		fprintf(log, "Reading in the feedlist...\n");
	}
	if (access(opts.list, R_OK) != 0) {
		printtime(log);
		fprintf(log, "Can not read the feed configuration file of %s. Exiting.", opts.list);
		
		freeMem(&opts, NULL, log);
		return 0;
	}
	if (opts.verbose) {
		printtime(log);
		fprintf(log, "Reading in feedlists...\n");
	}
	
	//List of feed items, currently NULL
	//this list will always end with a NULL pointer
	rssm_feeditem** feeds = getFeeds(opts.list, log, opts.verbose);
	
	if (opts.verbose) {
		printtime(log);
		fprintf(log, "Feedlists read in, setting up directory tree...\n");
	}
	//Now we need to setup the directory structure for each tag
	int stat = makeDir(opts.directory, log, opts.verbose);
	if (stat < 0) {
		printtime(log);
		fprintf(log, "Error creating directory %s. Exiting.\n", opts.directory);
		
		freeMem(&opts, feeds, log);
		return 0;
	}
	
	if (opts.verbose) {
		printtime(log);
		fprintf(log, "Directory %s is now useable for us!\n", opts.directory);
	}
	
	if (opts.verbose) {
		printtime(log);
		fprintf(log, "Making the tag fifo's...\n");
	}
	
	//Now we make a fifo for each tag we have
	size_t i = 0;
	while (feeds[i] != NULL) {
		char* tagPath = malloc(sizeof(char) * (strlen(opts.directory) + strlen(feeds[i]->tag) + 2));
		strcpy(tagPath, opts.directory);
		strcat(tagPath, "/");
		strcat(tagPath, feeds[i]->tag);
		if (opts.verbose) {
			printtime(log);
			fprintf(log, "Making %s file\n", tagPath);
		}
		stat = makeFile(tagPath, log, opts.verbose);
		if (stat < 0) {
			free(tagPath);
			
			freeMem(&opts, feeds, log);
			return 0;
		}
		
		feeds[i]->out = fopen(tagPath, "a+");
		if (feeds[i]->out == NULL) {
			printtime(log);
			fprintf(log, "Error opening fifod for %s . Exiting.\n", feeds[i]->tag);
			free(tagPath);
			
			freeMem(&opts, feeds, log);
			return 0;
		}
		
		char* descPath  = malloc(sizeof(char) * (strlen(tagPath) + 6));
		strcpy(descPath, tagPath);
		strcat(descPath, " desc");
		
		stat = makeFile(descPath, log, opts.verbose);
		if (stat < 0) {
			free(tagPath);
			free(descPath);
			
			freeMem(&opts, feeds, log);
			return 0;
		}
		
		feeds[i]->desc = fopen(descPath, "a+");
		if (feeds[i]->desc == NULL) {
			printtime(log);
			fprintf(log, "Error opening fifo for tag desc %s . Exiting.\n", feeds[i]->tag);
			free(tagPath);
			free(descPath);
			
			freeMem(&opts, feeds, log);
			return 0;
		}
		
		free(descPath);
		free(tagPath);
		i++;
	}
	
	//Loop for continously checking the rss feeds
	while (loop) {
		//getNewRss each feed
		size_t i = 0;
		while (feeds[i] != NULL) {
			if (opts.verbose) {
				printtime(log);
				fprintf(log, "Checking rss feed %s for new items...\n", feeds[i]->tag);
			}
			getNewRss(feeds[i], log, opts.verbose);
			i++;
		}
		
		if (opts.verbose) {
			printtime(log);
			fprintf(log, "Going to sleep for %d mins...\n", opts.mins);
		}
		fflush(log);
		//A smart sleep function that will exit within 1 sec of loop being set to 0
		smartSleep(opts.mins * 60);
	}
	
	//Clean up
	printtime(log);
	fprintf(log, "Cleaning up everything to close...\n");
	
	freeMem(&opts, feeds, log);
	//remove lock file
	remove("/tmp/rssm.lock");
	return 0;
}

//Handle a sigterm
void handleTerm(int signo, siginfo_t *sinfo, void *context) {
	loop = 0;
}
