/* BEGIN_ICS_COPYRIGHT5 ****************************************

Copyright (c) 2015, Intel Corporation

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
     documentation and/or other materials provided with the distribution.
    * Neither the name of Intel Corporation nor the names of its contributors
      may be used to endorse or promote products derived from this software
      without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

 * ** END_ICS_COPYRIGHT5   ****************************************/
/* [ICS VERSION STRING: unknown] */

/*
 * qlgc_fork.c -- TCL extension to workaround problems in exp_fork/exp_wait
 * when TCL_THREADS enabled in build
 */
#include <tcl.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>

static int
Qlgc_fork_Cmd(ClientData cdata, Tcl_Interp *interp, int objc,  Tcl_Obj * CONST objv[])
{
	pid_t pid;
	char result[20];

	fflush(stdout);
	fflush(stderr);
	/* this is a bit of a hack, but seems to solve the problem
	 * by stopping and starting the notifier we ensure a notifier
	 * exists for the child process.  We must stop it because the
	 * startup depends on hidden static status variables.
	 */
	Tcl_FinalizeNotifier(NULL);	// arg not used in TCL 8.4 on Linux
	pid = fork();
	(void)Tcl_InitNotifier();
	sprintf(result, "%d", pid);
	Tcl_SetObjResult(interp, Tcl_NewStringObj(result, -1));
	return TCL_OK;
}

static int
Qlgc_wait_Cmd(ClientData cdata, Tcl_Interp *interp, int objc,  Tcl_Obj * CONST objv[])
{
	pid_t pid;
	int status;
	char result[20];

	/* don't check args, this way the -i -1 arg of exp_wait is simply ignored */
	pid = wait(&status);

	sprintf(result, "%d", pid);
	Tcl_SetObjResult(interp, Tcl_NewStringObj(result, -1));
	return TCL_OK;
}

#if 0 /* this is comparible to TCL's normal exit command, don't need this */
static int
Qlgc_exit_Cmd(ClientData cdata, Tcl_Interp *interp, int objc,  Tcl_Obj * CONST objv[])
{
	int status;


   	/*
	 * Check params.
	 */
	if (objc != 2) {
		Tcl_WrongNumArgs(interp, 1, objv, "status");
		return TCL_ERROR;
	}
	if (Tcl_GetIntFromObj(interp, objv[1], &status) != TCL_OK) {
		return TCL_ERROR;
	}
	Tcl_Exit(status);
	//fflush(stdout);
	//fflush(stderr);
	//exit(status);

	/*NOTREACHED*/
	return TCL_OK;
}
#endif

/* This is a quick and dirty implementation of dup capability for TCL
 * Usage:  dup file
 * Parameters:
 * 	file - an existing TCL file/channel opened read/write
 * Additional Information:
 *  Implemented and tested for intended usage of: close stderr; dup stdout
 *  such that stderr can be redirected to the same file used for stdout
 */
static int
Qlgc_dup_Cmd(ClientData cdata, Tcl_Interp *interp, int objc,  Tcl_Obj * CONST objv[])
{
	char *file;
	Tcl_Channel channel;
	Tcl_Channel newchannel;
	ClientData handle;
	int fd, newfd;
	int mode;
	char result[100];

	/*
	 * Check params.
	 */
	if (objc != 2) {
		Tcl_WrongNumArgs(interp, 1, objv, "file");
		return TCL_ERROR;
	}

	file = Tcl_GetStringFromObj(objv[1], NULL);
	channel = Tcl_GetChannel(interp, file, &mode);
	if (! channel)
		goto failfile;

	if (Tcl_Flush(channel) == TCL_ERROR) {
		errno = Tcl_GetErrno();
		goto failerrno;
	}

	if (TCL_ERROR == Tcl_GetChannelHandle(channel, TCL_WRITABLE, &handle))
		goto failfile;
	fd = (int)(long int)handle;

	newfd = dup(fd);

	newchannel = Tcl_MakeFileChannel((ClientData)(long)newfd, mode);
	Tcl_RegisterChannel(interp, newchannel);
	Tcl_SetObjResult(interp, Tcl_NewStringObj(Tcl_GetChannelName(newchannel), -1));
	
	return TCL_OK;

failfile:
	Tcl_ResetResult(interp);
	Tcl_AppendResult(interp, "qlgc_dup failed: invalid file handle: ", file, NULL);
	goto fail;

failerrno:
	sprintf(result, "qlgc_dup failed: %s", Tcl_PosixError(interp));
	Tcl_SetObjResult(interp, Tcl_NewStringObj(result, -1));

fail:
	return TCL_ERROR;
}

#if 0
// this idea was viable and sort of worked.  Except that
// the TCL shell did not process the resulting trap handler
// until after the command being executed completed.
// This was intended as a way for opacmdall to set a timeout for
// remote ssh commands
typedef void (*signal_handler_t)(int);

static int have_old_alarm_handler = 0;
static signal_handler_t old_alarm_handler;

static void save_alarm_handler(signal_handler_t handler)
{
	if (! have_old_alarm_handler) {
		printf("saving alarm handler\n");
		old_alarm_handler = handler;
		have_old_alarm_handler = 1;
	}
}
		
static void restore_alarm_handler()
{
	if (have_old_alarm_handler) {
		printf("restoring alarm handler\n");
		signal(SIGALRM, old_alarm_handler);
		have_old_alarm_handler = 0;
	}
}


/* This is a quick and dirty way to get alarms into a "trapable" signal
 * for expect code.  Expect uses SIGALRM, so it won't allow trap for it
 * however, if we are careful not to use the qlgc_alarm code within
 * expect {} statements, we should be safe to borrow the alarms as long as
 * we restore the handler when we are done.
 */
static void alarm_handler(int x)
{
			int ret;
	printf("got alarm\n");
	restore_alarm_handler();
	printf("send SIGUSR1 to %d\n", getpid());
	if (0 != (ret = kill (0 /*getpid()*/, SIGUSR1)))
		printf("kill SIGUSR1 failed %s\n", strerror(errno));
	//if (0 != (ret = kill (getpid(), SIGHUP)))
	//	printf("kill SIGHUP  %s\n", strerror(errno));
}

/* This is a quick and dirty implementation of alarm capability for TCL
 * Usage:  alarm seconds
 * Parameters:
 * 	seconds - number of seconds til next SIGALRM, 0 to disable
 * Returns:
 *  number of seconds remaining until any previously scheduled alarm was due to
 *  be delivered.
 */
static int
Qlgc_alarm_Cmd(ClientData cdata, Tcl_Interp *interp, int objc,  Tcl_Obj * CONST objv[])
{
	int seconds;
	int secondsleft;
	char result[20];
	signal_handler_t ret;

   	/*
	 * Check params.
	 */
	if (objc != 2) {
		Tcl_WrongNumArgs(interp, 1, objv, "seconds");
		return TCL_ERROR;
	}
	if (Tcl_GetIntFromObj(interp, objv[1], &seconds) != TCL_OK) {
		return TCL_ERROR;
	}
printf("qlgc_alarm %d\n", seconds);
	if (seconds) {
		ret = signal(SIGALRM, alarm_handler);
		if (ret == SIG_ERR)
			goto failerrno;
		save_alarm_handler(ret);
	}
	secondsleft = (int)alarm((unsigned int)seconds);
	if (! seconds)
		restore_alarm_handler();

	sprintf(result, "%d", secondsleft);
	Tcl_SetObjResult(interp, Tcl_NewStringObj(result, -1));
	return TCL_OK;

failerrno:
	sprintf(result, "qlgc_alarm failed: %s", strerror(errno));
	Tcl_SetObjResult(interp, Tcl_NewStringObj(result, -1));
	return TCL_ERROR;
}
#endif

/*
 * qlgc_fork_Init -- Called when Tcl loads the extension.
 */
int DLLEXPORT
Qlgc_fork_Init(Tcl_Interp *interp)
{
	if (Tcl_InitStubs(interp, TCL_VERSION, 0) == NULL) {
		return TCL_ERROR;
	}
	/* changed this to check for an error - GPS */
	if (Tcl_PkgProvide(interp, "qlgc_fork", "1.0") == TCL_ERROR) {
		return TCL_ERROR;
	}
	Tcl_CreateObjCommand(interp, "qlgc_fork", Qlgc_fork_Cmd, NULL, NULL);
	Tcl_CreateObjCommand(interp, "qlgc_wait", Qlgc_wait_Cmd, NULL, NULL);
	//Tcl_CreateObjCommand(interp, "qlgc_exit", Qlgc_exit_Cmd, NULL, NULL);
	Tcl_CreateObjCommand(interp, "qlgc_dup", Qlgc_dup_Cmd, NULL, NULL);
	//Tcl_CreateObjCommand(interp, "qlgc_alarm", Qlgc_alarm_Cmd, NULL, NULL);
	return TCL_OK;
}
