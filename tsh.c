/* 
 * tsh - A tiny shell program with job control
 * 
 * Zach Lockett-Streiff (zlocket1)
 * Taylor Nation (tnation1)
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include "tsh.h"

/* Global variables */
extern char **environ;      /* defined in libc */
char prompt[] = "tsh> ";    /* command line prompt (DO NOT CHANGE) */
int verbose = 0;            /* if true, print additional output */
int nextjid = 1;            /* next job ID to allocate */
char sbuf[MAXLINE];         /* for composing sprintf messages */

struct job_t {              /* The job struct */
    pid_t pid;              /* job PID */
    int jid;                /* job ID [1, 2, ...] */
    int state;              /* UNDEF, BG, FG, or ST */
    char cmdline[MAXLINE];  /* command line */
};
struct job_t jobs[MAXJOBS]; /* The job list */
/* End global variables */


/* Function prototypes */

/* Hrere are the functions that you will implement */
void eval(char *cmdline);
int builtin_cmd(char **argv);
void do_bgfg(char **argv);
void waitfg(pid_t pid);

void sigchld_handler(int sig);
void sigtstp_handler(int sig);
void sigint_handler(int sig);

/* Here are helper routines that we've provided for you */
void sigquit_handler(int sig);

void clearjob(struct job_t *job);
void initjobs(struct job_t *jobs);
int maxjid(struct job_t *jobs); 
int addjob(struct job_t *jobs, pid_t pid, int state, char *cmdline);
int deletejob(struct job_t *jobs, pid_t pid); 
pid_t fgpid(struct job_t *jobs);
struct job_t *getjobpid(struct job_t *jobs, pid_t pid);
struct job_t *getjobjid(struct job_t *jobs, int jid); 
int pid2jid(pid_t pid); 
void listjobs(struct job_t *jobs);

void usage(void);
void unix_error(char *msg);
void app_error(char *msg);
typedef void handler_t(int);
handler_t *Signal(int signum, handler_t *handler);
void listbgjobs(struct job_t *jobs);
int pidexist(pid_t pid, struct job_t *jobs);

/*
 * main - The shell's main routine 
 */
int main(int argc, char **argv) 
{
    //system("clear");
    char c;
    char cmdline[MAXLINE];
    int emit_prompt = 1; /* emit prompt (default) */

    /* Redirect stderr to stdout (so that driver will get all output
     * on the pipe connected to stdout) */
    dup2(1, 2);

    /* Parse the command line */
    while ((c = getopt(argc, argv, "hvp")) != EOF) {
        switch (c) {
        case 'h':             /* print help message */
            usage();
	    break;
        case 'v':             /* emit additional diagnostic info */
            verbose = 1;
	    break;
        case 'p':             /* don't print a prompt */
            emit_prompt = 0;  /* handy for automatic testing */
	    break;
	default:
            usage();
	}
    }

    /* Install the signal handlers */

    /* These are the ones you will need to implement */
    Signal(SIGINT,  sigint_handler);   /* ctrl-c */
    Signal(SIGTSTP, sigtstp_handler);  /* ctrl-z */
    Signal(SIGCHLD, sigchld_handler);  /* Terminated or stopped child */

    /* This one provides a clean way to kill the shell */
    Signal(SIGQUIT, sigquit_handler); 

    /* Initialize the job list */
    initjobs(jobs);

    /* Execute the shell's read/eval loop */
    while (1) {
        
	/* Read command line */
	if (emit_prompt) {
	    printf("%s", prompt);
	    fflush(stdout);
	}
	if ((fgets(cmdline, MAXLINE, stdin) == NULL) && ferror(stdin))
	    app_error("fgets error");
	if (feof(stdin)) { /* End of file (ctrl-d) */
	    fflush(stdout);
	    exit(0);
	}

	/* Evaluate the command line */
	
        eval(cmdline);
        

        
	fflush(stdout);
	fflush(stdout);
    } 

    exit(0); /* control never reaches here */
}
  
  
/* 
 * eval - Evaluate the command line that the user has just typed in
 * 
 * If the user has requested a built-in command (quit, jobs, bg or fg)
 * then execute it immediately. Otherwise, fork a child process and
 * run the job in the context of the child. If the job is running in
 * the foreground, wait for it to terminate and then return.  Note:
 * each child process must have a unique process group ID so that our
 * background children don't receive SIGINT (SIGTSTP) from the kernel
 * when we type ctrl-c (ctrl-z) at the keyboard.  
*/
void eval(char *cmdline) 
{
    int jid = 0;
    pid_t pid;
    sigset_t mask;
    // cmdline is a pointer to the command line string (char array)
    char *argv[MAXARGS]; /* argv for execve() */
                         // call int bg = parseline(cmdline, argv);  
                         // to parse command line arguments into argv 
                         // that you can pass to execve
    
    /* Parse command line */
    int bg = parseline(cmdline, argv); /* bg=1:bg; bg=0:fg */
    //printf("bg: %d\n",bg);
    if (argv[0] == NULL) {
      return; /* Empty line - ignore it */
    }


    // If argv is a built-in command, execute it immediately and return
    if (!builtin_cmd(argv)) {
      // Fork a child process which runs job. block the sigchild signal until
      // added to the job list, so that a child that ends quickly doesn't 
      // cause a segfault by deleting a job that doesn't exist.
      
      sigemptyset(&mask);
      sigaddset(&mask, SIGCHLD);

      sigprocmask(SIG_BLOCK, &mask, NULL);
      if ((pid = fork()) == 0) {
        // This is the child process
        sigprocmask(SIG_UNBLOCK, &mask, NULL);

        //Sets group pid to the value of the parent's PID.
        setpgid(0,0);

        //If there's an execve error, that means the command doesn't exist.
        if (execve(argv[0], argv, environ) < 0) {
          printf("%s: Command not found. \n", argv[0]);
        } 
      }
      
      if (!bg) {
      /* This is the parent process */
        // Run process in foreground
        // use waitfg to wait for child process to terminate
        // proceed to next iteration upon termination of child process
        // block SIGCHILD signal until after job is added 
        
      
        addjob(jobs, pid, FG, cmdline);
        sigprocmask(SIG_UNBLOCK, &mask, NULL);
        
       
        waitfg(pid);

      } 
      else {
        // Run process in background
        // return to top of loop, await next command line entry
        // TODO: Get the jid of new job with that helper function
        //printf("Run in background\n");
        //
        //
        // We've added to the list, so now it's safe to unblock the
        // sigchld process.
        addjob(jobs, pid, BG, cmdline);
        sigprocmask(SIG_UNBLOCK, &mask, NULL);
        jid = getjobpid(jobs, pid)->jid;

        // Display information about the command.
        printf("[%d] (%d) %s", jid, pid, cmdline);
        
      }
    }
    
    
}

/* 
 * builtin_cmd - If the user has typed a built-in command then execute
 *    it immediately.  
 */
int builtin_cmd(char **argv) 
{
    
    // if cmdline represents built-in command, execute it immediately
    // else fork a child process and run the job in the context of the
    // child (outside this function)
    if (!(strcmp(argv[0],"quit"))) {
      exit(0); 
    } else if (!strcmp(argv[0],"jobs")) {
          // List all jobs running in background
          listjobs(jobs);
          return 1;
    } else if (!strcmp(argv[0],"bg")) {
          // Check for PID or JID argument
          do_bgfg(argv);
          // Restart <job> by sending SIGCONT signal, runs job in background
          return 1;
    } else if (!strcmp(argv[0],"fg")) {
          // Check for PID of JID argument
          do_bgfg(argv);
          // Restart <job> by sending SIGCONT signal, runs job in foreground
          return 1;
    }
    return 0;     /* not a builtin command */
  }
/* 
 * do_bgfg - Execute the builtin bg and fg commands
 */
void do_bgfg(char **argv) 
{
    pid_t pid_1;
    int temp2;
    int length,i;
    length  = strlen(argv[1]);
    char temp[length];
    struct job_t *currentjob; 
    int is_jid;
    is_jid = 0;
    // TODO: Implement this function
    // If given jid, get pid from jid
    // If job is already in bg, don't necessarily need to do anything
    // If bringing job into fg, change state in joblist, restart in fg 
    // with SIGCONT If sending job to the background from fg, stop with SIGSTP,
    // start it again with SIGCONT...and also change state in job struct

    
    
    //Check to see if the user passed a jid/pid to bg/fg 
    if(argv[1] == NULL || *argv[1] == ' '){
      printf("%s command requires PID or %%jobid argument\n",argv[0]);
      return;
    }

    // Handles a call using a jid, indicated by %jid

    if(argv[1][0] =='%'){
       is_jid = 1;
      // Copies every element of the jid except the %, then null terminates it.
      for(i=1; i<length;i++){
        temp[i-1] = argv[1][i];
      }
      temp[length-1] = '\0';
      // Turns the char into an int.
      temp2 = atoi(temp);
      
            
   }else{

      temp2 = atoi(argv[1]);    
   }
     // Error checking to see if the JID/PID actually exists.
     // If it's a JID, check to see if it's in the range of the job list
      if(is_jid && (temp2 > maxjid(jobs)||temp2<0)){
        printf("%s: No such job.\n",argv[1]);
        return;
      }
    // If it's a PID, make sure it is a PID that actually exists.  

      else if( (!is_jid)&&(pidexist(temp2,jobs)==0)){
        printf("%s: No such job.\n",argv[1]);
      }
    // When we do atoi, c turns any characters that aren't ascii
    // equivalents of integers into 0's. So we can use this
    // to check to see if a character was passed into fg/bg instead
    // of an int.
      else if(!temp2){
        printf("%s: argument must be a PID or %%jobid\n",argv[0]);
        return;
      }
      
      // Gets job based on jid or pid
      if(is_jid){
        currentjob = getjobjid(jobs,temp2);
      }else{
        currentjob = getjobpid(jobs,temp2);
      }
      
      pid_1 = currentjob->pid;


    // Restarts bg job if command is bg <job>
    if(currentjob->state==BG && !strcmp("bg",argv[0])){
      kill(currentjob->pid*-1,SIGCONT);
      printf("[%d] (%d) %s",currentjob->jid, currentjob->pid,
          currentjob->cmdline);
      return;      

    // Runs the given job in the foreground by restarting the job, changing
    // its state to FG, then making it wait for all child processes to finish.
    } else if(!strcmp("fg",argv[0])){
      
      kill(currentjob->pid*-1,SIGCONT);
      currentjob->state = FG;
      waitfg(currentjob->pid);

    // Runs in the background by stopping the job, then restarting it
    // and updating its status.
    } else if(!strcmp("bg",argv[0])){
      currentjob->state=BG;
      kill(currentjob->pid*-1,SIGSTOP);
      kill(currentjob->pid*-1,SIGCONT);
      printf("[%d] (%d) %s",currentjob->jid, currentjob->pid,
          currentjob->cmdline);
    } 
    

   
   return;
}

/* 
 * waitfg - Block until process pid is no longer the foreground process
 */
void waitfg(pid_t pid)
{
    // is the job still in joblist? If not, return
    // if job is still there, but in bg: don't have to wait any longer
    // if neither, return
    
    
    if (getjobpid(jobs,pid) == NULL) {
      return;
    }

    // Waits while there's a foreground job, and its status is FG.
    while ((getjobpid(jobs,pid) != NULL)&&(getjobpid(jobs,pid)->state == FG)) {
      sleep(2);
    }
    return;
}

/*****************
 * Signal handlers
 *****************/

/* 
 * sigchld_handler - The kernel sends a SIGCHLD to the shell whenever
 *     a child job terminates (becomes a zombie), or stops because it
 *     received a SIGSTOP or SIGTSTP signal. The handler reaps all
 *     available zombie children, but doesn't wait for any other
 *     currently running children to terminate.  
 */
void sigchld_handler(int sig) 
{
    int mask;
    mask = (WNOHANG|WUNTRACED);
    int status;
    pid_t pid;
    // Check separately for stopped and terminated jobs
    // Check for terminated children without waiting for them to terminate
    // printf("entered handler\n");
    while((pid = waitpid(-1,&status,mask)) > 0) {

      if(WIFEXITED(status)){
        //Regular terminated process. We just need to delete it from the job
        //list.
        deletejob(jobs,pid);
      }else if(WIFSTOPPED(status)){
        // Child stopped by sigstop. Update status in job list.
        
        getjobpid(jobs,pid)->state = ST;
      }else if(WIFSIGNALED(status)){
        //WIFSIGNALED=terminated process.
        // Delete from joblist as well.
        deletejob(jobs,pid);
      }

    }

 
  return;
}

/* 
 * sigint_handler - The kernel sends a SIGINT to the shell whenever the
 *    user types ctrl-c at the keyboard.  Catch it and send it along
 *    to the foreground job.  
 */
void sigint_handler(int sig)
{
    pid_t pid;
    pid= fgpid(jobs);
    
    // We don't need to wory about jobs in the background.
    if (pid==0) {
      return;
    }else{
    
      // DEATH TO FOREGROUND JOBS
      kill(pid*-1,sig);
      printf("Job [%d] (%d) terminated by signal %d\n",pid2jid(pid),pid,sig);
      
    }
 

}
/*
 * sigtstp_handler - The kernel sends a SIGTSTP to the shell whenever
 *     the user types ctrl-z at the keyboard. Catch it and suspend the
 *     foreground job by sending it a SIGTSTP.  
 */
void sigtstp_handler(int sig) 
{   
    pid_t pid;
    pid= fgpid(jobs);

    // Again, background jobs are not affected by sigstp here.
    if (pid==0) {     
      return;
    }else{      
    
      // Stops foreground jobs.
      kill(pid*-1,sig);
      getjobpid(jobs,pid)->state = ST; 
      printf("Job [%d] (%d) stopped by signal %d\n",pid2jid(pid),pid,sig);
      return;
    }
 

}

/*********************
 * End signal handlers
 *********************/

/***********************************************
 * Helper routines that manipulate the job list
 **********************************************/

/* clearjob - Clear the entries in a job struct */
void clearjob(struct job_t *job) {
    job->pid = 0;
    job->jid = 0;
    job->state = UNDEF;
    job->cmdline[0] = '\0';
}

/* initjobs - Initialize the job list */
void initjobs(struct job_t *jobs) {
    int i;

    for (i = 0; i < MAXJOBS; i++)
	clearjob(&jobs[i]);
}

/* maxjid - Returns largest allocated job ID */
int maxjid(struct job_t *jobs) 
{
    int i, max=0;

    for (i = 0; i < MAXJOBS; i++)
	if (jobs[i].jid > max)
	    max = jobs[i].jid;
    return max;
}

/* addjob - Add a job to the job list */
int addjob(struct job_t *jobs, pid_t pid, int state, char *cmdline) 
{
    int i;
    
    if (pid < 1)
	return 0;
    //printf("call to addjob\n");
    for (i = 0; i < MAXJOBS; i++) {
	if (jobs[i].pid == 0) {
	    jobs[i].pid = pid;
	    jobs[i].state = state;
	    jobs[i].jid = nextjid++;
	    if (nextjid > MAXJOBS)
		nextjid = 1;
	    strcpy(jobs[i].cmdline, cmdline);
  	    if(verbose){
	        printf("Added job [%d] %d %s\n", jobs[i].jid, 
                    jobs[i].pid, jobs[i].cmdline);
            }
            return 1;
	}
    }
    printf("Tried to create too many jobs\n");
    return 0;
}

/* deletejob - Delete a job whose PID=pid from the job list */
int deletejob(struct job_t *jobs, pid_t pid) 
{
    int i;

    if (pid < 1)
	return 0;

    for (i = 0; i < MAXJOBS; i++) {
	if (jobs[i].pid == pid) {
	    clearjob(&jobs[i]);
	    nextjid = maxjid(jobs)+1;
	    return 1;
	}
    }
    return 0;
}

/* fgpid - Return PID of current foreground job, 0 if no such job */
pid_t fgpid(struct job_t *jobs) {
    int i;

    for (i = 0; i < MAXJOBS; i++)
	if (jobs[i].state == FG)
	    return jobs[i].pid;
    return 0;
}

/* getjobpid  - Find a job (by PID) on the job list */
struct job_t *getjobpid(struct job_t *jobs, pid_t pid) {
    int i;

    if (pid < 1)
	return NULL;
    for (i = 0; i < MAXJOBS; i++)
	if (jobs[i].pid == pid)
	    return &jobs[i];
    return NULL;
}

/* getjobjid  - Find a job (by JID) on the job list */
struct job_t *getjobjid(struct job_t *jobs, int jid) 
{
    int i;

    if (jid < 1)
	return NULL;
    for (i = 0; i < MAXJOBS; i++)
	if (jobs[i].jid == jid)
           return &jobs[i];
    return NULL;
}

/* pid2jid - Map process ID to job ID */
int pid2jid(pid_t pid) 
{
    int i;

    if (pid < 1)
	return 0;
    for (i = 0; i < MAXJOBS; i++)
	if (jobs[i].pid == pid) {
            return jobs[i].jid;
        }
    return 0;
}

/* listjobs - Print the job list */
void listjobs(struct job_t *jobs) 
{
    int i;
    
    for (i = 0; i < MAXJOBS; i++) {
	if (jobs[i].pid != 0) {
	    printf("[%d] (%d) ", jobs[i].jid, jobs[i].pid);
	    switch (jobs[i].state) {
		case BG: 
		    printf("Running ");
		    break;
		case FG: 
		    printf("Foreground ");
		    break;
		case ST: 
		    printf("Stopped ");
		    break;
	    default:
		    printf("listjobs: Internal error: job[%d].state=%d ", 
			   i, jobs[i].state);
	    }
	    printf("%s", jobs[i].cmdline);
	}
    }
}
/******************************
 * end job list helper routines
 ******************************/


/***********************
 * Other helper routines
 ***********************/

/*
 * usage - print a help message
 */
void usage(void) 
{
    printf("Usage: shell [-hvp]\n");
    printf("   -h   print this message\n");
    printf("   -v   print additional diagnostic information\n");
    printf("   -p   do not emit a command prompt\n");
    exit(1);
}

/*
 * unix_error - unix-style error routine
 */
void unix_error(char *msg)
{
    fprintf(stdout, "%s: %s\n", msg, strerror(errno));
    exit(1);
}

/*
 * app_error - application-style error routine
 */
void app_error(char *msg)
{
    fprintf(stdout, "%s\n", msg);
    exit(1);
}

/*
 * Signal - wrapper for the sigaction function
 */
handler_t *Signal(int signum, handler_t *handler) 
{
    struct sigaction action, old_action;

    action.sa_handler = handler;  
    sigemptyset(&action.sa_mask); /* block sigs of type being handled */
    action.sa_flags = SA_RESTART; /* restart syscalls if possible */

    if (sigaction(signum, &action, &old_action) < 0)
	unix_error("Signal error");
    return (old_action.sa_handler);
}

/*
 * sigquit_handler - The driver program can gracefully terminate the
 *    child shell by sending it a SIGQUIT signal.
 */
void sigquit_handler(int sig) 
{
    printf("Terminating after receipt of SIGQUIT signal\n");
    exit(1);
}

/* Our helper functions */
void listbgjobs(struct job_t *jobs)
{
    int i;

    for (i = 0; i < MAXJOBS; i++) {
        if (jobs[i].state == 2) {
            printf("[%d] (%d) %s", jobs[i].jid, jobs[i].pid, jobs[i].cmdline);
        }
    }
}

int pidexist(pid_t pid, struct job_t *jobs)
{
  // Not necessarily the most efficient search algorithm, but easy to code
  // on the fly. Checks to see if a PID is in the job list. Used in do_bgfg.
  int i;
  for(i = 0; i < MAXJOBS; i++){
    if(jobs[i].pid == pid){
      return 1;
    }
  }
  return 0;
}
