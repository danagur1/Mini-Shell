#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/wait.h> 
#include <signal.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>

void handle_sigchld(int sig) {
    int status;
    pid_t pid;
    if (sig == SIGCHLD) {
        //Used for killing zombies

        while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
            if (WIFEXITED(status)) {
                //child exited- it is a zombie
                kill(pid, SIGKILL);
            } else if (WIFSIGNALED(status)) {
                //child terminated- it is a zombie
                kill(pid, SIGKILL);
            } else {
                //Unknown status
            }
        }
    }
    if (sig==SIGINT) {
        pid_t ppid = getpid();
        while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
            if (pid != ppid) { //kill evey process except the parent
                kill(pid, SIGKILL);
            }
    }
    }
}

int set_up_handler(){
    //use SIGCHLD handler for killing zombies as soon as the exit:
        struct sigaction sa;
        sa.sa_handler = handle_sigchld;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = SA_RESTART;
        if (sigaction(SIGCHLD, &sa, NULL) == -1) {
            //Failed to set up signal handler
            fprintf(stderr, "Failed to set up signal handle for SIGCHLD- (for killing zombies)\n");
            exit(1);
        }
        if (sigaction(SIGINT, &sa, NULL) == -1) {
            //Failed to set up signal handler
            fprintf(stderr, "Failed to set up signal handle for SIGINT\n");
            exit(1);
        }
        return 0;
}


int run_args(char** arglist){
    if (arglist[0]==NULL){
        fprintf(stderr, "Empty command\n");
        exit(1);
    }
    int return_execvp = execvp(arglist[0], arglist);
		if (return_execvp!=0){
			fprintf(stderr, "Failed to run command\n");
            exit(1);
		}
    return 1; //not suppose to go here because execvp started
}

int child2(char** arglist, int run_pipe, int *fd){
    pid_t pid2 = fork(); //this is the 2nd son
    switch (pid2){
        case -1: ;
            fprintf(stderr, "failed to create child process for pipe\n");
            exit(1);
            break;
        case 0: ;
            //Child:
            char** arglist2 = arglist+run_pipe+1;
            close(fd[1]); // close write end of pipe
            if (dup2(fd[0], STDIN_FILENO)==-1){
                fprintf(stderr, "failed to redirect stdin for pipe\n");
                exit(1);
            }
            int return_run = run_args(arglist2);
            close(fd[0]);
            return return_run;
            break;
        default: ;
            //Parent
            return pid2;
            break;
    }
    return 1;
    
}

int parent(char** arglist, pid_t pid, int run_background, int run_pipe, int *fd){
    if (!run_background && !run_pipe){
            int status;
            pid_t child_pid = waitpid(pid,&status, 0);
            if (child_pid == -1) {
                if (!(errno == EINTR || errno == ECHILD)) {
                    fprintf(stderr, "Error on waiting for child process\n");
                    return 0; //it is not considered an actual error that requires exiting the shell
                } 
            }
        }
    if (run_pipe){
        int pid2 = child2(arglist, run_pipe, fd);
        int status;
        waitpid(pid2, &status, 0);
    }
    return 1;
}

int pipe_checking(int count, char** arglist){
    for (int i=0; i<count; i++){
        if (strcmp(arglist[i], "|")==0){
            return i;
        }
    }
    return 0;
}

int child1(char** arglist, int run_pipe, int *fd, int run_redirection, int count){
    /* Child*/
        if (run_pipe){
            char *arglist_temp = arglist[run_pipe];
            arglist[run_pipe] = NULL;
            close(fd[0]); // close read end of pipe
            if (dup2(fd[1], STDOUT_FILENO)==-1){
                fprintf(stderr, "failed to redirect stdout in pipe\n");
                exit(1);
            }
            int return_exec = run_args(arglist);
            close(fd[1]);
            arglist[run_pipe] = arglist_temp;
            return return_exec;
        }
        if (run_redirection){   
            int fd = open(arglist[count-1], O_RDONLY);
            if (fd==-1){
                fprintf(stderr, "failed to open file for redirection\n");
                exit(1);
            }
            if (dup2(fd, STDIN_FILENO) == -1){
                close(fd);
                fprintf(stderr, "failed to redirect stdin\n");
                exit(1);
            }
            arglist[count-2] = NULL;
            int return_exec = run_args(arglist);
            close(fd);
            return return_exec;
        }
		return run_args(arglist);
}

int process_arglist(int count, char** arglist){
    int run_background = (strcmp(arglist[count-1],"&")==0);
    int run_pipe = pipe_checking(count, arglist);
    int run_redirection = ((count>=3) && (strcmp(arglist[count-2], "<")==0));
    int fd[2];
    if (run_pipe){ //Create pipe:
        if (pipe(fd) != 0) { 
            fprintf(stderr, "failed to create pipe syscall\n");
            return 0;
        }
    }
	pid_t pid = fork();
	switch(pid) {
	case -1: ;
        fprintf(stderr, "failed to create child process\n");
        return 0;
        break;
	case 0: ;
        //Child
		child1(arglist, run_pipe, fd, run_redirection, count);
        break;
	default: ;  
		//Parent
        return parent(arglist, pid, run_background, run_pipe, fd);
		break;
	}
    return 1;

}

int prepare(){return set_up_handler();}
int finalize(){return 0;}
