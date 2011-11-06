/**
    Copyright (C) 2003  Michael Ahlberg, Måns Rullgård

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

#include <stdio.h>
#include <stdlib.h>
#include <tctypes.h>
#include <tcstring.h>
#include <tcalloc.h>
#include <tcconf.h>
#include <unistd.h>

extern int
main(int argc, char **argv)
{
    tcconf_section_t *s = NULL;
    FILE *out = stdout;
    char *outfile = "-";
    int opt;
    int i;

    while((opt = getopt(argc, argv, "o:h")) != -1){
	switch(opt){
	case 'o':
	    outfile = optarg;
	    break;
	case 'h':
	    fprintf(stderr, "Usage: tcconfdump [-o file] [file ...]\n");
	}
    }

    if(strcmp(outfile, "-")){
	if(!(out = fopen(outfile, "w"))){
	    perror(outfile);
	    exit(1);
	}
    }

    argc -= optind;
    argv += optind;

    for(i = 0; i < argc; i++){
	tcconf_section_t *n;
	if(strcmp(argv[i], "-")){
	    n = tcconf_load_file(s, argv[i]);
	} else {
	    n = tcconf_load(s, stdin, (tcio_fn) fread);
	}
	if(n)
	    s = n;
	else
	    fprintf(stderr, "tcconfdump: error loading %s\n", argv[i]);
    }

    if(!argc)
	s = tcconf_load(s, stdin, (tcio_fn) fread);

    if(s){
	tcconf_write(s, out, (tcio_fn) fwrite);
	tcfree(s);
    }

    fclose(out);

    return 0;
}
