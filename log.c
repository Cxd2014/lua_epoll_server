#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include <sys/time.h>

#include "lua_lib.h"
#include "main.h"

char log_time[64];
static int log_fd = 0;
static int log_level = 0;

static char *debug_level[] = {
    "die",
    "perror",
    "error",
    "info",
    "debug",
    NULL,
};

void update_log_time()
{
	/* 获取当前时间 */
	struct timeval tv;
	struct tm *tmp;
    gettimeofday(&tv,NULL);
    tmp = localtime(&tv.tv_sec);  
    strftime(log_time, sizeof(log_time), "%Y-%m-%d %H:%M:%S", tmp); 
}

void log_init(char *path, char *level)
{
    log_fd = open(path, O_CREAT|O_APPEND|O_WRONLY, 0644);
    if(log_fd < 0){
        perror("open log file error");
        exit(0);
    }

    int i = 0;
    while (debug_level[i] != NULL) {
        if (strcmp(debug_level[i], level) == 0)
            break;
        i++;
    }
    log_level = i;
}
            
void log_error_core(int level, const char *file, int line, const char *fmt, ...)
{ 
    if (level > log_level)
        return;

    int len = 0;
    char str[LOG_STR_LEN] = {0};
    char tag[LOG_STR_LEN] = {0};

    snprintf(tag, LOG_STR_LEN, "%s (%s:%d)[%s] >> ", log_time, file, line, debug_level[level]);

    va_list args;
    va_start(args, fmt);
    vsnprintf(str, LOG_STR_LEN, fmt, args);
    va_end(args);

    strncat(tag, str, LOG_STR_LEN);

    if(level <= LOG_PERROR) {
        strncat(tag, ": ", LOG_STR_LEN);
        strncat(tag, strerror(errno), LOG_STR_LEN);        
    }
        
    len = strlen(tag);
    tag[len] = '\n';
    if (write(log_fd, tag, len + 1) < 0){
        perror("write log file error");
        exit(0);
    }
}

void log_exit()
{
    close(log_fd);
}
