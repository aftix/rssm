#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>

#include <curl/curl.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

#include "rssmio.h"

//Prints the current time to p, no newline
int printtime(FILE* p) {
	//get rawtime
	time_t rawtime;
	time(&rawtime);
	
	//Get string from time
	char* time = asctime(localtime(&rawtime));
	
	//Now we need to remove all \n's from the time
	int i, j=strlen(time);
	//Because strlen ignores \0, we can be sure time[i] won't reach a \0
	for (i=0; i<j; i++)
		if (time[i] == '\n')
			time[i] = ' ';
	
	return fprintf(p, "[%s] ", time);
}

//Ensures a directory exists
int makeDir(const char* path, FILE* log, int v) {
	if (v) {
		printtime(log);
		fprintf(log, "Checking if directory exists...\n");
	}
	
	struct stat st = {0};
	
	//If stat returns correctly then the directory is there
	if (stat(path, &st) == 0) {
		if (v) {
			printtime(log);
			fprintf(log, "Directory %s exists!\n", path);
		}
		
		return 0;
	}
	
	if (v) {
		printtime(log);
		fprintf(log, "Directory %s doesn't exist, creating!\n", path);
	}
	
	return mkdir(path, S_IRWXU);
}

//Ensures a file exists and is not a fifo
int makeFile(const char* path, FILE* log, int v) {
	if (v) {
		printtime(log);
		fprintf(log, "Checking that file %s exists...\n", path);
	}
	
	struct stat st = {0};
	
	//Get stat for the path
	//If file doesn't exist, make a file there (directory is ensured by prior call to makeDir)
	if (stat(path, &st) != 0) {
		if (v) {
			printtime(log);
			fprintf(log, "%s does not exist, creating!\n", path);
		}
		
		FILE* make = fopen(path, "w");
		if (make == NULL) {
			printtime(log);
			fprintf(log, "Error creating file %s . Exiting.\n", path);
			return -1;
		}
		fclose(make);
		
		return 0;
	}
	
	//Check if the file is not a fifo...
	if (v) {
		printtime(log);
		fprintf(log, "%s exists! Checking if it is a fifo...\n", path);
	}
	
	//We don't want it to be a fifo
	if (S_ISFIFO(st.st_mode) ) {
		if (v) {
			printtime(log);
			fprintf(log, "%s is a fifo! Deleting...\n", path);
		}
		
		if (remove(path) < 0) {
			printtime(log);
			fprintf(log, "Error removing fifo file %s . Exiting. \n", path);
			return -1;
		}
		
		FILE* make = fopen(path, "w");
		if (make == NULL) {
			printtime(log);
			fprintf(log, "Error making file %s . Exiting\n", path);
			return -1;
		}
		fclose(make);
		
		return 0;
	}
	
	if (v) {
		printtime(log);
		fprintf(log, "%s is not a fifo! Returning...\n", path);
	}
	
	return 0;
}

//check if fifo on fd contains the search given
static int contains(FILE *f, const char* search) {
	//Make sure we start reading from the start of the file every time
	fseek(f, SEEK_SET, 0);
	
	char line[256];
	//read through fifo line by line looking for search
	while (fgets(line, 255, f) != NULL) {
		size_t i = 0;
		//Since search can start anywhere, we need to check every char
		while (line[i] != '\0') {
			//If the char at i is the same as the first char of search, then we strncmp search with line starting at i
			//The right side only evaluates when the left side is true
			if (line[i] == search[0] && strncmp(line+i, search, sizeof(char) * strlen(search)) == 0)
				return 1;
			i++;
		}
	}
	
	return 0;
}

//Local struct variable for curlWrite
struct __curlResp {
	char* mem;
	size_t size;
};

//Writes data from curl into a string
size_t curlWrite(void* ptr, size_t size, size_t nmemb, void* userdata) {
	size_t nbytes = size * nmemb;
	struct __curlResp *memr = (struct __curlResp *)userdata;
	
	memr->mem = realloc(memr->mem, sizeof(char) * memr->size + nbytes + 1);
	if (memr->mem == NULL) {
		raise(SIGTERM);
	}
	
	memcpy(&(memr->mem[memr->size]), ptr, nbytes);
	memr->size += nbytes;
	memr->mem[memr->size] = '\0';
	
	return nbytes;
}

//Uses curl to get xml from the url
static char* getXmlFromCurl(const char* url, FILE* log, int v) {
	if (v) {
		printtime(log);
		fprintf(log, "Starting to get xml from %s with curl...\n", url);
	}
	
	CURL *curl;
	CURLcode res;
	
	//Initialize curl
	
	struct __curlResp resp = {malloc(sizeof(char)), 0};
	curl = curl_easy_init();
	if (curl) {
		//set options
		curl_easy_setopt(curl, CURLOPT_URL, url);
		curl_easy_setopt(curl, CURLOPT_BUFFERSIZE, 4096*2);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWrite);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&resp);
		
		//get the data
		res = curl_easy_perform(curl);
		if (res != CURLE_OK) {
			printtime(log);
			fprintf(log, "Curl error on url %s : %s\n", url, curl_easy_strerror(res));
			curl_easy_cleanup(curl);
			return NULL;
		}
	} else {
		printtime(log);
		fprintf(log, "Error initializing curl for url %s !\n", url);
		return NULL;
	}
	curl_easy_cleanup(curl);
	
	return resp.mem;
}

//helper functions to get atom or rss
static int getAtom(const xmlNode *xmlRoot, const rssm_feeditem* feed, FILE* log, int v);
static int getRss(const xmlNode *xmlRoot, const rssm_feeditem* feed, FILE* log, int v);

//This does the work of getting all the new rss stuff
void  getNewRss(const rssm_feeditem* feed, FILE* log, int v) {
	//Use libcurl to get the string
	char* xmlStr = getXmlFromCurl(feed->url, log, v);
	if (xmlStr == NULL) {
		return;
	}
	
	//It's time to (finally) parse the xml!
	xmlDoc *xmlDoc   = NULL;
	xmlNode *xmlRoot = NULL;
	LIBXML_TEST_VERSION
	
	if ((xmlDoc = xmlReadMemory(xmlStr, sizeof(char) * (strlen(xmlStr) + 1), NULL, "utf-8", 0)) == NULL) {
		printtime(log);
		fprintf(log, "Error parsing xml recieved from %s .\n", feed->url);
		free(xmlStr);
		return;
	}
	
	xmlRoot = xmlDocGetRootElement(xmlDoc);
	
	if (strcmp((char *)xmlRoot->name, "rss") != 0 && strcmp((char *)xmlRoot->name, "feed") != 0) {
		printtime(log);
		fprintf(log, "No rss found at %s .\n", feed->url);
		free(xmlStr);
		xmlFreeDoc(xmlDoc);
		return;
	}
	
	if (strcmp((char *)xmlRoot->name, "rss") == 0) {
		getRss(xmlRoot, feed, log, v);
	} else if (strcmp((char *)xmlRoot->name, "feed") == 0) {
		getAtom(xmlRoot, feed, log, v);
	}
	
	
	fflush(log);
	
	free(xmlStr);
	xmlFreeDoc(xmlDoc);
}

static void printChildren(const xmlNode* root, FILE* f) {
	xmlNode* n = root->last;
	
	while (n != NULL) {
		switch(n->type) {
			case XML_TEXT_NODE:
				if (strcmp((char *)n->content, "") != 0 && strncmp((char *)n->content, "\n", 1) != 0 && !contains(f, (char *)n->content))
					fprintf(f, "%s\n", (char *)n->content);
				fflush(f);
				break;
			default:
				if (n->children != NULL && n->children->type == XML_TEXT_NODE && 
				   strcmp((char *)n->children->content, "") != 0 && strncmp((char *)n->children->content, "\n", 1) != 0) {
					
					if (strcmp((char *)n->name, "link") == 0 || strcmp((char *)n->name, "guid") == 0 || strcmp((char *)n->name, "category") == 0)
						fprintf(f, "%s: %s\n", (char *)n->name, (char *)n->children->content);
					else
						fprintf(f, "%s: ", (char *)n->name);
				}
				
				if (n != NULL)
					printChildren(n, f);
				
				fflush(f);
				break;
		}
		n = n->prev;
	}
}

static int getAtom(const xmlNode* xmlRoot, const rssm_feeditem* feed, FILE* log, int v) {
	return 0;
}

static int getRss(const xmlNode* xmlRoot, const rssm_feeditem* feed, FILE* log, int v) {
	if (v) {
		printtime(log);
		fprintf(log, "rss found in xml on %s !\n", feed->url);
	}
	
	if (v) {
		printtime(log);
		fprintf(log, "Getting description data for %s ...\n", feed->tag);
	}
	
	//Write the description of the rss channel to the desc fifo
	xmlNode *channel     = xmlRoot->children;
	xmlNode *channelElem = NULL;
	while (channel != NULL && strcmp((char *)channel->name, "text") == 0) {
		channel = channel->next;
	}
	
	
	if (channel == NULL || strcmp((char *)channel->name, "channel") != 0) {
		if (v) {
			printtime(log);
			fprintf(log, "No rss channel was found at url %s .\n", feed->url);
		}
		
		fprintf(feed->desc, "No data found about rss channel.\n");
		fflush(feed->desc);
		
		return -1;
	}
	
	if (v) {
		printtime(log);
		fprintf(log, "Description data found!\n");
	}
	channelElem = channel->children;
	
	if (channelElem == NULL) {
		printtime(log);
		fprintf(log, "Error going through rss channel %s .\n", feed->tag);
		return -1;
	}
	
	while (channelElem != NULL && channelElem->type == XML_TEXT_NODE) {
		channelElem = channelElem->next;
	}
	
	if (channelElem == NULL) {
		printtime(log);
		fprintf(log, "Nothing found on rss channel %s .\n", feed->tag);
	}
	
	while (channelElem != NULL && strcmp((char *)channelElem->name, "item") != 0) {
		if (channelElem->type == XML_TEXT_NODE) {
			channelElem = channelElem->next;
			continue;
		}
		
		xmlNode* tmp = channelElem->children;
		while (tmp != NULL) {
			if (tmp->type != XML_TEXT_NODE) {
				printChildren(tmp->parent, feed->desc);
				fflush(feed->desc);
				break;
			}
			tmp = tmp->next;
		}
		
		if (tmp == NULL && channelElem->children != NULL && strcmp((char *)channelElem->children->content, "") != 0 && 
		   strncmp((char *)channelElem->children->content, "\n", 1) != 0) {
			char toWrite[strlen((char *)channelElem->name) + strlen((char *)channelElem->children->content) + 4];
			sprintf(toWrite, "%s: %s\n", (char *)channelElem->name, (char *)channelElem->children->content);
			if (!contains(feed->desc, toWrite))
				fprintf(feed->desc, toWrite);
			fflush(feed->desc);
		}
		
		channelElem = channelElem->next;
	}
	
	if (v) {
		printtime(log);
		fprintf(log, "Done getting descriptions for %s .\n", feed->tag);
	}
	
	channelElem = channel->last;
	while (channelElem != NULL && channelElem->type == XML_TEXT_NODE) {
		printf("%s\n", (char *)channelElem->name);
		channelElem = channelElem->prev;
	}
	
	if (channelElem == NULL) {
		printtime(log);
		fprintf(log, "Error going through xml for %s .\n", feed->tag);
	}
	
	while (channelElem != NULL && (strcmp((char *)channelElem->name, "item") == 0 || channelElem->type == XML_TEXT_NODE)) {
		if (channelElem->type == XML_TEXT_NODE) {
			channelElem = channelElem->prev;
			continue;
		}
		
		xmlNode* rssElem = channelElem->children;
		while (rssElem != NULL && strcmp((char *)rssElem->name, "link") != 0)
			rssElem = rssElem->next;
		
		if (rssElem == NULL || rssElem->children == NULL || rssElem->children->type != XML_TEXT_NODE)
			continue;
		
		char check[strlen((char *)rssElem->children->content) + 8];
		sprintf(check, "link: %s\n", (char *)rssElem->children->content);
		
		if (contains(feed->out, check)) {
			channelElem = channelElem->prev;
			continue;
		}
		
		printChildren(channelElem, feed->out);
		fprintf(feed->out, "ITEMS\n");
		fflush(feed->out);
		
		channelElem = channelElem->prev;
	}
	
	if (v) {
		printtime(log);
		fprintf(log, "Done reading rss data for %s .\n", feed->tag);
	}
	
	return 0;
}
