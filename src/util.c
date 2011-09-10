/* util.c */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/types.h> /* May be necessary for stat. */
#include <sys/stat.h>
#include <sys/time.h>  /* For gettimeofday. */
#include <ctype.h>
#include <netinet/in.h>
#include <unistd.h>

#include "util.h"
#include "type.h"
#include "parallel.h"
#include "defaults.h"

#define MAX_BLOCK_DEPTH 32
#define MAX_FILENAME    512

Tcl_Interp *Interp;
char Buffer[BUFFER_SIZE];
unsigned int lastSeed = 0;
flag Gui = DEF_GUI, Batch = DEF_BATCH, Console = DEF_CONSOLE;
int Verbosity = DEF_VERBOSITY;
union nanfunion NaNfunion = NaNfbytes;
union nandunion NaNdunion = NaNdbytes;
char *RootDir;

/********************************** Messages *********************************/

extern void TkConsolePrint _ANSI_ARGS_((Tcl_Interp *interp,
                        int devId, char *buffer, long size));

void print(int minVerbosity, char *fmt, ...) {
        va_list args;
        if (fmt == Buffer) {
                error("print() called on Buffer, report this to Doug");
                return;
        }
        if (minVerbosity > Verbosity) return;
        va_start(args, fmt);
        if (Console) {
                vsprintf(Buffer, fmt, args);
                TkConsolePrint(Interp, TCL_STDOUT, Buffer, strlen(Buffer));
        } else {
                vprintf(fmt, args);
                fflush(stdout);
        }
}

void debug(char *fmt, ...) {
        va_list args;
        if (fmt == Buffer) {
                error("debug() called on Buffer, report this to Doug");
                return;
        }
        va_start(args, fmt);
        if (Console) {
                vsprintf(Buffer, fmt, args);
                TkConsolePrint(Interp, TCL_STDOUT, Buffer, strlen(Buffer));
        } else {
                vfprintf(stderr, fmt, args);
                fflush(stderr);
        }
}

flag result(char *fmt, ...) {
        va_list args;
        if (!fmt[0]) {
                Tcl_ResetResult(Interp);
                return TCL_OK;
        }
        if (fmt == Buffer) {
                error("result() called on Buffer, report this to Doug");
                return TCL_OK;
        }
        va_start(args, fmt);
        vsprintf(Buffer, fmt, args);
        Tcl_SetResult(Interp, Buffer, TCL_VOLATILE);
        va_end(args);
        return TCL_OK;
}

/* Note that Tcl_AppendResult apparently tries to treat the string as a format
   and thus you have to turn %'s in to %%'s in anything you pass in here. */
flag append(char *fmt, ...) {
        va_list args;
        if (fmt == Buffer) {
                error("append() called on Buffer, report this to Doug");
                return TCL_OK;
        }
        va_start(args, fmt);
        vsprintf(Buffer, fmt, args);
        Tcl_AppendResult(Interp, Buffer, NULL);
        va_end(args);
        return TCL_OK;
}

flag warning(char *fmt, ...) {
        char *buf = Buffer;
        va_list args;
        if (fmt == Buffer) {
                error("warning() called on Buffer, report this to Doug");
                return TCL_ERROR;
        }
        va_start(args, fmt);
        if (Batch) {
                sprintf(buf, "Error: ");
                buf += 7;
        }
        vsprintf(buf, fmt, args);
        Tcl_SetResult(Interp, Buffer, TCL_VOLATILE);
        va_end(args);
        return TCL_ERROR;
}

int error(char *fmt, ...) {
        va_list args;
        if (fmt == Buffer) {
                return error("error() called on Buffer, report this to Doug");
        }
        va_start(args, fmt);
        if (!Batch && !ParallelState) {
                sprintf(Buffer, "bgerror {");
                vsprintf(Buffer + strlen(Buffer), fmt, args);
                strcat(Buffer, "}");
                Tcl_EvalEx(Interp, Buffer, -1, TCL_EVAL_GLOBAL);
        } else {
                beep();
                fprintf(stderr, "Error: ");
                vfprintf(stderr, fmt, args);
                fprintf(stderr, "\n");
        }
        /* Tcl_SetResult(Interp, Buffer, TCL_VOLATILE); */
        va_end(args);
        return TCL_ERROR;
}

/* Invoked for a programming error.  The user should not be able to cause 
   this. */
void fatalError(char *fmt, ...) {
        va_list args;
        va_start(args, fmt);
        beep();
        fprintf(stderr, "FATAL ERROR: ");
        vfprintf(stderr, fmt, args);
        fprintf(stderr, "\n");
        va_end(args);
        exit(1);
}

flag eval(char *fmt, ...) {
        char *command;
        flag result;
        va_list args;
        if (fmt == Buffer) {
                error("eval() called on Buffer, report this to Doug");
                return TCL_ERROR;
        }
        va_start(args, fmt);
        vsprintf(Buffer, fmt, args);
        command = copyString(Buffer);
        result = Tcl_EvalEx(Interp, command, -1, TCL_EVAL_GLOBAL);
        va_end(args);
        FREE(command);
        return result;
}

void beep(void) {
        fputc('\a', stderr);
        fflush(stderr);
}


/********************************* Allocation ********************************/

/* Returning NULL is useful when someone allocates an array of size 0 */
void *safeMalloc(unsigned size, char *name) {
        void *new;
        if (size == 0) return NULL;
        new = malloc(size);
        if (!new) fatalError("failed to allocate %s of size %d", name, size);
        return new;
}

void *safeCalloc(unsigned num, unsigned size, char *name) {
        void *new;
        if (size == 0 || num == 0) return NULL;
        new = calloc(num, size);
        if (!new) fatalError("failed to allocate %s of size %dx%d", name, num, size);
        return new;
}

void *safeRealloc(void *ptr, unsigned size, char *name) {
        void *new;
        if (size == 0) {
                FREE(ptr);
                return NULL;
        }
        if (ptr == NULL) return safeMalloc(size, name);
        new = realloc(ptr, size);
        if (!new) fatalError("failed to reallocate %s to size %d", name, size);
        return new;
}

int *intArray(int n, char *name) {
        return (int *) safeCalloc(n, sizeof(int), name);
}

int **intMatrix(int m, int n, char *name) {
        int i;
        int **M = (int **) safeMalloc(m * sizeof(int *), name);
        M[0] = (int *) safeCalloc(m * n, sizeof(int), name);
        for (i = 1; i < m; i++)
                M[i] = M[i - 1] + n;
        return M;
}

real *realArray(int n, char *name) {
        return (real *) safeCalloc(n, sizeof(real), name);
}

real **realMatrix(int m, int n, char *name) {
        int i;
        real **M = (real **) safeMalloc(m * sizeof(real *), name);
        M[0] = (real *) safeCalloc(m * n, sizeof(real), name);
        for (i = 1; i < m; i++)
                M[i] = M[i - 1] + n;
        return M;
}


/*********************************** Strings *********************************/

char *copyString(char *string) {
        int len;
        char *new;
        if (!string) return NULL;
        len = strlen(string) + 1;
        new = (char *) safeMalloc(len, "copyString:new");
        memcpy(new, string, len);
        return new;
}

/* Returns true if a is at least min chars long and is a substring of b */
flag subString(char *a, char *b, int minLength) {
        int i;
        for (i = 0; a[i] && a[i] == b[i]; i++);
        if (i >= minLength && !a[i]) return TRUE;
        else return FALSE;
}

/* 1 if string contains nothing but tabs and spaces */
flag isBlank(char *string) {
        char *s;
        for (s = string; *s; s++)
                if (!isspace((int) s[0])) return FALSE;
        return TRUE;
}

String newString(int maxChars) {
        String S = (String) safeMalloc(sizeof *S, "NewString:S");
        S->maxChars = maxChars;
        if (maxChars < 1) S->maxChars = 1;
        S->numChars = 0;
        S->s = (char *) safeMalloc(maxChars, "NewString:S->s");
        S->s[0] = '\0';
        return S;
}

String newStringCopy(char *s) {
        int len = strlen(s);
        String S = newString(len + 1);
        strcpy(S->s, s);
        S->numChars = len;
        return S;
}

/* Ensures that the S can hold a string of length maxChars */
void stringSize(String S, int maxChars) {
        if (S->maxChars <= maxChars) {
                S->maxChars = maxChars + 1;
                S->s = (char *) safeRealloc(S->s, S->maxChars, "StringSize:S->s");
        }
}

void stringCat(String S, char c) {
        if ((S->numChars + 1) >= S->maxChars) {
                S->maxChars *= 2;
                S->s = (char *) safeRealloc(S->s, S->maxChars, "StringCat:S->s");
        }
        S->s[S->numChars] = c;
        if (c != '\0') S->s[++S->numChars] = '\0';
}

void stringSet(String S, int idx, char c) {
        if (S->maxChars <= idx) {
                S->maxChars = ((S->maxChars * 2) > (idx + 1)) ? S->maxChars * 2 : idx + 1;
                S->s = (char *) safeRealloc(S->s, S->maxChars, "StringSet:S->s");
        }
        S->s[idx] = c;
}

void stringAppend(String S, char *t) {
        int tlen = strlen(t);
        if ((S->numChars + tlen + 1) > S->maxChars) {
                S->maxChars = ((S->maxChars * 2) > (S->numChars + tlen + 1)) ? 
                        S->maxChars * 2 : S->numChars + tlen + 1;
                S->s = (char *) safeRealloc(S->s, S->maxChars, "StringAppend:S->s");
        }
        memcpy(S->s + S->numChars, t, tlen + 1);
        S->numChars += tlen;
}

void stringAppendV(String S, char *fmt, ...) {
        va_list args;
        if (fmt == Buffer) {
                error("stringAppendV() called on Buffer, report this to Doug");
                return;
        }
        va_start(args, fmt);
        vsprintf(Buffer, fmt, args);
        stringAppend(S, Buffer);
}

void clearString(String S) {
        S->numChars = 0;
        S->s[0] = '\0';
}

void freeString(String S) {
        if (S) {
                FREE(S->s);
                free(S);
        }
}

flag readFileIntoString(String S, char *fileName) {
        int bytes;
        Tcl_Channel channel;
        if (!(channel = readChannel(fileName)))
                return warning("couldn't open file \"%s\"", fileName);
        while ((bytes = Tcl_Read(channel, S->s + S->numChars, 
                                        S->maxChars - S->numChars)) > 0) {
                S->numChars += bytes;
                if ((S->numChars * 2) > S->maxChars)
                        S->maxChars *= 2;
                S->s = (char *) safeRealloc(S->s, S->maxChars, "readFileIntoString:S->s");
        }
        S->s[S->numChars++] = '\0';
        closeChannel(channel);
        return TCL_OK;
}

char *readName(char *list, String name, flag *result) {
        int stack[MAX_BLOCK_DEPTH], depth = 0;
        char *s = list;
        clearString(name);
        if (!s) return NULL;
        while (*s && isspace(*s)) s++;
        if (!(*s)) return NULL;
        if (!strchr("{([\"", *s)) {
                for (; *s && !isspace(*s); s++)
                        stringCat(name, *s);
        } else {
                stack[depth++] = *s;
                for (s++; *s && depth > 0; s++) {
                        switch (*s) {
                                case '{':
                                case '(':
                                case '[':
                                        stack[depth++] = *s;
                                        break;
                                case '}':
                                        if (stack[depth - 1] == '{')
                                                depth--;
                                        else {
                                                *result = warning("error parsing name list, unexpected }");
                                                return NULL;
                                        }
                                        break;
                                case ')':
                                        if (stack[depth - 1] == '(')
                                                depth--;
                                        else {
                                                *result = warning("error parsing name list, unexpected )");
                                                return NULL;
                                        }
                                        break;
                                case ']':
                                        if (stack[depth - 1] == '[')
                                                depth--;
                                        else {
                                                *result = warning("error parsing name list, unexpected ]");
                                                return NULL;
                                        }
                                        break;
                                case '"':
                                        if (stack[depth - 1] == '"')
                                                depth--;
                                        else stack[depth++] = '"';
                                        break;
                        }
                        if (depth > 0) stringCat(name, *s);
                }
        }
        *result = TCL_OK;
        return s;
}


/************************************ Files **********************************/

static int stringEndsIn(char *s, char *t) {
        int ls = strlen(s);
        int lt = strlen(t);
        if (ls < lt) return FALSE;
        return (strcmp(s + ls - lt, t)) ? FALSE : TRUE;
}

static Tcl_Channel openPipe(char *command, int flags) {
        int argc;
        char **argv; 
        if (Tcl_SplitList(Interp, command, &argc, &argv))
                return NULL;
        return Tcl_OpenCommandChannel(Interp, argc, argv, flags);
}

static Tcl_Channel readZippedFile(char *command, char *fileName) {
        char buf[MAX_FILENAME];
        sprintf(buf, "%s < \"%s\" 2>%s", command, fileName, NULL_FILE);
        return openPipe(buf, TCL_STDOUT);
}

int TclpStat(const char *file_name, struct stat *buf);

/* Will silently return NULL if file couldn't be opened */
Tcl_Channel readChannel(char *fileName) {
        char fileBuf[MAX_FILENAME];
        struct stat statbuf;
        Tcl_Channel channel;
        int mode;

        /* Special file name */
        if (fileName[0] == '-') {
                if (!strcmp(fileName, "-"))
                        return Tcl_GetStdChannel(TCL_STDIN);
                /* Otherwise, the file name is actually a Tcl channel */
                if (!(channel = Tcl_GetChannel(Interp, fileName + 1, &mode)))
                        return NULL;
                if (!(mode & TCL_READABLE)) return NULL;
                return channel;
        }

        /* If it is a pipe */
        if (fileName[0] == '|')
                return openPipe(fileName + 1, TCL_STDOUT);

        /* Check if already ends in .gz or .Z and assume compressed */
        if (stringEndsIn(fileName, ".gz") || stringEndsIn(fileName, ".Z")) {
                if (!TclpStat(fileName, &statbuf))
                        return readZippedFile(UNZIP, fileName);
                return NULL;
        }
        /* Check if already ends in .bz or .bz2 and assume compressed */
        if (stringEndsIn(fileName, ".bz") || stringEndsIn(fileName, ".bz2")) {
                if (!stat(fileName, &statbuf))
                        return readZippedFile(BUNZIP2, fileName);
                return NULL;
        }
        /* Try just opening normally */
        if (!TclpStat(fileName, &statbuf))
                return Tcl_OpenFileChannel(Interp, fileName, "r", 0644);
        /* Try adding .gz */
        sprintf(fileBuf, "%s.gz", fileName);
        if (!TclpStat(fileBuf, &statbuf))
                return readZippedFile(UNZIP, fileBuf);
        /* Try adding .Z */
        sprintf(fileBuf, "%s.Z", fileName);
        if (!TclpStat(fileBuf, &statbuf))
                return readZippedFile(UNZIP, fileBuf);
        /* Try adding .bz2 */
        sprintf(fileBuf, "%s.bz2", fileName);
        if (!stat(fileBuf, &statbuf))
                return readZippedFile(BUNZIP2, fileBuf);
        /* Try adding .bz */
        sprintf(fileBuf, "%s.bz", fileName);
        if (!stat(fileBuf, &statbuf))
                return readZippedFile(BUNZIP2, fileBuf);

        return NULL;
}

static Tcl_Channel writeZippedFile(char *fileName, flag append) {
        char buf[MAX_FILENAME];
        char *op = (append) ? ">>" : ">";
        if (stringEndsIn(fileName, ".bz2") || stringEndsIn(fileName, ".bz"))
                sprintf(buf, "%s %s \"%s\"", BZIP2, op, fileName);
        else if (stringEndsIn(fileName, ".Z"))
                sprintf(buf, "%s %s \"%s\"", COMPRESS, op, fileName);
        else
                sprintf(buf, "%s %s \"%s\"", ZIP, op, fileName);
        return openPipe(buf, TCL_STDIN);
}

Tcl_Channel writeChannel(char *fileName, flag append) {
        Tcl_Channel channel;
        int mode;

        /* Special file name */
        if (fileName[0] == '-') {
                if (!strcmp(fileName, "-"))
                        return Tcl_GetStdChannel(TCL_STDOUT);
                /* Otherwise, the file name is actually a Tcl channel */
                if (!(channel = Tcl_GetChannel(Interp, fileName + 1, &mode)))
                        return NULL;
                if (!(mode & TCL_WRITABLE)) return NULL;
                return channel;
        }

        /* If it is a pipe */
        if (fileName[0] == '|')
                return openPipe(fileName + 1, TCL_STDIN);

        /* Check if ends in .gz or .Z */
        if (stringEndsIn(fileName, ".gz") || stringEndsIn(fileName, ".Z") ||
                        stringEndsIn(fileName, ".bz") || stringEndsIn(fileName, ".bz2"))
                return writeZippedFile(fileName, append);

        return (append) ? Tcl_OpenFileChannel(Interp, fileName, "a", 0644) :
                Tcl_OpenFileChannel(Interp, fileName, "w", 0644);
}

/* Could be a file or a stream. */
void closeChannel(Tcl_Channel channel) {
        char *error = copyString(Tcl_GetStringResult(Interp));
        Tcl_Flush(channel);
        /* Don't close it if it's stdin, stdout, or registered */
        if (!(channel == Tcl_GetStdChannel(TCL_STDIN) || 
                                channel == Tcl_GetStdChannel(TCL_STDOUT) ||
                                Tcl_GetChannel(Interp, Tcl_GetChannelName(channel), NULL)))
                Tcl_Close(Interp, channel);
        result(error);
        FREE(error);
        /* Tcl_ResetResult(Interp); */
}

flag binaryEncoding(Tcl_Channel channel) {
        return Tcl_SetChannelOption(Interp, channel, "-translation", "binary");
}


/************************************ Parsing ********************************/

#ifdef JUNK
static char *getL(char *s, int size, Tcl_Channel stream) {
        int i = 0;
        Tcl_DString *ds;
        while (parseBufPos < sizeof(int) && i < (size - 1) && 
                        parseBuf[parseBufPos] != '\n')
                s[i++] = parseBuf[parseBufPos++];
        if (i == size - 1) goto done;
        if (parseBufPos < sizeof(int) && parseBuf[parseBufPos] == '\n') {
                s[i++] = parseBuf[parseBufPos++];
                goto done;
        }
        Tcl_DStringInit(ds);

        Tcl_DStringFree(ds);

        foo
                return fgets(s + i, size - i, stream);
done:
        s[i] = '\0';
        return s;
}

/* Returns TCL_OK if line found, TCL_ERROR if file ends.  It will try to read
   in the whole line, expanding the buffer as needed. */
static flag getLine(ParseRec R) {
        int shift = 0;
        flag done = FALSE;

        R->line++;
        do {
                if (!getL(R->buf->s + shift, R->buf->maxChars - shift, R->file)) {
                        if (!shift) return TCL_ERROR;
                        else done = TRUE;
                }
                R->buf->numChars = strlen(R->buf->s);
                if (R->buf->s[R->buf->numChars - 1] == '\n') done = TRUE;

                if (!done) {
                        shift = R->buf->numChars;
                        stringSize(R->buf, 2 * shift);
                } else if (R->buf->s[0] == '#') {
                        if (R->buf->s[R->buf->numChars - 1] == '\n') {
                                shift = 0;
                                R->buf->numChars = 0;
                                R->line++;
                                done = FALSE;
                        } else return TCL_ERROR;
                }
        } while (!done);

        R->s = R->buf->s;
        return TCL_OK;
}
#endif 

static flag getOneLine(ParseRec R) {
        char c = '\0';
        String S = R->buf;
        R->line++;

        clearString(S);
        /* First copy any cookie that's left up to a newline. */
        while (R->cookiePos < sizeof(int) && c != '\n') {
                c = R->cookie[R->cookiePos++];
                stringCat(S, c);
        }
        /* If the line isn't done, concatenate the rest. */
        if (!S->numChars || S->s[S->numChars - 1] != '\n') {
                Tcl_DString ds;
                Tcl_DStringInit(&ds);
                if (Tcl_Gets(R->channel, &ds) < 0) {
                        Tcl_DStringFree(&ds);
                        return TCL_ERROR;
                }
                stringAppend(S, Tcl_DStringValue(&ds));
                Tcl_DStringFree(&ds);
        }
        /* Remove a trailing newline. */
        if (S->numChars && S->s[S->numChars - 1] == '\n')
                S->s[--S->numChars] = '\0';

        R->s = R->buf->s;
        return TCL_OK;
}

static flag getLine(ParseRec R) {
        flag result;
        do {result = getOneLine(R);} while (result == TCL_OK && *(R->s) == '#');
        return result;
}

/* This tries to recover from the binary cookie having been read. */
flag startParser(ParseRec R, int word) {
        R->line = 0;
        R->s = R->buf->s;
        R->s[0] = '\0';
        R->buf->numChars = 0;
        *((int *) R->cookie) = word;
        R->cookiePos = 0;
        return TCL_OK;
}

/* Returns TCL_OK if non-blank found, TCL_ERROR if file ends. */
flag skipBlank(ParseRec R) {
        while (!R->s[0] || isspace((int) R->s[0])) {
                if (!R->s[0] && getLine(R))
                        return TCL_ERROR;
                while (R->s < R->buf->s + R->buf->numChars && 
                                isspace((int) R->s[0])) R->s++;
        }
        return TCL_OK;
}

/* Looks for strings beginning with 9, -9, or .9 or -.9. */
flag isNumber(ParseRec R) {
        char *s;
        skipBlank(R);
        s = R->s;
        if (s[0] == '-') {
                if (!s[1] || isspace(s[1]) || strchr("});", s[1])) return TRUE;
                s++;
        }
        if (isdigit(s[0])) return TRUE;
        if (s[0] == '.' && isdigit(s[1])) return TRUE;
        return FALSE;
}

flag isInteger(char *s) {
        for (; *s; s++)
                if (!isdigit(*s)) return FALSE;
        return TRUE;
}

flag readInt(ParseRec R, int *val) {
        int shift;
        if (skipBlank(R)) return TCL_ERROR;
        if (sscanf(R->s, "%d%n", val, &shift) != 1)
                return TCL_ERROR;
        R->s += shift;
        return TCL_OK;
}

/* If NO_VALUE is found, it will be set to NaN. */
flag readReal(ParseRec R, real *val) {
        float v;
        int shift;
        if (skipBlank(R)) return TCL_ERROR;
        /* count the number of non-blank or ; or , spots in the next word */
        for (shift = 0; R->s[shift] && !strchr(",;{}[]", R->s[shift])
                        && !isspace((int) R->s[shift]); shift++);
        if (!strncmp(NO_VALUE, R->s, shift))
                *val = NaN;
        else {
                if (sscanf(R->s, "%f%n", &v, &shift) != 1)
                        return TCL_ERROR;
                *val = (real) v;
        }
        R->s += shift;
        return TCL_OK;
}

flag readLine(ParseRec R, String S) {
        int len;
        if (skipBlank(R)) return TCL_ERROR;
        for (len = R->buf->s - R->s + R->buf->numChars; 
                        isspace((int) R->s[len - 1]); len--);
        stringSize(S, len);
        memcpy(S->s, R->s, len);
        S->numChars = len;
        S->s[len] = '\0';
        R->s += len;
        return TCL_OK;
}

flag readBlock(ParseRec R, String S) {
        int stack[MAX_BLOCK_DEPTH], depth = 0, temp;
        char *p, protect;
        if (skipBlank(R)) return TCL_ERROR;
        p = R->s;
        clearString(S);
        /* If it doesn't begin with a delimiter just go up to the next space */
        if (!strchr("{([\"", *p)) {
                for (p = R->s; *p && !isspace((int) *p) && !strchr("{}()[]\"", *p); p++);
                temp = *p;
                *p = '\0';
                stringAppend(S, R->s);
                *p = temp;
        } else { /* Get everything in the next set of brackets and discard them */
                stack[depth++] = *(R->s++);
                protect = FALSE;
                while (depth > 0) {
                        for (p = R->s; *p && depth > 0; p++) {
                                if (*p == '\\') {
                                        protect = 1 - protect;
                                        continue;
                                }
                                if (protect) {
                                        protect = FALSE;
                                        continue;
                                }
                                if (*p == '{' || *p == '(' || *p == '[') stack[depth++] = *p;
                                else if (*p == '}') {
                                        if (stack[depth - 1] == '{') depth--;
                                        else return error("error parsing block, unexpected }");
                                }
                                else if (*p == ')') {
                                        if (stack[depth - 1] == '(') depth--;
                                        else return error("error parsing block, unexpected )");
                                }
                                else if (*p == ']') {
                                        if (stack[depth - 1] == '[') depth--;
                                        else return error("error parsing block, unexpected )");
                                }
                                else if (*p == '"') {
                                        if (stack[depth - 1] == '"') depth--;
                                        else stack[depth++] = '"';
                                }
                        }
                        if (depth == 0)
                                p[-1] = '\0';
                        if (S->numChars) stringAppend(S, "\n");
                        stringAppend(S, R->s);
                        if (depth != 0) {
                                if (getLine(R))
                                        return TCL_ERROR;
                                p = R->s;
                        }
                }
        }
        R->s = p;
        return TCL_OK;
}

/* This returns true/false, not an error code and advances to after 
   the matched string */
flag stringMatch(ParseRec R, char *s) {
        int shift = strlen(s);
        skipBlank(R);
        if (!strncmp(R->s, s, shift)) {
                R->s += shift;
                return TRUE;
        }
        return FALSE;
}

/* Like stringMatch but doesn't advance to the end of the matched string, 
   only the whitespace. */
flag stringPeek(ParseRec R, char *s) {
        int shift = strlen(s);
        skipBlank(R);
        if (!strncmp(R->s, s, shift))
                return TRUE;
        return FALSE;
}

flag fileDone(ParseRec R) {
        if (!R->channel) return TRUE;
        if (!R->binary) return skipBlank(R);
        if (Tcl_InputBuffered(R->channel) == 0) {
                debug("killing because nothing buffered\n");
                return TRUE;
        }
        return FALSE;
}


/************************************* Binary IO *****************************/

flag readBinInt(Tcl_Channel channel, int *val) {
        int x;
        if (Tcl_Read(channel, (char *) &x, sizeof(int)) == sizeof(int)) {
                *val = NTOHL(x);
                return TCL_OK;
        }
        return TCL_ERROR;
}

/* This reads a float in network order and converts to a real in host order. */
flag readBinReal(Tcl_Channel channel, real *val) {
        int x, z;
        float y;
        if (Tcl_Read(channel, (char *) &x, sizeof(int)) == sizeof(int)) {
                z = NTOHL(x);
                y = *((float *) &z);
                *val = (isNaNf(y)) ? NaN : (real) y;
                return TCL_OK;
        }
        return TCL_ERROR;
}

flag readBinFlag(Tcl_Channel channel, flag *val) {
        char c;
        if (Tcl_Read(channel, &c, sizeof(char)) == sizeof(char)) {
                *val = (flag) c;
                return TCL_OK;
        }
        return TCL_ERROR;
}

flag readBinString(Tcl_Channel channel, String S) {
        char c;
        clearString(S);
        do {
                Tcl_Read(channel, &c, 1);
                stringCat(S, c);
        } while (c != '\0');
        return TCL_OK;
}

flag writeBinInt(Tcl_Channel channel, int x) {
#ifdef LITTLE_END
        int y = HTONL(x);
        if (Tcl_Write(channel, (char *) &y, sizeof(int)) != sizeof(int)) 
                return TCL_ERROR;
#else
        if (Tcl_Write(channel, (char *) &x, sizeof(int)) != sizeof(int)) 
                return TCL_ERROR;
#endif
        return TCL_OK;
}

/* This takes a real in host order and writes a float in network order. */
flag writeBinReal(Tcl_Channel channel, real r) {
#ifdef FLOAT_REAL
#  ifdef LITTLE_END
        int y = HTONL(*((int *) &r));
        if (Tcl_Write(channel, (char *) &y, sizeof(int)) != sizeof(int)) 
                return TCL_ERROR;
#  else
        if (Tcl_Write(channel, (char *) &r, sizeof(int)) != sizeof(int)) 
                return TCL_ERROR;
#  endif /* LITTLE_END */
#else /* FLOAT_REAL */
        float x = (isNaN(r)) ? NaNf : (float) r;
#  ifdef LITTLE_END
        int y = HTONL(*((int *) &x));
        if (Tcl_Write(channel, (char *) &y, sizeof(int)) != sizeof(int)) 
                return TCL_ERROR;
#  else
        if (Tcl_Write(channel, (char *) &x, sizeof(int)) != sizeof(int)) 
                return TCL_ERROR;
#  endif /* LITTLE_END */
#endif /* FLOAT_REAL */
        return TCL_OK;
}

flag writeBinFlag(Tcl_Channel channel, flag f) {
        char c = (char) f;
        if (Tcl_Write(channel, &c, sizeof(char)) != sizeof(char)) return TCL_ERROR;
        return TCL_OK;
}

/* len == strlen(s) or it will be computed if it is negative. */
flag writeBinString(Tcl_Channel channel, char *s, int len) {
        char x = '\0';
        if (s) {
                if (len < 0) len = strlen(s);
                if (Tcl_Write(channel, s, len + 1) != len + 1) return TCL_ERROR;
        } else if (Tcl_Write(channel, &x, sizeof(char)) != sizeof(char))
                return TCL_ERROR;
        return TCL_OK;
}

flag cprintf(Tcl_Channel channel, char *fmt, ...) {
        va_list args;
        if (fmt == Buffer) {
                error("cprintf() called on Buffer, report this to Doug");
                return TCL_ERROR;
        }
        va_start(args, fmt);
        vsprintf(Buffer, fmt, args);
        Tcl_Write(channel, Buffer, strlen(Buffer));
        va_end(args);
        return TCL_ERROR;
}

void writeReal(Tcl_Channel channel, real r, char *pre, char *post) {
        if (isNaN(r)) cprintf(channel, "%s%s%s", pre, NO_VALUE, post);
        else          cprintf(channel, "%s%g%s", pre, r, post);
}

void smartFormatReal(char *s, real x, int width) {
        int i, decimals;
        char format[16];
        if (Verbosity < 1) return;
        if (isNaN(x)) {
                for (i = 0; i < width; i++)
                        if (i == width >> 1)
                                s[i] = '-';
                        else s[i] = ' ';
                s[i] = '\0';
        } else {
                if (x == 0.0) decimals = width - 3;
                else decimals = width - 3 - (int) log10((double) ABS(x));
                if (decimals > width - 3) decimals = width - 3;
                if (decimals < 0) decimals = 0;
                sprintf(format, "%c%%-%d.%df", (x < 0.0) ? '-' : ' ', width - 1, decimals);
                sprintf(s, format, ABS(x));
        }
}

/* This prints a real value formatted in a nice column of specified width */
void smartPrintReal(real x, int width, flag app) {
        char buf[64];
        if (width > 60) width = 60;
        smartFormatReal(buf, x, width);
        if (app) append(buf);
        else print(1, buf);
}


/************************************ Network IO *****************************/

flag receiveChar(Tcl_Channel channel, char *val) {
        return (Tcl_Read(channel, val, 1) == 1) ? TCL_OK : TCL_ERROR;
}

flag receiveInt(Tcl_Channel channel, int *val) {
        int i;
        if (Tcl_Read(channel, (char *) &i, sizeof(int)) != sizeof(int))
                return TCL_ERROR;
        *val = NTOHL(i);
        return TCL_OK;
}

flag receiveFlag(Tcl_Channel channel, flag *val) {
        char f;
        if (Tcl_Read(channel, &f, 1) != 1) return TCL_ERROR;
        *val = (flag) f;
        return TCL_OK;
}

flag receiveReal(Tcl_Channel channel, real *val) {
        int x, y;
        float f;
        if (Tcl_Read(channel, (char *) &x, sizeof(int)) != sizeof(int))
                return TCL_ERROR;
        y = NTOHL(x);
        f = *((float *) &y);
        *val = (isNaNf(f)) ? NaN : (real) f;
        return TCL_OK;
}

/* Assumes s is allocated large enough */
flag receiveString(Tcl_Channel channel, char *s) {
        int len;
        if (receiveInt(channel, &len)) return TCL_ERROR;
        if (Tcl_Read(channel, s, len) != len) return TCL_ERROR;
        s[len] = '\0';
        return TCL_OK;
}


flag sendChar(Tcl_Channel channel, char c) {
        return (Tcl_Write(channel, &c, 1) == 1) ? TCL_OK : TCL_ERROR;
}

flag sendInt(Tcl_Channel channel, int x) {
        int y = HTONL(x);
        if (Tcl_Write(channel, (char *) &y, sizeof(int)) != sizeof(int))
                return TCL_ERROR;
        return TCL_OK;
}

flag sendFlag(Tcl_Channel channel, flag f) {
        return sendChar(channel, (char) f);
}

flag sendReal(Tcl_Channel channel, real r) {
        int x;
        float f = (isNaN(r)) ? NaNf : (float) r;
        x = HTONL(*((int *) &f));
        if (Tcl_Write(channel, (char *) &x, sizeof(int)) != sizeof(int))
                return TCL_ERROR;
        return TCL_OK;
}

flag sendString(Tcl_Channel channel, char *s) {
        int x, len = strlen(s);
        x = HTONL(len);
        if (Tcl_Write(channel, (char *) &x, sizeof(int)) != sizeof(int))
                return TCL_ERROR;
        if (Tcl_Write(channel, s, len) != len)
                return TCL_ERROR;
        return TCL_OK;
}


/************************************ Randomness *****************************/

#ifdef NO_DRAND48
double drand48(void) {
        return ((double) random()) / LONG_MAX;
}
#endif

void seedRand(unsigned int seed) {
#ifndef NO_DRAND48
        srand48((long) seed);
#endif
        srandom(seed);
        lastSeed = seed;
}

void timeSeedRand(void) {
        unsigned int seed = getpid() * getUTime() * 31;
        seedRand(seed);
}

unsigned int getSeed(void) {
        return lastSeed;
}

int randInt(int max) {
        return (int) ((random()) % max);
}

real randReal(real mean, real range) {
        return (real) (drand48() * 2 - 1.L) * range + mean;
}

real randProb(void) {
        return (real) drand48();
}

/* Adapted from Numerical Methods in C, pg. 289 */
/* The range is the standard deviation, not the variance. */
real randGaussian(real mean, real range) {
        static int iset = 0;
        static real gset, lmean, lrange;
        real fac, rsq, v1, v2;

        if (iset == 0 || lmean != mean || lrange != range) {
                do {
                        v1 = randReal(0.0, 1.0);
                        v2 = randReal(0.0, 1.0);
                        rsq = v1 * v1 + v2 * v2;
                } while (rsq >= 1.0 || rsq == 0.0);
                fac = SQRT(-2.0 * LOG(rsq) / rsq);
                gset = v1 * fac * range + mean;
                iset = 1;
                lmean = mean;
                lrange = range;
                return v2 * fac * range + mean;
        } else {
                iset = 0;
                return gset;
        }
}

void randSort(int *array, int n) {
        int i, j, temp;
        for (i = 0; i < (n - 1); i++) {
                j = randInt(n - i);
                temp = array[i];
                array[i] = array[i + j];
                array[i + j] = temp;
        }
}


/********************************* Fast Sigmoid ******************************/

real *SigmoidTable;

void buildSigmoidTable(void) {
        int i, size = 2 * SIGMOID_RANGE * SIGMOID_SUB + 2; real x;
        SigmoidTable = realArray(size, "buildSigmoidTable:SigmoidTable");
        for (i = 0; i < size; i++) {
                x = (real) i / SIGMOID_SUB - SIGMOID_RANGE;
                SigmoidTable[i] = SIGMOID(x, 1.0);
        }
}

real fastSigmoid(real x) {
        int i; real v;
        if (x < -SIGMOID_RANGE) x = -SIGMOID_RANGE;
        else if (x > SIGMOID_RANGE) x = SIGMOID_RANGE;
        v = (x + SIGMOID_RANGE) * SIGMOID_SUB;
        i = (int) v;
        v -= i;
        return (1.0 - v) * SigmoidTable[i] + v * SigmoidTable[i + 1];
}


/************************************ Noise **********************************/

real multGaussianNoise(real value, real range) {
        return value * randGaussian(1.0, range);
}
real addGaussianNoise(real value, real range) {
        return value + randGaussian(0.0, range);
}
real multUniformNoise(real value, real range) {
        return value * randReal(1.0, range);
}
real addUniformNoise(real value, real range) {
        return value + randReal(0.0, range);
}


/************************************* Time **********************************/

/* This handles the problem of accounting for wrap around */
unsigned long timeElapsed(unsigned long a, unsigned long b) {
        if (a <= b) return b - a;
        if (b > a - 1000) {
                /* print("TimeElapsed problem %ld %ld\n", a, b); */
                return 0;
        }
        return 4000000000ul - a + b;
}

/* Real time in milliseconds (mod 4B) */
unsigned long getTime(void) {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        return ((unsigned long) tv.tv_sec % 4000000) * 1000 + 
                (unsigned long) tv.tv_usec * 1e-3;
}

/* Real time in microseconds (mod 4B) */
unsigned long getUTime(void) {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        return ((unsigned long) tv.tv_sec % 4000) * 1000000 + 
                (unsigned long) tv.tv_usec;
}

/* Time is in seconds.  This prints it in hr/min or min/sec or sec format  */
void printTime(unsigned long time, char *dest) {
        if (time > 3600)
                sprintf(dest, "%2luh %2lum", (time / 3600), 
                                ((time % 3600) / 60));
        else {
                if (time > 60) {
                        sprintf(dest, "%2lum ", time / 60);
                        time %= 60; 
                } else sprintf(dest, "    ");
                sprintf(dest + 4, "%2lus", time);
        }
}


/************************************* Misc **********************************/

/* Like atof but checks for the NaN code */
real ator(char *s) {
        if (!strcmp(s, NO_VALUE))
                return NaN;
        else return atof(s);
}

int roundr(real r) {
        int f = FLOOR(r);
        return (r > f + 0.5) ? f + 1 : f;
}

int  imin(int a, int b) {return (a < b) ? a : b;}
int  imax(int a, int b) {return (a < b) ? b : a;}
real rmin(real a, real b) {return (a < b) ? a : b;}
real rmax(real a, real b) {return (a < b) ? b : a;}

/* This will return a if it's a real number, otherwise it will return b.
   This is used for values such as learningRate that can be overridden at 
   various levels. */
real chooseValue(real a, real b) {
        return (isNaN(a)) ? b : a;
}

/* This will return a if it's a real number, else b if it's a real number,
   else c.  This is used for values such as learningRate that can be
   overridden at various levels. */
real chooseValue3(real a, real b, real c) {
        return (isNaN(a)) ? (isNaN(b)) ? c : b : a;
}

/*
 *----------------------------------------------------------------------
 *
 * TclpStat --
 *
 *	This function replaces the library version of stat().
 *
 * Results:
 *	See stat() documentation.
 *
 * Side effects:
 *	See stat() documentation.
 *
 *----------------------------------------------------------------------
 */

int TclpStat(path, bufPtr)
        CONST char *path;		/* Path of file to stat (in UTF-8). */
        struct stat *bufPtr;	/* Filled with results of stat call. */
{
        int result;
        Tcl_DString ds;
        char *native;

        native = Tcl_UtfToExternalDString(NULL, path, -1, &ds);
        result = stat(native, bufPtr);			/* INTL: Native. */
        Tcl_DStringFree(&ds);

        return result;
}
