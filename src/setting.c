#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>
#include <argp.h>
#include <signal.h>

#include <iniparser.h>

#include "setting.h"
#include "rssmio.h"

//Parse an argument into a rssm_option struct
error_t parseArg(int key, char* arg, struct argp_state *state) {
	//Get the rssm_option struct
	rssm_options *opts = state->input;
	
	//Decode the key into options
	switch (key) {
		case 'v':
			opts->verbose = 1;
			break;
		case 'f':
			opts->list = malloc(sizeof(char) * (strlen(arg) + 1));
			strcpy(opts->list, arg);
			break;
		case 'd':
			opts->directory = malloc(sizeof(char) * (strlen(arg) + 1));
			strcpy(opts->directory, arg);
			break;
		case 'l':
			opts->log = malloc(sizeof(char) * (strlen(arg) + 1));
			strcpy(opts->log, arg);
			break;
		case 'D':
			opts->daemon = 0;
			break;
		case 'c':
			opts->mins = atoi(arg);
			break;
		case 'F':
			opts->force = 1;
			break;
		case ARGP_KEY_END:
			break;
		default:
			return ARGP_ERR_UNKNOWN;
	}
	
	return 0;
}

//Get the config path of $HOME/.config
char* getConfigPath(int v) {
	if (v)
		printf("Trying to get path from $XDG_CONFIG_HOME...\n");
	
	//Try getting it directly 
	char* ret = getenv("XDG_CONFIG_HOME");
	if (ret != NULL) {
		if (v)
			printf("$XDG_CONFIG_HOME worked, returning that. (%s)\n", ret);
		ret = malloc(sizeof(char) * (strlen(ret) + 2));
		strcpy(ret, getenv("XDG_CONFIG_HOME"));
		strcat(ret, "/");
		return ret;
	}
	
	//If that didn't work try getting it from $HOME and concating .config
	if (v)
		printf("$XDG_CONFIG_HOME failed.\nTrying $HOME...\n");
	
	ret = getenv("HOME");
	if (ret != NULL) {
		if (v)
			printf("$HOME worked, cat'ing /.config/ that. (%s)\n", ret);
		//realloc it to accomadate for the "/.config/"
		ret = malloc(sizeof(char) * (strlen(ret) + 10));
		strcpy(ret, getenv("HOME"));
		strcat(ret, "/.config/");
		return ret;
	}
	
	if (v)
		printf("$HOME failed.\nTrying to use pwd.h ...\n");
	//Time to use pwd because variables didn't work
	struct passwd *pw = getpwuid(getuid());
	if (pw->pw_dir != NULL) {
		if (v)
			printf("pwd.h worked, cat'ing /.config/ to the passwd->pw_dir. (%s)\n", pw->pw_dir);
		ret = malloc(sizeof(char) * (strlen(pw->pw_dir) + 10));
		strcpy(ret, pw->pw_dir);
		strcat(ret, "/.config/");
		free(pw);
		return ret;
	}
	
	//just use /
	if (v)
		printf("pwd.h failed.\nDefaulting to /\n");
	
	free(pw);
	
	ret = malloc(sizeof(char)*2);
	strcpy(ret, "/");
	return ret;
}

//Get the homepath independent of $HOME
char* getHomePath(int v) {
	if (v)
		printf("Trying to get home path from $HOME.\n");
	
	//try getenv
	char* ret = getenv("HOME");
	if (ret != NULL) {
		if (v)
			printf("$HOME worked, returning that. (%s)\n", ret);
		ret = malloc(sizeof(char) * (strlen(ret) + 2));
		strcpy(ret, getenv("HOME"));
		strcat(ret, "/");
		return ret;
	}
	
	if (v)
		printf("$HOME failed.\nTrying to use pwd.h ...\n");
	
	//try a passwd struct
	struct passwd *pw = getpwuid(getuid());
	if (pw->pw_dir != NULL) {
		if (v)
			printf("pwd.h worked, returning that. (%s)", ret);
		ret = malloc(sizeof(char) * (strlen(pw->pw_dir) + 2));
		strcpy(ret, pw->pw_dir);
		strcat(ret, "/");
		free(pw);
		return ret;
	}
	
	if (v)
		printf("pwd.h failed, defaulting to /\n");
	
	//just use /
	free(pw);
	
	ret = malloc(sizeof(char)*2);
	strcpy(ret, "/");
	
	return ret;
}

//Get a path for the log
char* getLogPath(const rssm_options *opts, int v) {
	if (v)
		printf("Checking if the given log file is accessable for writing (%s)\n", opts->log);
	//Try to access the logfile
	if (access(opts->log, W_OK) == 0) {
		if (v)
			printf("The given log file is good, just return that.\n");
		//Ok, the one we have is alright, just return
		return opts->log;
	}
	
	if (v)
		printf("Trying to create file...\n");
	FILE* f;
	if ((f = fopen(opts->log, "w")) != NULL) {
		if (v)
			printf("Log file succesfully created!\n");
		fclose(f);
		return opts->log;
	}
	
	if (v)
		fprintf(stderr, "Can not access %s! Defaulting to $HOME/.rssmlog\n", opts->log);
	else
		fprintf(stderr, "Can not access %s! Using default.\n", opts->log);
	//Default to $(HOME)/.rssmlog
	char* home = getHomePath(v);
	char* ret = malloc(sizeof(char) * (strlen(home) + 9));
	strcpy(ret, home);
	strcat(ret, ".rssmlog");
	free(home);
	
	return ret;
}

rssm_feeditem** getFeeds(const char* list, FILE* log, int v) {
	//ini dictionary from the list file
	if (v) {
		printtime(log);
		fprintf(log, "Loading iniparser...\n");
	}
	dictionary *d = iniparser_load(list);
	
	if (!iniparser_find_entry(d, "rss")) {
		printtime(log);
		fprintf(log, "No Rss section if feedlist file! Exiting...\n");
		raise(SIGKILL);
	}
	
	int tagNum = iniparser_getsecnkeys(d, "rss");
	rssm_feeditem** feeds = malloc(sizeof(rssm_feeditem *) * (tagNum + 1));
	
	if (v) {
		printtime(log);
		fprintf(log, "Parsing feedlist...\n");
	}
	
	const char** name = malloc(sizeof(char *) * tagNum);
	if (iniparser_getseckeys(d, "rss", name) == NULL) {
		printtime(log);
		fprintf(log, "Error reading in section keys!\n");
		raise(SIGKILL);
	}
	
	size_t i = 0;
	for (i=0; i<tagNum; i++) {
		feeds[i] = malloc(sizeof(rssm_feeditem));
		char* tag = malloc(sizeof(char) * (strlen(name[i]) + 1));
		strcpy(tag, name[i] + 4);
		
		const char* val = iniparser_getstring(d, name[i], "");
		char* url = malloc(sizeof(char) * (strlen(val) + 1));
		strcpy(url, val);
		
		feeds[i]->tag  = tag;
		feeds[i]->url  = url;
		feeds[i]->out  = NULL;
		feeds[i]->desc = NULL;
	}
	feeds[i] = NULL;
	
	free(name);
	
	if (v) {
		printtime(log);
		fprintf(log, "Cleaning up iniparser...\n");
	}
	iniparser_freedict(d);
	
	return feeds;
}

//checks lock file
int checkLock(const char* path) {
	//Does it exist?	
	if (access(path, F_OK) == 0 && access(path, R_OK) == 0) {
		//Read the first line out of the file
		FILE* lock = fopen(path, "r");
		char line[256];
		memset(line, 0, 256);
		char* ret = fgets(line, 255, lock);
		fclose(lock);
		
		if (ret == NULL)
			return -1;
		
		return atoi(line);
	}
	
	//Create it and write our pid
	FILE* lock = fopen(path, "w");
	if (lock == NULL)
		return -1;
	fprintf(lock, "%d\n", getpid());
	fclose(lock);
	return 0;
}
