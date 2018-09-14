//
// Licensed under the MIT License. See LICENSE.TXT file in the project root for full license information.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cassert>

#ifdef _MSC_VER
    #include <direct.h>
    #define chdir  _chdir
    #define getcwd _getcwd
    #define strcat strcat_s
#else
    #include <unistd.h>
#endif//_MSC_VER

static bool Init(void)
{
    // initialization = switching working directory to the data directory

    // set working directory using a relative path:
    // (this is a handy hack to get a path relative to the location of this source file)
    char path [] = __FILE__ "/../../";

    // regrettably, simply calling chdir() with the path above may not work on
    // some implementations of the standard library... instead, it is best to
    // manipulate it first to ensure that only directory names are in the path:
    auto this_file = __FILE__;
    auto slash = strrchr(this_file, '/');
#ifdef _WIN32
    // Windows also accepts backslashes, which can even be mixed with forward
    // slashes in path strings! the correct choice is whichever is right-most:
    auto backslash = strrchr(this_file, '\\');
    if (backslash > slash)
    {
        slash = backslash;
    }
#endif//_WIN32
    assert(slash);
    auto slash_pos = slash - this_file;
    path[slash_pos] = '\0';
    strcat(path, "/../");

    if (chdir(path) != 0)
    {
        fprintf(stderr, "ERROR: unable to auto-locate the assets directory.\n");
        exit(-9);
    }

    // retrieve new working directory as an absolute path:
    // (sizeof(path) bytes should be always enough to hold the absolute path)
    char cwd [sizeof(path)-1] = { };
    auto ret = getcwd(cwd, sizeof(cwd));
    assert(ret == cwd);
    fprintf(stdout, "Switched working directory to the assets directory:\n\t'%s'\n", cwd);
    return true;
}



// This will change the working directory at program startup, prior to main():
static bool initialized = Init();
