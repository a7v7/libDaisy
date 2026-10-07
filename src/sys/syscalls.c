/** System calls for newlib.
 *
 *  These replace the libnosys stubs (--specs=nosys.specs), which fail every
 *  call and make the linker warn "_write is not implemented and will always
 *  fail" for any program that pulls in stdio.
 *
 *  This file is compiled into the executable rather than into libdaisy.a:
 *  the C library is searched after libdaisy.a, so system calls inside the
 *  archive would never be found.
 */
#include <errno.h>
#include <stddef.h>
#include <sys/stat.h>
#include <sys/times.h>
#include <sys/time.h>
#include "stm32h7xx.h"
#include "sys/syscalls.h"

/* Heap bounds from the linker script: end of .bss to the end of its RAM. */
extern char end;
extern char _heap_limit;

static dsy_stdio_write_t stdio_write = NULL;
static dsy_stdio_read_t  stdio_read  = NULL;
static char*             heap_top    = &end;

void dsy_stdio_set_write(dsy_stdio_write_t fn)
{
    stdio_write = fn;
}

void dsy_stdio_set_read(dsy_stdio_read_t fn)
{
    stdio_read = fn;
}

int _write(int fd, const char* data, int size)
{
    if(fd != 1 && fd != 2)
    {
        errno = EBADF;
        return -1;
    }
    /* Never from an interrupt; with no hook, discard. Either way report
     * success so stdio doesn't retry or flag an error. */
    if(__get_IPSR() != 0 || stdio_write == NULL)
        return size;
    return stdio_write(data, size);
}

int _read(int fd, char* data, int size)
{
    if(fd != 0)
    {
        errno = EBADF;
        return -1;
    }
    if(__get_IPSR() != 0 || stdio_read == NULL)
        return 0;
    return stdio_read(data, size);
}

void* _sbrk(ptrdiff_t incr)
{
    char* prev = heap_top;
    if(incr > 0 && (size_t)(&_heap_limit - heap_top) < (size_t)incr)
    {
        errno = ENOMEM;
        return (void*)-1;
    }
    heap_top += incr;
    return prev;
}

void _exit(int status)
{
    (void)status;
    /* Stop in the debugger if one is attached, otherwise restart. */
    if(CoreDebug->DHCSR & CoreDebug_DHCSR_C_DEBUGEN_Msk)
        __BKPT(0);
    NVIC_SystemReset();
    for(;;) {}
}

/* stdin, stdout and stderr are character devices; there are no files. */

int _isatty(int fd)
{
    if(fd >= 0 && fd <= 2)
        return 1;
    errno = EBADF;
    return 0;
}

int _fstat(int fd, struct stat* st)
{
    if(fd < 0 || fd > 2)
    {
        errno = EBADF;
        return -1;
    }
    st->st_mode = S_IFCHR;
    return 0;
}

int _lseek(int fd, int offset, int whence)
{
    (void)fd;
    (void)offset;
    (void)whence;
    errno = ESPIPE;
    return -1;
}

int _close(int fd)
{
    (void)fd;
    errno = EBADF;
    return -1;
}

int _open(const char* path, int flags, int mode)
{
    (void)path;
    (void)flags;
    (void)mode;
    errno = ENOSYS;
    return -1;
}

/* No processes, signals or file system. */

int _getpid(void)
{
    return 1;
}

int _kill(int pid, int sig)
{
    (void)pid;
    (void)sig;
    errno = EINVAL;
    return -1;
}

int _stat(const char* path, struct stat* st)
{
    (void)path;
    (void)st;
    errno = ENOSYS;
    return -1;
}

int _link(const char* old_path, const char* new_path)
{
    (void)old_path;
    (void)new_path;
    errno = ENOSYS;
    return -1;
}

int _unlink(const char* path)
{
    (void)path;
    errno = ENOSYS;
    return -1;
}

int _fork(void)
{
    errno = ENOSYS;
    return -1;
}

int _execve(const char* path, char* const argv[], char* const envp[])
{
    (void)path;
    (void)argv;
    (void)envp;
    errno = ENOSYS;
    return -1;
}

int _wait(int* status)
{
    (void)status;
    errno = ECHILD;
    return -1;
}

int _gettimeofday(struct timeval* tv, void* tz)
{
    (void)tv;
    (void)tz;
    errno = ENOSYS;
    return -1;
}

clock_t _times(struct tms* buf)
{
    (void)buf;
    errno = ENOSYS;
    return (clock_t)-1;
}
