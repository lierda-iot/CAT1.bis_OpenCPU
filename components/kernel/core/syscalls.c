#include <sys/stat.h>
// #include "sctdef.h"
// #ifdef FEATURE_BOOTLOADER_PROJECT_ENABLE
// #include "bl_link_mem_map.h"
// #else
// #define HEAP_EXIST     1
// #endif
extern int io_putc(int ch) __attribute__((weak));
extern int io_getc(void) __attribute__((weak));

extern int _close(int file) __attribute__((used));
extern int _fstat(int file, struct stat *st) __attribute__((used));
extern int _isatty(int file) __attribute__((used));
extern int _lseek(int file, int ptr, int dir) __attribute__((used));
extern int _open(const char *name, int flags, int mode) __attribute__((used));
extern int _read(int file, char *ptr, int len) __attribute__((used));
extern int _write(int file, char *ptr, int len) __attribute__((used));

int _close(int file)
{
    return 0;
}

int _fstat(int file, struct stat *st)
{
    return 0;
}

int _isatty(int file)
{
    return 1;
}

int _lseek(int file, int ptr, int dir)
{
    return 0;
}

int _open(const char *name, int flags, int mode)
{
    return -1;
}

int _read(int file, char *ptr, int len)
{
    return 0;
    int DataIdx;

    for (DataIdx = 0; DataIdx < len; DataIdx++) {
        *ptr++ = io_getc();
    }

    return len;
}


#define __MYPID 1
int _getpid()
{
    return __MYPID;
}



int _kill(int pid, int sig)
{
    return -1;
}

void _exit(int val)
{
    while(1);
}

caddr_t _sbrk(int incr)
{
#if HEAP_EXIST
    extern char _heap_memory_start, _heap_memory_end; /* Defined by the linker */

    static char *heap_end;
    char        *prev_heap_end;

    if (heap_end == NULL) {
        heap_end = (char *)&_heap_memory_start;
    }

    prev_heap_end = heap_end;

    if (heap_end + incr >= (char *)&_heap_memory_end) {

        return (caddr_t) - 1;
    }

    heap_end += incr;

    return (caddr_t) prev_heap_end;
#else
    // extern int EC_Printf(const char * pFormat, ...);
    // extern void delay_us(uint32_t us);
    // do
    // {
    //     EC_Printf("heap not exist err, malloc unsupported!");
    //     delay_us(1000000);
    // }while(1);

    return NULL;
#endif
}



int _write(int file, char *ptr, int len)
{
	// extern int io_putchar(int ch);
    // int DataIdx;

    // for (DataIdx = 0; DataIdx < len; DataIdx++) {
	// 	io_putchar(*ptr++);
    // }
    return len;
}





