
//
// This source file is part of appleseed.
// Visit http://appleseedhq.net/ for additional information and resources.
//
// This software is released under the MIT license.
//
// Copyright (c) 2010-2013 Francois Beaune, Jupiter Jazz Limited
// Copyright (c) 2014-2017 Francois Beaune, The appleseedhq Organization
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

// Interface header.
#include "path.h"

// appleseed.foundation headers.
#ifdef __APPLE__
#include "foundation/platform/types.h"
#endif
#include "foundation/utility/iterators.h"

// Boost headers.
#include "boost/filesystem/operations.hpp"

// Standard headers.
#include <cassert>
#include <cstddef>
#include <cstring>

// Platform headers.
#if defined __APPLE__
#include <mach-o/dyld.h>
#elif defined __linux__
#include <sys/types.h>
#include <pwd.h>
#include <unistd.h>
#elif defined __FreeBSD__
#include <sys/sysctl.h>
#include <pwd.h>
#include <unistd.h>
#endif

using namespace std;
namespace bf = boost::filesystem;

namespace foundation
{

const char* get_executable_path()
{
    static char path[FOUNDATION_MAX_PATH_LENGTH + 1];
    static bool path_initialized = false;

    if (!path_initialized)
    {
// Windows.
#if defined _WIN32

        const DWORD result =
            GetModuleFileName(
                GetModuleHandle(NULL),
                path,
                sizeof(path));
        assert(result != 0);

// macOS.
#elif defined __APPLE__

        uint32 path_len = MAXPATHLEN;
        const int result = _NSGetExecutablePath(path, &path_len);
        assert(result == 0);

// Linux.
#elif defined __linux__

        const ssize_t result = readlink("/proc/self/exe", path, sizeof(path) - 1);
        assert(result > 0);
        path[result] = '\0';

// FreeBSD.
#elif defined __FreeBSD__

        const int mib[4] = { CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, -1 };
        size_t len = sizeof(path);
        const int result = sysctl(mib, 4, path, &len, 0x0, 0);
        assert(result == 0);

// Other platforms.
#else

        #error Unsupported platform.

#endif

        path_initialized = true;
    }

    return path;
}

const char* get_executable_directory()
{
    static char path[FOUNDATION_MAX_PATH_LENGTH + 1];
    static bool path_initialized = false;

    if (!path_initialized)
    {
        bf::path executable_path(get_executable_path());

        assert(executable_path.has_filename());
        executable_path.remove_filename();

        assert(executable_path.string().size() <= FOUNDATION_MAX_PATH_LENGTH);
        strncpy(path, executable_path.string().c_str(), sizeof(path) - 1);
        path[sizeof(path) - 1] = '\0';

        path_initialized = true;
    }

    return path;
}

const char* get_home_directory()
{
    static char path[FOUNDATION_MAX_PATH_LENGTH + 1];
    static bool path_initialized = false;

    if (!path_initialized)
    {
// Windows.
#if defined _WIN32

        return 0;

// macOS.
#elif defined __APPLE__

        return 0;

// Other Unices.
#elif defined __linux__ || defined __FreeBSD__

        const char* home_dir = getenv("HOME");

        if (home_dir == 0)
            home_dir = getpwuid(getuid())->pw_dir;

        strncpy(path, home_dir, FOUNDATION_MAX_PATH_LENGTH);
        path[FOUNDATION_MAX_PATH_LENGTH] = '\0';

// Other platforms.
#else

        #error Unsupported platform.

#endif

        path_initialized = true;
    }

    return path;
}

void split_paths(
    const bf::path&     p1,
    const bf::path&     p2,
    bf::path&           common,
    bf::path&           r1,
    bf::path&           r2)
{
    assert(common.empty());
    assert(r1.empty());
    assert(r2.empty());

    bf::path::const_iterator i1 = p1.begin();
    bf::path::const_iterator i2 = p2.begin();

    while (i1 != p1.end() && i2 != p2.end())
    {
        if (*i1 != *i2)
            break;

        if ((p1.has_filename() && succ(i1) == p1.end()) ||
            (p2.has_filename() && succ(i2) == p2.end()))
            break;

        common /= *i1;

        ++i1, ++i2;
    }

    while (i1 != p1.end())
        r1 /= *i1++;

    while (i2 != p2.end())
        r2 /= *i2++;
}

}   // namespace foundation
