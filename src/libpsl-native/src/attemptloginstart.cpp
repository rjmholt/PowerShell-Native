// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "attemptloginstart.h"

#include <unistd.h>

#if defined(__APPLE__) && defined(__MACH__)

#include <sys/types.h>
#include <sys/sysctl.h>

#endif

int32_t AttemptLoginStart(const char *guardEnvVarName, char **programArgs, int programArgsCount)
{
    if (getenv(guardEnvVarName) != NULL)
    {
        unsetenv(guardEnvVarName);
        return 0;
    }

    setenv(guardEnvVarName, "1", /*overwrite*/ 1);

#if defined(__linux__)
    return AttemptLoginLinux(programArgs, programArgsCount);
#elif defined(__APPLE__) && defined(__MACH__)
    return AttemptLoginMacOS(programArgs, programArgsCount);
#endif
}

int32_t AttemptLoginMacOS(char **programArgs, int programArgsCount)
{
    int mib[3];
    mib[0] = CTL_KERN;
    mib[1] = KERN_ARGMAX;
    int mibsize = 2;
    size_t argmax;
    size_t size = sizeof(argmax);

    sysctl(mib, mibsize, &argmax, &size, NULL, 0);

    // We'll never free this; we need bits of it for the exec call, which will pave over it
    char *procargs = (char*)calloc(argmax, sizeof(char));

    pid_t pid = getpid();

    mib[0] = CTL_KERN;
    mib[1] = KERN_PROCARGS2;
    mib[2] = pid;
    mibsize = 3;

    sysctl(mib, mibsize, procargs, &argmax, NULL, 0);
    
    char *argsPtr = procargs;

    // Skip over argc
    argsPtr += sizeof(int);

    // Skip over the executable path, but remember where it is
    char *pwshExecutablePath = argsPtr;

    while (*argsPtr != NULL) { argsPtr++; }
    while (*argsPtr == NULL) { argsPtr++; }

    char argv0FirstChar = *argsPtr;

    if (!IsLogin(argv0FirstChar, programArgs, programArgsCount))
    {
        return 0;
    }

    return DoExec("/bin/zsh", pwshExecutablePath, programArgs, programArgsCount);
}

bool IsLogin(char argv0FirstChar, char **programArgs, int programArgsCount)
{
    if (argv0FirstChar == '-')
    {
        return true;
    }

    bool expectingArgument = false;
    for (int i = 0; i < programArgsCount; i++, programArgs++)
    {
        // Skip over arguments passed to parameters that expect them
        if (expectingArgument)
        {
            expectingArgument = false;
            continue;
        }

        char *arg = *programArgs;

        int length = strlen(arg);

        // Quickly eliminate file paths that don't look like parameters:
        //   - The empty string
        //   - Arguments not starting with '-' (including "/?")
        //   - The stdin argument "-"
        //   - The help parameter "-?"
        if (length == 0
            || arg[0] != '-'
            || length == 1
            || arg[1] == '?')
        {
            return false;
        }

        // Now look for the -Login parameter
        if (IsParam(arg, "login", "l"))
        {
            return true;
        }

        // Otherwise, look for parameters that preclude -Login from being seen afterward
        if (IsParam(arg, "file", "f")
            || IsParam(arg, "command", "c")
            || IsParam(arg, "version", "v")
            || IsParam(arg, "help", "h"))
        {
            return false;
        }

        // We need to look for parameters that take arguments,
        // since they preclude a file name
        if (IsParam(arg, "configurationname", "config")
            || IsParam(arg, "custompipename", "cus")
            || IsParam(arg, "encodedcommand", "e", "ec")
            || IsParam(arg, "executionpolicy", "ex", "ep")
            || IsParam(arg, "inputformat", "in", "if")
            || IsParam(arg, "outputformat", "o", "of")
            || IsParam(arg, "settingsfile", "settings")
            || IsParam(arg, "windowstyle", "w")
            || IsParam(arg, "workingdirectory", "wo", "wd"))
        {
            expectingArgument = true;
            continue;
        }

        // Otherwise, we need to skip over valid parameters
        if (IsParam("interactive", "i")
            || IsParam("noexit", "noe")
            || IsParam("nologo", "nol")
            || IsParam("noninteractive", "noni")
            || IsParam("noprofile", "nop"))
        {
            continue;
        }

        // An unrecognised parameter counts as a filename
        return false;
    }
}

bool IsParam(char *arg, const char *fullParam, const char *shortestPrefix)
{

}
