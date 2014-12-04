/*
 
 Copyright (c) 2014, Kristof Hannemann
 All rights reserved.
 
 Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
 
 1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
 
 2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
 
 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 
 
 */

#include <stdio.h>

#ifndef CMDLINEPROMPT_H
#define CMDLINEPROMPT_H


typedef struct __prompt_t {
    char **history;
    unsigned long histsize;
    char *buffer;
    unsigned long buffsize;
    unsigned long curpos;
    unsigned long histpos;
    char *label;
    FILE *instream;
    FILE *outstream;
    char **history_tmp;
}
prompt_t;

#define prompt_getinput(__pt) (__pt)->buffer
#define prompt_setcursor(__pt, __curspos) (__pt)->curpos=__curpos
#define prompt_getcursor(__pt) (__pt)->curpos
#define prompt_setlabel(__pt, __label) (__pt)->label=__label
#define prompt_getlabel(__pt) (__pt)->label
//#define prompt_sethistory(__pt, __histpos) (__pt)->histpos=__histpos //segfault if history[histsize+1]
#define prompt_gethistory(__pt) (__pt)->histpos

extern char * prompt(prompt_t *__pt, char *__label);
extern void prompt_init(prompt_t *__pt);
extern void prompt_destroy(prompt_t *__pt);
extern void prompt_setinput(prompt_t *__pt, char *__input);
extern void prompt_addhistory(prompt_t *__pt, char *__entry);
extern void prompt_clear(prompt_t *__pt);

#endif // CMDLINEPROMPT_H
