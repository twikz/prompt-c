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
#include "prompt.h"

static char *insertchar(char *str, char ch, unsigned long pos, unsigned long size) {
    unsigned long i;
    unsigned long len = strlen(str);
    if (((size-len) < 2)||(pos > len)) return NULL;
    for (i=len; i>pos; i--) {
        str[i] = str[i-1];
    }
    str[pos]=ch;
    str[len+1]='\0';
    
    return str;
}

static char *removechar(char *str, unsigned long pos) {
    unsigned long len = strlen(str);
    unsigned long i;
    for (i=pos; i<len-1; i++) {
        str[i] = str[i+1];
    }
    str[len-1] = '\0';
    
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

static char label[] = "> ";

void prompt_init(prompt_t *pt) {
    pt->buffsize = BUFFERSIZE*sizeof(char);
    pt->buffer = malloc(pt->buffsize);
    memset(pt->buffer, 0, pt->buffsize);
    pt->history = NULL;
    pt->histsize = 0;
    pt->label = label;
    pt->instream = stdin;
    pt->outstream = stdout;
    pt->curpos = 0;
    pt->histpos = pt->histsize;
    pt->history_tmp = malloc((pt->histsize+1)*sizeof(char*));
    memset(pt->history_tmp,0,(pt->histsize+1)*sizeof(char*));
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

void prompt_setinput(prompt_t *pt, char *input) {
    unsigned long len = strlen(input);
    if ((len+1)>pt->buffsize)
        pt->buffer=realloc(pt->buffer, (len+1)*sizeof(char));
    pt->buffsize=(len+1)*sizeof(char);
    strcpy(pt->buffer, input);
    pt->curpos=len;
}

void prompt_addhistory(prompt_t *pt, char *entry) {
    pt->histsize++;
    pt->history=realloc(pt->history,pt->histsize*sizeof(char*));
    pt->history[pt->histsize-1]=malloc((strlen(pt->buffer)+1)*sizeof(char));
    strcpy(pt->history[pt->histsize-1], pt->buffer);
    pt->history_tmp=realloc(pt->history_tmp ,(pt->histsize+1)*sizeof(char*));
    pt->history_tmp[pt->histsize]=pt->history_tmp[pt->histsize-1]; //swap
    pt->history_tmp[pt->histsize-1]=malloc((strlen(pt->buffer)+1)*sizeof(char));
    strcpy(pt->history_tmp[pt->histsize-1], pt->buffer);
}

void prompt_clear(prompt_t *pt) {
    unsigned long i;
    memset(pt->buffer, 0, pt->buffsize);
    pt->curpos=0;
    pt->histpos=pt->histsize;
    for (i=0; i<(pt->histsize+1); i++)
        free(pt->history_tmp[i]);
    memset(pt->history_tmp, 0, (pt->histsize+1)*sizeof(char*));
}

#define clrws(__ws)  do { \
                        __ws.ws_col=0; \
                        __ws.ws_row=0; \
                        __ws.ws_xpixel=0; \
                        __ws.ws_ypixel=0; \
                    } while(0)

char * prompt(prompt_t *pt, char *label) {
    struct termios oldterm, newterm;
    char ch;
    char *buffer = pt->buffer, **history=pt->history, **history_tmp=pt->history_tmp;
    unsigned long curpos=pt->curpos,len=strlen(buffer), histpos=pt->histpos;
    unsigned long buffsize=pt->buffsize, histsize=pt->histsize;
    FILE *instream=pt->instream, *outstream=pt->outstream;
    struct winsize ws;
    
    pt->label=(label!=NULL)? label : pt->label;
    
    fprintf(outstream, "%s%s", pt->label, buffer);
    if ((len-curpos) > 0)
        fprintf(outstream, ESC_SEQ_LEFT_N, (unsigned int)(len-curpos));

    tcgetattr(fileno(instream), &oldterm);
    newterm = oldterm;
    newterm.c_lflag &= (~ICANON)&(~ECHO);
    tcsetattr(fileno(instream), TCSANOW, &newterm);

    for (; ; ) {
        ch = fgetc(instream);
        if (ch==0x1b) { //esc sequence
            if (fgetc(instream)=='[') {
                switch (fgetc(instream)) {
                    
                    case 'A': //up
                        if (histpos == 0) {
                            fprintf(outstream, ESC_SEQ_BELL);
                            break;
                        }
                        
                        history_tmp[histpos] = realloc(history_tmp[histpos], (strlen(buffer)+1)*sizeof(char));
                        strcpy(history_tmp[histpos], buffer);
                        strcpy(buffer,history_tmp[histpos-1]? history_tmp[histpos-1] : history[histpos-1]);

                        histpos--;
                        if (curpos > 0)
                            fprintf(outstream, ESC_SEQ_LEFT_N, (unsigned int)curpos);
                        fprintf(outstream, "%s" ESC_SEQ_ERASE, buffer);
                        
                        curpos=strlen(buffer);
                        len=strlen(buffer);
                        break;
                    case 'B': //down
                        if (histpos == histsize) {
                            fprintf(outstream, ESC_SEQ_BELL);
                            break;
                        }
                        
                        history_tmp[histpos] = realloc(history_tmp[histpos], (strlen(buffer)+1)*sizeof(char));
                        strcpy(history_tmp[histpos], buffer);
                        strcpy(buffer,history_tmp[histpos+1]? history_tmp[histpos+1] : history[histpos+1]); //segfault


                        histpos++;
                        if (curpos > 0)
                            fprintf(outstream, ESC_SEQ_LEFT_N, (unsigned int)curpos);
                        fprintf(outstream, "%s" ESC_SEQ_ERASE, buffer);
                        
                        curpos=strlen(buffer);
                        len=strlen(buffer);
                        
                        break;
                    case 'C': //right
                        if (curpos<len) {
                            
                            curpos++;
                            
                            clrws(ws);
                            ioctl(fileno(outstream), TIOCGWINSZ, &ws);
                            if (((strlen(pt->label)+curpos)%ws.ws_col)==0)
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
                removechar(buffer,curpos);
                fprintf(outstream, ESC_SEQ_LEFT "%s" " " ESC_SEQ_LEFT_N,buffer+curpos,(unsigned int)(len-curpos+1));

            }
            else
                fprintf(outstream, ESC_SEQ_BELL);
        }
        else if ((ch=='\n')||(ch=='\r'))
            break;
        else {
            if ((len+1)>=buffsize) {
                buffsize+=BUFFERSIZE*sizeof(char);
                buffer=realloc(buffer,buffsize);
            }
            curpos++;
            len++;
            insertchar(buffer,ch,curpos-1,buffsize);
            fprintf(outstream, "%s", buffer+curpos-1);
            if ((len-curpos)>0)
                fprintf(outstream, ESC_SEQ_LEFT_N, (unsigned int)(len-curpos));
            
        }
        
    }
    while ((ch!='\n')&&(ch!='\r'));

    buffer[len]='\0'; //safety
    
    history_tmp[histpos] = realloc(history_tmp[histpos], (strlen(buffer)+1)*sizeof(char));
    strcpy(history_tmp[histpos], buffer);

    pt->buffer=buffer;
    pt->buffsize=buffsize;
    pt->curpos=curpos;
    pt->histpos=histpos;
    pt->history_tmp=history_tmp;

    fprintf(outstream, "\n\r");
    
    tcsetattr(fileno(instream), TCSANOW, &oldterm);
    
    return ((len>0) ? buffer : NULL);
}

