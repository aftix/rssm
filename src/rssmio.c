#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <arpa/inet.h>

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
		fprintf(log, "Checking that fifo %s exists...\n", path);
	}
	
	struct stat st = {0};
	
	//Get stat for the path
	//If file doesn't exist, make a fifo there (directory is ensured by prior call to makeDir)
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
	
	//Check if the file is a fifo...
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

//This function gets the port at the tail end of an url, defaults to 80
static int getPort(const char* url) {
	//pos is now at the length of url-1, which is the last element, or \0
	size_t pos = strlen(url);
	
	//Look for a : or a . . The : indicates a port number, the . before a : indicates no port on the tail end
	while (url[pos] != ':' && url[pos] != '.') pos--;
	
	//If it is a ., then no port, default to 0
	if (url[pos] == '.')
		return 80;
	
	//Now we get the port until the end by incrementing pos (to pass over :) and waiting until we see a \0
	//length of url - our position + 1 = how many chars are left
	char buffer[strlen(url) - pos  + 1];
	memset(buffer, 0, sizeof(char) * (strlen(url) - pos + 1));
	
	size_t i = 0;
	//The string can end with a \0 or a /, as something could be like google.com:80/, test.com/blah.html:80/
	while (url[pos] != '\0' && url[pos] != '/') {
		buffer[i] = url[pos];
		i++;
		pos++;
	}
	
	//Now we atoi
	return atoi(buffer);
}

//This function returns the ip address of a hostname. It ignores port number
//if hostOnly is 1 it just returns the hostname like www.google.com
static char* getIp(const char* url, int hostOnly) {
	//Pos is the last index of url, or \0
	int pos = strlen(url);
	
	//Look for a . or a :, a . before : implies no port
	while(url[pos] != ':' && url[pos] != '.') pos--;
	
	//If pos is . then no port, we can just set pos to strlen to negate the port removal part
	if (url[pos] == '.')
		pos = strlen(url);
	
	//This is the url without port: we start at pos-1 which is the char before :
	char portless[pos+1];
	portless[pos] = '\0';
	while (pos >= 0) {
		portless[pos] = url[pos];
		pos--;
	}
	
	//We have to get the core of the url - no http://, i.e. www.google.com not http://google.com/#q
	char core[strlen(portless) + 1];
	memset(core, 0, sizeof(char) * (strlen(portless) + 1));
	
	pos = 0;
	size_t core_pos = 0;
	//If the url has http://, ignore that. Check the first 4 chars. We want www. If not, just put it at the front of core
	if (strncmp(portless, "http://", sizeof(char) * 7) == 0) {
		pos = 7;
		
		strcpy(core, "www.");
		core_pos = 4;
		
		if (strncmp(portless, "http://www.", sizeof(char) * 11) == 0) {
			pos += 4;
		}
	} else if (strncmp(portless, "https://", sizeof(char) * 8) == 0) { 
		pos = 8;
		
		strcpy(core, "www.");
		core_pos = 4;
		
		if (strncmp(portless, "https://www.", sizeof(char) * 12) == 0) {
			pos += 4;
		}
	} else {
		strcpy(core, "www.");
		core_pos = 4;
		
		if (strncmp(portless, "www.", sizeof(char) * 4) == 0) {
			pos += 4;
		}
	}
	
	//Now just move over chars until a / or \0 is hit
	while (portless[pos] != '/' && portless[pos] != '\0') {
		core[core_pos] = portless[pos];
		core_pos++;
		pos++;
	}
	core[core_pos] = '\0';
	
	if (hostOnly) {
		char* ret = malloc(sizeof(char) * (strlen(core) + 1));
		strcpy(ret, core);
		return ret;
	}
	
	struct hostent *he;
	struct in_addr **addrList;
	size_t i;
	char* ip = malloc(sizeof(char) * 100);
	ip[0] = '\0';
	
	if ((he = gethostbyname(core)) == NULL) {
		return malloc(sizeof(char));
	}
	
	addrList = (struct in_addr **) he->h_addr_list;
	for (i = 0; addrList[i] != NULL; i++)
		strcpy(ip, inet_ntoa(*addrList[i]));
	return ip;
}

//Build the http request for a url
static char* httpGetForUrl(const char* url) {
	//hostname for http 1.1
	char* host = getIp(url, 1);
	
	size_t pos = 0;
	if (strncmp(url, "http://", sizeof(char) * 7) == 0)
		pos = 7;
	else if (strncmp(url, "https://", sizeof(char) * 8) == 0)
		pos = 8;
	
	char urlp[strlen(url)+1];
	memset(urlp, 0, sizeof(char) * (strlen(url) + 1));

	size_t bpos = strlen(url);
	//remove port from url
	while(url[bpos] != ':' && url[bpos] != '.') bpos--;
	
	if (url[bpos] == '.')
		bpos = strlen(url);
	
	strncpy(urlp, url, sizeof(char) * bpos);
	
	//Find first / after the https?://, those if statements places starting pos after any https?://
	while (urlp[pos] != '/' && urlp[pos] != '\0') pos++;
	
	//If we have no /'s, then we want to request root
	if (urlp[pos] == '\0' || pos == strlen(urlp)-1) {
		char* ret = malloc(sizeof(char) * (strlen(host) + 27));
		snprintf(ret, sizeof(char) * (strlen(host) + 27), "GET / HTTP/1.1\r\nHost: %s\r\n\r\n", host);
		free(host);
		return ret;
	}
	
	//Now we can build up the get request
	char* ret = malloc(sizeof(char) * (strlen(urlp) + strlen(host) - pos + 27));
	snprintf(ret, sizeof(char) * (strlen(urlp) + strlen(host) - pos + 27), "GET %s HTTP/1.1\r\nHost: %s\r\n\r\n", urlp+pos, host);
	free(host);
	return ret;
}

//Gets the length of the content from http response
static int getBytes(const char* resp) {
	//what we're looking for
	char* checkFor = "Content-Length: ";
	
	size_t i = 0;
	//Go through string
	while (resp[i] != '\0') {
		//If the character is a 'C' we need to check if we have a match on checkFor
		//The if statement only evaluates the right if [i] is 'C'
		if (resp[i] == 'C' && strncmp(resp+i, checkFor, strlen(checkFor)) == 0) {
			//Ok, we have a match. Time to get the number from this
			//First we set the index to the end of checkFor
			i += strlen(checkFor);
			if (resp[i] == '\0' || resp[i] == '\r' || resp[i] == '\n')
				return -1;
			//the length can be reasonable contained by 127 digits
			char buffer[128];
			memset(buffer, 0, sizeof(char) * 128);
			//copy at most 127 chars into buffer from resp starting at i 
			strncpy(buffer, resp + i, sizeof(char)*127);
			
			//buffer is now the number
			//return it converted to an int
			return atoi(buffer);
		}
		i++;
	}
	
	//failure
	return -1;
}

//gets the number of bytes a message has - the header
static int contentBytes(const char* resp) {
	size_t i = 0;
	
	while (resp[i] != '\0') {
		if (strncmp(resp+i, "\r\n\r\n", sizeof(char) * 4) == 0) 
			break;
		i++;
	}

	return sizeof(char) * (strlen(resp) - i - 1);
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

//This does the work of getting all the new rss stuff
void  getNewRss(const rssm_feeditem* feed, FILE* log, int v) {
	//get a descriptor for a socket
	int socketDesc = socket(AF_INET, SOCK_STREAM, 0);
	if (socketDesc < 0) {
		printtime(log);
		fprintf(log, "Failed to create socket for url %s, tag %s.\n", feed->url, feed->tag);
		return;
	}
	
	//socket address to the server specified by feed
	struct sockaddr_in server;
	server.sin_family = AF_INET;
	char *tmp;
	if (v) {
		printtime(log);
		fprintf(log, "Getting port from %s...\n", feed->url);
	}
	server.sin_port = htons(getPort(feed->url));
	if (v) {
		printtime(log);
		fprintf(log, "Getting ip from %s...\n", feed->url);
	}
	server.sin_addr.s_addr = inet_addr(tmp = getIp(feed->url, 0));
	free(tmp);
	
	if (v) {
		printtime(log);
		fprintf(log, "Starting connection to socket at url %s, ip %s, and port %d...\n", feed->url, getIp(feed->url, 0), getPort(feed->url));
	}
	if (connect(socketDesc, (struct sockaddr *)&server, sizeof(server)) < 0) {
		printtime(log);
		fprintf(log, "Error connecting to %s .\n", feed->url);
		return;
	}
	
	if (v) {
		printtime(log);
		fprintf(log, "Connection succesful to %s !", feed->url);
	}
	
	if (v) {
		printtime(log);
		fprintf(log, "Getting http request for %s\n", feed->url);
	}
	//Now we get the correct page off of the rss;
	char* msg = httpGetForUrl(feed->url);
	//Send the GET request to the server
	if (send(socketDesc, msg, strlen(msg), 0) < 0) {
		printtime(log);
		fprintf(log, "Error sending http request to %s .\n", feed->url);
		close(socketDesc);
		free(msg);
		return;
	}
	free(msg);
	
	if (v) {
		printtime(log);
		fprintf(log, "Sent http request to %s !\n", feed->url);
	}
	
	//Get reply
	//Since it's http the data will end with a \r\n, so check for that
	char* reply = malloc(sizeof(char) * REPLY_SIZE);
	memset(reply, 0, sizeof(char) * REPLY_SIZE);
	size_t alreadyRead = 0;
	
	//Get initial part (header)
	int rd = recv(socketDesc, reply, REPLY_SIZE - 1, 0);
	if (rd < 0) {
		printtime(log);
		fprintf(log, "Error reading http response from %s .\n", feed->url);
		close(socketDesc);
		free(reply);
		return;
	}
	alreadyRead = rd;
	
	if (v) {
		printtime(log);
		fprintf(log, "Recieving http respnse...\n");
	}
	
	int neededBytes = getBytes(reply);
	int hasBytes    = contentBytes(reply);
	
	//make reply sufficently large
	if (neededBytes + alreadyRead > REPLY_SIZE) {
		reply = realloc(reply, neededBytes + alreadyRead);
		memset(reply + alreadyRead + 1, 0, sizeof(char)*(neededBytes + alreadyRead - REPLY_SIZE));
	}
	
	if (neededBytes < 0 || hasBytes < 0) {
		printtime(log);
		fprintf(log, "Error parsing http header from %s .\n", feed->url);
		close(socketDesc);
		free(reply);
		return;
	}
	
	fcntl(socketDesc, F_SETFL, O_NONBLOCK);
	while (hasBytes < neededBytes) {
		rd = recv(socketDesc, reply + alreadyRead, (sizeof reply) - alreadyRead - 1, 0);
		if (rd < 0) {
			switch (errno) {
				case EINTR:
				case EAGAIN:
					continue;
			}
			alreadyRead = -1;
			break;
		}
		
		alreadyRead += rd;
		hasBytes    += rd;
	}
	
	if (alreadyRead < 0) {
		free(reply);
		printtime(log);
		fprintf(log, "Error recieving message from %s .\n", feed->url);
		close(socketDesc);
		return;
	}
	
	//Now we have what we need from the connection, close it
	close(socketDesc);
	if (v) {
		printtime(log);
		fprintf(log, "Closing connection to %s ...\n", feed->url);
		printtime(log);
		fprintf(log, "Parsing xml into memory...\n");
	}
	
	//We have to get the http header off of our xml.
	char* xmlStr = malloc(sizeof(char) * (neededBytes + 1));
	memset(xmlStr, 0, sizeof(char) * (neededBytes + 1));
	//sizeof(char)*strlen(reply) - neededBytes is the number of bytes the header takes up
	//the null character does not count and strlen ignores it
	size_t headerBytes = sizeof(char)*strlen(reply) - (size_t)neededBytes;
	//Now copy the right string into xmlStr. This is reply+headerBytes as to skip the header
	strncpy(xmlStr, reply + headerBytes, neededBytes);
	free(reply);
	
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
	
	if (strcmp((char *)xmlRoot->name, "rss") != 0 ) {
		printtime(log);
		fprintf(log, "No rss found at %s .\n", feed->url);
		free(xmlStr);
		xmlFreeDoc(xmlDoc);
		return;
	}
	
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
	if (strcmp((char *)channel->name, "channel") != 0) {
		if (v) {
			printtime(log);
			fprintf(log, "No rss channel was found at url %s \n.", feed->url);
		}

		fprintf(feed->desc, "No data found about rss channel.\n");
		fflush(feed->desc);
		
		xmlFreeDoc(xmlDoc);
		free(xmlStr);
		return;
	} else {
		if (v) {
			printtime(log);
			fprintf(log, "Description data found!\n");
		}
		channelElem = channel->children;
		while (strcmp((char *)channelElem->name, "item") != 0) {
			//Check if the desc file contains the exact data we've found
			//It's up to the end user to only get the newest data
			//The user can do this by keeping a cache of what name's they've read in (from the bottom) and skipping lines that have that name further up b/c those are older
			
			char tmp[strlen((char *)channelElem->children->content) + 2];
			tmp[0] = '\0';
			strcpy(tmp, (char *)channelElem->children->content);
			strcat(tmp, "\n");
			
			if (!contains(feed->desc, tmp)) {
				fprintf(feed->desc, "%s: %s\n", (char *)channelElem->name, (char *)channelElem->children->content);
				fflush(feed->desc);
			}
		
			channelElem = channelElem->next;
		}
	}
	
	if (v) {
		printtime(log);
		fprintf(log, "Getting items for %s ...\n", feed->tag);
	}
	
	//The top item of the rss feed is the newest. We want to add the items to the fifo oldest first, so we start at the last element
	//Go through all items until reach an element not named item
	channelElem = channel->last;
	while (strcmp((char *)channelElem->name, "item") == 0) {
		//First we find the title, which is what the fifo sorts by
		xmlNode* rssElem = channelElem->children;
		while (strcmp((char *)rssElem->name, "title") != 0)
			rssElem = rssElem->next;
		//Now rssElem is the title of the current item
		//See if the fifo contains the title. If not, we can go ahead and add the current item backwards into the fifo (backwards for tail)
		char search[strlen((char *)rssElem->children->content) + 8];
		search[0] = '\0';
		strcpy(search, "title: ");
		strcat(search, (char *)rssElem->children->content);
		
		if (!contains(feed->out, search)) {
			//start from the last element of the item
			rssElem = channelElem->last;
			//Stop at last to make sure we don't go out of bounds
			while (rssElem != channelElem->children) {
				if (rssElem->type == XML_ELEMENT_NODE) {
					fprintf(feed->out, "%s: %s\n", (char *)rssElem->name, (char *)rssElem->children->content);
					fflush(feed->out);
				} else {
					fprintf(log, "%d %s", rssElem->type, (char *)rssElem->name);
				}
				
				rssElem = rssElem->prev;
			}
			
			if (rssElem->type == XML_ELEMENT_NODE) {
				fprintf(feed->out, "%s: %s\nITEMS\n", (char *)rssElem->name, (char *)rssElem->children->content);
				fflush(feed->out);
			} else {
				fprintf(log, "%d %s", rssElem->type, (char *)rssElem->name);
			}
		}
		
		channelElem = channelElem->prev;
	}
	
	if (v) {
		printtime(log);
		fprintf(log, "Done reading rss data for %s .\n", feed->tag);
	}
	
	xmlFreeDoc(xmlDoc);
	free(xmlStr);
}
