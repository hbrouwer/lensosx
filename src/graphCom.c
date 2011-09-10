/* graphCom.c */

#include <string.h>

#include "system.h"
#include "util.h"
#include "type.h"
#include "network.h"
#include "command.h"
#include "control.h"
#include "graph.h"

int C_graph(TCL_CMDARGS) {
        Graph G;
        char *usage = "graph [create | list] | [delete | refresh | update | store | clear |\n\thide | show] <graph-list>";
        if (argc == 2 && !strcmp(argv[1], "-h")) 
                return commandHelp(argv[0]);
        /* if (Batch) return result("%s is not available in batch mode", argv[0]); */
        if (argc < 2) return usageError(argv[0], usage);
        if (subString(argv[1], "create", 2)) {
                if (argc != 2) return usageError(argv[0], usage);
                if (!(G = createGraph())) return TCL_ERROR;
                return result("%d", G->num);
        } else if (subString(argv[1], "list", 1)) {
                if (argc != 2) return usageError(argv[0], usage);
                {
                        char gname[16];
                        String graphs = newString(64);
                        FOR_EACH_GRAPH({
                                        sprintf(gname, "%d ", G->num);
                                        stringAppend(graphs, gname);
                                        });
                        if (graphs->numChars) graphs->s[--graphs->numChars] = '\0';
                        result(graphs->s);
                        freeString(graphs);
                }
        } else {
                if (argc != 3) return usageError(argv[0], usage);
                FOR_EACH_GRAPH_IN_LIST(argv[2], {
                                if (subString(argv[1], "delete", 1)) {
                                deleteGraph(G);
                                } else if (subString(argv[1], "refresh", 1)) {
                                drawLater(G);
                                refreshPropsLater(G);
                                } else if (subString(argv[1], "update", 1)) {
                                if (G->updateOn) updateGraph(G);
                                } else if (subString(argv[1], "store", 2)) {
                                storeGraph(G);
                                } else if (subString(argv[1], "clear", 1)) {
                                clearGraph(G);
                                } else if (subString(argv[1], "hide", 1)) {
                                hideGraph(G);
                                } else if (subString(argv[1], "show", 2)) {
                                showGraph(G);
                                }
                                });
        }
        return TCL_OK;
}

int C_trace(TCL_CMDARGS) {
        Graph G;
        Trace T;
        char *usage = "trace create <graph> [<object>] | list <graph> | [delete | store | clear] <graph> <trace-list>";
        if (argc == 2 && !strcmp(argv[1], "-h")) 
                return commandHelp(argv[0]);
        /* if (Batch) return result("%s is not available in batch mode", argv[0]); */
        if (argc < 2) return usageError(argv[0], usage);
        if (!(G = lookupGraph(atoi(argv[2])))) 
                return warning("%s: graph \"%s\" doesn't exist", argv[0], argv[2]);
        if (subString(argv[1], "create", 2)) {
                if (argc != 3 && argc != 4) return usageError(argv[0], usage);
                if (argc == 4) {
                        if (!(T = createTrace(G, argv[3]))) return TCL_ERROR;
                } else {
                        if (!(T = createTrace(G, ""))) return TCL_ERROR;
                }
                return result("%d", T->num);
        } else if (subString(argv[1], "list", 1)) {
                if (argc != 3) return usageError(argv[0], usage);
                {
                        char tname[16];
                        String traces = newString(64);
                        FOR_EACH_TRACE(G, {
                                        sprintf(tname, "%d ", T->num);
                                        stringAppend(traces, tname);
                                        });
                        if (traces->numChars) traces->s[--traces->numChars] = '\0';
                        result(traces->s);
                        freeString(traces);
                }
        } else {
                if (argc != 4) return usageError(argv[0], usage);
                FOR_EACH_TRACE_IN_LIST(G, argv[3], {
                                if (subString(argv[1], "delete", 1)) {
                                deleteTrace(T);
                                } else if (subString(argv[1], "store", 1)) {
                                storeTrace(T);
                                } else if (subString(argv[1], "clear", 2)) {
                                clearTrace(T);
                                }
                                });
        }
        return TCL_OK;
}

int C_graphObject(TCL_CMDARGS) {
        Graph G;
        int arg = 2;
        mask updates = ON_REPORT;
        char *objects;
        char *usage = "graphObject [<object-list> [-updates <update-rate>]]";
        if (argc == 2 && !strcmp(argv[1], "-h")) 
                return commandHelp(argv[0]);
        /* if (Batch) return result("%s is not available in batch mode", argv[0]); */
        for (; arg < argc && argv[arg][0] == '-'; arg++) {
                switch (argv[arg][1]) {
                        case 'u': 
                                if (++arg >= argc) return usageError(argv[0], usage);
                                if (lookupTypeMask(argv[arg], UPDATE_SIGNAL, &updates))
                                        return warning("%s: unknown update signal: %s\n", argv[0], argv[arg]);
                                break;
                        default: return usageError(argv[0], usage);
                }
        }
        if (arg < argc) return usageError(argv[0], usage);
        if (!(G = createGraph())) return TCL_ERROR;
        G->updateOn = updates;
        objects = (argc > 1) ? argv[1] : "error";
        FOR_EACH_STRING_IN_LIST(objects, {
                        if (!createTrace(G, S->s)) return TCL_ERROR;
                        });
        return result("%d", G->num);
}

int C_exportGraph(TCL_CMDARGS) {
        Graph G;
        int arg;
        flag labels = FALSE, gnuplot = FALSE;
        char *usage = "exportGraph <graph> <file-name> [-labels | -gnuplot]";
        if (argc < 3) return usageError(argv[0], usage);
        if (!(G = lookupGraph(atoi(argv[1])))) 
                return warning("Graph %s doesn't exist", argv[1]);
        for (arg = 3; arg < argc && argv[arg][0] == '-'; arg++) {
                switch (argv[arg][1]) {
                        case 'l':
                                labels = TRUE;
                                break;
                        case 'g':
                                gnuplot = TRUE;
                                break;
                        default: return usageError(argv[0], usage);
                }
        }
        if (arg < argc) return usageError(argv[0], usage);
        return exportGraphData(G, argv[2], labels, gnuplot);
}

int C_drawAllTraceProps(TCL_CMDARGS) {
        Graph G;
        if (argc != 2) return warning("%s was called improperly", argv[0]);
        if (!(G = lookupGraph(atoi(argv[1])))) 
                return warning("Graph %s doesn't exist", argv[1]);
        return drawTraceProps(G);
}

int C_setGraphSize(TCL_CMDARGS) {
        Graph G;
        int w, h;
        if (argc != 4) return warning("%s was called improperly", argv[0]);
        if (!(G = lookupGraph(atoi(argv[1])))) 
                return warning("Graph %s doesn't exist", argv[1]);
        w = atoi(argv[2]);
        h = atoi(argv[3]);
        if (h == G->height && w == G->width) return TCL_OK;
        if (h == 1 && w == 1) return TCL_OK;
        G->height = h;
        G->width = w;
        return drawLater(G);
}

int C_colorName(TCL_CMDARGS) {
        real H, S, B;
        char color[32];
        if (argc != 4) return warning("%s was called improperly", argv[0]);
        H = ator(argv[1]);
        S = ator(argv[2]);
        B = ator(argv[3]);
        colorName(color, H, S, B);
        return result(color);
}

void registerGraphCommands(void) {

        registerCommand(C_graphObject, "graphObject", 
                        "opens a graph of the value of an object or a command");
        registerCommand(C_graph, "graph", 
                        "creates, deletes, or manipulates graphs");
        registerCommand(C_exportGraph, "exportGraph",
                        "export the data in a graph to a file");
        registerCommand(C_trace, "trace",
                        "creates, deletes, or manipulates traces within a graph");

        createCommand(C_drawAllTraceProps, ".drawAllTraceProps");
        createCommand(C_setGraphSize, ".setGraphSize");
        createCommand(C_colorName, ".colorName");
}
