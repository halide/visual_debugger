#ifndef IO_STREAM_BROADCAST_H
#define IO_STREAM_BROADCAST_H

#include "io-redirect.hpp"

#ifdef _MSC_VER
    #include <crtdbg.h> // for _CrtSetReportMode
    #include <fcntl.h>  // for O_BINARY and O_TEXT
    #define PIPE_BUF    4096
    #define pipe(fds)   _pipe(fds, PIPE_BUF, O_TEXT)
    #define read        _read
    #define write       _write
    #define isatty      _isatty
#endif

#include <mutex>
#include <thread>
#include <vector>
#include <atomic>
#include <cassert>
#include <functional>

// stdin    0   STDIN_FILENO
// stdout   1   STDOUT_FILENO
// stderr   2   STDERR_FILENO

struct RefCountBlock
{
    std::atomic<int> ref_count { 1 };
    std::function<void()> cleanup = []{};
    void inc() { ++ref_count; }
    void dec()
    {
        if (--ref_count == 0)
        {
            cleanup();
        }
    }
};

struct AutoRefCount
{
    RefCountBlock* block = nullptr;
    AutoRefCount() { }
    AutoRefCount(const AutoRefCount& rhs)
    : block(rhs.block) { block->inc(); }
    AutoRefCount(AutoRefCount&& rhs) { std::swap(block, rhs.block); }
    ~AutoRefCount() { if (block) block->dec(); }
    AutoRefCount& operator = (const AutoRefCount& rhs)
    {
        if (block) block->dec();
        block = rhs.block;
        if (block) block->inc();
        return *this;
    }
};

struct Broadcaster
{
    typedef void(ReceiverProc)(void*, int);

    struct Implementation
    {
        // keep a reference to the original stream around to restore it once the
        // breadcasting gets terminated
        FILE* stream = nullptr;
        int fd_original = -1;
        int fd_backedup = -1;

        // IO pipe:
        // fd_pipe_endpoints[0]: the  read-end of the pipe
        // fd_pipe_endpoints[1]: the write-end of the pipe
        int fd_pipe_endpoints [2] = { -1, -1 };

        std::mutex mtx;
        std::vector< std::function<ReceiverProc> > receivers;

        std::thread::id transmitter_thread_id;
    };

    Implementation* impl = nullptr;

    AutoRefCount ref_count;

    bool Valid() const
    {
        return (impl != nullptr);
    }

    void Terminate()
    {
        *this = Broadcaster();
    }

    template<typename ReceiverProcT>
    void AddReceiver(ReceiverProcT recv_proc)
    {
        if (!Valid())
        {
            return;
        }
        impl->mtx.lock();
        impl->receivers.emplace_back(recv_proc);
        impl->mtx.unlock();
    }

    void AddEcho(bool* toggle = nullptr)
    {
        // echo: write to the original stream, as if not redirected
        // avoid using 'this' in the lambda since it is possible for this instance
        // to be 'Terminate()'d and 'this->impl' will become null; instead, pass a
        // copy of 'impl' to the lambda since it should outlive the Boradcaster as
        // the implementation object persists in the transmitter thread.
        auto this_impl = impl;
        AddReceiver([this_impl, toggle](void* buff, int bytes)
        {
            if (!toggle || *toggle)
            {
                write(this_impl->fd_backedup, buff, bytes);
            }
        });
    }

    void AddFile(FILE* file, bool* toggle = nullptr)
    {
        AddReceiver([file, toggle](void* buff, int bytes)
        {
            if (!toggle || *toggle)
            {
                fwrite(buff, 1, bytes, file);
            }
        });
    }
};

Broadcaster redirect_broadcast(FILE* stream)
{
    Broadcaster outer;

    // spawn a dedicated thread to handle the broadcast; altenatively, there is
    // also POSIX poll/select to handle IO stream multiplexing; unfortunately,
    // these routines do not have POSIX-compliant implementation on Windows...
    // (Winsock provides a POSIX-like select() function, but only for sockets)
    volatile bool ready = false;
    std::thread(
        [&]()
        {
            Broadcaster::Implementation bci;

            if (0 != pipe(bci.fd_pipe_endpoints))
            {
                fprintf(stderr, "ERROR: [io-broadcast] unable to open redirection pipes!\n");
                ready = true;
                return;
            }
            // disable buffering on the file stream object -> immediate flush
            //setbuf(stream, NULL);
            //setvbuf(stream, NULL, _IOFBF, BUFSIZ);    // re-enable buffering

            // analogous to 'redirect_temporarily()':
            bci.stream = stream;
            fflush(bci.stream);
            bci.fd_original = fileno(bci.stream);
            bci.fd_backedup = dup(bci.fd_original);
            if (-1 == bci.fd_backedup)
            {
                fprintf(stderr, "ERROR: [io-broadcast] unable backup stream!\n");
                return;
            }
            // MENTAL NOTE(marcos): It seems that the reason why the stream is
            // losing its original buffering settings when restoring from pipe
            // redirection is because 'dup2()' ends up closing 'fd_original' in
            // the process (but this might also be a red herring...)
            if (dup2(bci.fd_pipe_endpoints[1], bci.fd_original) == -1)
            {
                fprintf(stderr, "ERROR: [io-broadcast] unable to tie stream to pipe!\n");
                close(bci.fd_backedup);
                return;
            }

            std::mutex running, cleaning;
            running.lock();

            RefCountBlock rcb;
            rcb.cleanup = [&]()
            {
                cleaning.lock();

                // copy values, since it will be unsafe to touch 'bci' once the
                // write pipe gets closed below...
                FILE* stream = bci.stream;
                int fd_pipe_write_end = bci.fd_pipe_endpoints[1];
                int fd_original = bci.fd_original;
                int fd_backedup = bci.fd_backedup;

                fflush(stream);
                //fsync(fd_pipe_write_end);   // flush the write end first,
                //fsync(fd_pipe_read_end);    // then flush the read end
                close(fd_pipe_write_end);
                // from now on, do not access 'bci' fields!

                // restore original stream
                assert(fd_original == fileno(stream));
                dup2(fd_backedup, fd_original);
                close(fd_backedup);

                #ifdef _MSC_VER
                // Visual Studio shenanigans...
                // if the original fd is tied to a terminal/console (or to any
                // character device, really) then FILE* streaming on it should
                // be unbuffered; just restoring the fd mapping above does not
                // seem to recover the original unbuffered behavior...
                if (isatty(fd_original) != 0)
                {
                    setvbuf(stream, NULL, _IONBF, 0);
                }
                #endif//_MSC_VER

                // sync with transmitter thread:
                cleaning.unlock();
                running.lock();
                running.unlock();
            };

            // prepare the originating 'outer' object
            bci.transmitter_thread_id = std::this_thread::get_id();
            outer.ref_count.block = &rcb;
            outer.impl = &bci;
            ready = true;                  // <- thread syncing point
            // from now on, 'outer' should never be referenced again!

            #ifdef  _MSC_VER
            // Visual Studio CRT shenanigans, otherwise a read on a broken pipe
            // will assert and abort...
            auto myInvalidParameterHandler =
                [](
                    const wchar_t* expression,
                    const wchar_t* function,
                    const wchar_t* file,
                    unsigned int line,
                    uintptr_t pReserved
                )
                {
                    //fwprintf(stderr, L"Invalid parameter detected in function %s."
                    //                 L" File: %s Line: %d\n", function, file, line);
                    //fwprintf(stderr, L"Expression: %s\n", expression);
                    //abort();
                };
            _set_thread_local_invalid_parameter_handler(myInvalidParameterHandler);
            // It would seem that "_CrtSetReportMode()" is also operating on a
            // per-thread basis, so there's no need for reversing it later
            _CrtSetReportMode(_CRT_ASSERT, 0);
            #endif//_MSC_VER

            char buff [PIPE_BUF];
            while (true)
            {
                auto bytes = read(bci.fd_pipe_endpoints[0], buff, sizeof(buff));
                if (bytes == 0)
                {
                    fprintf(stderr, "BROKEN PIPE: [io-broadcast] the write-end of the pipe has been closed!\n");
                    close(bci.fd_pipe_endpoints[0]);
                }
                if (bytes == -1)
                {
                    fprintf(stderr, "BROKEN PIPE: [io-broadcast] the read-end of the pipe has been closed!\n");
                    break;
                }
                bci.mtx.lock();
                for (auto& receiver : bci.receivers)
                {
                    receiver((void*)buff, bytes);
                }
                bci.mtx.unlock();
            }

            int rc = rcb.ref_count;
            if (rc != 0)
            {
                fprintf(stderr, "FATAL: [io-broadcast] broken pipe, but ref_count is non-zero (=%d)!\n", rc);
                abort();  // TODO(marcos): should terminate the process?
            }

            // sync with cleanup thread:
            running.unlock();
            cleaning.lock();
            cleaning.unlock();
            running.lock();
            running.unlock();
        }
    ).detach();

    // sync with transmitter thread:
    // TODO(marcos): replace this busy-waiting by some dual cross-mutex pattern
    // as is done with the 'running-cleaning' mutexes above
    while (!ready)
    {
        std::this_thread::yield();
    }

    return outer;
}

#endif//IO_STREAM_BROADCAST_H
