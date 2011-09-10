/* networkCom.c */

#include <string.h>

#include "system.h"
#include "util.h"
#include "type.h"
#include "network.h"
#include "connect.h"
#include "command.h"
#include "control.h"
#include "act.h"
#include "display.h"

int C_addNet(TCL_CMDARGS) {
        int arg, timeIntervals = 1, ticksPerInterval = 1, units, hiddenNum, 
            inputNum, outputNum, biasNum, numTypes;
        unsigned int typeClass[MAX_TYPES], typeMode[MAX_TYPES], groupClass = GROUP;
        mask netType = 0, type[MAX_TYPES], linkType, baseType, elmanType = ELMAN,
             toggleType = TOGGLE_TYPE;
        flag isElman;
        Group last, this, context;
        char *typeName;
        String name;

        char *usage = "addNet <network> [-intervals <timeIntervals> |\n"
                "\t-ticks <ticksPerInterval>] [<net-type>]*\n"
                "\t[<num-units> [[+|-]<group-type>]*]*";
        if (argc == 2 && !strcmp(argv[1], "-h")) 
                return commandHelp(argv[0]);
        if (argc < 2)
                return usageError(argv[0], usage);
        if (!argv[1][0])
                return warning("%s: network must have a real name", argv[0]);
        if (unsafeToRun(argv[0], NETWORK_TASKS)) return TCL_ERROR;

        for (arg = 2; arg < argc && argv[arg][0] == '-'; arg++) {
                switch (argv[arg][1]) {
                        case 'i': 
                                if (++arg >= argc) return usageError(argv[0], usage);
                                timeIntervals = atoi(argv[arg]);
                                break;
                        case 't': 
                                if (++arg >= argc) return usageError(argv[0], usage);
                                ticksPerInterval = atoi(argv[arg]);
                                break;
                        default: return usageError(argv[0], usage);
                }
        }

        for (; arg < argc; arg++) {
                if (isInteger(argv[arg])) break;
                if (lookupTypeMask(argv[arg], NETWORK, &baseType)) {
                        warning("%s: invalid network type: \"%s\"\n"
                                        "Try one of the following:\n", argv[0], argv[arg]);
                        printTypes("Network Type", NETWORK, ~0);
                        return TCL_ERROR;
                }
                netType |= baseType;
        }

        if (addNet(argv[1], netType, timeIntervals, ticksPerInterval))
                return TCL_ERROR;

        last = NULL;
        hiddenNum = inputNum = outputNum = 1;
        biasNum = 2;
        name = newString(32);
        while (arg < argc) {
                if ((units = atoi(argv[arg++])) <= 0) return usageError(argv[0], usage);
                numTypes = baseType = 0;
                isElman = FALSE;
                for (; arg < argc && !isInteger(argv[arg]); arg++) {
                        typeName = argv[arg];
                        if (typeName[0] == '+') {
                                typeMode[numTypes] = ADD_TYPE;
                                typeName++;
                        } else if (typeName[0] == '-') {
                                typeMode[numTypes] = REMOVE_TYPE;
                                typeName++;
                        } else {
                                typeMode[numTypes] = TOGGLE_TYPE;
                        }
                        if (lookupGroupType(typeName, typeClass + numTypes, type + numTypes))
                                return TCL_ERROR;
                        if (typeClass[numTypes] == GROUP && type[numTypes] == ELMAN) {
                                isElman = TRUE;
                        } else {
                                if (typeClass[numTypes] == GROUP) baseType |= type[numTypes];
                                numTypes++;
                        }
                }

                if (isElman) {
                        if (baseType & INPUT) {
                                if (inputNum > 1) sprintf(name->s, "inContext%d", inputNum);
                                else sprintf(name->s, "context");
                        } else if (baseType & OUTPUT) {
                                if (outputNum > 1) sprintf(name->s, "outContext%d", outputNum);
                                else sprintf(name->s, "context");
                        } else {
                                if (hiddenNum > 1) sprintf(name->s, "context%d", hiddenNum);
                                else sprintf(name->s, "context");
                        }
                        if (addGroup(name->s, 1, &groupClass, &elmanType, &toggleType, units))
                                return TCL_ERROR;
                        context = lookupGroup(name->s);
                } else context = NULL;

                if (baseType & INPUT || 
                                (Net->numGroups == 1 && !(baseType & FIXED_MASKS))) {
                        type[numTypes] = INPUT;
                        typeClass[numTypes] = GROUP;
                        typeMode[numTypes++] = TOGGLE_TYPE;
                        if (inputNum > 1) sprintf(name->s, "input%d", inputNum);
                        else sprintf(name->s, "input");
                        inputNum++;
                }
                else if (baseType & OUTPUT  || 
                                (arg == argc && !(baseType & FIXED_MASKS))) {
                        type[numTypes] = OUTPUT;
                        typeClass[numTypes] = GROUP;
                        typeMode[numTypes++] = TOGGLE_TYPE;
                        if (outputNum > 1) sprintf(name->s, "output%d", outputNum);
                        else sprintf(name->s, "output");
                        outputNum++;
                }
                else if (baseType & BIAS) {
                        sprintf(name->s, "bias%d", biasNum++);
                }
                else {
                        if (hiddenNum > 1)
                                sprintf(name->s, "hidden%d", hiddenNum);
                        else sprintf(name->s, "hidden");
                        hiddenNum++;
                }

                if (addGroup(name->s, numTypes, typeClass, type, typeMode, units))
                        return TCL_ERROR;
                this = lookupGroup(name->s);

                if (context) {
                        elmanConnect(this, context);
                        if (registerLinkType(context->name, &linkType)) return TCL_ERROR;
                        fullConnectGroups(context, this, linkType, 1.0, NaN, NaN);
                }

                if (last) {
                        if (registerLinkType(last->name, &linkType)) return TCL_ERROR;
                        fullConnectGroups(last, this, linkType, 1.0, NaN, NaN);
                }

                last = this;
        }
        Net->resetNet(TRUE);
        freeString(name);
        return signalNetStructureChanged();
}

int C_optimizeNet(TCL_CMDARGS) {
        char *usage = "optimizeNet";
        if (argc == 2 && !strcmp(argv[1], "-h")) 
                return commandHelp(argv[0]);
        if (argc != 1) return usageError(argv[0], usage);
        if (!Net) return warning("%s: no current network", argv[0]);
        if (Net->type & OPTIMIZED) return warning("%s: network already optimized");
        return optimizeNet();
}

int C_useNet(TCL_CMDARGS) {
        int n;
        Network N;

        char *usage = "useNet [<network>]";
        if (argc == 2 && !strcmp(argv[1], "-h")) 
                return commandHelp(argv[0]);
        if (argc > 2)
                return usageError(argv[0], usage);
        /* If no arguments, return a list of the networks */
        if (argc == 1) {
                for (n = 0; n < Root->numNetworks; n++)
                        append("\"%s\" ", Root->net[n]->name);
                return TCL_OK;
        }

        if (unsafeToRun(argv[0], NETWORK_TASKS)) return TCL_ERROR;
        /* Otherwise, lookup the specified net and use it */
        if (!(N = lookupNet(argv[1])))
                return warning("%s: network \"%s\" doesn't exist", argv[0], argv[1]);

        if (useNet(N)) return TCL_ERROR;
        return result("%s", Net->name);
}

int C_addGroup(TCL_CMDARGS) {
        int arg, numUnits, numTypes = 0;
        unsigned int typeClass[MAX_TYPES], typeMode[MAX_TYPES];
        mask type[MAX_TYPES];
        char *group, *typeName;

        char *usage = "addGroup <group> <num-units> [[+|-]<group-type>]*";
        if (argc == 2 && !strcmp(argv[1], "-h")) 
                return commandHelp(argv[0]);
        if (argc < 3)
                return usageError(argv[0], usage);
        if (!Net) return warning("%s: no current network", argv[0]);
        if (unsafeToRun(argv[0], NETWORK_TASKS)) return TCL_ERROR;
        if (!argv[1][0])
                return warning("%s: group must have a real name", argv[0]);

        if (lookupGroup((group = argv[1])))
                return warning("%s: group \"%s\" already exists in the current "
                                "network", argv[0], group);
        if ((numUnits = atoi(argv[2])) <= 0)
                return warning("%s: group must have at least one unit", argv[0]);
        for (arg = 3; arg < argc; arg++, numTypes++) {
                typeName = argv[arg];
                if (typeName[0] == '+') {
                        typeMode[numTypes] = ADD_TYPE;
                        typeName++;
                } else if (typeName[0] == '-') {
                        typeMode[numTypes] = REMOVE_TYPE;
                        typeName++;
                } else {
                        typeMode[numTypes] = TOGGLE_TYPE;
                }
                if (lookupGroupType(typeName, typeClass + numTypes, type + numTypes))
                        return TCL_ERROR;
        }

        if (addGroup(group, numTypes, typeClass, type, typeMode, numUnits)) 
                return TCL_ERROR;
        if (signalNetStructureChanged()) return TCL_ERROR;
        return result("group(%d)", Net->numGroups - 1);
}


int C_groupType(TCL_CMDARGS) {
        int arg, numTypes = 0;
        unsigned int typeClass[MAX_TYPES], typeMode[MAX_TYPES];
        mask type[MAX_TYPES];
        Group H = NULL;
        char *typeName;

        /* char *usage = "groupType [(<group> | <group-list> ([+|-]<group-type>)*)]"; */
        if (argc == 2 && !strcmp(argv[1], "-h")) 
                return commandHelp(argv[0]);
        if (argc == 1) {
                printGroupTypes();
                return TCL_OK;
        }
        if (!Net) return warning("%s: no current network", argv[0]);

        if (argc == 2) {
                if (!(H = lookupGroup(argv[1])))
                        return warning("%s: group \"%s\" doesn't exist", argv[0], argv[1]);
                printGroupType(H);
                return TCL_OK;
        }

        for (arg = 2; arg < argc; arg++, numTypes++) {
                typeName = argv[arg];
                if (typeName[0] == '+') {
                        typeMode[numTypes] = ADD_TYPE;
                        typeName++;
                } else if (typeName[0] == '-') {
                        typeMode[numTypes] = REMOVE_TYPE;
                        typeName++;
                } else {
                        typeMode[numTypes] = TOGGLE_TYPE;
                        if (typeName[0] == '~') typeName++;
                }
                if (lookupGroupType(typeName, typeClass + numTypes, type + numTypes))
                        return TCL_ERROR;
        }

        FOR_EACH_GROUP_IN_LIST(argv[1], {
                        if (setGroupType(G, numTypes, typeClass, type, typeMode)) return TCL_ERROR;
                        H = G;
                        });

        if (signalNetStructureChanged()) return TCL_ERROR;
        if (H) printGroupType(H);
        return TCL_OK;
}

int C_changeGroupType(TCL_CMDARGS) {
        int arg, numTypes = 0;
        unsigned int typeClass[MAX_TYPES], typeMode[MAX_TYPES];
        mask type[MAX_TYPES];
        Group H = NULL;
        char *typeName;
        flag wasOutput, wasInput;

        char *usage = "changeGroupType <group-list> [<group-type> | +<group-type> |"
                "\n\t-<group-type>]*";
        if (argc == 2 && !strcmp(argv[1], "-h")) 
                return commandHelp(argv[0]);
        if (argc < 2) return usageError(argv[0], usage);
        if (!Net) return warning("%s: no current network", argv[0]);

        if (argc == 2) {
                if (!(H = lookupGroup(argv[1])))
                        return warning("%s: group \"%s\" doesn't exist", argv[0], argv[1]);
                printGroupType(H);
                return TCL_OK;
        }

        for (arg = 2; arg < argc; arg++, numTypes++) {
                typeName = argv[arg];
                if (typeName[0] == '+') {
                        typeMode[numTypes] = ADD_TYPE;
                        typeName++;
                } else if (typeName[0] == '-') {
                        typeMode[numTypes] = REMOVE_TYPE;
                        typeName++;
                } else {
                        typeMode[numTypes] = TOGGLE_TYPE;
                        if (typeName[0] == '~') typeName++;
                }
                if (lookupGroupType(typeName, typeClass + numTypes, type + numTypes))
                        return TCL_ERROR;
        }

        FOR_EACH_GROUP_IN_LIST(argv[1], {
                        wasOutput = G->type & OUTPUT; wasInput = G->type & INPUT;
                        for (arg = 0; arg < numTypes; arg++)
                        changeGroupType(G, typeClass[arg], type[arg], typeMode[arg]);
                        if (finalizeGroupType(G, wasOutput, wasInput)) return TCL_ERROR;
                        H = G;
                        });

        if (signalNetStructureChanged()) return TCL_ERROR;
        if (H) printGroupType(H);
        return TCL_OK;
}

int C_orderGroups(TCL_CMDARGS) {
        char *usage = "orderGroups (<group>)*";
        if (argc == 2 && !strcmp(argv[1], "-h")) 
                return commandHelp(argv[0]);
        if (argc == 1)
                return usageError(argv[0], usage);
        if (!Net) return warning("%s: no current network", argv[0]);

        if (orderGroups(argc - 1, argv + 1)) return TCL_ERROR;
        return signalNetStructureChanged();
}

int C_deleteNets(TCL_CMDARGS) {
        Network N;
        flag code;
        char *list;
        String netName;
        /* char *usage = "deleteNets [<network-list>]"; */
        if (argc == 2 && !strcmp(argv[1], "-h")) 
                return commandHelp(argv[0]);

        if (argc == 1) {
                if (unsafeToRun(argv[0], NETWORK_TASKS)) return TCL_ERROR;
                if (Net) return deleteNet(Net);
                else return warning("%s: no current network", argv[0]);
        }

        netName = newString(32);
        if (!strcmp(argv[1], "*")) {
                if (unsafeToRun(argv[0], NETWORK_TASKS)) return TCL_ERROR;
                return deleteAllNets();
        } else for (list = argv[1]; (list = readName(list, netName, &code));) {
                if (code == TCL_ERROR) return TCL_ERROR;
                if (!(N = lookupNet(netName->s)))
                        return warning("%s: network \"%s\" doesn't exist", argv[0], netName->s);
                if (N == Net && unsafeToRun(argv[0], NETWORK_TASKS)) return TCL_ERROR;
                deleteNet(N);
        }
        freeString(netName);
        return TCL_OK;
}

int C_deleteGroups(TCL_CMDARGS) {
        char *usage = "deleteGroups <group-list>";

        if (argc == 2 && !strcmp(argv[1], "-h")) 
                return commandHelp(argv[0]);
        if (!Net) return warning("%s: no current network", argv[0]);
        if (unsafeToRun(argv[0], NETWORK_TASKS)) return TCL_ERROR;
        if (argc != 2) return usageError(argv[0], usage);

        if (!strcmp(argv[1], "*")) {
                FOR_EACH_GROUP_BACK(if (!(G->type & BIAS) && deleteGroup(G)) 
                                return TCL_ERROR);
        } else FOR_EACH_GROUP_IN_LIST(argv[1], if (deleteGroup(G)) return TCL_ERROR);
        return signalNetStructureChanged();
}

int C_setTime(TCL_CMDARGS) {
        int timeIntervals, ticksPerInterval, historyLength = -1, arg;
        flag setDT = -1;
        char *usage = "setTime [-intervals <timeIntervals> |\n"
                "\t-ticks <ticksPerInterval> | -history <historyLength> | -dtfixed]";
        if (argc == 2 && !strcmp(argv[1], "-h")) 
                return commandHelp(argv[0]);
        /*  if (argc > 4)
            return usageError(argv[0], usage); */
        if (!Net) return warning("%s: no current network", argv[0]);

        ticksPerInterval = Net->ticksPerInterval;

        if (argc == 1)
                return result("%d %d %d", Net->timeIntervals, 
                                Net->ticksPerInterval, Net->historyLength);
        timeIntervals = Net->timeIntervals;
        ticksPerInterval = Net->ticksPerInterval;

        for (arg = 1; arg < argc && argv[arg][0] == '-'; arg++) {
                switch (argv[arg][1]) {
                        case 'i': 
                                if (++arg >= argc) return usageError(argv[0], usage);
                                timeIntervals = atoi(argv[arg]);
                                break;
                        case 't': 
                                if (++arg >= argc) return usageError(argv[0], usage);
                                ticksPerInterval = atoi(argv[arg]);
                                if (setDT == -1) setDT = TRUE;
                                break;
                        case 'h':
                                if (++arg >= argc) return usageError(argv[0], usage);
                                historyLength = atoi(argv[arg]);
                                break;
                        case 'd':
                                setDT = FALSE;
                                break;
                        default: return usageError(argv[0], usage);
                }
        }
        if (arg != argc) return usageError(argv[0], usage);
        if (setDT == -1) setDT = FALSE;

        if (setTime(Net, timeIntervals, ticksPerInterval, historyLength, setDT))
                return TCL_ERROR;
        if (signalNetStructureChanged()) return TCL_ERROR;
        return result("%d %d %d", Net->timeIntervals, 
                        Net->ticksPerInterval, Net->historyLength);
}

#ifdef JUNK
int C_setHistoryLength(TCL_CMDARGS) {
        int historyLength = -1;
        char *usage = "setHistoryLength [<historyLength>]";
        if (argc == 2 && !strcmp(argv[1], "-h")) 
                return commandHelp(argv[0]);
        if (argc > 2)
                return usageError(argv[0], usage);
        if (!Net) return warning("%s: no current network", argv[0]);

        if (argc > 1)
                historyLength = atoi(argv[1]);
        if (setHistoryLength(Net, historyLength))
                return TCL_ERROR;
        if (signalNetStructureChanged()) return TCL_ERROR;
        return TCL_OK;
}
#endif

static flag unitOffset(char *name, int *offsetp, char *command) {
        Unit U;
        int offset;

        switch (name[0]) {
                case 'i': 
                        if (subString(name, "inputDeriv", 6)) {
                                offset = OFFSET(U, inputDeriv);
                        } else {
                                offset = OFFSET(U, input);
                        }
                        break;
                case 'o': 
                        if (subString(name, "outputDeriv", 7)) {
                                offset = OFFSET(U, outputDeriv);
                        } else {      
                                offset = OFFSET(U, output); 
                        }
                        break;
                case 't': 
                        offset = OFFSET(U, target);
                        break;
                case 'e':
                        offset = OFFSET(U, externalInput);
                        break;
                default:
                        return warning("%s: unrecognized unit field: %s\n", command, name);
        }
        *offsetp = offset;
        return TCL_OK;
}

int C_copyUnitValues(TCL_CMDARGS) {
        Group from, to;
        int fromoff, tooff, arg;

        char *usage = "copyUnitValues <group1> -<field1> (<group2> | -<field2> |\n"
                "\t<group2> -<field2>)";
        if (argc == 2 && !strcmp(argv[1], "-h")) 
                return commandHelp(argv[0]);
        if (argc < 3 || argc > 5)
                return usageError(argv[0], usage);
        if (!Net) return warning("%s: no current network", argv[0]);

        if (!(from = lookupGroup(argv[1])))
                return warning("%s: group \"%s\" doesn't exist", argv[0], argv[1]);
        if (argv[2][0] != '-')
                return usageError(argv[0], usage);
        if (unitOffset(argv[2] + 1, &fromoff, argv[0])) return TCL_ERROR;

        if (argc == 5 || argv[3][0] != '-') {
                if (!(to = lookupGroup(argv[3])))
                        return warning("%s: group \"%s\" doesn't exist", argv[0], argv[3]);
                arg = 4;
        } else {
                to = from;
                arg = 3;
        }
        if (argc > arg) {
                if (unitOffset(argv[arg] + 1, &tooff, argv[0])) return TCL_ERROR;
        } else tooff = fromoff;

        return copyUnitValues(from, fromoff, to, tooff);
}

int C_resetUnitValues(TCL_CMDARGS) {
        Unit U;
        int offset;
        real value = 0.0;
        char *usage = "resetUnitValues <group-list> [-<field> [<value>]]";
        if (argc == 2 && !strcmp(argv[1], "-h")) 
                return commandHelp(argv[0]);
        if (argc < 2 || argc > 4) return usageError(argv[0], usage);
        if (!Net) return warning("%s: no current network", argv[0]);

        if (argc > 2) {
                if (argv[2][0] != '-')
                        return usageError(argv[0], usage);
                if (unitOffset(argv[2] + 1, &offset, argv[0])) return TCL_ERROR;
                if (offset == OFFSET(U, input) || offset == OFFSET(U, output) ||
                                offset == OFFSET(U, target))
                        value = NaN;
        } else {
                offset = OFFSET(U, output);
                value = NaN;
        }
        if (argc > 3) value = ator(argv[3]);

        FOR_EACH_GROUP_IN_LIST(argv[1], {
                        if (isNaN(value)) {
                        if (offset == OFFSET(U, input))
                        value = chooseValue(G->initInput, Net->initInput);
                        else 
                        value = chooseValue(G->initOutput, Net->initOutput);
                        }
                        if (setUnitValues(G, offset, value)) return TCL_ERROR;
                        if (offset == OFFSET(U, output)) cacheOutputs(G);
                        });

        return TCL_OK;
}

int C_printUnitValues(TCL_CMDARGS) {
        int code = TCL_OK, arg;
        Tcl_Channel channel;
        char message[256];
        flag append = FALSE;
        char *usage = "printUnitValues <file> <printProc> <group-list> [-append]";
        if (argc == 2 && !strcmp(argv[1], "-h")) 
                return commandHelp(argv[0]);
        if (argc < 4) return usageError(argv[0], usage);
        if (!Net) return warning("%s: no current network", argv[0]);

        for (arg = 4; arg < argc && argv[arg][0] == '-'; arg++) {
                switch (argv[arg][1]) {
                        case 'a':
                                append = TRUE; break;
                        default: return usageError(argv[0], usage);
                }
        }
        if (arg != argc) return usageError(argv[0], usage);

        if (!(channel = writeChannel(argv[1], append)))
                return warning("%s: couldn't write to the file %s", argv[0], argv[1]);
        FOR_EACH_GROUP_IN_LIST(argv[3], {
                        FOR_EACH_UNIT(G, {
                                if (eval("%s group(%d) group(%d).unit(%d)", argv[2], G->num, G->num, 
                                                U->num)) {
                                strcpy(message, Tcl_GetStringResult(Interp));
                                debug("message = [%s]\n", message);
                                code = TCL_ERROR;
                                goto abort;
                                }
                                Tcl_Write(channel, Tcl_GetStringResult(Interp), -1);
                                });
                        });  

abort:
        closeChannel(channel);
        if (code) Tcl_SetResult(Interp, message, TCL_VOLATILE);
        else result("");
        return code;
}

int C_polarity(TCL_CMDARGS) {
        char *usage = "polarity [reset | update | report] <group-list>";

        if (argc == 2 && !strcmp(argv[1], "-h")) 
                return commandHelp(argv[0]);
        if (!Net) return warning("%s: no current network", argv[0]);
        if (argc != 3) return usageError(argv[0], usage);

        if (subString(argv[1], "reset", 3)) {
                FOR_EACH_GROUP_IN_LIST(argv[2], {
                                G->polaritySum = 0.0;
                                G->polarityNum = 0;
                                });
        } else if (subString(argv[1], "update", 1)) {
                real scale;
                FOR_EACH_GROUP_IN_LIST(argv[2], {
                                scale = 1.0 / (G->maxOutput - G->minOutput);
                                FOR_EACH_UNIT(G, {
                                        real x = (U->output - G->minOutput) * scale;
                                        real d = (x <= 0 || x >= 1) ? 1.0 : 
                                        (x * LOG(x) + (1.0-x)*LOG(1.0-x)) / LOG(2) + 1.0;
                                        G->polaritySum += d;
                                        G->polarityNum++;
                                        });
                                });
        } else if (subString(argv[1], "report", 3)) {
                String S = newString(128);
                char data[128];
                FOR_EACH_GROUP_IN_LIST(argv[2], {
                                sprintf(data, "{\"%s\" %g} ", G->name, (G->polarityNum > 0) ?
                                        G->polaritySum / G->polarityNum : 0);
                                stringAppend(S, data);
                                });
                if (S->numChars) S->s[--S->numChars] = '\0';
                result(S->s);
        }
        return TCL_OK;
}

void registerNetworkCommands(void) {
        registerCommand(C_addNet, "addNet", 
                        "creates a new network and makes it the active network");
        /*  registerCommand(C_optimizeNet, "optimizeNet",
            "moves the network to a contiguous block of memory");*/
        registerCommand(C_useNet, "useNet", 
                        "activates a network or prints a list of all networks");
        registerCommand(C_addGroup, "addGroup",
                        "adds a group to the active network");
        registerCommand(C_orderGroups, "orderGroups",
                        "sets the order in which groups are updated");
        registerCommand(C_deleteNets, "deleteNets",
                        "deletes a list of networks or the active network");
        registerCommand(C_deleteGroups, "deleteGroups",
                        "deletes a list of groups from the active network");
        registerCommand(NULL, "", "");

        registerCommand(C_groupType, "groupType",
                        "sets or displays the types of a group");
        registerCommand(C_changeGroupType, "changeGroupType",
                        "adds, removes, or toggles group types");
        registerCommand(C_setTime, "setTime",
                        "sets the network's time and history parameters");
        registerCommand(C_copyUnitValues, "copyUnitValues",
                        "copies inputs, outputs, or other fields between groups");
        registerCommand(C_resetUnitValues, "resetUnitValues",
                        "resets inputs, outputs, or other fields for a group");
        registerCommand(C_printUnitValues, "printUnitValues",
                        "prints values for each unit in a list of groups to a file");
        registerCommand(C_polarity, "polarity",
                        "compute the avg polarization of unit outputs in a group");
}
