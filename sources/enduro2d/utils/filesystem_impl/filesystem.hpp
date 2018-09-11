/*******************************************************************************
 * This file is part of the "Enduro2D"
 * For conditions of distribution and use, see copyright notice in LICENSE.md
 * Copyright (C) 2018 Matvey Cherevko
 ******************************************************************************/

#pragma once

#include <enduro2d/utils/path.hpp>
#include <enduro2d/utils/strings.hpp>
#include <enduro2d/utils/filesystem.hpp>

namespace e2d { namespace filesystem { namespace impl
{
    bool remove_file(str_view path);
    bool remove_directory(str_view path);

    bool file_exists(str_view path);
    bool directory_exists(str_view path);

    bool create_directory(str_view path);

    bool extract_predef_path(str& dst, predef_path path_type);
}}}
