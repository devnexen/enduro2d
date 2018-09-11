/*******************************************************************************
 * This file is part of the "enduro2d"
 * For conditions of distribution and use, see copyright notice in LICENSE.md
 * Copyright (C) 2018 Matvey Cherevko
 ******************************************************************************/

#include "filesystem.hpp"

#if defined(E2D_PLATFORM) && E2D_PLATFORM == E2D_PLATFORM_WINDOWS

#include <shlobj.h>
#include <windows.h>

namespace
{
    using namespace e2d;

    bool extract_home_directory(str& dst) {
        WCHAR buf[MAX_PATH + 1] = {0};
        if ( SUCCEEDED(::SHGetFolderPathW(0, CSIDL_PROFILE | CSIDL_FLAG_CREATE, 0, 0, buf)) ) {
            dst = make_utf8(buf);
            return true;
        }
        return false;
    }

    bool extract_appdata_directory(str& dst) {
        WCHAR buf[MAX_PATH + 1] = {0};
        if ( SUCCEEDED(::SHGetFolderPathW(0, CSIDL_APPDATA | CSIDL_FLAG_CREATE, 0, 0, buf)) ) {
            dst = make_utf8(buf);
            return true;
        }
        return false;
    }

    bool extract_desktop_directory(str& dst) {
        WCHAR buf[MAX_PATH + 1] = {0};
        if ( SUCCEEDED(::SHGetFolderPathW(0, CSIDL_DESKTOP | CSIDL_FLAG_CREATE, 0, 0, buf)) ) {
            dst = make_utf8(buf);
            return true;
        }
        return false;
    }

    bool extract_working_directory(str& dst) {
        WCHAR buf[MAX_PATH + 1] = {0};
        const DWORD len = ::GetCurrentDirectoryW(E2D_COUNTOF(buf) - 1, buf);
        if ( len > 0 && len <= MAX_PATH ) {
            dst = make_utf8(buf);
            return true;
        }
        return false;
    }

    bool extract_documents_directory(str& dst) {
        WCHAR buf[MAX_PATH + 1] = {0};
        if ( SUCCEEDED(::SHGetFolderPathW(0, CSIDL_MYDOCUMENTS | CSIDL_FLAG_CREATE, 0, 0, buf)) ) {
            dst = make_utf8(buf);
            return true;
        }
        return false;
    }

    bool extract_executable_path(str& dst) {
        WCHAR buf[MAX_PATH + 1] = {0};
        const DWORD len = ::GetModuleFileNameW(0, buf, E2D_COUNTOF(buf) - 1);
        if ( len > 0 && len <= MAX_PATH ) {
            dst = make_utf8(buf);
            return true;
        }
        return false;
    }

    bool extract_resources_directory(str& dst) {
        str executable_path;
        if ( extract_executable_path(executable_path) ) {
            dst = path::parent_path(executable_path);
            return true;
        }
        return false;
    }
}

namespace e2d { namespace filesystem { namespace impl
{
    bool remove_file(str_view path) {
        const wstr wide_path = make_wide(path);
        return ::DeleteFileW(wide_path.c_str())
            || ::GetLastError() == ERROR_FILE_NOT_FOUND
            || ::GetLastError() == ERROR_PATH_NOT_FOUND;
    }

    bool remove_directory(str_view path) {
        const wstr wide_path = make_wide(path);
        return ::RemoveDirectoryW(wide_path.c_str())
            || ::GetLastError() == ERROR_FILE_NOT_FOUND
            || ::GetLastError() == ERROR_PATH_NOT_FOUND;
    }

    bool file_exists(str_view path) {
        const wstr wide_path = make_wide(path);
        DWORD attributes = ::GetFileAttributesW(wide_path.c_str());
        return attributes != INVALID_FILE_ATTRIBUTES
            && !(attributes & FILE_ATTRIBUTE_DIRECTORY);
    }

    bool directory_exists(str_view path) {
        const wstr wide_path = make_wide(path);
        DWORD attributes = ::GetFileAttributesW(wide_path.c_str());
        return attributes != INVALID_FILE_ATTRIBUTES
            && (attributes & FILE_ATTRIBUTE_DIRECTORY);
    }

    bool create_directory(str_view path) {
        const wstr wide_path = make_wide(path);
        return ::CreateDirectoryW(wide_path.c_str(), nullptr)
            || ::GetLastError() == ERROR_ALREADY_EXISTS;
    }

    bool extract_predef_path(str& dst, predef_path path_type) {
        switch ( path_type ) {
            case predef_path::home:
                return extract_home_directory(dst);
            case predef_path::appdata:
                return extract_appdata_directory(dst);
            case predef_path::desktop:
                return extract_desktop_directory(dst);
            case predef_path::working:
                return extract_working_directory(dst);
            case predef_path::documents:
                return extract_documents_directory(dst);
            case predef_path::resources:
                return extract_resources_directory(dst);
            case predef_path::executable:
                return extract_executable_path(dst);
            default:
                E2D_ASSERT_MSG(false, "unexpected predef path");
                return false;
        }
    }
}}}

#endif
