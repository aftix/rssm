#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>
#include <argp.h>
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
		case ARGP_KEY_END:
			break;
		default:
			return ARGP_ERR_UNKNOWN;
	}
	
	return 0;
}

//Get the config path of $HOME/.config
char* getConfigPath() {
	//Try getting it directly 
	char* ret = getenv("XDG_CONFIG_HOME");
	if (ret != NULL) {
		ret = malloc(sizeof(char) * (strlen(ret) + 2));
		strcpy(ret, getenv("XDG_CONFIG_HOME"));
		strcat(ret, "/");
		return ret;
	}
	
	//If that didn't work try getting it from $HOME and concating .config
	ret = getenv("HOME");
	if (ret != NULL) {
		//realloc it to accomadate for the "/.config/"
		ret = malloc(sizeof(char) * (strlen(ret) + 10));
		strcpy(ret, getenv("HOME"));
		strcat(ret, "/.config/");
		return ret;
	}
	
	//Time to use pwd because variables didn't work
	struct passwd *pw = getpwuid(getuid());
	if (pw->pw_dir != NULL) {
		ret = malloc(sizeof(char) * (strlen(pw->pw_dir) + 10));
		strcpy(ret, pw->pw_dir);
		strcat(ret, "/.config/");
		free(pw);
		return ret;
	}
	
	//just use /
	free(pw);
	ret = malloc(sizeof(char)*2);
	ret[0] = '/';
	ret[1] = '\0';
	return ret;
}

//Get the homepath independent of $HOME
char* getHomePath() {
	//try getenv
	char* ret = getenv("HOME");
	if (ret != NULL) {
		ret = malloc(sizeof(char) * (strlen(ret) + 2));
		strcpy(ret, getenv("HOME"));
		strcat(ret, "/");
		return ret;
	}
	
	//try a passwd struct
	struct passwd *pw = getpwuid(getuid());
	if (pw->pw_dir != NULL) {
		ret = malloc(sizeof(char) * (strlen(pw->pw_dir) + 2));
		strcpy(ret, pw->pw_dir);
		strcat(ret, "/");
		free(pw);
		return ret;
	}
	
	//just use /
	free(pw);
	ret = malloc(sizeof(char)*2);
	ret[0] = '/';
	ret[1] = '\0';
	return ret;
}
