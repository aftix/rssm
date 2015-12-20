#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>
#include <argp.h>
#include <math.h>
#include "setting.h"

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
	ret[0] = '/';
	ret[1] = '\0';
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
	ret[0] = '/';
	ret[1] = '\0';
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
