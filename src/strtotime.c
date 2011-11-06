/**
    Copyright (C) 2001  Michael Ahlberg, Måns Rullgård

    Permission is hereby granted, free of charge, to any person
    obtaining a copy of this software and associated documentation
    files (the "Software"), to deal in the Software without
    restriction, including without limitation the rights to use, copy,
    modify, merge, publish, distribute, sublicense, and/or sell copies
    of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be
    included in all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
    EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
    NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
    HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
    WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
**/

#define _XOPEN_SOURCE 1
#define _XOPEN_SOURCE_EXTENDED 1

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tctypes.h>
#include <tctime.h>


static char *timefmts[13] = {
    "%Y-%m-%d %H:%M:%S",
    "%y-%m-%d %H:%M:%S",
    "%y%m%d %H:%M:%S",
    "%Y%m%d %H:%M:%S",
    "%y%m%d %H:%M",
    "%Y%m%d %H:%M",
    "%b %d %Y %H:%M:%S",
    "%b %d %Y %H:%M",
    "%d %b %Y %H:%M:%S"
    "%d %b %Y %H:%M",
    "%H:%M:%S",
    "%H:%M",
    NULL
};


extern char *
strtotime(char *ts, struct tm *tm, char **fmts)
{
    char *t;
    time_t lt;
    int i;

    if(fmts == NULL)
	fmts = timefmts;

    for(i = 0;
	fmts[i] != NULL &&
	    ((t = strptime(ts, fmts[i], tm)) == NULL || *t != 0);
	i++);

    if(fmts[i] != NULL){
	lt = time(NULL);
	localtime_r(&lt, tm);
	strptime(ts, fmts[i], tm);
    }

    return fmts[i];
}
