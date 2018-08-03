#ifndef IO_STREAM_REDIRECT_H
#define IO_STREAM_REDIRECT_H

#ifdef _MSC_VER
    #include <io.h>
    #define fileno      _fileno
    #define dup2        _dup2
#else // UNIX/POSIX
    #include <poll.h>
#endif

bool redirect_permanently(FILE* before, FILE* after)
{
    // this routine will permanently redirect writes to the 'before' stream to
    // the 'after' stream, such that both become the same thing; there will be
    // NO WAY to reverse/restore the original 'before' stream!

    // could use 'freopen' instead of 'dup2' but it requires setting the 'mode'

    // from there on, writing to either stream will have the same effect as to
    // writing to the 'after' stream (NOTE: dup2 will silently close 'before')
    int fd_before = fileno(before);
    int fd_after  = fileno(after);
    int ret = dup2(fd_after, fd_before);
    return (ret != -1);
}

#endif//IO_STREAM_REDIRECT_H
