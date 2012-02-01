/* trainCom.c */

#include <string.h>

#include "system.h"
#include "util.h"
#include "type.h"
#include "network.h"
#include "train.h"
#include "act.h"
#include "command.h"
#include "control.h"
#include "display.h"
#include "graph.h"

#include "main.h"

int C_train(TCL_CMDARGS) {
        int arg = 1;
        flag result, train = TRUE;
        char *usage = "train [<num-updates>] [-report <report-interval> | -algorithm"
                " <algorithm> | -setOnly]";
        if (argc == 2 && !strcmp(argv[1], "-h")) 
                return commandHelp("train");

        if (!Net) return warning("%s: no current network", argv[0]);

        if (argc > 1 && isInteger(argv[1])) {
                Net->numUpdates = atoi(argv[1]);
                arg = 2;
        }

        for (; arg < argc && argv[arg][0] == '-'; arg++) {
                switch (argv[arg][1]) {
                        case 'r':
                                if (++arg >= argc) return usageError(argv[0], usage);
                                Net->reportInterval = atoi(argv[arg]);
                                break;
                        case 'a':
                                if (++arg >= argc) return usageError(argv[0], usage);
                                if (lookupTypeMask(argv[arg], ALGORITHM, &(Net->algorithm)))
                                        return warning("%s: unrecognized algorithm: %s", argv[0], argv[arg]);
                                break;
                        case 's':
                                train = FALSE;
                                break;
                        default: return usageError(argv[0], usage);
                }
        }
        if (arg != argc) return usageError(argv[0], usage);

        Tcl_Eval(Interp, ".getParameters");
        if (!train) return TCL_OK;
        startTask(TRAINING);
        result = Net->netTrain();
        stopTask(TRAINING);
        return result;
}

int C_updateWeights(TCL_CMDARGS) {
        Algorithm A;
        int arg = 1;
        mask alg;
        flag reset = TRUE, report = FALSE;
        char *usage = "updateWeights [-algorithm <algorithm> | -noreset | -report]";

        if (argc == 2 && !strcmp(argv[1], "-h")) return commandHelp(argv[0]);
        if (!Net) return warning("%s: no current network", argv[0]);
        alg = Net->algorithm;

        for (; arg < argc && argv[arg][0] == '-'; arg++) {
                switch (argv[arg][1]) {
                        case 'n':
                                reset = FALSE;
                                break;
                        case 'r':
                                report = TRUE;
                                break;
                        case 'a':
                                if (++arg >= argc) return usageError(argv[0], usage);
                                if (lookupTypeMask(argv[arg], ALGORITHM, &alg))
                                        return warning("%s: unrecognized algorithm: %s", argv[0], argv[arg]);
                                break;
                        default: return usageError(argv[0], usage);
                }
        }
        A = getAlgorithm(alg);
        A->updateWeights(report);
        RUN_PROC(postUpdateProc);
        if (report) printReport(0, Net->numUpdates, 0);
        if (reset) resetDerivs();
        return TCL_OK;
}

int C_test(TCL_CMDARGS) {
        int arg = 1, numExamples = 0;
        flag retval, resetError = TRUE, print = TRUE;
        char *res;
        char *usage = "test [<num-examples>] [-noreset | -return]";
        if (argc == 2 && !strcmp(argv[1], "-h")) return commandHelp(argv[0]);
        if (argc > 3) return usageError(argv[0], usage);

        if (!Net) return warning("test: no current network");
        if (argc > 1 && isInteger(argv[1])) {
                if ((numExamples = atoi(argv[1])) < 0) 
                        return warning("%s: number of examples (%d) can't be negative", 
                                        argv[0], numExamples);
                arg = 2;
        }
        for (; arg < argc; arg++) {
                if (subString(argv[arg], "-noreset", 2)) resetError = FALSE;
                else if (subString(argv[arg], "-return", 2)) print = FALSE;
                else return usageError(argv[0], usage);
        }

        startTask(TESTING);
        retval = Net->netTestBatch(numExamples, resetError, print);
        res = copyString(Tcl_GetStringResult(Interp));
        stopTask(TESTING);
        result(res);
        return retval;
}

int C_resetNet(TCL_CMDARGS) {
        char *usage = "resetNet";
        if (argc == 2 && !strcmp(argv[1], "-h")) return commandHelp(argv[0]);
        if (argc != 1) return usageError(argv[0], usage);
        if (!Net) return warning("%s: no current network", argv[0]);

        if (Net->resetNet(TRUE)) return TCL_ERROR;
        updateUnitDisplay();
        updateLinkDisplay();
        resetGraphs();
        return TCL_OK;
}

int C_resetDerivs(TCL_CMDARGS) {
        char *usage = "resetDerivs";
        if (argc == 2 && !strcmp(argv[1], "-h")) return commandHelp(argv[0]);
        if (argc != 1) return usageError(argv[0], usage);
        if (!Net) return warning("%s: no current network", argv[0]);

        if (resetDerivs()) return TCL_ERROR;
        return TCL_OK;
}

int C_openNetOutputFile(TCL_CMDARGS) {
        int arg;
        flag binary = FALSE, append = FALSE;
        char *usage = "openNetOutputFile <file-name> [-binary | -append]";
        if (argc == 2 && !strcmp(argv[1], "-h")) return commandHelp(argv[0]);
        if (argc < 2) return usageError(argv[0], usage);
        if (!Net) return warning("%s: no current network", argv[0]);

        for (arg = 2; arg < argc; arg++) {
                if (argv[arg][0] != '-') return usageError(argv[0], usage);
                switch (argv[arg][1]) {
                        case 'b':
                                binary = TRUE;
                                break;
                        case 'a':
                                append = TRUE;
                                break;
                        default: return usageError(argv[0], usage);
                }
        }

        eval("set _record(path) [.normalizeDir {} [file dirname %s]];"
                        "set _record(file) [file tail %s]", argv[1], argv[1]);

        if (openNetOutputFile(argv[1], binary, append)) return TCL_ERROR;
        if (Gui) return eval(".trainingControlPanel.r.outp configure -text "
                        "\"Stop Recording\" -command closeNetOutputFile");
        else return TCL_OK;
}

int C_closeNetOutputFile(TCL_CMDARGS) {
        char *usage = "closeNetOutputFile";
        if (argc == 2 && !strcmp(argv[1], "-h")) return commandHelp(argv[0]);
        if (argc != 1)
                return usageError(argv[0], usage);
        if (!Net) return warning("%s: no current network", argv[0]);

        if (closeNetOutputFile()) return TCL_ERROR;
        if (Gui) return eval(".trainingControlPanel.r.outp configure -text "
                        "\"Record Outputs\" -command .openOutputFile");
        else return TCL_OK;
}

#ifdef JUNK
int C_testBinary(TCL_CMDARGS) {
        int arg = 1, numExamples = 0;
        flag result, resetError = TRUE;
        char *usage = "test [<num-examples>] [-noreset]";
        if (argc == 2 && !strcmp(argv[1], "-h")) return commandHelp(argv[0]);
        if (argc > 3) return usageError(argv[0], usage);

        if (!Net) return warning("test: no current network");
        if (argc > 1 && isInteger(argv[1])) {
                if ((numExamples = atoi(argv[1])) < 0) 
                        return warning("%s: number of examples (%d) can't be negative", 
                                        argv[0], numExamples);
                arg = 2;
        }
        if (argc > arg) {
                if (subString(argv[arg], "-noreset", 2)) resetError = FALSE;
                else return usageError(argv[0], usage);
        }

        startTask(TESTING);
        result = Net->netTestBatch(numExamples, resetError);
        stopTask(TESTING);
        return result;
}

int C_testCost(TCL_CMDARGS) {
        Group G;
        Unit U;
        int i, steps = 100;
        real r;
        char *usage = "testCost <group>";
        if (argc == 2 && !strcmp(argv[1], "-h")) return commandHelp(argv[0]);
        if (argc != 2) return usageError(argv[0], usage);
        if (!(G = lookupGroup(argv[1])))
                return warning("no such group");
        Net->outputCostStrength = 1.0;
        U = G->unit;

        result("");
        for (i = 0; i <= steps; i++) {
                r = (real) i / steps;
                U->output = r;
                append("%.2g ", r);

                Net->outputCost = 0.0;
                U->outputDeriv = 0.0;
                boundedLinearCost(G);
                boundedLinearCostBack(G);
                append("%f %f  ", Net->outputCost, U->outputDeriv);

                Net->outputCost = 0.0;
                U->outputDeriv = 0.0;
                boundedQuadraticCost(G);
                boundedQuadraticCostBack(G);
                append("%f %f  ", Net->outputCost, U->outputDeriv);

                Net->outputCost = 0.0;
                U->outputDeriv = 0.0;
                convexQuadraticCost(G);
                convexQuadraticCostBack(G);
                append("%f %f  ", Net->outputCost, U->outputDeriv);

                Net->outputCost = 0.0;
                U->outputDeriv = 0.0;
                logisticCost(G);
                logisticCostBack(G);
                append("%f %f  ", Net->outputCost, U->outputDeriv);

                Net->outputCost = 0.0;
                U->outputDeriv = 0.0;
                cosineCost(G);
                cosineCostBack(G);
                append("%f %f\n", Net->outputCost, U->outputDeriv);
        }

        return TCL_OK;
}
#endif /*JUNK*/

void registerTrainingCommands(void) {
        /* createCommand(C_testCost, "testCost"); */

        registerCommand(C_train, "train",
                        "trains the network using a specified algorithm");
        registerSynopsis("steepest", "trains the network using steepest descent");
        registerSynopsis("momentum", "trains the network using momentum descent");
        registerSynopsis("dougsMomentum", 
                        "trains the network using normalized momentum descent");
#ifdef ADVANCED
        registerSynopsis("deltaBarDelta",
                        "trains the network using delta-bar-delta");
        /*
           registerSynopsis("quickProp", 
           "trains the network using the quick-prop algorithm");
           */
#endif /* ADVANCED */
        registerCommand(C_updateWeights, "updateWeights", 
                        "updates the weights using the current link derivatives");
        registerCommand(C_test, "test", "tests the network on the testing set");
        registerCommand(C_resetNet, "resetNet",
                        "randomizes unfrozen weights and clears direction info.");
        registerCommand(C_resetDerivs, "resetDerivs",
                        "clears the unit and link error derivatives.");
        registerCommand(C_openNetOutputFile, "openNetOutputFile",
                        "begins writing OUTPUT group outputs to a file");
        registerCommand(C_closeNetOutputFile, "closeNetOutputFile",
                        "stops writing OUTPUT group outputs to the output file");
}
