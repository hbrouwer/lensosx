/* objectCom.c */

#include <string.h>

#include "system.h"
#include "util.h"
#include "type.h"
#include "network.h"
#include "object.h"
#include "command.h"
#include "control.h"

#define MAX_ARRAY  60
#define MAX_FIELDS 100

int C_getObject(TCL_CMDARGS) {
        int type, rows, cols, arg = 1;
        flag writable;
        ObjInfo O;
        char *object, *objName = "";
        int depth, additionalDepth = 1;

        char *usage = "getObject [<object>] [-depth <max-depth>]";
        if (argc == 2 && !strcmp(argv[1], "-h")) 
                return commandHelp(argv[0]);
        if (argc > 1 && argv[1][0] != '-') {
                objName = argv[1];
                arg = 2;
        }
        if (arg < argc) {
                if (arg != argc - 2) return usageError(argv[0], usage);
                if (subString(argv[arg], "-depth", 2))
                        additionalDepth = atoi(argv[++arg]);
                else return usageError(argv[0], usage);
        }

        if (!(object = getObject(objName, &O, &type, &rows, &cols, &writable)))
                return TCL_ERROR;

        depth = MAX(O->maxDepth, 0);
        Tcl_ResetResult(Interp);
        printObject(object, O, type, rows, cols, depth, depth, 
                        O->maxDepth + additionalDepth);

        return TCL_OK;
}

int C_setObject(TCL_CMDARGS) {
        int type, valoffset, rows, cols;
        flag writable;
        char *object;
        ObjInfo O;

        char *usage = "setObject <object-member> [<value>]";
        if (argc == 2 && !strcmp(argv[1], "-h"))
                return commandHelp(argv[0]);
        if (argc != 2 && argc != 3)
                return usageError(argv[0], usage);
        if (argc == 2)
                return C_getObject(data, interp, argc, argv);

        if (!(object = getObject(argv[1], &O, &type, &rows, &cols, &writable)))
                return TCL_ERROR;

        if (!O->setStringValue) 
                return warning("%s: \"%s\" is not a basic type and cannot be set", argv[0],
                                argv[1]);
        if (!writable)
                return warning("%s is write-protected", argv[1]);
        O->setStringValue(object, argv[2]);
        sprintf(Buffer, "uplevel #0 set \".%s\" ", argv[1]);
        valoffset = strlen(Buffer);
        O->getName(object, Buffer + valoffset);

        Tcl_Eval(interp, Buffer);
        Tcl_SetResult(interp, Buffer + valoffset, TCL_VOLATILE);

        return TCL_OK;
}

int C_path(TCL_CMDARGS) {
        char *usage = "path (-network <network> | -group <group> | -unit <unit> |\n"
                "\t-set <example-set>)";
        if (argc == 2 && !strcmp(argv[1], "-h"))
                return commandHelp(argv[0]);
        if (argc != 3) return usageError(argv[0], usage);
        if (argv[1][0] != '-') return usageError(argv[0], usage);
        switch (argv[1][1]) {
                case 'n': {
                                  Network N;
                                  if (!(N = lookupNet(argv[2])))
                                          return warning("%s: network \"%s\" doesn't exist", argv[0], argv[2]);
                                  return result("root.net(%d)", N->num);
                          }
                case 'g': {
                                  Group G;
                                  if (!(G = lookupGroup(argv[2])))
                                          return warning("%s: group \"%s\" doesn't exist", argv[0], argv[2]);
                                  return result("group(%d)", G->num);
                          }
                case 'u': {
                                  Unit U;
                                  if (!(U = lookupUnit(argv[2])))
                                          return warning("%s: unit \"%s\" doesn't exist", argv[0], argv[2]);
                                  return result("group(%d).unit(%d)", U->group->num, U->num);
                          }
                case 's': {
                                  ExampleSet S;
                                  if (!(S = lookupExampleSet(argv[2])))
                                          return warning("%s: example set \"%s\" doesn't exist", argv[0], argv[2]);
                                  return result("root.set(%d)", S->num);
                          }}
                          return usageError(argv[0], usage);
}

#ifdef JUNK
int C_netPath(TCL_CMDARGS) {
        Network N;
        char *usage = "netPath <network>";
        if (argc == 2 && !strcmp(argv[1], "-h"))
                return commandHelp(argv[0]);
        if (argc != 2)
                return usageError(argv[0], usage);

        if (!(N = lookupNet(argv[1])))
                return warning("%s: network \"%s\" doesn't exist", argv[0], argv[1]);
        return result("root.net(%d)", N->num);
}

int C_groupPath(TCL_CMDARGS) {
        Group G;
        char *usage = "groupPath <group>";
        if (argc == 2 && !strcmp(argv[1], "-h"))
                return commandHelp(argv[0]);
        if (argc != 2)
                return usageError(argv[0], usage);
        if (!Net) return warning("%s: no current network", argv[0]);

        if (!(G = lookupGroup(argv[1])))
                return warning("%s: group \"%s\" doesn't exist", argv[0], argv[1]);
        return result("group(%d)", G->num);
}

int C_unitPath(TCL_CMDARGS) {
        Unit U;
        char *usage = "unitPath <unit>";
        if (argc == 2 && !strcmp(argv[1], "-h"))
                return commandHelp(argv[0]);
        if (argc != 2)
                return usageError(argv[0], usage);
        if (!Net) return warning("%s: no current network", argv[0]);

        if (!(U = lookupUnit(argv[1])))
                return warning("%s: unit \"%s\" doesn't exist", argv[0], argv[1]);
        return result("group(%d).unit(%d)", U->group->num, U->num);
}

int C_setPath(TCL_CMDARGS) {
        ExampleSet S;
        char *usage = "setPath <example-set>";
        if (argc == 2 && !strcmp(argv[1], "-h"))
                return commandHelp(argv[0]);
        if (argc != 2)
                return usageError(argv[0], usage);

        if (!(S = lookupExampleSet(argv[1])))
                return warning("%s: example set \"%s\" doesn't exist", argv[0], argv[1]);
        return result("root.set(%d)", S->num);
}
#endif

static flag addField(char *win, char *name, void *object, ObjInfo O, int type, 
                int rows, int cols, flag writable, flag array) {
        char value[512], dest[128];
        if (array)
                sprintf(dest, "(%s)", name);
        else
                sprintf(dest, ".%s", name);

        switch (type) {
                case OBJ:
                case OBJP:
                        if (object) 
                                O->getName(object, value);
                        else *value = '\0';
                        if (!O->members && O->maxDepth < 0) {
                                sprintf(Buffer, ".addMember %s {%s} {%s} %d", win, name, 
                                                value, writable);
                        } else {
                                sprintf(Buffer, ".addObject %s {%s} {%s} {%s} %d", 
                                                win, name, value, dest, (object) ? 1 : 0);
                        }
                        break;
                case OBJA:
                case OBJPA:
                        sprintf(Buffer, ".addObjectArray %s {%s} {%s} {%s} %d", win, name, 
                                        O->name, dest, (object) ? 1 : 0);
                        break;
                case OBJAA:
                case OBJPAA:
                        sprintf(Buffer, ".addObjectArrayArray %s {%s} {%s} {%s} %d %d", win, 
                                        name, O->name, dest, rows, (object) ? 1 : 0);
                        break;
                default:
                        fatalError("Bad case in addField");
        }
        return Tcl_Eval(Interp, Buffer);
}

int C_sendObjectArray(TCL_CMDARGS) {
        char value[512];
        ObjInfo O;
        int i, type, rows, cols;
        flag writable;
        char *win, *menu, *object, *name, *dest;
        if (argc != 6)
                return warning("%s was called with the wrong # of arguments", argv[0]);
        win = argv[1];
        menu = argv[2];
        name = argv[4];
        dest = argv[5];

        if (!(object = getObject(argv[3], &O, &type, &rows, &cols, &writable)))
                return TCL_ERROR;
        sprintf(Buffer, ".buildObjectArray %s %s {%s} {%s} %d {", win, menu, name, 
                        dest, rows);
        for (i = 0; i < rows && i < MAX_ARRAY; i++) {
                if (type == OBJA)
                        O->getName(ObjA(object, O->size, i), value);
                else
                        O->getName(ObjPA(object, i), value);
                sprintf(Buffer + strlen(Buffer), "{%s} ", value);
        }
        strcat(Buffer, "}");
        return Tcl_Eval(Interp, Buffer);
}

/* Assumes the path is cleaned up.  If the last thing on the path is an index 
   into an array, this returns the index, else -1 */
static int arrayElement(char *path) {
        int index;
        char *s = path + strlen(path) - 1;
        if (*s != ')') return -1;
        for (s--; s >= path && *s != ',' && *s != '('; s--);
        if (s++ < path) return -1;
        sscanf(s, "%d", &index);
        return index;
}

static char *nextElement(char *path) {
        int index;
        char *new, *s = path + strlen(path) - 1;
        if (*s != ')') return NULL;
        for (s--; s >= path && *s != ',' && *s != '('; s--);
        if (s++ < path) return NULL;
        sscanf(s, "%d", &index);
        new = (char *) safeMalloc(strlen(path) + 1, "nextElement:new");
        strcpy(new, path);
        s = new + (s - path);
        sprintf(s, "%d)", index + 1);
        return new;
}

static char *prevElement(char *path) {
        int index;
        char *new, *s = path + strlen(path) - 1;
        if (*s != ')') return NULL;
        for (s--; s >= path && *s != ',' && *s != '('; s--);
        if (s++ < path) return NULL;
        sscanf(s, "%d", &index);
        new = (char *) safeMalloc(strlen(path) + 1, "nextElement:new");
        strcpy(new, path);
        s = new + (s - path);
        sprintf(s, "%d)", index - 1);
        return new;
}

/* This is not a user accessible function */
int C_loadObject(TCL_CMDARGS) {
        int i, type, rows, cols;
        flag writable;
        ObjInfo O;
        MemInfo M;
        char *object, *win, label[32];

        if (argc != 3)
                return warning("%s was called with the wrong # of arguments", argv[0]);
        win = argv[1];

        if (!(object = getObject(argv[2], &O, &type, &rows, &cols, &writable)))
                return TCL_ERROR;

        else if (type == OBJ || type == OBJPP) 
                fatalError("got OBJ type in loadObject");
        else if (type == OBJP && !O->members) {
                if (addField(win, "", object, O, OBJP, -1, -1, writable, FALSE))
                        return TCL_ERROR;
        }
        else if (type == OBJP) {
                for (M = O->members; M; M = M->next) {
                        switch (M->type) {
                                case SPACER:
                                        if (eval(".addSpacer %s", win)) return TCL_ERROR;
                                        break;
                                case OBJ:
                                        if (addField(win, M->name, Obj(object, M->offset), M->info, OBJP, 
                                                                -1, -1, M->writable, FALSE)) return TCL_ERROR;
                                        break;
                                case OBJP:
                                        if (addField(win, M->name, ObjP(object, M->offset), M->info, OBJP,
                                                                -1, -1, M->writable, FALSE)) return TCL_ERROR;
                                        break;
                                case OBJPP:
                                        if (addField(win, M->name, ObjPP(object, M->offset), M->info, OBJP,
                                                                -1, -1, M->writable, FALSE)) return TCL_ERROR;
                                        break;
                                case OBJA:
                                case OBJPA:
                                        if (addField(win, M->name, ObjP(object, M->offset), M->info, M->type,
                                                                M->rows(object), -1, M->writable, 
                                                                FALSE)) return TCL_ERROR;
                                        break;
                                case OBJAA:
                                case OBJPAA:
                                        if (addField(win, M->name, ObjP(object, M->offset), M->info, M->type,
                                                                M->rows(object), M->cols(object), M->writable,
                                                                FALSE)) return TCL_ERROR;
                                        break;
                        }
                }
        }
        else if (type == OBJA || type == OBJPA) {
                for (i = 0; i < rows && i < MAX_FIELDS; i++) {
                        sprintf(label, "%d", i);
                        if (type == OBJA) {
                                if (addField(win, label, ObjA(object, O->size, i), O, OBJP, 
                                                        -1, -1, writable, TRUE)) return TCL_ERROR;
                        } else {
                                if (addField(win, label, ObjPA(object, i), O, OBJP,
                                                        -1, -1, writable, TRUE)) return TCL_ERROR;
                        }
                }
                if (i >= MAX_FIELDS)
                        eval(".addLabel %s toobig \"Array(%d) too large to display\"", 
                                        win, rows);
        }
        else if (type == OBJAA || type == OBJPAA) {
                for (i = 0; i < rows && i < MAX_FIELDS; i++) {
                        sprintf(label, "%d", i);
                        if (type == OBJAA) {
                                if (addField(win, label, ObjPA(object, i), O, OBJA, 
                                                        cols, -1, writable, TRUE)) return TCL_ERROR;
                        } else {
                                if (addField(win, label, ObjPA(object, i), O, OBJPA, 
                                                        cols, -1, writable, TRUE)) return TCL_ERROR;
                        }
                }
                if (i >= MAX_FIELDS)
                        eval(".addLabel %s toobig \"Array(%d) too large to display\"", 
                                        win, rows);
        }
        if ((i = arrayElement(argv[2])) >= 0) {
                char *newpath = nextElement(argv[2]);
                if (!newpath) return warning("nextElement failed on %s", argv[2]);
                if (i > 0) eval(".setArrayCanGoLeft %s", win);

                if (getObject(newpath, &O, &type, &rows, &cols, &writable))
                        eval(".setArrayCanGoRight %s", win);
                FREE(newpath);
        }

        return TCL_OK;
}

/* This is not a user accessible function */
int C_cleanPath(TCL_CMDARGS) {
        int count;
        char *r, *w;
        if (argc != 2)
                return warning("%s was called with the wrong # of arguments", argv[0]);
        for (r = argv[1], w = argv[1], count = 0; *r; r++) {
                if (*r == '[') *r = '(';
                else if (*r == ']') *r = ')';
                if (*r == '(') count++;
                else if (*r == ')') count--;
        }

        for (r = argv[1], w = argv[1]; *r; r++) {
                if (*r == ')' && r[1] == '(') {
                        *(w++) = ',';
                        r++;
                }
                else if (*r == '.' && r[1] == '(') {
                        *(w++) = '(';
                        r++;
                } 
                else if (!r[1] && (*r == '.' || *r == '(')) {
                        if (*r == '(') count--;
                        *w = '\0';
                }
                else if (!(r == argv[1] && *r == '.'))
                        *(w++) = *r;
        }
        *w = '\0';
        result(argv[1]);
        if (count > 0) append(")");
        return TCL_OK;
}

/* This is not a user accessible function */
int C_pathParent(TCL_CMDARGS) {
        char *r;
        if (argc != 2)
                return warning("%s was called with the wrong # of arguments", argv[0]);
        for (r = argv[1] + strlen(argv[1]) - 1; r >= argv[1] && *r != '.'; r--);
        if (r >= argv[1]) *r = '\0';
        else argv[1][0] = '\0';
        return result(argv[1]);
}

/* This is not a user accessible function */
int C_pathUp(TCL_CMDARGS) {
        char *r;
        if (argc != 2)
                return warning("%s was called with the wrong # of arguments", argv[0]);
        for (r = argv[1] + strlen(argv[1]) - 1; 
                        r >= argv[1] && *r != '.' && *r != '(' && *r != ','; r--);
        if (r >= argv[1]) {
                if (*r == ',')
                        *(r++) = ')';
                *r = '\0';
        } else argv[1][0] = '\0';
        return result(argv[1]);
}

/* This is not a user accessible function */
int C_pathRight(TCL_CMDARGS) {
        char *newpath;
        if (argc != 2)
                return warning("%s was called with the wrong # of arguments", argv[0]);
        if (!(newpath = nextElement(argv[1])))
                return warning(".pathRight failed on %s", argv[1]);
        result(newpath);
        FREE(newpath);
        return TCL_OK;
}

/* This is not a user accessible function */
int C_pathLeft(TCL_CMDARGS) {
        char *newpath;
        if (argc != 2)
                return warning("%s was called with the wrong # of arguments", argv[0]);
        if (!(newpath = prevElement(argv[1])))
                return warning(".pathLeft failed on %s", argv[1]);
        result(newpath);
        FREE(newpath);
        return TCL_OK;
}

int C_saveParameters(TCL_CMDARGS) {
        Tcl_Channel channel;
        int arg = 2;
        char groupName[32];
        flag append = FALSE;
        char *usage = "saveParameters <file-name> [-append] [<object> ...]";
        if (argc == 2 && !strcmp(argv[1], "-h")) 
                return commandHelp(argv[0]);
        if (argc < 2)
                return usageError(argv[0], usage);
        if (!Net) return warning("%s: no current network", argv[0]);

        if (argc > arg && subString(argv[arg], "-append", 2)) {
                append = TRUE;
                arg++;
        }
        if (!(channel = writeChannel(argv[1], append)))
                return warning("saveParameters: couldn't open the file \"%s\"", argv[1]);

        if (argc == arg) {
                if (writeParameters(channel, "")) {
                        closeChannel(channel);
                        return TCL_ERROR;
                }
                FOR_EACH_GROUP({
                                sprintf(groupName, "group(%d)", g);
                                if (writeParameters(channel, groupName)) {
                                closeChannel(channel);
                                return TCL_ERROR;
                                }
                                });
        } else for (; arg < argc; arg++)
                if (writeParameters(channel, argv[arg])) {
                        closeChannel(channel);
                        return TCL_ERROR;
                }

        closeChannel(channel);
        return TCL_OK;
}

void registerObjectCommands(void) {
        registerCommand(C_setObject, "setObject", 
                        "sets the value of an object field");
        registerCommand(C_getObject, "getObject", 
                        "prints the contents of an object");
        registerCommand(C_path, "path", 
                        "returns the full object path name of a major object");
        /*
           registerCommand(C_groupPath, "groupPath", 
           "returns the full object path name of the group");
           registerCommand(C_unitPath, "unitPath", 
           "returns the full object path name of the unit");
           registerCommand(C_setPath, "setPath", 
           "returns the full object path name of the example set");
           */
        registerCommand(C_saveParameters, "saveParameters",
                        "writes network or group parameters to a script file");

        createCommand(C_loadObject, ".loadObject");
        createCommand(C_sendObjectArray, ".sendObjectArray");
        createCommand(C_cleanPath, ".cleanPath");
        createCommand(C_pathParent, ".pathParent");
        createCommand(C_pathUp, ".pathUp");
        createCommand(C_pathRight, ".pathRight");
        createCommand(C_pathLeft, ".pathLeft");
}
