/* connectCom.c */

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
#include "object.h"

#include "main.h"

/******************************* Connection Commands *************************/

int C_connectGroups(TCL_CMDARGS) {
        Group H;
        int arg, firstType;
        mask type = FULL, linkType, biLinkType;
        char *typeName = NULL;
        real strength = 1.0, range = NaN, mean = NaN;
        flag bidirectional = FALSE, typeSet = FALSE;
        flag (*connectProc)(Group, Group, mask, real, real, real) = 
                fullConnectGroups;

        char *usage = "connectGroups <group-list1> (<group-list>)* [-projection "
                "<proj-type> |\n\t-strength <strength> | -mean <mean> | -range <range> |\n"
                "\t-type <link-type> | -bidirectional]";
        if (argc == 2 && !strcmp(argv[1], "-h")) 
                return commandHelp(argv[0]);
        if (argc < 3) return usageError(argv[0], usage);
        if (!Net) return warning("%s: no current network", argv[0]);
        if (unsafeToRun(argv[0], NETWORK_TASKS)) return TCL_ERROR;

        for (arg = 1; arg < argc && isGroupList(argv[arg]); arg++);
        if (arg <= 2) return usageError(argv[0], usage);

        /* Read the options. */
        for (firstType = arg; arg < argc && argv[arg][0] == '-'; arg++) {
                switch (argv[arg][1]) {
                        case 'p':
                                if (++arg >= argc) return usageError(argv[0], usage);
                                if (lookupTypeMask(argv[arg], PROJECTION, &type))
                                        return warning("%s: bad projection type: %s\n", argv[0], argv[arg]);
                                if (!(connectProc = projectionProc(type)))
                                        return warning("%s: bad projection type: %s\n", argv[0], argv[arg]);
                                break;
                        case 's':
                                if (++arg >= argc) return usageError(argv[0], usage);
                                strength = atof(argv[arg]);
                                if (strength <= 0.0 || strength > 1.0)
                                        return warning("%s: strength value (%f) must be in the range (0,1]",
                                                        argv[0], strength);
                                break;
                        case 'm':
                                if (++arg >= argc) return usageError(argv[0], usage);
                                mean = ator(argv[arg]);
                                break;
                        case 'r':
                                if (++arg >= argc) return usageError(argv[0], usage);
                                range = ator(argv[arg]);
                                if (range < 0.0) return warning("%s: range must be positive", argv[0]);
                                break;
                        case 't':
                                if (++arg >= argc) return usageError(argv[0], usage);
                                typeName = argv[arg];
                                typeSet = TRUE;
                                break;
                        case 'b':
                                bidirectional = TRUE;
                                break;
                        default: return usageError(argv[0], usage);
                }
        }
        if (arg != argc) return usageError(argv[0], usage);

        if (type == ONE_TO_ONE) {
                if (isNaN(mean)) mean = 1.0;
                if (isNaN(range)) range = 0.0;
        }

        for (arg = 2; arg < firstType; arg++)
                FOR_EACH_GROUP_IN_LIST(argv[arg-1], {
                                H = G;
                                if (!typeSet) typeName = H->name;
                                if (registerLinkType(typeName, &linkType)) return TCL_ERROR;
                                FOR_EACH_GROUP_IN_LIST(argv[arg], {
                                        if (connectProc(H, G, linkType, strength, range, mean))
                                        return TCL_ERROR;
                                        if (bidirectional) {
                                        if (!typeSet) typeName = G->name;
                                        if (registerLinkType(typeName, &biLinkType)) return TCL_ERROR;
                                        if (connectProc(G, H, biLinkType, strength, range, mean))
                                        return TCL_ERROR;
                                        }
                                        });
                                });

        return signalNetStructureChanged();
}

int C_connectGroupToUnits(TCL_CMDARGS) {
        int arg, lastUnit;
        mask linkType;
        char *typeName = NULL;
        real range = NaN, mean = NaN;
        flag typeSet = FALSE;

        char *usage = "connectGroupToUnits <group-list> <unit-list> [-mean <mean> |\n"
                "\t-range <range> | -type <link-type>]";
        if (argc == 2 && !strcmp(argv[1], "-h")) 
                return commandHelp(argv[0]);
        if (argc < 3) return usageError(argv[0], usage);
        if (!Net) return warning("%s: no current network", argv[0]);
        if (unsafeToRun(argv[0], NETWORK_TASKS)) return TCL_ERROR;

        /* Skip the unit list. */
        for (arg = 3; arg < argc && lookupUnit(argv[arg]); arg++);
        lastUnit = arg - 1;

        /* Read the options. */
        for (; arg < argc && argv[arg][0] == '-'; arg++) {
                switch (argv[arg][1]) {
                        case 'm':
                                if (++arg >= argc) return usageError(argv[0], usage);
                                mean = ator(argv[arg]);
                                break;
                        case 'r':
                                if (++arg >= argc) return usageError(argv[0], usage);
                                range = ator(argv[arg]);
                                if (range < 0.0) return warning("%s: range must be positive", argv[0]);
                                break;
                        case 't':
                                if (++arg >= argc) return usageError(argv[0], usage);
                                typeName = argv[arg];
                                typeSet = TRUE;
                                break;
                        default: return usageError(argv[0], usage);
                }
        }
        if (arg != argc) return usageError(argv[0], usage);

        FOR_EACH_GROUP_IN_LIST(argv[1], {
                        if (!typeSet) typeName = G->name;
                        if (registerLinkType(typeName, &linkType)) return TCL_ERROR;
                        FOR_EACH_UNIT_IN_LIST(argv[2], {
                                if (connectGroupToUnit(G, U, linkType, range, mean, FALSE))
                                return TCL_ERROR;
                                });
                        });

        return signalNetStructureChanged();
}

int C_connectUnits(TCL_CMDARGS) {
        Unit V;
        int arg, firstType;
        mask linkType;
        char *typeName = NULL;
        real range = NaN, mean = NaN;
        flag bidirectional = FALSE;

        char *usage = "connectUnits <unit-list1> (<unit-list>)* [-mean <mean>\n"
                "\t| -range <range> | -type <link-type>]";
        if (argc == 2 && !strcmp(argv[1], "-h")) 
                return commandHelp(argv[0]);
        if (argc < 3) return usageError(argv[0], usage);
        if (!Net) return warning("%s: no current network", argv[0]);
        if (unsafeToRun(argv[0], NETWORK_TASKS)) return TCL_ERROR;

        for (arg = 1; arg < argc && isUnitList(argv[arg]); arg++);
        if (arg <= 2) return usageError(argv[0], usage);

        /* Read the options. */
        for (firstType = arg; arg < argc && argv[arg][0] == '-'; arg++) {
                switch (argv[arg][1]) {
                        case 'm':
                                if (++arg >= argc) return usageError(argv[0], usage);
                                mean = ator(argv[arg]);
                                break;
                        case 'r':
                                if (++arg >= argc) return usageError(argv[0], usage);
                                range = ator(argv[arg]);
                                if (range < 0.0) return warning("%s: range must be positive", argv[0]);
                                break;
                        case 't':
                                if (++arg >= argc) return usageError(argv[0], usage);
                                typeName = argv[arg];
                                break;
                        case 'b':
                                bidirectional = TRUE;
                                break;
                        default: return usageError(argv[0], usage);
                }
        }
        if (arg != argc) return usageError(argv[0], usage);

        for (arg = 2; arg < firstType; arg++)
                FOR_EACH_UNIT_IN_LIST(argv[arg-1], {
                                V = U;
                                if (!typeName) typeName = V->group->name;
                                if (registerLinkType(typeName, &linkType)) return TCL_ERROR;
                                FOR_EACH_UNIT_IN_LIST(argv[arg], {
                                        if (connectUnits(V, U, linkType, range, mean, FALSE)) 
                                        return TCL_ERROR;
                                        if (bidirectional)
                                        if (connectUnits(U, V, linkType, range, mean, FALSE)) 
                                        return TCL_ERROR;
                                        });
                                });

        return signalNetStructureChanged();
}

int C_elmanConnect(TCL_CMDARGS) {
        Group source, context;
        int arg;
        char *usage = "elmanConnect <source-group> <context-group>\n"
                "\t[-reset <reset-on-example> | -initOutput <init-output>]";
        if (argc == 2 && !strcmp(argv[1], "-h")) 
                return commandHelp(argv[0]);
        if (argc < 3) return usageError(argv[0], usage);
        if (!Net) return warning("%s: no current network", argv[0]);
        if (unsafeToRun(argv[0], NETWORK_TASKS)) return TCL_ERROR;

        if (!(source = lookupGroup(argv[1])))
                return warning("%s: group \"%s\" doesn't exist", argv[0], argv[1]);
        if (!(context = lookupGroup(argv[2])))
                return warning("%s: group \"%s\" doesn't exist", argv[0], argv[2]);

        /* Read the options. */
        for (arg = 3; arg < argc && argv[arg][0] == '-'; arg++) {
                switch (argv[arg][1]) {
                        case 'r':
                                if (++arg >= argc) return usageError(argv[0], usage);
                                if (atoi(argv[arg]))
                                        changeGroupType(source, GROUP, RESET_ON_EXAMPLE, ADD_TYPE);
                                else
                                        changeGroupType(source, GROUP, RESET_ON_EXAMPLE, REMOVE_TYPE);
                                break;
                        case 'i':
                                if (++arg >= argc) return usageError(argv[0], usage);
                                source->initOutput = ator(argv[arg]);
                                break;
                        default: return usageError(argv[0], usage);
                }
        }
        if (arg != argc) return usageError(argv[0], usage);

        if (elmanConnect(source, context)) return TCL_ERROR;
        if (updateUnitDisplay()) return TCL_ERROR;
        return updateLinkDisplay();
}

int C_copyConnect(TCL_CMDARGS) {
        Group source, copy;
        int offset, set = 0;
        Unit U;
        GroupProc P;
        CopyData D;

        char *usage = "copyConnect <source-group> <copy-group> <field>";
        if (argc == 2 && !strcmp(argv[1], "-h")) 
                return commandHelp(argv[0]);
        if (argc != 4) return usageError(argv[0], usage);
        if (!Net) return warning("%s: no current network", argv[0]);
        if (unsafeToRun(argv[0], NETWORK_TASKS)) return TCL_ERROR;

        if (!(source = lookupGroup(argv[1])))
                return warning("%s: group \"%s\" doesn't exist", argv[0], argv[1]);
        if (!(copy = lookupGroup(argv[2])))
                return warning("%s: group \"%s\" doesn't exist", argv[0], argv[2]);

        if (source->numUnits != copy->numUnits) 
                return warning("%s: source and copy groups aren't the same size", argv[0]);

        if (subString(argv[3], "inputs", 6)) 
                offset = OFFSET(U, input);
        else if (subString(argv[3], "externalInputs", 1)) 
                offset = OFFSET(U, externalInput);
        else if (subString(argv[3], "outputs", 7))
                offset = OFFSET(U, output);
        else if (subString(argv[3], "targets", 1)) 
                offset = OFFSET(U, target);
        else if (subString(argv[3], "inputDerivs", 6)) 
                offset = OFFSET(U, inputDeriv);
        else if (subString(argv[3], "outputDerivs", 7)) 
                offset = OFFSET(U, outputDeriv);
        else return warning("%s: bad field type: %s\n", argv[0], argv[3]);

        for (P = copy->inputProcs; P && !set; P = P->next)
                if (P->type == IN_COPY && !P->otherData) {
                        set = 1; break;}
        if (!P) for (P = copy->outputProcs; P && !set; P = P->next)
                if (P->type == OUT_COPY && !P->otherData) {set = 2; break;}
        if (!P) for (P = copy->costProcs; P && !set; P = P->next)
                if (P->type == TARGET_COPY && !P->otherData) {set = 3; break;}

        if (!set) return warning("Group \"%s\" has no empty *_COPY slots.",
                        copy->name);

        D = (CopyData) safeMalloc(sizeof(struct copyData), "copyConnect");
        D->source = source;
        D->offset = offset;
        P->otherData = (void *) D;

        if (set == 2) {
                if (isNaN(copy->minOutput)) copy->minOutput = source->minOutput;
                if (isNaN(copy->maxOutput)) copy->maxOutput = source->maxOutput;
        }

        return TCL_OK;
}

int C_addLinkType(TCL_CMDARGS) {
        mask type;
        char *usage = "addLinkType [<link-type>]";
        if (argc == 2 && !strcmp(argv[1], "-h"))
                return commandHelp(argv[0]);
        if (argc > 2) return usageError(argv[0], usage);

        if (argc == 1) return listLinkTypes();
        return registerLinkType(argv[1], &type);
}

int C_deleteLinkType(TCL_CMDARGS) {
        char *usage = "deleteLinkType [<link-type>]";
        if (argc == 2 && !strcmp(argv[1], "-h"))
                return commandHelp(argv[0]);
        if (argc > 2) return usageError(argv[0], usage);

        if (argc == 1) return listLinkTypes();
        return unregisterLinkType(argv[1]);
}

int C_saveWeights(TCL_CMDARGS) {
        flag binary = TRUE, frozen = TRUE, thawed = TRUE;
        int arg, values = 1;
        char *usage = "saveWeights <file-name> [-values <num-values> | -text |\n"
                "\t-noFrozen | -onlyFrozen]";
        if (argc == 2 && !strcmp(argv[1], "-h")) 
                return commandHelp(argv[0]);
        if (argc < 2) return usageError(argv[0], usage);
        if (!Net) return warning("%s: no current network", argv[0]);

        /* Read the options. */
        for (arg = 2; arg < argc && argv[arg][0] == '-'; arg++) {
                switch (argv[arg][1]) {
                        case 'v':
                                if (++arg >= argc) return usageError(argv[0], usage);
                                if ((values = atoi(argv[arg])) < 1) 
                                        return warning("%s: numValues must be positive", argv[0]);
                                break;
                        case 't':
                                binary = FALSE;
                                break;
                        case 'n':
                                frozen = FALSE;
                                break;
                        case 'o':
                                thawed = FALSE;
                                break;
                        default: return usageError(argv[0], usage);
                }
        }
        if (arg != argc) return usageError(argv[0], usage);

#ifdef ADVANCED
        if (values > 3) values = 3;
#else
        if (values > 2) values = 2;
#endif /* ADVANCED */

        eval(".setPath _weights %s", argv[1]);

        if (Net->saveWeights(argv[1], binary, values, thawed, frozen))
                return TCL_ERROR;
        return result(argv[1]);
}

int C_loadWeights(TCL_CMDARGS) {
        int arg;
        flag frozen = TRUE, thawed = TRUE;
        char *usage = "loadWeights <file-name> [-noFrozen | -onlyFrozen]";
        if (argc == 2 && !strcmp(argv[1], "-h")) 
                return commandHelp(argv[0]);
        if (argc != 2 && argc != 3) return usageError(argv[0], usage);
        if (!Net) return warning("%s: no current network", argv[0]);

        /* Read the options. */
        for (arg = 2; arg < argc && argv[arg][0] == '-'; arg++) {
                switch (argv[arg][1]) {
                        case 'n':
                                frozen = FALSE;
                                break;
                        case 'o':
                                thawed = FALSE;
                                break;
                        default: return usageError(argv[0], usage);
                }
        }
        if (arg != argc) return usageError(argv[0], usage);

        eval(".setPath _weights %s", argv[1]);

        if (Net->loadWeights(argv[1], thawed, frozen)) return TCL_ERROR;
        if (updateUnitDisplay()) return TCL_ERROR;
        if (updateLinkDisplay()) return TCL_ERROR;
        return result(argv[1]);
}

int C_loadXerionWeights(TCL_CMDARGS) {
        char *usage = "loadXerionWeights <file-name>";
        if (argc == 2 && !strcmp(argv[1], "-h")) 
                return commandHelp(argv[0]);
        if (argc != 2) return usageError(argv[0], usage);
        if (!Net) return warning("%s: no current network", argv[0]);

        eval(".setPath _weights %s", argv[1]);

        if (loadXerionWeights(argv[1])) return TCL_ERROR;
        if (updateUnitDisplay()) return TCL_ERROR;
        return updateLinkDisplay();
}


/****************************** Disconnection Commands ***********************/

int C_disconnectGroups(TCL_CMDARGS) {
        Group preGroup, postGroup;
        mask linkType = ALL_LINKS;
        char *usage = "disconnectGroups <group1> <group2> [-type <link-type>]";
        if (argc == 2 && !strcmp(argv[1], "-h")) 
                return commandHelp(argv[0]);
        if (argc != 3 && argc != 5) return usageError(argv[0], usage);
        if (!Net) return warning("%s: no current network", argv[0]);
        if (unsafeToRun(argv[0], NETWORK_TASKS)) return TCL_ERROR;

        if (!(preGroup = lookupGroup(argv[1])))
                return warning("%s: group \"%s\" doesn't exist", argv[0], argv[1]);
        if (!(postGroup = lookupGroup(argv[2])))
                return warning("%s: group \"%s\" doesn't exist", argv[0], argv[2]);
        if (argc == 5) {
                if (!subString(argv[3], "-type", 2)) return usageError(argv[0], usage);
                if (!(linkType = lookupLinkType(argv[4])))
                        return warning("%s: link type \"%s\" doesn't exist", argv[0], argv[4]);
        }

        if (disconnectGroups(preGroup, postGroup, linkType)) return TCL_ERROR;
        return linksChanged();
}

int C_disconnectGroupUnit(TCL_CMDARGS) {
        Unit U;
        Group G;
        mask linkType = ALL_LINKS;
        char *usage = "disconnectGroupUnit <group> <unit> [-type <link-type>]";
        if (argc == 2 && !strcmp(argv[1], "-h")) 
                return commandHelp(argv[0]);
        if (argc != 3 && argc != 5) return usageError(argv[0], usage);
        if (!Net) return warning("%s: no current network", argv[0]);
        if (unsafeToRun(argv[0], NETWORK_TASKS)) return TCL_ERROR;

        if (!(G = lookupGroup(argv[1])))
                return warning("%s: group \"%s\" doesn't exist", argv[0], argv[1]);
        if (!(U = lookupUnit(argv[2])))
                return warning("%s: unit \"%s\" doesn't exist", argv[0], argv[2]);
        if (argc == 5) {
                if (!subString(argv[3], "-type", 2)) return usageError(argv[0], usage);
                if (!(linkType = lookupLinkType(argv[4])))
                        return warning("%s: link type \"%s\" doesn't exist", argv[0], argv[4]);
        }

        if (disconnectGroupFromUnit(G, U, linkType)) return TCL_ERROR;
        return signalNetStructureChanged();
}

int C_disconnectUnits(TCL_CMDARGS) {
        Unit preUnit, postUnit;
        mask linkType = ALL_LINKS;
        char *usage = "disconnectUnits <unit1> <unit2> [-type <link-type>]";
        if (argc == 2 && !strcmp(argv[1], "-h")) 
                return commandHelp(argv[0]);
        if (argc != 3 && argc != 5) return usageError(argv[0], usage);
        if (!Net) return warning("%s: no current network", argv[0]);
        if (unsafeToRun(argv[0], NETWORK_TASKS)) return TCL_ERROR;

        if (!(preUnit = lookupUnit(argv[1])))
                return warning("%s: unit \"%s\" doesn't exist", argv[0], argv[1]);
        if (!(postUnit = lookupUnit(argv[2])))
                return warning("%s: unit \"%s\" doesn't exist", argv[0], argv[2]);
        if (argc == 5) {
                if (!subString(argv[3], "-type", 2)) return usageError(argv[0], usage);
                if (!(linkType = lookupLinkType(argv[4])))
                        return warning("%s: link type \"%s\" doesn't exist", argv[0], argv[4]);
        }

        if (disconnectUnits(preUnit, postUnit, linkType)) return TCL_ERROR;
        return linksChanged();
}

int C_deleteLinks(TCL_CMDARGS) {
        mask linkType = ALL_LINKS;
        char *usage = "deleteLinks [-type <link-type>]";
        if (argc == 2 && !strcmp(argv[1], "-h")) 
                return commandHelp(argv[0]);
        if (argc != 1 && argc != 3) return usageError(argv[0], usage);
        if (!Net) return warning("%s: no current network", argv[0]);
        if (unsafeToRun(argv[0], NETWORK_TASKS)) return TCL_ERROR;

        if (argc == 3) {
                if (!subString(argv[1], "-type", 2)) return usageError(argv[0], usage);
                if (!(linkType = lookupLinkType(argv[2])))
                        return warning("%s: link type \"%s\" doesn't exist", argv[0], argv[2]);
        }

        if (deleteAllLinks(linkType)) return TCL_ERROR;
        return linksChanged();
}

int C_deleteGroupInputs(TCL_CMDARGS) {
        mask linkType = ALL_LINKS;
        char *usage = "deleteGroupInputs <group-list> [-type <link-type>]";
        if (argc == 2 && !strcmp(argv[1], "-h")) 
                return commandHelp(argv[0]);
        if (argc != 2 && argc != 4) return usageError(argv[0], usage);
        if (!Net) return warning("%s: no current network", argv[0]);
        if (unsafeToRun(argv[0], NETWORK_TASKS)) return TCL_ERROR;

        if (argc == 4) {
                if (!subString(argv[2], "-type", 2)) return usageError(argv[0], usage);
                if (!(linkType = lookupLinkType(argv[3])))
                        return warning("%s: link type \"%s\" doesn't exist", argv[0], argv[3]);
        }

        FOR_EACH_GROUP_IN_LIST(argv[1], {
                        if (deleteGroupInputs(G, linkType)) return TCL_ERROR;
                        });
        return linksChanged();
}

int C_deleteGroupOutputs(TCL_CMDARGS) {
        mask linkType = ALL_LINKS;
        char *usage = "deleteGroupOutputs <group-list> [-type <link-type>]";
        if (argc == 2 && !strcmp(argv[1], "-h")) 
                return commandHelp(argv[0]);
        if (argc != 2 && argc != 4) return usageError(argv[0], usage);
        if (!Net) return warning("%s: no current network", argv[0]);
        if (unsafeToRun(argv[0], NETWORK_TASKS)) return TCL_ERROR;

        if (argc == 4) {
                if (!subString(argv[2], "-type", 2)) return usageError(argv[0], usage);
                if (!(linkType = lookupLinkType(argv[3])))
                        return warning("%s: link type \"%s\" doesn't exist", argv[0], argv[3]);
        }

        FOR_EACH_GROUP_IN_LIST(argv[1], {
                        if (deleteGroupOutputs(G, linkType)) return TCL_ERROR;
                        });
        return linksChanged();
}

int C_deleteUnitInputs(TCL_CMDARGS) {
        mask linkType = ALL_LINKS;
        char *usage = "deleteUnitInputs <unit-list> [-type <link-type>]";
        if (argc == 2 && !strcmp(argv[1], "-h")) 
                return commandHelp(argv[0]);
        if (argc != 2 && argc != 4) return usageError(argv[0], usage);
        if (!Net) return warning("%s: no current network", argv[0]);
        if (unsafeToRun(argv[0], NETWORK_TASKS)) return TCL_ERROR;

        if (argc == 4) {
                if (!subString(argv[2], "-type", 2)) return usageError(argv[0], usage);
                if (!(linkType = lookupLinkType(argv[3])))
                        return warning("%s: link type \"%s\" doesn't exist", argv[0], argv[3]);
        }

        FOR_EACH_UNIT_IN_LIST(argv[1], {
                        if (deleteUnitInputs(U, linkType)) return TCL_ERROR;
                        });
        return linksChanged();
}

int C_deleteUnitOutputs(TCL_CMDARGS) {
        mask linkType = ALL_LINKS;
        char *usage = "deleteUnitOutputs <unit-list> [-type <link-type>]";
        if (argc == 2 && !strcmp(argv[1], "-h")) 
                return commandHelp(argv[0]);
        if (argc != 2 && argc != 4) return usageError(argv[0], usage);
        if (!Net) return warning("%s: no current network", argv[0]);
        if (unsafeToRun(argv[0], NETWORK_TASKS)) return TCL_ERROR;

        if (argc == 4) {
                if (!subString(argv[2], "-type", 2)) return usageError(argv[0], usage);
                if (!(linkType = lookupLinkType(argv[3])))
                        return warning("%s: link type \"%s\" doesn't exist", argv[0], argv[3]);
        }

        FOR_EACH_UNIT_IN_LIST(argv[1], {
                        if (deleteUnitOutputs(U, linkType)) return TCL_ERROR;
                        });
        return linksChanged();
}


/***************************** Link Value Commands ***************************/

int C_randWeights(TCL_CMDARGS) {
        int arg;
        mask linkType = ALL_LINKS;
        real range = NaN, mean = NaN;
        char *groupList = NULL, *unitList = NULL;
        char *usage = "randWeights [-group <group-list> | -unit <unit-list> |\n"
                "\t-mean <mean> | -range <range> | -type <link-type>]";

        if (argc == 2 && !strcmp(argv[1], "-h")) 
                return commandHelp(argv[0]);
        if (!Net) return warning("%s: no current network", argv[0]);

        for (arg = 1; arg < argc && argv[arg][0] == '-'; arg++) {
                switch (argv[arg][1]) {
                        case 'g':
                                if (++arg >= argc) return usageError(argv[0], usage);
                                groupList = argv[arg];
                                break;
                        case 'u':
                                if (++arg >= argc) return usageError(argv[0], usage);
                                unitList = argv[arg];
                                break;
                        case 'm':
                                if (++arg >= argc) return usageError(argv[0], usage);
                                mean = ator(argv[arg]);
                                break;
                        case 'r':
                                if (++arg >= argc) return usageError(argv[0], usage);
                                range = ator(argv[arg]);
                                if (range < 0.0) return warning("%s: range must be positive", argv[0]);
                                break;
                        case 't':
                                if (++arg >= argc) return usageError(argv[0], usage);
                                if (!(linkType = lookupLinkType(argv[arg])))
                                        return warning("%s: link type %s doesn't exist", argv[0], argv[arg]);
                                break;
                        default: return usageError(argv[0], usage);
                }
        }
        if (arg != argc) return usageError(argv[0], usage);

        if (groupList) {
                FOR_EACH_GROUP_IN_LIST(groupList, {
                                randomizeGroupWeights(G, chooseValue(range, Net->randRange), 
                                        chooseValue(mean, Net->randMean), range, mean, 
                                        linkType, TRUE);
                                });
        }
        if (unitList) {
                FOR_EACH_UNIT_IN_LIST(unitList, {
                                randomizeUnitWeights(U, chooseValue3(range, U->group->randRange,
                                                Net->randRange),
                                        chooseValue3(mean, U->group->randMean, 
                                                Net->randMean), 
                                        range, mean, linkType, TRUE);
                                });
        }
        if (!groupList && !unitList)
                randomizeWeights(range, mean, linkType, TRUE);
        if (updateUnitDisplay()) return TCL_ERROR;
        return updateLinkDisplay();
}

#ifdef JUNK
int C_randGroupWeights(TCL_CMDARGS) {
        int arg;
        mask linkType = ALL_LINKS;
        real range = NaN, mean = NaN;
        char *usage = "randGroupWeights <group-list> [-mean <mean> | -range <range>"
                " |\n\t-type <link-type>]";
        if (argc == 2 && !strcmp(argv[1], "-h")) 
                return commandHelp(argv[0]);
        if (argc < 2) return usageError(argv[0], usage);
        if (!Net) return warning("%s: no current network", argv[0]);

        for (arg = 2; arg < argc && argv[arg][0] == '-'; arg++) {
                switch (argv[arg][1]) {
                        case 'm':
                                if (++arg >= argc) return usageError(argv[0], usage);
                                mean = ator(argv[arg]);
                                break;
                        case 'r':
                                if (++arg >= argc) return usageError(argv[0], usage);
                                range = ator(argv[arg]);
                                if (range < 0.0) return warning("%s: range must be positive", argv[0]);
                                break;
                        case 't':
                                if (++arg >= argc) return usageError(argv[0], usage);
                                if (!(linkType = lookupLinkType(argv[arg])))
                                        return warning("%s: link type %s doesn't exist", argv[0], argv[arg]);
                                break;
                        default: return usageError(argv[0], usage);
                }
        }
        if (arg != argc) return usageError(argv[0], usage);

        FOR_EACH_GROUP_IN_LIST(argv[1], {
                        randomizeGroupWeights(G, chooseValue(range, Net->randRange), 
                                chooseValue(mean, Net->randMean), range, mean, 
                                linkType, TRUE);
                        });
        if (updateUnitDisplay()) return TCL_ERROR;
        return updateLinkDisplay();
}

int C_randUnitWeights(TCL_CMDARGS) {
        int arg;
        mask linkType = ALL_LINKS;
        real range = NaN, mean = NaN;
        char *usage = "randUnitWeights <unit-list> [-mean <mean> -range <range>\n"
                "\t-type <link-type>]";
        if (argc == 2 && !strcmp(argv[1], "-h")) 
                return commandHelp(argv[0]);
        if (argc < 2) return usageError(argv[0], usage);
        if (!Net) return warning("%s: no current network", argv[0]);

        for (arg = 2; arg < argc && argv[arg][0] == '-'; arg++) {
                switch (argv[arg][1]) {
                        case 'm':
                                if (++arg >= argc) return usageError(argv[0], usage);
                                mean = ator(argv[arg]);
                                break;
                        case 'r':
                                if (++arg >= argc) return usageError(argv[0], usage);
                                range = ator(argv[arg]);
                                if (range < 0.0) return warning("%s: range must be positive", argv[0]);
                                break;
                        case 't':
                                if (++arg >= argc) return usageError(argv[0], usage);
                                if (!(linkType = lookupLinkType(argv[arg])))
                                        return warning("%s: link type %s doesn't exist", argv[0], argv[arg]);
                                break;
                        default: return usageError(argv[0], usage);
                }
        }
        if (arg != argc) return usageError(argv[0], usage);

        FOR_EACH_UNIT_IN_LIST(argv[1], {
                        randomizeUnitWeights(U, chooseValue3(range, U->group->randRange,
                                        Net->randRange),
                                chooseValue3(mean, U->group->randMean, 
                                        Net->randMean), 
                                range, mean, linkType, TRUE);
                        });
        if (updateUnitDisplay()) return TCL_ERROR;
        return updateLinkDisplay();
}
#endif

int C_setLinkValues(TCL_CMDARGS) {
        mask linkType = ALL_LINKS;
        flag ext = FALSE;
        MemInfo M;
        int arg;
        char *groupList = NULL, *unitList = NULL;
        char *usage = "setLinkValues <parameter> <value> [-group <group-list> |\n"
                "\t-unit <unit-list> | -type <link-type>]";
        if (argc == 2 && !strcmp(argv[1], "-h")) 
                return commandHelp(argv[0]);
        if (argc < 3) return usageError(argv[0], usage);
        if (!Net) return warning("%s: no current network", argv[0]);

        if (!(M = lookupMember(argv[1], BlockInfo))) {
                if (!(M = lookupMember(argv[1], BlockExtInfo)))
                        return warning("%s: %s is not a field of a block or block extension", 
                                        argv[0], argv[1]);
                ext = TRUE;
        }
        if (!(M->info->setValue && M->info->setStringValue && M->writable))
                return warning("%s: %s is not a writable field", argv[0], argv[1]);
        M->info->setStringValue(Buffer, argv[2]);

        for (arg = 3; arg < argc && argv[arg][0] == '-'; arg++) {
                switch (argv[arg][1]) {
                        case 'g':
                                if (++arg >= argc) return usageError(argv[0], usage);
                                groupList = argv[arg];
                                break;
                        case 'u':
                                if (++arg >= argc) return usageError(argv[0], usage);
                                unitList = argv[arg];
                                break;
                        case 't':
                                if (++arg >= argc) return usageError(argv[0], usage);
                                if (!(linkType = lookupLinkType(argv[arg])))
                                        return warning("%s: link type %s doesn't exist", argv[0], argv[arg]);
                                break;
                        default: return usageError(argv[0], usage);
                }
        }
        if (arg != argc) return usageError(argv[0], usage);

        if (groupList) {
                FOR_EACH_GROUP_IN_LIST(groupList, {
                                setGroupBlockValues(G, ext, M, Buffer, linkType);
                                });
        }
        if (unitList) {
                FOR_EACH_UNIT_IN_LIST(unitList, {
                                setUnitBlockValues(U, ext, M, Buffer, linkType);
                                });
        }
        if (!groupList && !unitList)
                setBlockValues(ext, M, Buffer, linkType);
        return TCL_OK;
}

#ifdef JUNK
int C_setGroupBlockValues(TCL_CMDARGS) {
        mask linkType = ALL_LINKS;
        flag ext = FALSE;
        MemInfo M;
        char *usage = "setGroupBlockValues <group-list> <parameter> <value>\n"
                "\t[-type <link-type>]";
        if (argc == 2 && !strcmp(argv[1], "-h")) 
                return commandHelp(argv[0]);
        if (argc != 4 && argc != 6) return usageError(argv[0], usage);
        if (!Net) return warning("%s: no current network", argv[0]);

        if (!(M = lookupMember(argv[2], BlockInfo))) {
                if (!(M = lookupMember(argv[2], BlockExtInfo)))
                        return warning("%s: %s is not a field of a block or block extension", 
                                        argv[0], argv[2]);
                ext = TRUE;
        }
        if (!(M->info->setValue && M->info->setStringValue && M->writable))
                return warning("%s: %s is not a writable field", argv[0], argv[2]);
        M->info->setStringValue(Buffer, argv[3]);

        if (argc == 6) {
                if (!subString(argv[4], "-type", 2)) return usageError(argv[0], usage);
                if (!(linkType = lookupLinkType(argv[5])))
                        return warning("%s: link type %s doesn't exist", argv[0], argv[5]);
        }

        FOR_EACH_GROUP_IN_LIST(argv[1], {
                        setGroupBlockValues(G, ext, M, Buffer, linkType);
                        });
        return TCL_OK;
}

int C_setUnitBlockValues(TCL_CMDARGS) {
        mask linkType = ALL_LINKS;
        flag ext = FALSE;
        MemInfo M;
        char *usage = "setUnitBlockValues <unit-list> <parameter> <value>\n"
                "\t[-type <link-type>]";
        if (argc == 2 && !strcmp(argv[1], "-h")) 
                return commandHelp(argv[0]);
        if (argc != 4 && argc != 6) return usageError(argv[0], usage);
        if (!Net) return warning("%s: no current network", argv[0]);

        if (!(M = lookupMember(argv[2], BlockInfo))) {
                if (!(M = lookupMember(argv[2], BlockExtInfo)))
                        return warning("%s: %s is not a field of a block or block extension", 
                                        argv[0], argv[2]);
                ext = TRUE;
        }
        if (!(M->info->setValue && M->info->setStringValue && M->writable))
                return warning("%s: %s is not a writable field", argv[0], argv[2]);
        M->info->setStringValue(Buffer, argv[3]);

        if (argc == 6) {
                if (!subString(argv[4], "-type", 2)) return usageError(argv[0], usage);
                if (!(linkType = lookupLinkType(argv[5])))
                        return warning("%s: link type %s doesn't exist", argv[0], argv[5]);
        }

        FOR_EACH_UNIT_IN_LIST(argv[1], {
                        setUnitBlockValues(U, ext, M, Buffer, linkType);
                        });
        return TCL_OK;
}
#endif

int C_printLinkValues(TCL_CMDARGS) {
        mask linkType = ALL_LINKS;
        int i, link, code = TCL_OK, arg;
        Tcl_Channel channel;
        char message[256];
        Group F;
        char *fromGroups, *printProc, *gl1, *gl2;
        flag append = FALSE;

        char *usage = "printLinkValues <file> <printProc> <group-list1> "
                "<group-list2>\n\t[-type <link-type> | -append]";
        if (argc == 2 && !strcmp(argv[1], "-h")) 
                return commandHelp(argv[0]);
        if (argc < 5) return usageError(argv[0], usage);
        if (!Net) return warning("%s: no current network", argv[0]);

        printProc = argv[2];  
        gl1 = argv[3];
        gl2 = argv[4];

        for (arg = 5; arg < argc && argv[arg][0] == '-'; arg++) {
                switch (argv[arg][1]) {
                        case 't':
                                if (++arg >= argc) return usageError(argv[0], usage);
                                if (!(linkType = lookupLinkType(argv[arg])))
                                        return warning("%s: link type %s doesn't exist", argv[0], argv[arg]);
                                break;
                        case 'a':
                                append = TRUE; break;
                        default: return usageError(argv[0], usage);
                }
        }
        if (arg != argc) return usageError(argv[0], usage);

        if (!(channel = writeChannel(argv[1], append)))
                return warning("%s: couldn't write to the file %s", argv[0], argv[1]);

        fromGroups = (char *) safeCalloc(1, Net->numGroups, "printLinkValues");
        FOR_EACH_GROUP_IN_LIST(gl1, fromGroups[G->num] = 1);

        FOR_EACH_GROUP_IN_LIST(gl2, {
                        FOR_EACH_UNIT(G, {
                                link = 0;
                                FOR_EACH_BLOCK(U, {
                                        F = B->unit->group;
                                        if (LINK_TYPE_MATCH(linkType, B) && fromGroups[F->num]) {
                                        for (i = 0; i < B->numUnits; i++, link++) {
                                        if (eval("%s group(%d) group(%d).unit(%d) group(%d) "
                                                        "group(%d).unit(%d) %d %d",
                                                        printProc, F->num, F->num, (B->unit + i)->num, 
                                                        G->num, G->num, U->num, b, link)) {
                                        strcpy(message, Tcl_GetStringResult(Interp));
                                        code = TCL_ERROR;
                                        goto abort;
                                        }
                                        Tcl_Write(channel, Tcl_GetStringResult(Interp), -1);
                                        }
                                        } else link += B->numUnits;
                                        });
                                });
                        });

abort:
        FREE(fromGroups);
        closeChannel(channel);
        if (code) Tcl_SetResult(Interp, message, TCL_VOLATILE);
        else result("");
        return code;
}


/*********************** Freezing, Lesioning, and Noise **********************/

int C_freezeWeights(TCL_CMDARGS) {
        mask linkType = ALL_LINKS;
        int arg;
        char *groupList = NULL, *unitList = NULL;
        char *usage = "freezeWeights [-group <group-list> | -unit <unit-list> | "
                "-type <link-type>]";
        if (argc == 2 && !strcmp(argv[1], "-h")) 
                return commandHelp(argv[0]);
        if (!Net) return warning("%s: no current network", argv[0]);

        for (arg = 1; arg < argc && argv[arg][0] == '-'; arg++) {
                switch (argv[arg][1]) {
                        case 'g':
                                if (++arg >= argc) return usageError(argv[0], usage);
                                groupList = argv[arg];
                                break;
                        case 'u':
                                if (++arg >= argc) return usageError(argv[0], usage);
                                unitList = argv[arg];
                                break;
                        case 't':
                                if (++arg >= argc) return usageError(argv[0], usage);
                                if (!(linkType = lookupLinkType(argv[arg])))
                                        return warning("%s: link type %s doesn't exist", argv[0], argv[arg]);
                                break;
                        default: return usageError(argv[0], usage);
                }
        }
        if (arg != argc) return usageError(argv[0], usage);

        if (groupList) {
                FOR_EACH_GROUP_IN_LIST(groupList, freezeGroupInputs(G, linkType));
        }
        if (unitList) {
                FOR_EACH_UNIT_IN_LIST(unitList, freezeUnitInputs(U, linkType));
        }
        if (!groupList && !unitList)
                freezeAllLinks(linkType);
        return linksChanged();
}

#ifdef JUNK
int C_freezeGroups(TCL_CMDARGS) {
        mask linkType = ALL_LINKS;
        char *usage = "freezeGroups <group-list> [-type <link-type>]";
        if (argc == 2 && !strcmp(argv[1], "-h")) 
                return commandHelp(argv[0]);
        if (argc != 2 && argc != 4) return usageError(argv[0], usage);
        if (!Net) return warning("%s: no current network", argv[0]);

        if (argc == 4) {
                if (!subString(argv[2], "-type", 2)) return usageError(argv[0], usage);
                if (!(linkType = lookupLinkType(argv[3])))
                        return warning("%s: link type %s doesn't exist", argv[0], argv[3]);
        }

        FOR_EACH_GROUP_IN_LIST(argv[1], freezeGroupInputs(G, linkType));
        return linksChanged();
}

int C_freezeUnits(TCL_CMDARGS) {
        mask linkType = ALL_LINKS;
        char *usage = "freezeUnits <unit-list> [-type <link-type>]";
        if (argc == 2 && !strcmp(argv[1], "-h")) 
                return commandHelp(argv[0]);
        if (argc != 2 && argc != 4)  return usageError(argv[0], usage);
        if (!Net) return warning("%s: no current network", argv[0]);

        if (argc == 4) {
                if (!subString(argv[2], "-type", 2)) return usageError(argv[0], usage);
                if (!(linkType = lookupLinkType(argv[3])))
                        return warning("%s: link type %s doesn't exist", argv[0], argv[3]);
        }

        FOR_EACH_UNIT_IN_LIST(argv[1], freezeUnitInputs(U, linkType));
        return linksChanged();
}
#endif

int C_thawWeights(TCL_CMDARGS) {
        mask linkType = ALL_LINKS;
        int arg;
        char *groupList = NULL, *unitList = NULL;
        char *usage = "thawWeights [-group <group-list> | -unit <unit-list> | "
                "-type <link-type>]";
        if (argc == 2 && !strcmp(argv[1], "-h")) 
                return commandHelp(argv[0]);
        if (!Net) return warning("%s: no current network", argv[0]);

        for (arg = 1; arg < argc && argv[arg][0] == '-'; arg++) {
                switch (argv[arg][1]) {
                        case 'g':
                                if (++arg >= argc) return usageError(argv[0], usage);
                                groupList = argv[arg];
                                break;
                        case 'u':
                                if (++arg >= argc) return usageError(argv[0], usage);
                                unitList = argv[arg];
                                break;
                        case 't':
                                if (++arg >= argc) return usageError(argv[0], usage);
                                if (!(linkType = lookupLinkType(argv[arg])))
                                        return warning("%s: link type %s doesn't exist", argv[0], argv[arg]);
                                break;
                        default: return usageError(argv[0], usage);
                }
        }
        if (arg != argc) return usageError(argv[0], usage);

        if (groupList) {
                FOR_EACH_GROUP_IN_LIST(groupList, thawGroupInputs(G, linkType));
        }
        if (unitList) {
                FOR_EACH_UNIT_IN_LIST(unitList, thawUnitInputs(U, linkType));
        }
        if (!groupList && !unitList)
                thawAllLinks(linkType);
        return linksChanged();
}

#ifdef JUNK
int C_thawNet(TCL_CMDARGS) {
        mask linkType = ALL_LINKS;
        char *usage = "thawNet [-type <link-type>]";
        if (argc == 2 && !strcmp(argv[1], "-h")) 
                return commandHelp(argv[0]);
        if (argc > 2)
                return usageError(argv[0], usage);
        if (!Net) return warning("%s: no current network", argv[0]);

        if (argc == 3) {
                if (!subString(argv[1], "-type", 2)) return usageError(argv[0], usage);
                if (!(linkType = lookupLinkType(argv[2])))
                        return warning("%s: link type %s doesn't exist", argv[0], argv[2]);
        }

        thawAllLinks(linkType);
        return linksChanged();
}

int C_thawGroups(TCL_CMDARGS) {
        mask linkType = ALL_LINKS;
        char *usage = "thawGroups <group-list> [-type <link-type>]";
        if (argc == 2 && !strcmp(argv[1], "-h")) 
                return commandHelp(argv[0]);
        if (argc != 2 && argc != 3)
                return usageError(argv[0], usage);
        if (!Net) return warning("%s: no current network", argv[0]);

        if (argc == 4) {
                if (!subString(argv[2], "-type", 2)) return usageError(argv[0], usage);
                if (!(linkType = lookupLinkType(argv[3])))
                        return warning("%s: link type %s doesn't exist", argv[0], argv[3]);
        }

        FOR_EACH_GROUP_IN_LIST(argv[1], thawGroupInputs(G, linkType));
        return linksChanged();
}

int C_thawUnits(TCL_CMDARGS) {
        mask linkType = ALL_LINKS;
        char *usage = "thawUnits <unit-list> [-type <link-type>]";
        if (argc == 2 && !strcmp(argv[1], "-h")) 
                return commandHelp(argv[0]);
        if (argc != 2 && argc != 3)
                return usageError(argv[0], usage);
        if (!Net) return warning("%s: no current network", argv[0]);

        if (argc == 4) {
                if (!subString(argv[2], "-type", 2)) return usageError(argv[0], usage);
                if (!(linkType = lookupLinkType(argv[3])))
                        return warning("%s: link type %s doesn't exist", argv[0], argv[3]);
        }

        FOR_EACH_UNIT_IN_LIST(argv[1], thawUnitInputs(U, linkType));
        return linksChanged();
}
#endif

#ifdef JUNK
int C_holdGroups(TCL_CMDARGS) {
        Group G;
        int i;
        /* char *usage = "holdGroups [<group> ...]"; */
        if (argc == 2 && !strcmp(argv[1], "-h")) 
                return commandHelp(argv[0]);
        if (!Net) return warning("%s: no current network", argv[0]);

        if (argc == 1) {
                FOR_EACH_GROUP(if (!(G->type & HELD)) append("\"%s\" ", G->name));
                return TCL_OK;
        }

        FOR_EACH_GROUP(G, G->type &= ~HOLD_PENDING);

        for (i = 1; i < argc; i++) {
                if (!(G = lookupGroup(argv[i])))
                        return warning("%s: group \"%s\" doesn't exist", argv[0], argv[i]);
                G->type |= HOLD_PENDING;
        }
        return holdGroups();
}

int C_releaseGroups(TCL_CMDARGS) {
        Group G;
        int i;
        /* char *usage = "releaseGroups [<group> ...]"; */
        if (argc == 2 && !strcmp(argv[1], "-h")) 
                return commandHelp(argv[0]);

        if (!Net) return warning("%s: no current network", argv[0]);

        if (argc == 1) {
                FOR_EACH_GROUP(if (G->type & HELD) append("\"%s\" ", G->name));
                return TCL_OK;
        }

        FOR_EACH_GROUP(G, G->type &= ~HOLD_PENDING);

        if (argc == 2 && !strcmp(argv[1], "*")) {
                FOR_EACH_GROUP(G, if (G->type & HELD) G->type |= HOLD_PENDING);
        } else 
                for (i = 1; i < argc; i++) {
                        if (!(G = lookupGroup(argv[i])))
                                return warning("%s: group \"%s\" doesn't exist", argv[0], argv[i]);
                        G->type |= HOLD_PENDING;
                }

        return releaseGroups();
}
#endif

int C_lesionLinks(TCL_CMDARGS) {
        int arg;
        mask linkType = ALL_LINKS;
        real proportion = 1.0, value = NaN, range = NaN;
        flag mult = FALSE, flat = FALSE, discard = FALSE;
        real (*proc)(real value, real range);
        char *usage = "lesionLinks <group-list> [-proportion <proportion> |\n"
                "\t-value <value> | -range <range> | -multiply | -flat | -discard\n"
                "\t-type <link-type>]";
        if (argc == 2 && !strcmp(argv[1], "-h")) 
                return commandHelp(argv[0]);
        if (argc < 2) return usageError(argv[0], usage);
        if (!Net) return warning("%s: no current network", argv[0]);

        /* Read the options. */
        for (arg = 2; arg < argc && argv[arg][0] == '-'; arg++) {
                switch (argv[arg][1]) {
                        case 'p':
                                if (++arg >= argc) return usageError(argv[0], usage);
                                proportion = atof(argv[arg]);
                                if (proportion <= 0.0 || proportion > 1.0)
                                        return warning("%s: link proportion (%f) must be in the range (0,1]",
                                                        argv[0], proportion);
                                break;
                        case 'v':
                                if (++arg >= argc) return usageError(argv[0], usage);
                                value = atof(argv[arg]); break;
                        case 'r':
                                if (++arg >= argc) return usageError(argv[0], usage);
                                range = ator(argv[arg]); break;
                        case 'm':
                                mult = TRUE; break;
                        case 'f':
                                flat = TRUE; break;
                        case 'd':
                                discard = TRUE; break;
                        case 't':
                                if (++arg >= argc) return usageError(argv[0], usage);
                                if (!(linkType = lookupLinkType(argv[arg])))
                                        return warning("%s: link type %s doesn't exist", argv[0], argv[arg]);
                                break;
                        default: return usageError(argv[0], usage);
                }
        }
        if (arg != argc) return usageError(argv[0], usage);

        if (isNaN(value) && isNaN(range)) {
                FOR_EACH_GROUP_IN_LIST(argv[1], {
                                if (lesionLinks(G, proportion, 0, 0, NULL, linkType)) return TCL_ERROR;
                                });
                return linksChanged();
        } else if (isNaN(range)) {
                FOR_EACH_GROUP_IN_LIST(argv[1], {
                                if (lesionLinks(G, proportion, 1, value, NULL, linkType)) 
                                return TCL_ERROR;
                                });
        } else {
                if (discard) {
                        proc = (flat) ? addUniformNoise : addGaussianNoise;
                } else {
                        if (flat)	proc = (mult) ? multUniformNoise : addUniformNoise;
                        else	proc = (mult) ? multGaussianNoise : addGaussianNoise;
                }
                FOR_EACH_GROUP_IN_LIST(argv[1], {
                                if (lesionLinks(G, proportion, (discard) ? 3:2, range, proc, linkType)) 
                                return TCL_ERROR;
                                });
        }

        if (updateUnitDisplay()) return TCL_ERROR;
        return updateLinkDisplay();
}

int C_lesionUnits(TCL_CMDARGS) {
        real proportion;
        char *usage = "lesionUnits (<group-list> <proportion> | <unit-list>)";
        if (argc == 2 && !strcmp(argv[1], "-h")) 
                return commandHelp(argv[0]);
        if (argc != 2 && argc != 3)
                return usageError(argv[0], usage);
        if (!Net) return warning("%s: no current network", argv[0]);

        if (argc == 2) {
                FOR_EACH_UNIT_IN_LIST(argv[1], lesionUnit(U));
                return TCL_OK;
        }

        proportion = atof(argv[2]);
        if (proportion <= 0.0 || proportion > 1.0)
                return warning("%s: proportion of units (%f) must be in the range (0,1]",
                                argv[0], proportion);

        FOR_EACH_GROUP_IN_LIST(argv[1], {
                        FOR_EACH_UNIT(G, if (randProb() < proportion) lesionUnit(U));
                        });
        return signalNetStructureChanged();
}

int C_healUnits(TCL_CMDARGS) {
        real proportion;
        char *usage = "healUnits [(<group-list> <proportion> | <unit-list>)]";
        if (argc == 2 && !strcmp(argv[1], "-h")) 
                return commandHelp(argv[0]);
        if (argc > 3)
                return usageError(argv[0], usage);
        if (!Net) return warning("%s: no current network", argv[0]);

        if (argc == 1) {
                FOR_EACH_GROUP(FOR_EVERY_UNIT(G, if (U->type & LESIONED) healUnit(U)));
                return TCL_OK;
        }
        else if (argc == 2) {
                FOR_EACH_UNIT_IN_LIST(argv[1], healUnit(U));
                return TCL_OK;
        }

        proportion = atof(argv[2]);
        if (proportion <= 0.0 || proportion > 1.0)
                return warning("%s: proportion of units (%f) must be in the range (0,1]",
                                argv[0], proportion);
        FOR_EACH_GROUP_IN_LIST(argv[1], {
                        FOR_EACH_UNIT(G, if (randProb() < proportion) healUnit(U));
                        });
        return signalNetStructureChanged();
}

int C_noiseType(TCL_CMDARGS) {
        flag mult = FALSE, flat = FALSE, rangeSet = FALSE;
        real range = NaN, (*proc)(real value, real range);
        int arg;
        char *usage = "noiseType <group-list> [-range <range> | -multiply | -flat]";
        if (argc == 2 && !strcmp(argv[1], "-h")) 
                return commandHelp(argv[0]);
        if (argc < 2) return usageError(argv[0], usage);
        if (!Net) return warning("%s: no current network", argv[0]);

        for (arg = 2; arg < argc; arg++) {
                if (argv[arg][0] != '-') return usageError(argv[0], usage);
                switch (argv[arg][1]) {
                        case 'r':
                                range = ator(argv[++arg]);
                                rangeSet = TRUE;
                                break;
                        case 'm':
                                mult = TRUE;
                                break;
                        case 'f':
                                flat = TRUE;
                                break;
                        default: return usageError(argv[0], usage);
                }
        }
        if (arg != argc) return usageError(argv[0], usage);

        if (flat)
                proc = (mult) ? multUniformNoise : addUniformNoise;
        else
                proc = (mult) ? multGaussianNoise : addGaussianNoise;

        FOR_EACH_GROUP_IN_LIST(argv[1], {
                        if (rangeSet) G->noiseRange = range;
                        G->noiseProc = proc;
                        });

        return TCL_OK;
}

/****************************** Command Registration *************************/

void registerConnectCommands(void) {
        registerCommand(C_connectGroups, "connectGroups",
                        "creates links with a specified pattern between groups");
        registerCommand(C_connectGroupToUnits, "connectGroupToUnits",
                        "creates links from all units in a group to given units");
        registerCommand(C_connectUnits, "connectUnits",
                        "creates a link from one unit to another");
        registerCommand(C_elmanConnect, "elmanConnect",
                        "connects a source group to an ELMAN context group");
        registerCommand(C_copyConnect, "copyConnect",
                        "connects a group to an IN_, OUT_, or TARGET_COPY group");
        registerCommand(C_addLinkType, "addLinkType", 
                        "creates a new link type");
        registerCommand(C_deleteLinkType, "deleteLinkType", 
                        "deletes a user-defined link type");
        registerCommand(C_saveWeights, "saveWeights",
                        "saves the link weights and other values in a file");
        registerCommand(C_loadWeights, "loadWeights",
                        "loads the link weights and other values from a file");
        registerCommand(C_loadXerionWeights, "loadXerionWeights",
                        "loads weights from a Xerion text-format weight file");

        registerCommand(NULL, "", "");
        registerCommand(C_disconnectGroups, "disconnectGroups",
                        "deletes links of a specified type between two groups"); 
        registerCommand(C_disconnectGroupUnit, "disconnectGroupUnit",
                        "deletes links from a group to a unit"); 
        registerCommand(C_disconnectUnits, "disconnectUnits",
                        "deletes links of a specified type between two units");
        registerCommand(C_deleteLinks, "deleteLinks",
                        "deletes all links of a specified type");
        registerCommand(C_deleteGroupInputs, "deleteGroupInputs",
                        "deletes inputs to a group (including Elman inputs)");
        registerCommand(C_deleteGroupOutputs, "deleteGroupOutputs",
                        "deletes outputs from a group (including Elman outputs)");
        registerCommand(C_deleteUnitInputs, "deleteUnitInputs",
                        "deletes inputs of a specified type to a unit");
        registerCommand(C_deleteUnitOutputs, "deleteUnitOutputs",
                        "deletes outputs of a specified type from a unit");

        registerCommand(NULL, "", "");
        registerCommand(C_randWeights, "randWeights",
                        "randomizes all of the weights of a selected type");
        registerCommand(C_freezeWeights, "freezeWeights",
                        "stops weight updates on specified links");
        registerCommand(C_thawWeights, "thawWeights",
                        "resumes weight updates on specified links");
        registerCommand(C_setLinkValues, "setLinkValues",
                        "sets parameters for specified links");
        registerCommand(C_printLinkValues, "printLinkValues",
                        "prints values for specified links to a file");

        registerCommand(NULL, "", "");
        registerCommand(C_lesionLinks, "lesionLinks",
                        "removes links or sets or adds noise to their weights");
        registerCommand(C_lesionUnits, "lesionUnits",
                        "inactivates units");
        registerCommand(C_healUnits, "healUnits",
                        "restores lesioned units");
        registerCommand(C_noiseType, "noiseType",
                        "sets a group's noise parameters");
}
