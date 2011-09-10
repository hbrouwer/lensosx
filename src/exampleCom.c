/* exampleCom.c */

#include <string.h>

#include "system.h"
#include "util.h"
#include "type.h"
#include "example.h"
#include "network.h"
#include "command.h"
#include "control.h"
#include "display.h"
#include "act.h"
#include "defaults.h"

int C_loadExamples(TCL_CMDARGS) {
        ExampleSet S;
        char *setName = NULL;
        int arg, mode = 0, numExamples = 0;
        flag code, madeName = FALSE;
        mask exmode = DEF_S_mode;
        char *usage = "loadExamples <file-name> [-set <example-set> | -num "
                "<num-examples> |\n\t-exmode <example-mode> | -mode <load-mode>]";
        if (argc == 2 && !strcmp(argv[1], "-h")) 
                return commandHelp(argv[0]);
        if (argc < 2) return usageError(argv[0], usage);
        if (unsafeToRun(argv[0], LOADING_EXAMPLES | SAVING_EXAMPLES)) 
                return TCL_ERROR;

        for (arg = 2; arg < argc && argv[arg][0] == '-'; arg++) {
                switch (argv[arg][1]) {
                        case 's':
                                if (++arg >= argc) return usageError(argv[0], usage);
                                if (!argv[arg][0])
                                        return warning("%s: example set must have a real name", argv[0]);
                                else setName = argv[arg];
                                break;
                        case 'n':
                                if (++arg >= argc) return usageError(argv[0], usage);
                                numExamples = atoi(argv[arg]);
                                break;
                        case 'e':
                                if (++arg >= argc) return usageError(argv[0], usage);
                                if (lookupTypeMask(argv[arg], EXAMPLE_MODE, &exmode)) {
                                        warning("%s: invalid example mode: \"%s\"\n"
                                                        "Try one of the following:\n", argv[0], argv[arg]);
                                        printTypes("Example Modes", EXAMPLE_MODE, ~0);
                                        return TCL_ERROR;
                                }
                                break;
                        case 'm':
                                if (++arg >= argc) return usageError(argv[0], usage);
                                switch (argv[arg][0]) {
                                        case 'R': mode = 1; break;
                                        case 'A': mode = 2; break;
                                        case 'P': mode = 3; exmode = PIPE; break;
                                        default: 
                                                  return warning("%s: invalid mode name: %s.  Try REPLACE, APPEND, or "
                                                                  "PIPE.", argv[0], argv[arg]);
                                }
                                break;
                        default: return usageError(argv[0], usage);
                }
        }
        if (arg != argc) return usageError(argv[0], usage);

        if (!setName) {
                if (eval(".exampleSetRoot {%s}", argv[1]))
                        return TCL_ERROR;
                setName = copyString(Tcl_GetStringResult(Interp));
                madeName = TRUE;
        }

        startTask(LOADING_EXAMPLES);
        eval(".setPath _examples %s", argv[1]);
        code = loadExamples(setName, argv[1], mode, numExamples);
        if ((S = lookupExampleSet(setName)) && S->mode != exmode)
                exampleSetMode(S, exmode);
        if (code == TCL_OK) result(setName);
        if (madeName) FREE(setName);
        stopTask(LOADING_EXAMPLES);
        return code;
}

int C_useTrainingSet(TCL_CMDARGS) {
        int s;
        ExampleSet S;

        char *usage = "useTrainingSet [<example-set>]";
        if (argc == 2 && !strcmp(argv[1], "-h")) 
                return commandHelp(argv[0]);
        if (argc > 2)
                return usageError(argv[0], usage);

        /* If no arguments, return a list of the training sets */
        if (argc == 1) {
                Tcl_ResetResult(interp);
                for (s = 0; s < Root->numExampleSets; s++) {
                        S = Root->set[s];
                        /* if (S->checkCompatibility(S) == TCL_OK) */
                        append("\"%s\" ", S->name);
                }
                return TCL_OK;
        }

        if (!Net) return warning("%s: no current network", argv[0]);
        if (unsafeToRun(argv[0], NETWORK_TASKS)) return TCL_ERROR;
        if (!argv[1][0]) S = NULL;
        else {   /* Otherwise, lookup the specified set and use it */
                if (!(S = lookupExampleSet(argv[1])))
                        return warning("%s: example set \"%s\" doesn't exist", argv[0], argv[1]);
                /*
                   if (S->checkCompatibility(S))
                   return warning("%s: set \"%s\" isn't compatible with the "
                   "current network", argv[0], argv[1]);
                   */
        }
        if (S == Net->trainingSet) return TCL_OK;
        return useTrainingSet(S);
}

int C_useTestingSet(TCL_CMDARGS) {
        int s;
        ExampleSet S;

        char *usage = "useTestingSet [<example-set>]";
        if (argc == 2 && !strcmp(argv[1], "-h")) 
                return commandHelp(argv[0]);
        if (argc > 2)
                return usageError(argv[0], usage);

        /* If no arguments, return a list of the networks */
        if (argc == 1) {
                Tcl_ResetResult(interp);
                for (s = 0; s < Root->numExampleSets; s++) {
                        S = Root->set[s];
                        /* if (S->checkCompatibility(S) == TCL_OK) */
                        append("\"%s\" ", S->name);
                }
                return TCL_OK;
        }
        if (!Net) return warning("%s: no current network", argv[0]);
        if (unsafeToRun(argv[0], NETWORK_TASKS)) return TCL_ERROR;
        if (!argv[1][0]) S = NULL;
        else {   /* Otherwise, lookup the specified set and use it */
                if (!(S = lookupExampleSet(argv[1])))
                        return warning("%s: example set \"%s\" doesn't exist", argv[0], argv[1]);
                /*
                   if (S->checkCompatibility(S))
                   return warning("%s: set \"%s\" isn't compatible with the "
                   "current network", argv[0], argv[1]);
                   */
        }
        if (S == Net->testingSet) return TCL_OK;
        return useTestingSet(S);
}

int C_deleteExampleSets(TCL_CMDARGS) {
        char *usage = "deleteExampleSets <example-set-list>";

        if (argc == 2 && !strcmp(argv[1], "-h")) 
                return commandHelp(argv[0]);
        if (argc != 2) return usageError(argv[0], usage);
        if (unsafeToRun(argv[0], NETWORK_TASKS)) return TCL_ERROR;

        if (!strcmp(argv[1], "*")) deleteAllExampleSets();
        else FOR_EACH_SET_IN_LIST(argv[1], deleteExampleSet(S));

        signalSetsChanged(TRAIN_SET);
        signalSetsChanged(TEST_SET);
        signalSetListsChanged();
        return TCL_OK;
}

int C_moveExamples(TCL_CMDARGS) {
        int first = 0, num = 0;
        real proportion = 1.0;
        flag copy = FALSE;
        ExampleSet S;
        char *usage = "moveExamples <example-set1> <example-set2>\n"
                "\t([<first-example> [<num-examples>]] | <proportion> | ) [-copy]";
        if (argc == 2 && !strcmp(argv[1], "-h")) 
                return commandHelp(argv[0]);
        if (argc < 3 || argc > 6)
                return usageError(argv[0], usage);
        if (!argv[2][0]) 
                return warning("%s: example set must have a real name", argv[0]);

        if (!(S = lookupExampleSet(argv[1])))
                return warning("%s: example set \"%s\" doesn't exist", argv[0], argv[1]);
        if (argv[argc-1][0] == '-' && argv[argc-1][1] == 'c') {
                copy = TRUE;
                argc--;
        }
        /* if (argc == 3) return moveAllExamples(S, argv[2]); */
        if (argc > 3) {
                first = atoi(argv[3]);
                if (argc == 5) {
                        num = atoi(argv[4]);
                } else if (argc == 4) {
                        if ((first == 0 && strcmp(argv[3], "0")) ||
                                        (first == 1 && strcmp(argv[3], "1"))) {
                                proportion = ator(argv[3]);
                        } else num = 1;
                }
        }
        return moveExamples(S, argv[2], first, num, proportion, copy);
}

int C_saveExamples(TCL_CMDARGS) {
        int arg;
        ExampleSet S;
        flag code, binary = FALSE, append = FALSE;
        char *usage = "saveExamples <example-set> <file-name> [-binary | -append]";
        if (argc == 2 && !strcmp(argv[1], "-h")) 
                return commandHelp(argv[0]);
        if (argc < 3) return usageError(argv[0], usage);
        if (unsafeToRun(argv[0], LOADING_EXAMPLES | SAVING_EXAMPLES)) 
                return TCL_ERROR;
        if (!(S = lookupExampleSet(argv[1])))
                return warning("%s: example set \"%s\" doesn't exist", argv[0], argv[1]);

        for (arg = 3; arg < argc && argv[arg][0] == '-'; arg++) {
                switch (argv[arg][1]) {
                        case 'b': binary = TRUE; break;
                        case 'a': append = TRUE; break;
                        default: return usageError(argv[0], usage);
                }
        }
        if (arg != argc) return usageError(argv[0], usage);

        if (binary && append)
                return warning("saveExamples: you cannot use both the -binary and -append flags");

        startTask(SAVING_EXAMPLES);
        if (binary)
                code = writeBinaryExampleFile(S, argv[2]);
        else
                code = writeExampleFile(S, argv[2], append);
        stopTask(SAVING_EXAMPLES);
        if (code) return TCL_ERROR;
        return result(argv[2]);
}

int C_exampleSetMode(TCL_CMDARGS) {
        mask mode;
        char *modeName;
        char *usage = "exampleSetMode <example-set-list> [<mode>]";
        if (argc == 2 && !strcmp(argv[1], "-h")) 
                return commandHelp(argv[0]);
        if (argc != 2 && argc != 3)
                return usageError(argv[0], usage);

        if (argc == 3) {
                if (lookupTypeMask(argv[2], EXAMPLE_MODE, &mode)) {
                        warning("%s: invalid example mode: \"%s\"\n"
                                        "Try one of the following:\n", argv[0], argv[2]);
                        printTypes("Example Modes", EXAMPLE_MODE, ~0);
                        return TCL_ERROR;
                }
        }
        result("");
        FOR_EACH_SET_IN_LIST(argv[1], {
                        if (argc == 2) {
                        if (!(modeName = (char *) lookupTypeName(S->mode, EXAMPLE_MODE)))
                        return warning("%s: example set \"%s\" has an invalid mode: %d\n",
                                argv[0], S->name, S->mode);
                        append("{%s} %s\n", S->name, modeName);
                        } else if (exampleSetMode(S, mode)) return TCL_ERROR;
                        });
        return TCL_OK;
}

int C_resetExampleSets(TCL_CMDARGS) {
        char *usage = "resetExampleSets <example-set-list>";
        if (argc == 2 && !strcmp(argv[1], "-h")) 
                return commandHelp(argv[0]);
        if (argc != 2)
                return usageError(argv[0], usage);

        FOR_EACH_SET_IN_LIST(argv[1], {
                        resetExampleSet(S);
                        if (Net && UnitUp && 
                                (((Net->trainingSet == S) && Net->unitDisplaySet == TRAIN_SET) ||
                                 ((Net->testingSet == S) && Net->unitDisplaySet == TEST_SET)))
                        updateUnitDisplay();
                        });
        return TCL_OK;
}

int C_doExample(TCL_CMDARGS) {
        ExampleSet S = NULL;
        int arg = 1, num = -1;
        char testing = 0;
        char *usage = "doExample [<example-num>] [-set <example-set> | "
                "-train | -test]";
        if (argc == 2 && !strcmp(argv[1], "-h")) 
                return commandHelp(argv[0]);
        if (!Net) return warning("%s: no current network", argv[0]);

        if (argc >= 2 && isInteger(argv[1])) {
                num = atoi(argv[1]);
                arg = 2;
        }

        for (; arg < argc && argv[arg][0] == '-'; arg++) {
                switch (argv[arg][1]) {
                        case 's':
                                if (++arg >= argc) return usageError(argv[0], usage);
                                if (!(S = lookupExampleSet(argv[arg])))
                                        return warning("%s: example set \"%s\" doesn't exist", 
                                                        argv[0], argv[arg]);
                                break;
                        case 't':
                                if (subString(argv[arg]+1, "train", 2)) {
                                        Net->netRunExample = Net->netTrainExample;
                                        Net->netRunTick = Net->netTrainTick;
                                } else if (subString(argv[arg]+1, "test", 2)) {
                                        Net->netRunExample = Net->netTestExample;
                                        Net->netRunTick = Net->netTestTick;
                                        testing = 1;
                                } else return usageError(argv[0], usage);
                                break;
                        default: return usageError(argv[0], usage);
                }
        }
        if (arg != argc) return usageError(argv[0], usage);

        if (!S) {
                if (Net->trainingSet && (!Net->testingSet || !testing)) 
                        S = Net->trainingSet;
                else if (Net->testingSet) S = Net->testingSet;
                else return warning("%s: there are no current example sets", argv[0]);
        }
        if (Net->netRunExampleNum(num, S)) return TCL_ERROR;
        updateDisplays(ON_EXAMPLE);
        return TCL_OK;
}

void registerExampleCommands(void) {
        registerCommand(C_loadExamples, "loadExamples", 
                        "reads an example file into an example set");
        registerCommand(C_useTrainingSet, "useTrainingSet", 
                        "makes an example set the current training set");
        registerCommand(C_useTestingSet, "useTestingSet", 
                        "makes an example set the current testing set");
        /*
           registerCommand(C_listExampleSets, "listExampleSets",
           "returns a list of all example sets");
           */
        registerCommand(C_deleteExampleSets, "deleteExampleSets",
                        "deletes a list of example sets");
        registerCommand(C_moveExamples, "moveExamples",
                        "moves examples from one set to another");
        /*
           registerCommand(C_moveAllExamples, "moveAllExamples",
           "moves all examples from one set to another");
           */
        registerCommand(C_saveExamples, "saveExamples",
                        "writes an example set to an example file");
        registerCommand(C_exampleSetMode, "exampleSetMode",
                        "sets or returns the example set's example selection mode");
        registerCommand(C_resetExampleSets, "resetExampleSets",
                        "returns the example set to the first example");
        registerCommand(C_doExample, "doExample",
                        "runs the network on the next or a specified example");
}
