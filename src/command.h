/* command.h */

#ifndef COMMAND_H
#define COMMAND_H

#define TCL_CMDARGS ClientData data, Tcl_Interp *interp, int argc, char *argv[]

extern flag usageError(char *command, char *usage);
extern flag commandHelp(char *command);
extern void registerCommands(void);

extern void registerNetworkCommands(void);
extern void registerConnectCommands(void);
extern void registerExampleCommands(void);
extern void registerTrainingCommands(void);
extern void registerObjectCommands(void);
extern void registerViewCommands(void);
extern void registerDisplayCommands(void);
extern void registerGraphCommands(void);
extern void registerParallelCommands(void);

extern flag createConsole(flag firstTime);

extern int Verbosity;

#endif /* COMMAND_H */
