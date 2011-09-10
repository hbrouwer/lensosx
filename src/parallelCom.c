/* parallelCom.c */

#include <string.h>

#include "system.h"
#include "util.h"
#include "type.h"
#include "network.h"
#include "command.h"
#include "control.h"
#include "train.h"
#include "parallel.h"

/* Make the port optional */
int C_startServer(TCL_CMDARGS) {
        int port = 0;
        char *usage = "startServer [<port>]";
        if (argc == 2 && !strcmp(argv[1], "-h")) 
                return commandHelp(argv[0]);
        if (argc > 2)
                return usageError(argv[0], usage);
        if (argc == 2)
                port = atoi(argv[1]);
        return startServer(port);
}

int C_stopServer(TCL_CMDARGS) {
        char *usage = "stopServer";
        if (argc == 2 && !strcmp(argv[1], "-h")) 
                return commandHelp(argv[0]);
        if (argc != 1)
                return usageError(argv[0], usage);
        return stopServer();
}

int C_startClient(TCL_CMDARGS) {
        char *myaddr;
        int port;
        char *usage = "startClient <hostname> <port> [<my-address>]";
        if (argc == 2 && !strcmp(argv[1], "-h")) 
                return commandHelp(argv[0]);
        if (argc != 3 && argc != 4)
                return usageError(argv[0], usage);

        myaddr = (argc > 3) ? argv[3] : NULL;
        port = atoi(argv[2]);
        return startClient(argv[1], port, myaddr);
}

int C_stopClient(TCL_CMDARGS) {
        char *usage = "stopClient";
        if (argc == 2 && !strcmp(argv[1], "-h")) 
                return commandHelp(argv[0]);
        if (argc != 1)
                return usageError(argv[0], usage);
        return stopClient();
}

int C_sendEval(TCL_CMDARGS) {
        int client = 0;
        char *usage = "sendEval <command> [<client>]";
        if (argc == 2 && !strcmp(argv[1], "-h")) 
                return commandHelp(argv[0]);
        if (argc != 2 && argc != 3)
                return usageError(argv[0], usage);
        if (argc == 3)
                client = atoi(argv[2]);
        return sendEvals(argv[1], client);
}

int C_clientInfo(TCL_CMDARGS) {
        int client = 0;
        char *usage = "clientInfo [<client>]";
        if (argc == 2 && !strcmp(argv[1], "-h")) 
                return commandHelp(argv[0]);
        if (argc != 1 && argc != 2)
                return usageError(argv[0], usage);
        if (argc == 2)
                client = atoi(argv[1]);
        return clientInfo(client);
}

int C_waitForClients(TCL_CMDARGS) {
        int clients = 0;
        char *usage = "waitForClients [<num-clients> [<command>]]";
        if (argc == 2 && !strcmp(argv[1], "-h")) 
                return commandHelp(argv[0]);
        if (argc > 3)
                return usageError(argv[0], usage);
        if (argc > 1) {
                clients = atoi(argv[1]);
                if (clients <= 0) 
                        return warning("%s: number of clients must be positive", 
                                        argv[0], clients);
        }
        if (argc > 2)
                return waitForClients(clients, argv[2]);
        else return waitForClients(clients, NULL);
}

int C_trainParallel(TCL_CMDARGS) {
        int arg = 1, testInterval = 0;
        flag synchronous = TRUE;
        char *usage = "trainParallel [<num-updates>] [-nonsynchronous |\n"
                "\t-report <report-interval> | -algorithm <algorithm> |\n"
                "\t-test <test-interval>]";
        if (argc == 2 && !strcmp(argv[1], "-h")) return commandHelp("trainParallel");
        if (argc > 6) return usageError("trainParallel", usage);
        if (!Net) return warning("%s: no current network", argv[0]);
        if (ParallelState != SERVER)
                return warning("trainParallel: must be the server to train in parallel");

        if (argc > 1 && isInteger(argv[1])) {
                Net->numUpdates = atoi(argv[1]);
                arg = 2;
        }

        for (; arg < argc && argv[arg][0] == '-'; arg++) {
                switch (argv[arg][1]) {
                        case 'n':
                                synchronous = FALSE;
                                break;
                        case 'r':
                                if (++arg >= argc) return usageError(argv[0], usage);
                                Net->reportInterval = atoi(argv[arg]);
                                break;
                        case 'a':
                                if (++arg >= argc) return usageError(argv[0], usage);
                                if (lookupTypeMask(argv[arg], ALGORITHM, &(Net->algorithm)))
                                        return warning("%s: unrecognized algorithm: %s", argv[0], argv[arg]);
                                break;
                        case 't':
                                if (++arg >= argc) return usageError(argv[0], usage);
                                testInterval = atoi(argv[arg]);
                                break;
                        default: return usageError(argv[0], usage);
                }
        }
        if (arg != argc) return usageError(argv[0], usage);

        return trainParallel(synchronous, testInterval);
}

void registerParallelCommands(void) {
        registerCommand(C_startServer, "startServer",
                        "makes the current process a parallel training server");
        registerCommand(C_stopServer, "stopServer",
                        "disengages all clients and stops running as a server");
        registerCommand(C_startClient, "startClient",
                        "makes the current process a parallel training client");
        registerCommand(C_stopClient, "stopClient",
                        "closes the connection with the server");
        registerCommand(C_sendEval, "sendEval",
                        "executes a command on the server or on one or all clients");
        registerSynopsis("sendObject",
                        "a shortcut for doing a local and remote setObject");
        registerCommand(C_clientInfo, "clientInfo",
                        "returns information about one or all clients");
        registerCommand(C_waitForClients, "waitForClients",
                        "executes a command when enough clients have connected");
        registerCommand(C_trainParallel, "trainParallel", 
                        "trains the network in parallel on the clients");
        Tcl_LinkVar(Interp, ".ClientsWaiting", (char *) &ClientsWaiting,
                        TCL_LINK_INT);
}
