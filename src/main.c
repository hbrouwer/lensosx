/* main.c */

#include <stdio.h>
#include <math.h>
#include <string.h>
#include <unistd.h>
#include <tk.h>

#include "system.h"
#include "util.h"
#include "network.h"
#include "type.h"
#include "connect.h"
#include "command.h"
#include "control.h"
#include "display.h"
#include "canvRect.h"
#include "train.h"
#include "extension.h"

#include "sys/param.h"

#include "main.h"

#define VERSION "OSX-1.0"

int  FirstCommand, Argc;
char **Argv;
char Version[32];

void printUsage(char *progName) {
        fprintf(stderr, "usage: %s [-console | -nogui | -batch] [<script-file> | "
                        "<command>]*\a\n", progName);
        exit(1);
}

int Tcl_AppInit(Tcl_Interp *interp) {
        int isScript, arg;
        Interp = interp; /* set the global interpreter */

        if (Tcl_Init(interp) == TCL_ERROR)
                fatalError("Tcl_Init failed:\n%s\n"
                                "Have you set the LENSDIR environment variable properly?\n"
                                "If not, see Running Lens in the online manual.",
                                Tcl_GetStringResult(interp));

        if (!Batch) {
                if (Tk_Init(interp) == TCL_ERROR)
                        fatalError("Tk_Init failed: %s", Tcl_GetStringResult(interp));
        }

        Tcl_LinkVar(interp, ".GUI", (char *) &Gui, TCL_LINK_INT);
        Tcl_LinkVar(interp, ".BATCH", (char *) &Batch, TCL_LINK_INT);
        Tcl_LinkVar(interp, ".CONSOLE", (char *) &Console, TCL_LINK_INT);

        eval("set .WINDOWS 0");

        Tcl_SetVar(interp, ".RootDir", RootDir, TCL_GLOBAL_ONLY);

#ifdef ADVANCED
        Tcl_Eval(interp, "set .ADVANCED 1");
        sprintf(Version, "%s%sa", VERSION, VersionExt);
#else
        Tcl_Eval(interp, "set .ADVANCED 0");
        sprintf(Version, "%s%s", VERSION, VersionExt);
#endif /* ADVANCED */

#ifdef DOUBLE_REAL
        strcat(Version, " dbl");
#endif

        Tcl_SetVar(interp, ".Version", Version, TCL_GLOBAL_ONLY);

        sprintf(Buffer, "%s/src/shell.tcl", RootDir);
        if (Tcl_EvalFile(interp, Buffer) != TCL_OK)
                fatalError("%s\nwhile reading %s/src/shell.tcl", 
                                Tcl_GetStringResult(interp), RootDir);

        registerCommands();
        registerAlgorithms();
        createObjects();
        if (!Batch) createCanvRectType();
        if (userInit()) fatalError("user initialization failed");
        initObjects();

        if (Gui) {
                sprintf(Buffer, "%s/src/interface.tcl", RootDir);
                if (Tcl_EvalFile(interp, Buffer) != TCL_OK)
                        fatalError("%s\nwhile reading %s/src/interface.tcl", 
                                        Tcl_GetStringResult(interp), RootDir);
        } else if (!Batch) {
                if (Tcl_Eval(Interp, "wm withdraw ."))
                        fatalError("error withdrawing main window");
        }

        if (Console) {
                createConsole(1);
        }

        print(1,        "                            LENS Version %s\n"
                        "                 Copyright (C) 1998-2004  Douglas Rohde,\n"
                        "      Carnegie Mellon University, Center for the Neural Basis of "
                        "Cognition\n\n", Version);

        print(1,        "                MacOSX port by Harm Brouwer' (me@hbrouwer.eu),\n"
                        "    Daniel de Kok' (me@danieldk.eu) and Hartmut Fitz* (hartmut.fitz@gmail.com)\n"
                        "             ' University of Groningen, Center for Language and Cognition\n"
                        "             * Max Planck Institute for Psycholinguistics, Nijmegen\n\n"
             );


        eval("set _script(path) [pwd]; set _script(file) {}");

        Tcl_Eval(Interp, "update");

        /* Processing the command-line scripts and commands. */
        for (arg = FirstCommand; arg < Argc; arg++) {
                sprintf(Buffer, "[file readable {%s}]", Argv[arg]);
                Tcl_ExprBoolean(Interp, Buffer, &isScript);
                if (isScript) {
                        if (eval("source %s", Argv[arg])) error(Tcl_GetStringResult(interp));
                } else {
                        if (eval("history add {%s}", Argv[arg])) 
                                error(Tcl_GetStringResult(interp));
                        if (eval(Argv[arg])) error(Tcl_GetStringResult(interp));
                }
        }

        if (Console) Tcl_Eval(Interp, "$tcl_prompt1");

        return TCL_OK;
}


extern int TclObjCommandComplete(Tcl_Obj *cmdPtr);

/* The original has a bug that causes it to exit whenever a signal is received
   because the signal causes an error reading from stdin. 
   This version doesn't do all the argc and argv stuff because I don't need it.
   */
void Tcl_Main2(int argc, char *argv[], Tcl_AppInitProc *appInitProc)
{
        Tcl_Obj *resultPtr;
        Tcl_Obj *commandPtr = NULL;
        int code, gotPartial, tty, length;
        int exitCode = 0;
        Tcl_Channel inChannel, outChannel, errChannel;
        Tcl_Interp *interp;

        Tcl_FindExecutable(argv[0]);
        interp = Tcl_CreateInterp();

        /*
         * Set the "tcl_interactive" variable.
         */
        tty = isatty(0);
        Tcl_SetVar(interp, "tcl_interactive",
                        (tty) ? "1" : "0", TCL_GLOBAL_ONLY);

        /*
         * Invoke application-specific initialization.
         */
        if ((*appInitProc)(interp) != TCL_OK) {
                errChannel = Tcl_GetStdChannel(TCL_STDERR);
                if (errChannel) {
                        Tcl_WriteChars(errChannel,
                                        "application-specific initialization failed: ", -1);
                        Tcl_WriteObj(errChannel, Tcl_GetObjResult(interp));
                        Tcl_WriteChars(errChannel, "\n", 1);
                }
        }

        /*
         * Process commands from stdin until there's an end-of-file.  Note
         * that we need to fetch the standard channels again after every
         * eval, since they may have been changed.
         */
        commandPtr = Tcl_NewObj();
        Tcl_IncrRefCount(commandPtr);

        inChannel = Tcl_GetStdChannel(TCL_STDIN);
        outChannel = Tcl_GetStdChannel(TCL_STDOUT);
        gotPartial = 0;
        while (1) {
                if (tty) {
                        Tcl_Obj *promptCmdPtr;

                        promptCmdPtr = Tcl_GetVar2Ex(interp, (gotPartial ? "tcl_prompt2" :
                                                "tcl_prompt1"),
                                        NULL, TCL_GLOBAL_ONLY);
                        if (promptCmdPtr == NULL) {
defaultPrompt:
                                if (!gotPartial && outChannel) {
                                        Tcl_WriteChars(outChannel, "% ", 2);
                                }
                        } else {
                                code = Tcl_EvalObjEx(interp, promptCmdPtr, 0);
                                inChannel = Tcl_GetStdChannel(TCL_STDIN);
                                outChannel = Tcl_GetStdChannel(TCL_STDOUT);
                                errChannel = Tcl_GetStdChannel(TCL_STDERR);
                                if (code != TCL_OK) {
                                        if (errChannel) {
                                                Tcl_WriteObj(errChannel, Tcl_GetObjResult(interp));
                                                Tcl_WriteChars(errChannel, "\n", 1);
                                        }
                                        Tcl_AddErrorInfo(interp,
                                                        "\n    (script that generates prompt)");
                                        goto defaultPrompt;
                                }
                        }
                        if (outChannel) {
                                Tcl_Flush(outChannel);
                        }
                }
                if (!inChannel) goto done;

                length = Tcl_GetsObj(inChannel, commandPtr);

                if (length < 0) continue;

                if ((length == 0) && Tcl_Eof(inChannel) && (!gotPartial))
                        goto done;

                /*
                 * Add the newline removed by Tcl_GetsObj back to the string.
                 */
                Tcl_AppendToObj(commandPtr, "\n", 1);
                if (!TclObjCommandComplete(commandPtr)) {
                        gotPartial = 1;
                        continue;
                }

                gotPartial = 0;
                code = Tcl_RecordAndEvalObj(interp, commandPtr, 0);
                inChannel = Tcl_GetStdChannel(TCL_STDIN);
                outChannel = Tcl_GetStdChannel(TCL_STDOUT);
                errChannel = Tcl_GetStdChannel(TCL_STDERR);
                Tcl_SetObjLength(commandPtr, 0);
                if (code != TCL_OK) {
                        if (errChannel) {
                                Tcl_WriteObj(errChannel, Tcl_GetObjResult(interp));
                                Tcl_WriteChars(errChannel, "\n", 1);
                        }
                } else if (tty) {
                        resultPtr = Tcl_GetObjResult(interp);
                        Tcl_GetStringFromObj(resultPtr, &length);
                        if ((length > 0) && outChannel) {
                                Tcl_WriteObj(outChannel, resultPtr);
                                Tcl_WriteChars(outChannel, "\n", 1);
                        }
                }
        }

        /*
         * Rather than calling exit, invoke the "exit" command so that
         * users can replace "exit" with some other command to do additional
         * cleanup on exit.  The Tcl_Eval call should never return.
         */

done:
        if (commandPtr != NULL) {
                Tcl_DecrRefCount(commandPtr);
        }
        sprintf(Buffer, "exit %d", exitCode);
        Tcl_Eval(interp, Buffer);
}


int main(int argc, char *argv[]) {
        int arg = 1;

        Console = TRUE;

        while (arg < argc) {
                if        (subString(argv[arg], "-nogui",   2)) {
                        // XXX: Not sure if this is what we want ...
                        Console = FALSE;
                        Gui = FALSE;
                        Batch = TRUE;
                } else if (subString(argv[arg], "-batch",   2)) {
                        Batch = TRUE;
                        Gui = FALSE;
                        Console = FALSE;
                } else if (subString(argv[arg], "-console", 2)) {
                        Console = TRUE;
                        Batch = FALSE;
                } else if (strncmp(argv[arg], "-psn", 2) == 0) { 
                        // exploit -psn_* argument (allegedly used for
                        // AppleScript) to determine that this session
                        // is not initiated from the command line
                        
                        // XXX: No longer necessary?
                        //Console = TRUE;
                        //Batch = FALSE;
                } else if (subString(argv[arg], "-help",    2)) {
                        printUsage(argv[0]);
                } else break;
                arg++;
        }
        FirstCommand = arg;
        Argc = argc; Argv = argv;

        RootDir = (char *) malloc(MAXPATHLEN);
        realpath(argv[0], RootDir);

        char *lastSlash = rindex(RootDir, '/');
        *lastSlash = '\0';

        sprintf(Buffer, "TCL_LIBRARY=%s/../Frameworks/Tcl.framework/Resources/Scripts/", RootDir);
        if (putenv(copyString(Buffer))) fatalError("couldn't set %s", Buffer);
        sprintf(Buffer, "TK_LIBRARY=%s/../Frameworks/Tk.framework/Resources/Scripts/", RootDir);
        if (putenv(copyString(Buffer))) fatalError("couldn't set %s", Buffer);
        
        initializeSimulator();

        if (Batch) Tcl_Main2(1, argv, Tcl_AppInit);
        else Tk_Main(1, argv, Tcl_AppInit);
        return 0;   /* Needed only to prevent compiler warning. */
}
