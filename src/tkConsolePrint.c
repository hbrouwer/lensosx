/* tkConsolePrint.c */

#include <stdio.h>
#include <stdlib.h>
#include <tcl.h>

void TkConsolePrint (Tcl_Interp *interp, int devId, char *buffer, long size)
{
        Tcl_Write(Tcl_GetStdChannel(devId), buffer, size);
        Tcl_Flush(Tcl_GetStdChannel(devId));
}
