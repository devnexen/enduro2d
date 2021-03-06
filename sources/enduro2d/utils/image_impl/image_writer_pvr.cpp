/*******************************************************************************
 * This file is part of the "Enduro2D"
 * For conditions of distribution and use, see copyright notice in LICENSE.md
 * Copyright (C) 2018-2019, by Matvey Cherevko (blackmatov@gmail.com)
 ******************************************************************************/

#include "image_impl.hpp"

namespace
{
    using namespace e2d;

    bool is_save_image_pvr_supported(image_data_format data_format) noexcept {
        switch ( data_format ) {
            case image_data_format::g8:
            case image_data_format::ga8:
            case image_data_format::rgb8:
            case image_data_format::rgba8:
            case image_data_format::rgb_dxt1:
            case image_data_format::rgba_dxt1:
            case image_data_format::rgba_dxt3:
            case image_data_format::rgba_dxt5:
            case image_data_format::rgb_pvrtc2:
            case image_data_format::rgb_pvrtc4:
            case image_data_format::rgba_pvrtc2:
            case image_data_format::rgba_pvrtc4:
            case image_data_format::rgba_pvrtc2_v2:
            case image_data_format::rgba_pvrtc4_v2:
                return true;
            default:
                return false;
        }
    }
}

namespace e2d { namespace images { namespace impl
{
    bool try_save_image_pvr(const image& src, buffer& dst) noexcept {
        E2D_UNUSED(src, dst);
        if ( is_save_image_pvr_supported(src.format()) ) {
            //TODO(BlackMat): implme
            E2D_ASSERT_MSG(false, "implme");
        }
        return false;
    }
}}}
