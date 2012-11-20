#ifndef __TSH_H__
#define __TSH_H__

/* Misc manifest constants */
#define MAXLINE    1024   /* max line size */
#define MAXARGS     128   /* max args on a command line */
#define MAXJOBS      16   /* max jobs at any point in time */
#define MAXJID    1<<16   /* max job ID */

/* Job states */
#define UNDEF 0 /* undefined */
#define FG 1    /* running in foreground */
#define BG 2    /* running in background */
#define ST 3    /* stopped */
/* 
 * Jobs states: FG (foreground), BG (background), ST (stopped)
 * Job state transitions and enabling actions:
 *     FG -> ST  : ctrl-z
 *     ST -> FG  : fg command
 *     ST -> BG  : bg command
 *     BG -> FG  : fg command
 * At most 1 job can be in the FG state.
 */

/* 
 * parseline - Parse the command line and build the argv array.
 *    cmdline: the command line string entered at the shell prompt
 *    argv:  an array of size MAXARGS of char *
 *           parseline will initialize its contents from the cmdline
 *           string.
 *           The caller should pass in a variable delcared as:
 *              char *argv[MAXARGS];
 *              (ex) int bg = parseline(commandLine, argv);
 *
 *    returns: non-zero (true) if the command line includes & at the end
 *                             to run the command in the BG
 *             zero (false) for a foreground command FG
 */
int parseline(const char *cmdline, char **argv);

#endif
