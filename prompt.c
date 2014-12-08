/*
 
 Copyright (c) 2014, Kristof Hannemann
 All rights reserved.
 
 Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
 
 1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
 
 2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
 
 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 
 
 */


#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <wchar.h>
#include <locale.h>
#include <limits.h>
#include "prompt.h"

static wchar_t *insertwchar(wchar_t *str, wchar_t ch, unsigned long pos, unsigned long size) {
    unsigned long i;
    unsigned long len = wcslen(str);
    if (((size-len) < 2)||(pos > len)) return NULL;
    for (i=len; i>pos; i--) {
        str[i] = str[i-1];
    }
    str[pos]=ch;
    str[len+1]=L'\0';
    
    return str;
}

static wchar_t *removewchar(wchar_t *str, unsigned long pos) {
    unsigned long len = wcslen(str);
    unsigned long i;
    for (i=pos; i<len-1; i++) {
        str[i] = str[i+1];
    }
    str[len-1] = L'\0';
    
    return str;
}

#define ESC_SEQ_UP      "\x1b[A"
#define ESC_SEQ_DOWN    "\x1b[B"
#define ESC_SEQ_RIGHT   "\x1b[C"
#define ESC_SEQ_RIGHT_N "\x1b[%uC"
#define ESC_SEQ_LEFT    "\x1b[D"
#define ESC_SEQ_LEFT_N  "\x1b[%uD"
#define ESC_SEQ_ERASE   "\x1b[J"
#define ESC_SEQ_BELL    "\a"

#define BUFFERSIZE 8

static wchar_t label[] = L"> ";

void prompt_init(prompt_t *pt) {
    pt->buffsize = BUFFERSIZE;
    pt->buffer = malloc(pt->buffsize*sizeof(wchar_t));
    memset(pt->buffer, 0, pt->buffsize*sizeof(wchar_t));
    pt->history = NULL;
    pt->histsize = 0;
    pt->label = label;
    pt->instream = stdin;
    pt->outstream = stdout;
    pt->curpos = 0;
    pt->histpos = pt->histsize;
    pt->history_tmp = malloc((pt->histsize+1)*sizeof(wchar_t*));
    memset(pt->history_tmp,0,(pt->histsize+1)*sizeof(wchar_t*));
    pt->chbuffer=NULL;
}

void prompt_destroy(prompt_t *pt) {
    unsigned long i;
    for (i=0; i<pt->histsize; i++) {
        free(pt->history[i]);
    }
    free(pt->history);
    for (i=0; i<(pt->histsize+1); i++)
        free(pt->history_tmp[i]);
    free(pt->history_tmp);
    free(pt->buffer);
}

void prompt_setinput_wc(prompt_t *pt, wchar_t *input) {
    unsigned long len = wcslen(input);
    if ((len+1)>pt->buffsize)
        pt->buffer=realloc(pt->buffer, (len+1)*sizeof(wchar_t));
    pt->buffsize=len+1;
    wcscpy(pt->buffer, input);
    pt->curpos=len;
}

void prompt_addhistory_wc(prompt_t *pt, wchar_t *entry) {
    pt->histsize++;
    pt->history=realloc(pt->history,pt->histsize*sizeof(wchar_t*));
    pt->history[pt->histsize-1]=malloc((wcslen(entry)+1)*sizeof(wchar_t));
    wcscpy(pt->history[pt->histsize-1], entry);
    
    pt->history_tmp=realloc(pt->history_tmp ,(pt->histsize+1)*sizeof(wchar_t*));
    pt->history_tmp[pt->histsize]=pt->history_tmp[pt->histsize-1]; //swap
    pt->history_tmp[pt->histsize-1]=malloc((wcslen(entry)+1)*sizeof(wchar_t));
    wcscpy(pt->history_tmp[pt->histsize-1], entry);
}

void prompt_clear(prompt_t *pt) {
    unsigned long i;
    memset(pt->buffer, 0, pt->buffsize*sizeof(wchar_t));
    pt->curpos=0;
    pt->histpos=pt->histsize;
    for (i=0; i<(pt->histsize+1); i++)
        free(pt->history_tmp[i]);
    memset(pt->history_tmp, 0, (pt->histsize+1)*sizeof(wchar_t*));
}

#define clrws(__ws)  do { \
                        __ws.ws_col=0; \
                        __ws.ws_row=0; \
                        __ws.ws_xpixel=0; \
                        __ws.ws_ypixel=0; \
                    } while(0)

wchar_t * prompt_wc(prompt_t *pt, wchar_t *label) {
    struct termios oldterm, newterm;
    char ch;
    wchar_t wch;
    wchar_t *buffer = pt->buffer, **history=pt->history, **history_tmp=pt->history_tmp;
    unsigned long curpos=pt->curpos,len=wcslen(buffer), histpos=pt->histpos;
    unsigned long buffsize=pt->buffsize, histsize=pt->histsize;
    FILE *instream=pt->instream, *outstream=pt->outstream;
    struct winsize ws;
    char *localenv = setlocale(LC_CTYPE, NULL); //retrieve local env
    mbstate_t mbs;
    
    pt->label=(label!=NULL)? label : pt->label;
    
    fprintf(outstream, "%ls%ls", pt->label, buffer);
    if ((len-curpos) > 0)
        fprintf(outstream, ESC_SEQ_LEFT_N, (unsigned int)(len-curpos));

    tcgetattr(fileno(instream), &oldterm);
    newterm = oldterm;
    newterm.c_lflag &= (~ICANON)&(~ECHO);
    tcsetattr(fileno(instream), TCSANOW, &newterm);
    
    
    mbrlen(NULL, 0, &mbs);
    setlocale(LC_CTYPE, "");
    
    for (; ; ) {
        ch = fgetc(instream);
        if (mbrtowc(&wch, &ch, 1, &mbs) != 1) continue;
        
        
        if (ch==0x1b) { //esc sequence
            if (fgetc(instream)=='[') {
                switch (fgetc(instream)) {
                    
                    case 'A': //up
                        if (histpos == 0) {
                            fprintf(outstream, ESC_SEQ_BELL);
                            break;
                        }
                        
                        history_tmp[histpos] = realloc(history_tmp[histpos], (wcslen(buffer)+1)*sizeof(wchar_t));
                        wcscpy(history_tmp[histpos], buffer);
                        wcscpy(buffer,history_tmp[histpos-1]? history_tmp[histpos-1] : history[histpos-1]);

                        histpos--;
                        if (curpos > 0)
                            fprintf(outstream, ESC_SEQ_LEFT_N, (unsigned int)curpos);
                        fprintf(outstream, "%ls" ESC_SEQ_ERASE, buffer);
                        
                        curpos=wcslen(buffer);
                        len=wcslen(buffer);
                        break;
                    case 'B': //down
                        if (histpos == histsize) {
                            fprintf(outstream, ESC_SEQ_BELL);
                            break;
                        }
                        
                        history_tmp[histpos] = realloc(history_tmp[histpos], (wcslen(buffer)+1)*sizeof(wchar_t));
                        wcscpy(history_tmp[histpos], buffer);
                        wcscpy(buffer,history_tmp[histpos+1]? history_tmp[histpos+1] : history[histpos+1]); //segfault


                        histpos++;
                        if (curpos > 0)
                            fprintf(outstream, ESC_SEQ_LEFT_N, (unsigned int)curpos);
                        fprintf(outstream, "%ls" ESC_SEQ_ERASE, buffer);
                        
                        curpos=wcslen(buffer);
                        len=wcslen(buffer);
                        
                        break;
                    case 'C': //right
                        if (curpos<len) {
                            
                            curpos++;
                            
                            clrws(ws);
                            ioctl(fileno(outstream), TIOCGWINSZ, &ws);
                            if (((wcslen(pt->label)+curpos)%ws.ws_col)==0)
                                fprintf(outstream, ESC_SEQ_LEFT_N ESC_SEQ_DOWN,ws.ws_col-1);
                            else
                                fprintf(outstream, ESC_SEQ_RIGHT);
                        }
                        else
                            fprintf(outstream, ESC_SEQ_BELL);
                        
                        break;
                    case 'D': //left
                        if (curpos>0) {
                            
                            curpos--;
                            
                            //not necessary
                            /*
                            clrws(ws);
                            ioctl(fileno(outstream), TIOCGWINSZ, &ws);
                            if (((strlen(pt->prompttext)+curpos+1)%ws.ws_col)==0)
                                fprintf(outstream, ESC_SEQ_RIGHT_N ESC_SEQ_UP,ws.ws_col-1);
                            else */
                                fprintf(outstream, ESC_SEQ_LEFT);
                        }
                        else
                            fprintf(outstream, ESC_SEQ_BELL);
                        break;
                        
                    default:
                        break;
                }
            }
        }
        else if (ch==0x7f) {
            if (curpos) {
                curpos--;
                len--;
                removewchar(buffer,curpos);
                fprintf(outstream, ESC_SEQ_LEFT "%ls" " " ESC_SEQ_LEFT_N,&buffer[curpos],(unsigned int)(len-curpos+1));

            }
            else
                fprintf(outstream, ESC_SEQ_BELL);
        }
        else if ((wch==L'\n')||(wch==L'\r'))
            break;
        else {
            if ((len+1)>=buffsize) {
                buffsize+=BUFFERSIZE;
                buffer=realloc(buffer,buffsize*sizeof(wchar_t));
            }
            curpos++;
            len++;
            insertwchar(buffer,wch,curpos-1,buffsize);
            fprintf(outstream, "%ls", &buffer[curpos-1]);
            if ((len-curpos)>0)
                fprintf(outstream, ESC_SEQ_LEFT_N, (unsigned int)(len-curpos));
            
        }
        
    }

    buffer[len]=L'\0'; //safety
    
    history_tmp[histpos] = realloc(history_tmp[histpos], (wcslen(buffer)+1)*sizeof(wchar_t));
    wcscpy(history_tmp[histpos], buffer);
    
    pt->buffer=buffer;
    pt->buffsize=buffsize;
    pt->curpos=curpos;
    pt->histpos=histpos;
    pt->history_tmp=history_tmp;

    fprintf(outstream, "\n\r");
    
    tcsetattr(fileno(instream), TCSANOW, &oldterm);
    
    setlocale(LC_CTYPE, localenv); //restore local env
    
    return ((len>0) ? buffer : NULL);
}

