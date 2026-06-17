#ifndef AI_LOG_H
#define AI_LOG_H

#define LOG_DEBUG(fmt, ...)         aiLog(0, "[%s](%d): "fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)          aiLog(1, "[%s](%d): "fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)          aiLog(2, "[%s](%d): "fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...)         aiLog(3, "[%s](%d): "fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__)

void aiLogInit(int log_level);
void aiLogRelease(void);
void aiLog(int level, const char *fmt, ...);

#endif /* AI_LOG_H */
