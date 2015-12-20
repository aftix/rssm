#include <stdio.h>
#include <unistd.h>
#include "control.h"

//Make the child process and return the pid_t from fork
//returns 0 if the current process is the child
//returns -1 on error
//returns child's pid otherwise
//the arguements are the pipes for stdin, stdout, and stderr on the child
pid_t makeChild(int *in, int *out, int *err, int v) {
	int p_stdin[2], p_stdout[2], p_stderr[2];
	pid_t pid;
	
	if (v)
		printf("Setting up the pipes to talk to the child process...\n");
	if (pipe(p_stdin) != 0 || pipe(p_stdout) != 0 || pipe(p_stderr) != 0) {
		fprintf(stderr, "Error! Pipe creation failure on makeChild\n");
		return -1;
	}
	
	if (v)
		printf("Forking the child...\n");
	//Make child
	pid = fork();
	if (pid < 0)
		return pid;
	
	//Setup the child
	if (pid == 0) {
		//Close the writing end of p_stdin and then make stdin be the reading end
		close(p_stdin[1]);
		dup2(p_stdin[0], 0);
		//Close reading end of p_stdout and make stdout the writing end
		close(p_stdout[0]);
		dup2(p_stdout[1], 1);
		//Close reading on p_stderr and make stderr the writing end
		close(p_stderr[0]);
		dup2(p_stderr[1], 2);
	} else {
		if (v)
			printf("Setting up the communication pipes with fd's...");
		//Check if the arguements are null. If so, close the pipe for child communication. If not, set the communication pipe to be the value the arguement points at
		if (in == NULL)
			close(p_stdin[1]);
		else
			*in = p_stdin[1];
		
		if (out == NULL)
			close(p_stdout[0]);
		else
			*out = p_stdout[0];
		
		if (err == NULL)
			close(p_stderr[0]);
		else
			*err = p_stderr[0];
	}
	
	return pid;
}
