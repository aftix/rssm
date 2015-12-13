#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define MAIN_FILE
#include "setting.h"

int main(int argc, char** argv) {
	//default configuration
	rssm_options opts;
	opts.verbose = 0;
	//Get the config path of $HOME/.config/ through all means avaliable
	char* configPath = getConfigPath();
	//Allocate enough memory for defConf to be the length of the configPath + the length of "rssm.conf"
	char* defConf = malloc(sizeof(char) * (strlen(configPath) + 10));
	strcpy(defConf, configPath);
	strcat(defConf, "rssm.conf");
	opts.list = defConf;
	//Get the $HOME
	char* homePath = getHomePath();
	//length of homePath + "rss"
	char* defDir   = malloc(sizeof(char) * (strlen(homePath) + 4));
	strcpy(defDir, homePath);
	strcat(defDir, "rss");
	opts.directory = defDir;
	
	
	//Parse arguements
	argp_parse(&argp, argc, argv, 0, 0, &opts);
	
	//Test to see if it worked
	printf("%d %s %s\n", opts.verbose, opts.list, opts.directory);
	
	//If opt's char*'s are different, free them
	if (strcmp(opts.list, defConf) != 0)
		free(opts.list);
	if (strcmp(opts.directory, defDir) != 0)
		free(opts.directory);
	free(defConf);
	free(defDir);
	return 0;
}
