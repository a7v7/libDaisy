/** @addtogroup system
    @{
*/

/** Hooks that connect the C library's stdin / stdout / stderr to hardware.
 *
 *  libDaisy provides its own system calls (src/sys/syscalls.c) for newlib.
 *  Until a hook is installed, output to stdout / stderr is discarded and
 *  stdin reads as end-of-file, so printf() is safe to call but goes nowhere.
 *
 *  Output from interrupt context is always discarded: printf() is far too
 *  slow, and not reentrant, to run inside an interrupt handler.
 *
 *  Example - send printf() to a UART:
 *  @code
 *  static int UartWrite(const char* data, int size)
 *  {
 *      uart.BlockingTransmit((uint8_t*)data, size);
 *      return size;
 *  }
 *  ...
 *  dsy_stdio_set_write(UartWrite);
 *  @endcode
 */
#ifndef DSY_SYSCALLS_H
#define DSY_SYSCALLS_H

#ifdef __cplusplus
extern "C"
{
#endif

    /** Writes size bytes for stdout / stderr. Returns the number written. */
    typedef int (*dsy_stdio_write_t)(const char* data, int size);

    /** Reads up to size bytes for stdin. Returns the number read (0 = none). */
    typedef int (*dsy_stdio_read_t)(char* data, int size);

    /** Installs the stdout / stderr hook. Pass NULL to discard output. */
    void dsy_stdio_set_write(dsy_stdio_write_t fn);

    /** Installs the stdin hook. Pass NULL for end-of-file. */
    void dsy_stdio_set_read(dsy_stdio_read_t fn);

#ifdef __cplusplus
}
#endif

#endif
/** @} */
