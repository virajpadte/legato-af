//--------------------------------------------------------------------------------------------------
/**
 * @file envVars.cpp
 *
 * Environment variable helper functions used by various modules.
 *
 * Copyright (C) Sierra Wireless Inc.
 */
//--------------------------------------------------------------------------------------------------

#include "mkTools.h"
#include <string.h>

/// The standard C environment variable list.
extern char**environ;


namespace envVars
{


//--------------------------------------------------------------------------------------------------
/**
 * Fetch the value of a given optional environment variable.
 *
 * @return  The value ("" if not found).
 */
//--------------------------------------------------------------------------------------------------
std::string Get
(
    const std::string& name  ///< The name of the environment variable.
)
//--------------------------------------------------------------------------------------------------
{
    const char* value = getenv(name.c_str());

    if (value == nullptr)
    {
        return "";
    }

    return value;
}


//--------------------------------------------------------------------------------------------------
/**
 * Fetch the value of a given mandatory environment variable.
 *
 * @return  The value.
 *
 * @throw   Exception if environment variable not found.
 */
//--------------------------------------------------------------------------------------------------
std::string GetRequired
(
    const std::string& name  ///< The name of the environment variable.
)
//--------------------------------------------------------------------------------------------------
{
    const char* value = getenv(name.c_str());

    if (value == nullptr)
    {
        throw mk::Exception_t(
            mk::format(LE_I18N("The required environment variable %s has not been set."), name)
        );
    }

    return value;
}


//--------------------------------------------------------------------------------------------------
/**
 * Set the value of a given environment variable.  If the variable already exists, replaces its
 * value.
 */
//--------------------------------------------------------------------------------------------------
void Set
(
    const std::string& name,  ///< The name of the environment variable.
    const std::string& value  ///< The value of the environment variable.
)
//--------------------------------------------------------------------------------------------------
{
    if (setenv(name.c_str(), value.c_str(), true /* overwrite existing */) != 0)
    {
        throw mk::Exception_t(
            mk::format(LE_I18N("Failed to set environment variable '%s' to '%s'."), name, value)
        );
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Set compiler, linker, etc. environment variables according to the target device type, if they're
 * not already set.
 */
//--------------------------------------------------------------------------------------------------
static void SetToolChainVars
(
    const mk::BuildParams_t& buildParams
)
//--------------------------------------------------------------------------------------------------
{
    if (!buildParams.cCompilerPath.empty())
    {
        auto cCompilerPath = buildParams.cCompilerPath;
        if (!buildParams.compilerCachePath.empty())
        {
            cCompilerPath = buildParams.compilerCachePath + " " +
                            buildParams.cCompilerPath;
        }

        Set("CC", cCompilerPath.c_str());
    }

    if (!buildParams.cxxCompilerPath.empty())
    {
        auto cxxCompilerPath = buildParams.cxxCompilerPath;
        if (!buildParams.compilerCachePath.empty())
        {
            cxxCompilerPath = buildParams.compilerCachePath + " " +
                              buildParams.cxxCompilerPath;
        }

        Set("CXX", cxxCompilerPath.c_str());
    }

    if (!buildParams.sysrootPath.empty())
    {
        Set("LEGATO_SYSROOT", buildParams.sysrootPath.c_str());
    }

    if (!buildParams.linkerPath.empty())
    {
        Set("LD", buildParams.linkerPath.c_str());
    }

    if (!buildParams.archiverPath.empty())
    {
        Set("AR", buildParams.archiverPath.c_str());
    }

    if (!buildParams.assemblerPath.empty())
    {
        Set("AS", buildParams.assemblerPath.c_str());
    }

    if (!buildParams.stripPath.empty())
    {
        Set("STRIP", buildParams.stripPath.c_str());
    }

    if (!buildParams.objcopyPath.empty())
    {
        Set("OBJCOPY", buildParams.objcopyPath.c_str());
    }

    if (!buildParams.readelfPath.empty())
    {
        Set("READELF", buildParams.readelfPath.c_str());
    }
    else
    {
        std::cout << "*************************** readelfPath is empty\n";
    }

    if (!buildParams.compilerCachePath.empty())
    {
        Set("CCACHE", buildParams.compilerCachePath.c_str());
    }
}


//----------------------------------------------------------------------------------------------
/**
 * Adds target-specific environment variables (e.g., LEGATO_TARGET) to the process's environment.
 *
 * The environment will get inherited by any child processes, including the shell that is used
 * to run the compiler and linker.  Also, this allows these environment variables to be used in
 * paths in .sdef, .adef, and .cdef files.
 */
//----------------------------------------------------------------------------------------------
void SetTargetSpecific
(
    const mk::BuildParams_t& buildParams
)
//--------------------------------------------------------------------------------------------------
{
    // WARNING: If you add another target-specific variable, remember to update IsReserved().

    // Set compiler, linker, etc variables specific to the target device type, if they're not set.
    SetToolChainVars(buildParams);

    // Set LEGATO_TARGET.
    Set("LEGATO_TARGET", buildParams.target.c_str());

    // Set LEGATO_BUILD based on the contents of LEGATO_ROOT, which must be already defined.
    std::string path = GetRequired("LEGATO_ROOT");
    if (path.empty())
    {
        throw mk::Exception_t(LE_I18N("LEGATO_ROOT environment variable is empty."));
    }
    path = path::Combine(path, "build/" + buildParams.target);
    Set("LEGATO_BUILD", path.c_str());
}


//----------------------------------------------------------------------------------------------
/**
 * Checks if a given environment variable name is one of the reserved environment variable names
 * (e.g., LEGATO_TARGET).
 *
 * @return true if reserved, false if not.
 */
//----------------------------------------------------------------------------------------------
bool IsReserved
(
    const std::string& name  ///< Name of the variable.
)
//--------------------------------------------------------------------------------------------------
{
    return (   (name == "LEGATO_ROOT")
            || (name == "LEGATO_TARGET")
            || (name == "LEGATO_BUILD")
            || (name == "LEGATO_SYSROOT")
            || (name == "CURDIR"));
}


//--------------------------------------------------------------------------------------------------
/**
 * Gets the file system path to the file in which environment variabls are saved.
 **/
//--------------------------------------------------------------------------------------------------
static std::string GetSaveFilePath
(
    const mk::BuildParams_t& buildParams
)
//--------------------------------------------------------------------------------------------------
{
    return path::Combine(buildParams.workingDir, "mktool_environment");
}


//--------------------------------------------------------------------------------------------------
/**
 * Saves the environment variables (in a file in the build's working directory)
 * for later use by MatchesSaved().
 */
//--------------------------------------------------------------------------------------------------
void Save
(
    const mk::BuildParams_t& buildParams
)
//--------------------------------------------------------------------------------------------------
{
    auto filePath = GetSaveFilePath(buildParams);

    // Make sure the containing directory exists.
    file::MakeDir(buildParams.workingDir);

    // Open the file
    std::ofstream saveFile(filePath);
    if (!saveFile.is_open())
    {
        throw mk::Exception_t(
            mk::format(LE_I18N("Failed to open file '%s' for writing."), filePath)
        );
    }

    // Write each environment variable as a line in the file.
    for (int i = 0; environ[i] != NULL; i++)
    {
        saveFile << environ[i] << '\n';

        if (saveFile.fail())
        {
            throw mk::Exception_t(
                mk::format(LE_I18N("Error writing to file '%s'."), filePath)
            );
        }
    }

    // Close the file.
    saveFile.close();
    if (saveFile.fail())
    {
        throw mk::Exception_t(
            mk::format(LE_I18N("Error closing file '%s'."), filePath)
        );
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Compares the current environment variables with those stored in the build's working directory.
 *
 * @return true if the environment variabls are effectively the same or
 *         false if there's a significant difference.
 */
//--------------------------------------------------------------------------------------------------
bool MatchesSaved
(
    const mk::BuildParams_t& buildParams
)
//--------------------------------------------------------------------------------------------------
{
    auto filePath = GetSaveFilePath(buildParams);

    if (!file::FileExists(filePath))
    {
        if (buildParams.beVerbose)
        {
            std::cout << LE_I18N("Environment variables from previous run not found.") << std::endl;
        }
        return false;
    }

    // Open the file
    std::ifstream saveFile(filePath);
    if (!saveFile.is_open())
    {
        throw mk::Exception_t(
            mk::format(LE_I18N("Failed to open file '%s' for reading."), filePath)
        );
    }

    int i;
    char lineBuff[8 * 1024];

    // For each environment variable in the process's current set,
    for (i = 0; environ[i] != NULL; i++)
    {
        // Read a line from the file (discarding '\n') and check for EOF or error.
        saveFile.getline(lineBuff, sizeof(lineBuff));
        if (saveFile.eof())
        {
            if (buildParams.beVerbose)
            {
                std::cout << mk::format(LE_I18N("Env var '%s' was added."), environ[i])
                          << std::endl;
            }

            goto different;
        }
        else if (!saveFile.good())
        {
            throw mk::Exception_t(
                mk::format(LE_I18N("Error reading from file '%s'."), filePath)
            );
        }

        // Compare the line from the file with the environment variable.
        if (strcmp(environ[i], lineBuff) != 0)
        {
            if (buildParams.beVerbose)
            {
                std::cout << mk::format(LE_I18N("Env var '%s' became '%s'."), lineBuff, environ[i])
                          << std::endl;
            }

            goto different;
        }
    }

    // Read one more line to make sure we get an end-of-file, otherwise there are less args
    // this time than last time.
    saveFile.getline(lineBuff, sizeof(lineBuff));
    if (saveFile.eof())
    {
        return true;
    }

    if (buildParams.beVerbose)
    {
        std::cout << mk::format(LE_I18N("Env var '%s' was removed."), lineBuff)
                  << std::endl;
    }

different:

    if (buildParams.beVerbose)
    {
        std::cout << LE_I18N("Environment variables are different this time.") << std::endl;
    }
    return false;
}



} // namespace envVars
