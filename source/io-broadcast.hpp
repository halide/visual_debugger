#ifndef IO_STREAM_BROADCAST_H
#define IO_STREAM_BROADCAST_H

#include "io-redirect.hpp"

#ifdef _MSC_VER
    #include <fcntl.h>  // for O_BINARY
    #define PIPE_BUF    4096
    #define pipe(fds)   _pipe(fds, PIPE_BUF, O_BINARY)
    #define read        _read
    #define write       _write
#endif

#include <mutex>
#include <thread>
#include <vector>
#include <atomic>
#include <functional>

struct RefCountBlock
{
    std::atomic<int> ref_count = 1;
    std::function<void()> cleanup = []{};
    void inc() { ++ref_count; }
    void dec()
    {
        if (--ref_count == 0)
        {
            cleanup();
            cleanup = nullptr;
        }
    }
};

struct AutoRefCount
{
    RefCountBlock* block = nullptr;
    AutoRefCount() { }
    AutoRefCount(const AutoRefCount& rhs)
    : block(rhs.block) { block->inc(); }
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
    // keep a reference to the original stream around to restore it once the
    // breadcasting gets terminated
    FILE* stream = nullptr;
    int fd_restore = -1;

    // IO pipe:
    // fd_pipe_endpoints[0]: the  read-end of the pipe
    // fd_pipe_endpoints[1]: the write-end of the pipe
    int fd_pipe_endpoints [2] = { -1, -1 };

    AutoRefCount ref_count;

    typedef void(ReceiverProc)(void*,int);

    std::mutex* mtx = nullptr;
    std::vector< std::function<ReceiverProc> >* receivers = nullptr;

    std::thread::id transmitter_thread_id;

    void Terminate()
    {
        *this = Broadcaster();
    }

    template<typename ReceiverProcT>
    void AddReceiver(ReceiverProcT recv_proc)
    {
        mtx->lock();
        receivers->emplace_back(recv_proc);
        mtx->unlock();
    }

    void AddEcho()
    {
        // echo: write to the original stream, as if not redirected
        auto fd = fd_restore;
        AddReceiver([fd](void* buff, int bytes)
        {
            write(fd, buff, bytes);
        });
    }

    void AddFile(FILE* file)
    {
        AddReceiver([file](void* buff, int bytes)
        {
            fwrite(buff, 1, bytes, file);
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
    std::thread(
        [&]()
        {
            Broadcaster bc;
            bc.stream = stream;

            if (0 != pipe(bc.fd_pipe_endpoints))
            {
                // TODO(marcos): error handling and thread syncing
                return;
            }

            // analogous to 'redirect_temporarily()':
            fflush(stream);
            int fd_before = fileno(stream);
            bc.fd_restore = dup(fd_before);
            int ret = dup2(bc.fd_pipe_endpoints[1], fd_before);

            RefCountBlock rcb;
            rcb.cleanup = [&bc]()
            {
                fflush(bc.stream);
                //fsync(bc.fd_pipe_endpoints[1]);    // flush the write end first,
                //fsync(bc.fd_pipe_endpoints[0]);    // then flush the read end
                close(bc.fd_pipe_endpoints[1]);
                // restore original stream
                dup2(bc.fd_restore, fileno(bc.stream));
            };

            std::mutex mtx;
            std::vector< std::function<Broadcaster::ReceiverProc> > receivers;

            // prepare the originating 'outer' object
            bc.mtx = &mtx;
            bc.receivers = &receivers;
            bc.transmitter_thread_id = std::this_thread::get_id();
            outer = bc;
            outer.ref_count.block = &rcb;   // <- thread syncing point
            // outer should never be referenced again from now on...

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
            _CrtSetReportMode(_CRT_ASSERT, 0);  // <- TODO(marcos): reverse this changes later?
            #endif//_MSC_VER

            char buff [PIPE_BUF];
            while (true)
            {
                auto bytes = read(bc.fd_pipe_endpoints[0], buff, sizeof(buff));
                if (bytes == 0)
                {
                    fprintf(stderr, "BROKEN PIPE: write-end has been closed!\n");
                    close(bc.fd_pipe_endpoints[0]);
                }
                if (bytes == -1)
                {
                    fprintf(stderr, "BROKEN PIPE: read-end has been closed!\n");
                    break;
                }
                mtx.lock();
                for (auto& receiver : receivers)
                {
                    receiver((void*)buff, bytes);
                }
                mtx.unlock();
            }

            int rc = rcb.ref_count;
            if (rc != 0)
            {
                fprintf(stderr, "FATAL: broken pipe, but ref_count is non-zero (=%d)!\n", rc);
                //abort();  // TODO(marcos): should terminate the process?
            }
        }
    ).detach();

    // sync with thread (busy-wait...):
    while (!outer.ref_count.block)
    {
        std::this_thread::yield();
    }

    return outer;
}

#endif//IO_STREAM_BROADCAST_H
