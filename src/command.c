/* command.c */

#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/resource.h>
#include <sys/wait.h>

#include "system.h"
#include "util.h"
#include "type.h"
#include "command.h"
#include "control.h"

int SourceDepth = 0;

flag usageError(char *command, char *usage) {
        return warning("%s: usage\n    %s", command, usage);
}

flag commandHelp(char *command) {
        return eval(".showHelp %s", command);
}

/*****************************************************************************/

static int C_beep(TCL_CMDARGS) {
        beep();
        return TCL_OK;
}

static int C_seed(TCL_CMDARGS) {
        char *usage = "seed [<seed-value>]";
        if (argc > 1 && !strcmp(argv[1], "-h"))
                return commandHelp(argv[0]);
        if (argc > 2)
                return usageError(argv[0], usage);
        if (argc == 2)
                seedRand(atoi(argv[1]));
        else
                timeSeedRand();

        return result("%u", getSeed());
}

static int C_getSeed(TCL_CMDARGS) {
        char *usage = "getSeed";
        if (argc > 1 && !strcmp(argv[1], "-h"))
                return commandHelp(argv[0]);
        if (argc != 1)
                return usageError(argv[0], usage);

        return result("%u", getSeed());
}

static int C_rand(TCL_CMDARGS) {
        real min = 0.0, max = 1.0, temp;
        char *usage = "rand [[<min>] <max>]";
        if (argc > 1 && !strcmp(argv[1], "-h"))
                return commandHelp(argv[0]);
        if (argc > 3)
                return usageError(argv[0], usage);
        if (argc == 2)
                max = ator(argv[1]);
        else if (argc == 3) {
                min = ator(argv[1]);
                max = ator(argv[2]);
        }
        if (min > max) {
                temp = min;
                min = max;
                max = temp;
        }
        return result("%g", randReal((min + max) / 2, (max - min) / 2));
}

static int C_randInt(TCL_CMDARGS) {
        int min = 0, max = 2, temp;
        char *usage = "randInt [[<min>] <max>]";
        if (argc > 1 && !strcmp(argv[1], "-h"))
                return commandHelp(argv[0]);
        if (argc > 3)
                return usageError(argv[0], usage);
        if (argc == 2)
                max = atoi(argv[1]);
        else if (argc == 3) {
                min = atoi(argv[1]);
                max = atoi(argv[2]);
        }
        if (min > max) {
                temp = min;
                min = max;
                max = temp;
        }
        return result("%d", min + randInt(max - min));
}

#ifndef MACHINE_WINDOWS
static int C_nice(TCL_CMDARGS) {
        int val;
        char *usage = "nice [<priority-increment>]";
        if (argc > 1 && !strcmp(argv[1], "-h"))
                return commandHelp(argv[0]);
        if (argc > 2) return usageError(argv[0], usage);
        if (argc == 2) {
                val = atoi(argv[1]);
                if (val < 0) return warning("%s: priority increment can't be negative",
                                argv[0]);
                nice(val);
        }
        return result("%d", getpriority(PRIO_PROCESS, 0));
}
#else
static int C_exit(TCL_CMDARGS) {
        int val = 0;
        if (argc > 1) val = atoi(argv[1]);
        exit(val);
        return TCL_ERROR;
}
#endif /* MACHINE_WINDOWS */


static int C_time(TCL_CMDARGS) {
        real userticks, systicks, realticks;
        struct tms timesstart, timesstop;
        int i, iters = 1;
        char *usage = "time <command> [-iterations <iterations>]";
        if (argc > 1 && !strcmp(argv[1], "-h"))
                return commandHelp(argv[0]);
        if (argc != 2 && argc != 4) return usageError(argv[0], usage);

        if (argc == 4) {
                if (subString(argv[2], "-iterations", 2))
                        iters = atoi(argv[3]);
                else return usageError(argv[0], usage);
        }

        realticks = times(&timesstart);

        for (i = 0; i < iters; i++)
                eval(argv[1]);
        print(1, "%s\n", Tcl_GetStringResult(interp));

        realticks = times(&timesstop) - realticks;
        userticks = timesstop.tms_utime - timesstart.tms_utime;
        systicks  = timesstop.tms_stime - timesstart.tms_stime;

        return result("%.3f active  %.3f user  %.3f system  %.3f real",
                        (real) (userticks + systicks) / CLOCKS_PER_SEC,
                        (real) userticks / CLOCKS_PER_SEC,
                        (real) systicks / CLOCKS_PER_SEC,
                        (real) realticks / CLOCKS_PER_SEC);
}

static int C_wait(TCL_CMDARGS) {
        unsigned long stopTime = -1;
        char *usage = "wait [<max-time>]";
        if (argc > 1 && !strcmp(argv[1], "-h"))
                return commandHelp(argv[0]);
        if (argc > 2) return usageError(argv[0], usage);

        if (argc == 2) {
                sscanf(argv[1], "%lu", &stopTime);
                if (stopTime == 0) return haltWaiting();
        }
        startTask(WAITING);
        if (stopTime != -1) stopTime = getTime() + stopTime * 1e3;

        while ((stopTime == -1 || getTime() < stopTime) && !smartUpdate(TRUE))
                eval("after 50");
        stopTask(WAITING);
        return TCL_OK;
}

static int C_signal(TCL_CMDARGS) {
        int code;
        char *usage = "signal [<code>]"; 
        if (argc > 1 && !strcmp(argv[1], "-h"))
                return commandHelp(argv[0]);
        if (argc > 2) return usageError(argv[0], usage);
        if (argc == 1)
                return result("Useful codes are:\n"
                                "INT   interrupts ongoing work or does a soft quit\n"
                                "USR1  runs the sigUSR1Proc\n"
                                "USR2  runs the sigUSR2Proc\n"
                                "QUIT, KILL, ABRT, ALRM, and TERM all kill the process.\n"
                                "Any other code can be produced by specifying the "
                                "integer value.");
        if (!strcmp(argv[1], "INT")) code = SIGINT;
        else if (!strcmp(argv[1], "USR1")) code = SIGUSR1;
        else if (!strcmp(argv[1], "USR2")) code = SIGUSR2;
        else if (!strcmp(argv[1], "QUIT")) code = SIGQUIT;
        else if (!strcmp(argv[1], "KILL")) code = SIGKILL;
        else if (!strcmp(argv[1], "ABRT")) code = SIGABRT;
        else if (!strcmp(argv[1], "ALRM")) code = SIGALRM;
        else if (!strcmp(argv[1], "TERM")) code = SIGTERM;
        else code = atoi(argv[1]);
        if (code <= 0)
                return warning("%s: unrecognized code: %s", argv[0], argv[1]);
        raise(code);
        return TCL_OK;
}


/*****************************************************************************/

static int C_source(TCL_CMDARGS) {
        int val = TCL_OK;
        Tcl_Obj *filename;
        Tcl_Obj *message;
        char *olddir;
        char *usage = "source <script file>";
        String script;
        if (argc > 1 && !strcmp(argv[1], "-h"))
                return commandHelp(argv[0]);

        if (argc != 2) return usageError(argv[0], usage);

        /* olddir becomes the original directory */
        eval("pwd");
        olddir = copyString(Tcl_GetStringResult(interp));
        if (eval("cd [file dirname \"%s\"]; file tail \"%s\"", argv[1], argv[1])) 
                return TCL_ERROR;
        filename = Tcl_GetObjResult(interp);
        Tcl_IncrRefCount(filename);

        /* actually evaluate the script */
        script = newString(1024);
        if ((val = readFileIntoString(script, Tcl_GetStringFromObj(filename, NULL)))
                        == TCL_OK) {
                SourceDepth++;
                val = Tcl_Eval(interp, script->s);
                SourceDepth--;
        }
        freeString(script);

        /* now message is the return value of the script */
        message = Tcl_GetObjResult(interp);
        Tcl_IncrRefCount(message);

        eval("cd %s\n", olddir);
        FREE(olddir);

        /* This was commented out because when Tcl sources something it can screw 
           up the user's desired path. */
        eval("set .dir [file dirname %s];"
                        "if {${.dir} != $tcl_library && ${.dir} != $tk_library} {"
                        "set _script(path) [.normalizeDir {} ${.dir}];"
                        "set _script(file) [file tail %s]}", argv[1], argv[1]);

        if (val == TCL_ERROR)
                result("in script \"%s\": %s", Tcl_GetStringFromObj(filename, NULL), 
                                Tcl_GetStringFromObj(message, NULL));
        else Tcl_SetObjResult(interp, message);

        Tcl_DecrRefCount(filename);
        Tcl_DecrRefCount(message);
        return val;
}

/*****************************************************************************/

/* These use a cached directory name so pwd is faster.  
   Hopefully this is not buggy */
char *DirName;

static int C_cd(TCL_CMDARGS) {
        flag result;
        Tcl_DString ds;
        char *dirName;
        char *usage = "cd [<directory>]";
        if (argc > 2) return usageError(argv[0], usage);
        dirName = (argc == 2) ? argv[1] : "~";
        if (Tcl_TranslateFileName(interp, dirName, &ds) == NULL) {
                return TCL_ERROR;
        }
        result = Tcl_Chdir(Tcl_DStringValue(&ds));
        FREE(DirName);
        Tcl_DStringFree(&ds);
        if (result != 0) {
                Tcl_AppendResult(interp, "couldn't change working directory to \"",
                                dirName, "\": ", Tcl_PosixError(interp), (char *) NULL);
                return TCL_ERROR;
        }
        return TCL_OK;
}

static int C_pwd(TCL_CMDARGS) {
        Tcl_DString ds;
        char *usage = "pwd";
        if (argc != 1) return usageError(argv[0], usage);
        if (DirName) return result(DirName);
        if (Tcl_GetCwd(interp, &ds) == NULL) {
                return TCL_ERROR;
        }
        DirName = copyString(Tcl_DStringValue(&ds));
        Tcl_DStringResult(interp, &ds);
        return TCL_OK;
}

static int C_verbosity(TCL_CMDARGS) {
        char *usage = "verbosity [<level>]";
        if (argc > 1 && !strcmp(argv[1], "-h"))
                return commandHelp(argv[0]);
        if (argc > 2) return usageError(argv[0], usage);
        if (argc == 2) Verbosity = atoi(argv[1]);
        if (Verbosity < 0) Verbosity = 0;
        return result("%d", Verbosity);
}

/*****************************************************************************/

typedef struct TclFile_ *TclFile;
extern void TkConsolePrint (Tcl_Interp *interp, int devId, char *buffer, long size);
extern int  TclpCloseFile (TclFile file);

#ifdef MACHINE_WINDOWS
typedef struct pipeState {
        struct PipeInfo *nextPtr;   /* Pointer to next registered pipe. */
        Tcl_Channel channel;        /* Pointer to channel structure. */
        int validMask;              /* OR'ed combination of TCL_READABLE,
                                     * TCL_WRITABLE, or TCL_EXCEPTION: indicates
                                     * which operations are valid on the file. */
        int watchMask;              /* OR'ed combination of TCL_READABLE,
                                     * TCL_WRITABLE, or TCL_EXCEPTION: indicates
                                     * which events should be reported. */
        int flags;                  /* State flags, see above for a list. */
        TclFile readFile;           /* Output from pipe. */
        TclFile writeFile;          /* Input from pipe. */
        TclFile errorFile;          /* Error output from pipe. */
        int numPids;                /* Number of processes attached to pipe. */
        Tcl_Pid *pidPtr;            /* Pids of attached processes. */
} *PipeState;
#else
typedef struct pipeState {
        Tcl_Channel channel;
        TclFile readFile;           /* Output from pipe. */
        TclFile writeFile;          /* Input to pipe. */
        TclFile errorFile;          /* Error output from pipe. */
        int numPids;                /* Number of processes attached to pipe. */
        Tcl_Pid *pidPtr;            /* Pids of attached processes. */
} *PipeState;
#endif /* MACHINE_WINDOWS */

Tcl_Channel consoleChannel = NULL, errorChannel = NULL;
PipeState   channelState = NULL;
mask        channelMode = 0;

static void closeConsoleExecChannel(Tcl_Channel channel) {
        if (!channel) return;
        if (channel == errorChannel) {
                Tcl_UnregisterChannel(Interp, channel);
                errorChannel = NULL;
        }
        if (channel == consoleChannel) {
                Tcl_Close(Interp, channel);
                consoleChannel = NULL;
                channelState = NULL;
                channelMode = 0;
                eval("consoleinterp eval stopExecFile");
        }
}

static void closeConsoleExec(Tcl_Channel channel, mask mode) {
        if (channel == consoleChannel) {
                channelMode &= ~mode;
                if (!channelMode) closeConsoleExecChannel(channel);
        } else closeConsoleExecChannel(channel);
}

static void handleConsoleExecReadable(ClientData data, int event) {
        Tcl_Channel channel = (Tcl_Channel) data;
        int bytes;
        while ((bytes = Tcl_Read(channel, Buffer, BUFFER_SIZE - 1)) > 0) {
                Buffer[bytes] = '\0';
                TkConsolePrint(Interp, (channel == errorChannel) ? TCL_STDERR : 
                                TCL_STDOUT, Buffer, bytes);

        }
        if (bytes == -1) {
                error("An error occurred on an exec channel");
                closeConsoleExec(channel, TCL_READABLE);
                return;
        }
        if (bytes == 0) {
                if (Tcl_Eof(channel)) {
                        closeConsoleExec(channel, TCL_READABLE);
                        return;
                }
        }
}

static void handleConsoleExecException(ClientData data, int event) {
        Tcl_Channel channel = (Tcl_Channel) data;
        closeConsoleExec(channel, TCL_WRITABLE);
}

static int C_consoleExec(TCL_CMDARGS) {
        static char *errorChannelName = NULL;
        char *temp;

        if (!errorChannel) {
                FREE(errorChannelName);
                if (eval("open |cat RDWR"))
                        return warning("%s: failed to open error channel", argv[0]);
                errorChannelName = copyString(Tcl_GetStringResult(interp));

                if (!(errorChannel = Tcl_GetChannel(interp, errorChannelName, NULL)))
                        return warning("%s: no error channel registered", argv[0]);

                if (Tcl_SetChannelOption(interp, errorChannel, "-blocking", "0"))
                        return TCL_ERROR;
                if (Tcl_SetChannelOption(interp, errorChannel, "-buffering", "none"))
                        return TCL_ERROR;

                Tcl_CreateChannelHandler(errorChannel, TCL_READABLE, 
                                handleConsoleExecReadable, 
                                (ClientData) errorChannel);
        }
        temp = argv[argc - 1];
        argv[argc - 1] = errorChannelName;

        if (!(consoleChannel = Tcl_OpenCommandChannel(interp, argc-1, argv+1, 
                                        TCL_STDIN | TCL_STDOUT)))
                return TCL_ERROR;
        argv[argc - 1] = temp;

        channelMode = Tcl_GetChannelMode(consoleChannel);

        if (channelMode & (TCL_READABLE | TCL_WRITABLE)) {
                if (channelMode & TCL_WRITABLE) {
                        if (Tcl_SetChannelOption(interp, consoleChannel, "-buffering", "line"))
                                return TCL_ERROR;
                }
                if (channelMode & TCL_READABLE) {
                        Tcl_DString option;
                        Tcl_DStringInit(&option);
                        if (Tcl_SetChannelOption(interp, consoleChannel, "-blocking", "0"))
                                return TCL_ERROR;
                        Tcl_GetChannelOption(interp, consoleChannel, NULL, &option);
                        Tcl_CreateChannelHandler(consoleChannel, TCL_READABLE,
                                        handleConsoleExecReadable, (ClientData) 
                                        consoleChannel);
                }
                Tcl_CreateChannelHandler(consoleChannel, TCL_EXCEPTION,
                                handleConsoleExecException, (ClientData) 
                                consoleChannel);
                channelState = (PipeState) Tcl_GetChannelInstanceData(consoleChannel);
                eval("consoleinterp eval startExecFile");
                eval("after 100 .consoleExecCleanup");
        } else closeConsoleExecChannel(consoleChannel);
        return result("");
}

static int C_consoleExecWrite(TCL_CMDARGS) {
        int bytes = strlen(argv[1]);
        if (!consoleChannel) return TCL_OK;
        if (Tcl_Write(consoleChannel, argv[1], bytes) != bytes) {
                closeConsoleExec(consoleChannel, TCL_WRITABLE);
                return warning("write failed on console exec channel");
        }
        return TCL_OK;
}

static int C_consoleExecClose(TCL_CMDARGS) {
        if (!consoleChannel) return TCL_OK;
        Tcl_Flush(consoleChannel);
        if (channelState->writeFile)
                TclpCloseFile(channelState->writeFile);
        channelState->writeFile = NULL;
        closeConsoleExec(consoleChannel, TCL_WRITABLE);
        return TCL_OK;
}

static int C_consoleExecCleanup(TCL_CMDARGS) {
        int i, status;
        Tcl_Pid pid;
        if (!consoleChannel) return TCL_OK;
        for (i = 0; i < channelState->numPids; i++) {
                pid = Tcl_WaitPid(channelState->pidPtr[i], &status, WNOHANG);
                if (!consoleChannel) return TCL_OK;
                if (pid == 0) break;
        }
        if (i == channelState->numPids) {
                closeConsoleExecChannel(consoleChannel);
                return TCL_OK;
        }
        eval("after 100 .consoleExecCleanup");
        return TCL_OK;
}


/********************************** More *************************************/

/* The isprint junk here is to force it to ignore formatting codes that appear
   in the .txt files. */
static char *printLine(char *s, int columns) {
        char *t, n = '\n', *x = Buffer;
        for (t = s; *t && *t != '\n' && (x-Buffer) < columns; t++) {
                if (isprint(*t)) *(x++) = *t;
                else if (x) x--;
        }
        TkConsolePrint(Interp, TCL_STDOUT, Buffer, x - Buffer);
        TkConsolePrint(Interp, TCL_STDOUT, &n, 1);
        if (*t && *(t+1)) return t+1;
        else return NULL;
}

static int C_more(TCL_CMDARGS) {
        int i, rows, columns;
        String S = NULL;
        char *s, code = ' ';
        char *usage = "more (<filename> | << <string>)";
        if (argc > 3 || argc == 1) return usageError(argv[0], usage);
        if (argc == 2) {
                if (strlen(argv[1]) > 256) return usageError(argv[0], usage);
                S = newString(1024);
                if (readFileIntoString(S, argv[1])) 
                        return warning("%s: couldn't open file %s", argv[0], argv[1]);
                s = S->s;
        } else {
                if (strcmp(argv[1], "<<"))
                        return usageError(argv[0], usage);
                s = argv[2];
        }
        eval("lindex [split [eval {wm geometry .}] {x+}] 0");
        Tcl_GetIntFromObj(interp, Tcl_GetObjResult(interp), &columns);
        eval("lindex [split [eval {wm geometry .}] {x+}] 1");
        Tcl_GetIntFromObj(interp, Tcl_GetObjResult(interp), &rows);

        eval("consoleinterp eval startMore");

        while (s && code != 'q') {
                switch (code) {
                        case ' ':
                                for (i = 0; i < rows - 1 && s; i++) {
                                        s = printLine(s, columns);
                                }
                                break;
                        case '-':
                                s = printLine(s, columns);
                                break;
                }

                if (s) {
                        TkConsolePrint(Interp, TCL_STDERR, "--More--", 8);
                        eval("set .moreCode {}");
                        eval("vwait .moreCode");
                        eval("set .moreCode");
                        code = Tcl_GetStringResult(interp)[0];
                        eval("consoleinterp eval removeMoreMessage");
                }
        }
        freeString(S);

        eval("consoleinterp eval stopMore");
        return TCL_OK;
}

/*****************************************************************************/

//extern void Tk_InitConsoleChannels(Tcl_Interp *interp);
//extern flag Tk_CreateConsoleWindow(Tcl_Interp *interp);

flag createConsole(flag firstTime) {
        Tk_InitConsoleChannels(Interp);
        if (Tk_CreateConsoleWindow(Interp))
                return warning("viewConsole: Tk_CreateConsoleWindow failed: %s", 
                                Tcl_GetStringResult(Interp));
        if (firstTime) {
                createCommand(C_more, "more");
                createCommand(C_more, "less");
                createCommand(C_consoleExec,        ".consoleExec");
                createCommand(C_consoleExecWrite,   ".consoleExecWrite");
                createCommand(C_consoleExecClose,   ".consoleExecClose");
                createCommand(C_consoleExecCleanup, ".consoleExecCleanup");
        }
        Console = TRUE;
        return TCL_OK;
}

/*****************************************************************************/

static int C_signalHalt(TCL_CMDARGS) {
        signalHalt();
        return TCL_OK;
}

static int C_configureDisplay(TCL_CMDARGS) {
        return configureDisplay();
}

/*****************************************************************************/

static void registerShellCommands(void) {
        registerSynopsis("help",     "prints this list or explains a command");
        registerSynopsis("manual",   "opens the Lens manual in a web browser");
        registerSynopsis("set",      "creates and sets the value of a variable");
        registerSynopsis("unset",    "removes variables");
        registerSynopsis("alias",    "creates or prints a command alias");
        registerSynopsis("unalias",  "deletes a command alias");
        registerSynopsis("index",    "creates a .tclIndex file storing the locations of commands");
        registerSynopsis("history",  "manipulates the command history list");
        registerSynopsis("exec",     "runs a subprocess");
        registerSynopsis("expr",     "performs a mathematical calculation");
        registerSynopsis("open",     "opens a Tcl channel");
        registerSynopsis("close",    "closes a Tcl channel");
        registerSynopsis("puts",     "prints a string to standard output or a Tcl channel");
        registerSynopsis("ls",       "lists the contents of a directory");
        registerCommand(C_cd, "cd",  "changes the current directory");
        registerCommand(C_pwd, "pwd","returns the current directory");
        createCommand(C_beep, "beep");
        registerCommand(C_seed, "seed",
                        "seeds the random number generator");
        registerCommand(C_getSeed, "getSeed",
                        "returns the last seed used on the random number generator");
        registerCommand(C_rand, "rand",
                        "returns a random real number in a given range");
        registerCommand(C_randInt, "randInt",
                        "returns a random integer in a given range");
        registerSynopsis("pid",      "returns the current process id");
#ifndef MACHINE_WINDOWS
        registerCommand(C_nice, "nice",
                        "increments or returns the process's priority");
#endif /* MACHINE_WINDOWS */
        registerCommand(C_time, "time",
                        "runs a command and returns the time elapsed");
        registerSynopsis("glob",     "performs file name lookup");
        registerCommand(C_signal, "signal",
                        "raises a signal in the current process");
        registerCommand(NULL, "", "");

        registerSynopsis("source",   "runs a script file");
        registerSynopsis("eval",     "performs an evaluation pass on a command");
        registerSynopsis("if",       "conditional test");
        registerSynopsis("for",      "for loop");
        registerSynopsis("while",    "while loop");
        registerSynopsis("foreach",  "loops over the elements in a list");
        registerSynopsis("repeat",   "loops a specified number of times");
        registerSynopsis("switch",   "switch statement");
        registerSynopsis("return",   "returns from the current procedure or script");
        registerCommand(C_wait, "wait",
                        "stops the interactive shell for batch jobs");
        registerCommand(C_verbosity, "verbosity",
                        "check or set how verbose Lens is (mostly during training)");
#ifdef MACHINE_WINDOWS
        if (Batch) createCommand(C_exit, "exit");
#endif
        registerSynopsis("exit",     "exits from the simulator");
        createCommand(C_signalHalt, ".signalHalt");
        createCommand(C_configureDisplay, ".configureDisplay");
}

/*****************************************************************************/
void registerCommands(void) {
        registerNetworkCommands();
        registerCommand(NULL, "", "");

        registerConnectCommands();
        registerCommand(NULL, "", "");

        registerExampleCommands();
        registerCommand(NULL, "", "");

        registerTrainingCommands();
        registerCommand(NULL, "", "");

        registerObjectCommands();
        registerCommand(NULL, "", "");

        registerViewCommands();
        registerCommand(NULL, "", "");

        registerDisplayCommands();
        registerCommand(NULL, "", "");

        registerGraphCommands();
        registerCommand(NULL, "", "");

        registerParallelCommands();
        registerCommand(NULL, "", "");

        registerShellCommands();
        createCommand(C_source, "source");

        Tcl_LinkVar(Interp, ".sourceDepth", (char *) &SourceDepth,
                        TCL_LINK_INT);
}
