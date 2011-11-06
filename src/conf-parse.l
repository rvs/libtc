/** -*- c -*-
    Copyright (C) 2001, 2002, 2003  Michael Ahlberg, Måns Rullgård

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

%option noyywrap

%x sect
%x p_val
%x s_merge

keyword [[:alnum:]_/-]+
keyref  (\.\.\/)*{keyword}

%{
#include <string.h>
#include <tclist.h>
#include <tcalloc.h>
#include "tcconf.h"
#include "tcc-internal.h"

#define yyterminate()						\
    if(section_stack) tclist_destroy(section_stack, tcfree);	\
    YY_FLUSH_BUFFER;						\
    BEGIN(INITIAL);						\
    return NULL

#define YY_DECL conf_section *tcc_lex(void *data, tcio_fn rfn,	\
				      conf_section *sec)

#define YY_INPUT(buf,res,size) {			\
   res = conf_file.read(buf, 1, size, conf_file.data);	\
   res = res <= 0? YY_NULL: res;			\
}

#define PUSH_SECTION(q, t) do {			\
    tcc_entry *te;				\
    char *n = yytext+q;				\
    yytext[yyleng-q] = 0;			\
    te = create_entry(cur_section, n, t);	\
    tclist_push(section_stack, cur_section);	\
    cur_section = te->section;			\
} while(0)

static struct {
    void *data;
    tcio_fn read;
} conf_file;

%}

%%
    tclist_t *section_stack = tclist_new(TC_LOCK_SLOPPY);
    conf_section *cur_section;
    tcc_entry *cur_entry = NULL;
    int line_num = 1;
    int secn = 0;
    if(sec){
	cur_section = sec;
    } else {
	cur_section = conf_new(NULL);
    }
    conf_file.data = data;
    conf_file.read = rfn;

    YY_FLUSH_BUFFER;
    BEGIN(INITIAL);

<*>{
    [ \t] /* skip blanks */
    #.*$  /* comment */
}

<INITIAL>{
    section {
	secn = 0;
	BEGIN(sect);
    }
    {keyword}/([[:space:]]*:[[:space:]]*{keyref})*[[:space:]]*\{ {
	PUSH_SECTION(0, TCC_SECTION);
	secn = 1;
	BEGIN(sect);
    }
    {keyword}/([[:space:]]*:[[:space:]]*{keyref})*[[:space:]]*\[ {
	PUSH_SECTION(0, TCC_MSECTION);
	secn = 1;
	BEGIN(sect);
    }
    {keyword} {
	cur_entry = create_entry(cur_section, yytext, TCC_VALUE);
	BEGIN(p_val);
    }
    [\}\]] { conf_section *sec;
	if((sec = tclist_pop(section_stack)) != NULL){
	    cur_section = sec;
	} else {
	    fprintf(stderr, "line %d: Unexpected '%c'\n", line_num, yytext[0]);
	    yyterminate();
	}
    }
    <<EOF>> {
	if(tclist_items(section_stack) == 0){
	    tclist_destroy(section_stack, NULL);
	    YY_FLUSH_BUFFER;
	    return cur_section;
	} else {
	    fprintf(stderr, "Unexpected end of file.\n");
	    yyterminate();
	}
    }
}

<p_val>{
    [+\-]?0x[[:xdigit:]]+ |
    [+\-]?[[:digit:]]+ tcc_addint(cur_entry, strtoll(yytext, NULL, 0));
    [+\-]?[[:digit:]]+\.[[:digit:]]*([eE][+\-]?[[:digit:]]+)? tcc_addfloat(cur_entry, strtod(yytext, NULL));
    \'[^\']*\' yytext[yyleng-1]=0; tcc_addstring(cur_entry,strdup(yytext+1),0);
    \"([^\"]*(\\\")?)*[^\\]\" yytext[yyleng-1]=0; tcc_addstring(cur_entry,strdup(yytext+1),1);
    {keyref} tcc_addref(cur_entry, strdup(yytext));
    true tcc_addbool(cur_entry, 1);
    false tcc_addbool(cur_entry, 0);
    [[:space:]]*/[\]\}] |
    [;\n] {
	BEGIN(INITIAL);
	if(yytext[0] == '\n')
	    line_num++;
    }
    <<EOF>> fprintf(stderr, "Unexpected end of file.\n"); yyterminate();
}

<sect>{
    \"[^\"]*\" {
	if(secn == 0){
	    PUSH_SECTION(1, TCC_SECTION);
	    secn = 1;
	} else {
	    fprintf(stderr, "line %d: Expected '{', found %s.\n",
		    line_num, yytext);
	    yyterminate();
	}
    }
    [\{\[] {
	if(secn){
	    BEGIN(INITIAL);
	} else {
	    fprintf(stderr, "line %d: Missing section name.\n", line_num);
	    yyterminate();
	}
    }
    : BEGIN(s_merge);
}

<s_merge>[[:alnum:]_/.-]+ {
    tclist_push(cur_section->merge, strdup(yytext));
    BEGIN(sect);
}

<*>\n line_num++;
<*>[^ \t\n;]+ fprintf(stderr, "line %d: Syntax error near '%s'.\n", line_num, yytext); yyterminate();
