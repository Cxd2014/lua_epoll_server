#ifndef main_h
#define main_h

#define LOG_DIE 0
#define LOG_PERROR 1
#define LOG_ERROR 2
#define LOG_INFO 3
#define LOG_DEBUG 4


#define log_die(fmt...) \
    do {log_error_core(LOG_DIE, __FILE__, __LINE__, fmt); \
        exit(0);} while(0);

#define log_perror(fmt...) \
    log_error_core(LOG_PERROR, __FILE__, __LINE__, fmt)

#define log_error(fmt...)                                        \
    log_error_core(LOG_ERROR, __FILE__, __LINE__, fmt)

#define log_info(fmt...)                                        \
    log_error_core(LOG_INFO, __FILE__, __LINE__, fmt)

#define log_lua_debug(file, line, fmt...) \
    log_error_core(LOG_INFO, file, line, fmt)

#define log_debug(fmt...)                                        \
    log_error_core(LOG_DEBUG, __FILE__, __LINE__, fmt)


extern char log_time[64];

void update_log_time();
void log_init(char *path, char *level);
void log_exit();
void log_error_core(int level, const char *file, int line, const char *fmt, ...);

#define LOG_STR_LEN 2048

#ifdef __GNUC__
	
#define zero_alloc(size) ({					\
	void *__r = malloc(size);				\
	if (!__r) log_die("Out of memory\n");		\
	memset(__r, 0, size);					\
	__r;									\
})
#else
#error not gcc compiling, TODO
#endif

#endif