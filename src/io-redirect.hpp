//
// Licensed under the MIT License. See LICENSE.TXT file in the project root for full license information.
//

#ifndef IO_STREAM_REDIRECT_H
#define IO_STREAM_REDIRECT_H

#include <stdio.h>

#ifdef _MSC_VER
    #include <io.h>
    #define fileno      _fileno
    #define close       _close
    #define dup         _dup
    #define dup2        _dup2
#else // UNIX/POSIX
    #include <unistd.h>
#endif

bool redirect_permanently(FILE* before, FILE* after)
{
    // this routine will permanently redirect writes to the 'before' stream to
    // the 'after' stream, such that both become the same thing; there will be
    // NO WAY to reverse/restore the original 'before' stream!

    // NOTE(marcos): could use 'freopen' here instead of 'dup2' but that would
    // requires passing the correct 'mode' argument to the call...

    // from there on, writing to either stream will have the same effect as to
    // writing to the 'after' stream (NOTE: dup2 will silently close 'before')
    fflush(before);
    fflush(after);
    int fd_before = fileno(before);
    int fd_after  = fileno(after);
    int ret = dup2(fd_after, fd_before);
    return (ret != -1);
}

int redirect_temporarily(FILE* before, FILE* after)
{
    // clone the original file descriptor to keep the original stream alive:
    int fd_before = fileno(before);
    int fd_cloned = dup(fd_before);

    // even though 'redirect_permanently()' gets called here, it is all right
    // since the original file descriptor has been cloned, making the 'before'
    // stream remain alive in 'fd_cloned'
    if (!redirect_permanently(before, after))
    {
        close(fd_cloned);
        return -1;
    }
    return fd_cloned;
}

bool redirect_restore(FILE* stream, int fd)
{
    // this routine will restore the 'stream' to its original behavior prior to
    // the redirection started by 'redirect_temporarily()'; note that 'fd' must
    // be the value returned by the call to 'redirect_temporarily()'
    fflush(stream);
    int fd_stream = fileno(stream);
    int ret = dup2(fd, fd_stream);
    return (ret != -1);
}

#endif//IO_STREAM_REDIRECT_H
