#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>
#include <argp.h>
#include <math.h>
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

rssm_feeditem** getFeeds(FILE* list, FILE* log, int v) {
	//allocate the list of feed item pointers
	rssm_feeditem** items = malloc(sizeof(rssm_feeditem *) * 2);
	//Pre-allocate the first element, set last one to NULL
	items[0] = malloc(sizeof(rssm_feeditem));
	items[1] = NULL;
	
	//Read through the file by character
	int ch; //Character read
	int last = '\0'; //Last character read
	char* curr = malloc(sizeof(char)); //Stores the word currently being read
	curr[0]    = '\0';
	size_t item = 0; //index for storing feed items
	unsigned char inComment = 0; //flag for if we're in a comment
	unsigned char inQuote   = 0; //flag for if we're in a quote
	unsigned char escaped   = 0; //flag for if this character is escaped. Reset after reading in 1 more character
	unsigned char tag       = 1; //flag to tell if we're reading in the tag or the url
	while ((ch = fgetc(list)) != EOF) {
		//If we see a \ then the next character is escaped; set the flag and continue
		if (ch == '\\' && !escaped) {
			escaped = 1;
			continue;
		}
		
		//If we see a " or ' that is not escaped we toggle the state of a quote and continue, the " doesn't actually get recorded.
		if ((ch == '"' || ch == '\'') && !escaped) {
			inQuote = !inQuote;
			escaped = 0;
			continue;
		}
		
		//If we see a # not in quotes or not escaped it dumps the current word into memory and sets inComment to 1
		if (ch == '#' && !escaped && !inQuote) {
			inComment = 1; //Make sure comment flag is one
			
			//If we were reading a tag when the comment started, then drop the tag because it has no url
			if (tag) {
				printtime(log);
				fprintf(log, "Tag %s has no url, ignoring...\n", curr);
			//If we had nothing in the url, i.e. storing into url but no characters had been read, drop the tag by freeing it (since we're on url it'd of been dumped to memory)
			} else if (strlen(curr) == 0) {
				printtime(log);
				fprintf(log, "Tag %s has no url, ignoring...\n", items[item]->tag);
				free(items[item]->tag);
			//Dump the tag and the url into memory if we had both when the comment started
			} else {
				//Allocate space for the url, which we're dumping
				items[item]->url = malloc(sizeof(char) * (strlen(curr) + 1));
				strcpy(items[item]->url, curr);
				
				free(curr);
				curr    = malloc(sizeof(char));
				curr[0] = '\0';
				item++;
				
				//Reallocate items to add one more to the end
				items             = realloc(items, sizeof(rssm_feeditem *) * (item + 2));
				items[item]       = malloc(sizeof(rssm_feeditem));
				items[item]->desc = NULL;
				items[item]->out  = NULL;
				items[item+1]     = NULL;
			}
		}
		
		//On newline we reset comments and dump current word
		if (ch == '\n') {
			inComment = 0; //set comment to false
			inQuote = 0;   //set quote to false
			escaped = 0;   //set escape to false
			
			//If we had a url that was longer than 0
			if (strlen(curr) != 0 && !tag) {
				items[item]->url = malloc(sizeof(char) * strlen(curr)+1);
				strcpy(items[item]->url, curr);
				
				free(curr);
				curr    = malloc(sizeof(char));
				curr[0] = '\0';
				item++;
				
				items             = realloc(items, sizeof(rssm_feeditem *) * (item + 2));
				items[item]       = malloc(sizeof(rssm_feeditem));
				items[item]->desc = NULL;
				items[item]->out  = NULL;
				items[item+1]     = NULL;
			//If we had no url
			} else if (!tag) {
				printtime(log);
				fprintf(log, "Tag %s has no url, ignoring...\n", items[item]->tag);
			}//Other option is that we were on the tag. In that case we have no memory to print an error message
			tag = 1; //the start of the next line will be a tag
		}
		
		//On the first space after a tag, dump data 
		if (tag && last != ' ' && ch == ' ') {
			tag = 0;
			if (strlen(curr) != 0) {
				items[item]->tag = malloc(sizeof(char) * (strlen(curr) + 1));
				strcpy(items[item]->tag, curr);
				
				free(curr);
				curr   = malloc(sizeof(char));
				curr[0] = '\0';
			}
		}
		
		//add everything else to the current word
		if (ch != ' ' && ch != '\n' && !(ch == '#' && !escaped && !inQuote) && !(ch == '"' && !escaped) && !(ch == '\'' && !escaped) && !inComment) {
			char* tmp = curr; //Tmp is what we had before
			curr = malloc(sizeof(char) * (strlen(curr) + 2)); //Make curr big enough to hold 1 more char
			strcpy(curr, tmp); //Copy the old curr into the new memory
			curr[strlen(tmp)]   = (char)ch; //add the new ch and \0
			curr[strlen(tmp)+1] = '\0';
			free(tmp);
			//Didn't use realloc because ch is set with strlen(tmp)
		}
		
		last = ch; //set the last char
		escaped = 0; //reset escaped
	}
	
	printtime(log);
	fprintf(log, "Feedlist loaded!\n");
	
	//free what we malloc'd on the last \n
	free(items[item]);
	items[item] = NULL;
	
	return items;
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
