/*******************************************************************************
 * This file is part of the "Enduro2D"
 * For conditions of distribution and use, see copyright notice in LICENSE.md
 * Copyright (C) 2018 Matvey Cherevko
 ******************************************************************************/

#include "render_opengl_base.hpp"

#if defined(E2D_RENDER_MODE) && E2D_RENDER_MODE == E2D_RENDER_MODE_OPENGL

namespace
{
    using namespace e2d;
    using namespace e2d::opengl;

    void gl_get_string(GLenum name, const char** str) noexcept {
        E2D_ASSERT(str);
        *str = reinterpret_cast<const char*>(glGetString(name));
    }

    void gl_create_shader(GLenum type, GLuint* id) noexcept {
        E2D_ASSERT(id);
        *id = glCreateShader(type);
    }

    void gl_create_program(GLuint* id) noexcept {
        E2D_ASSERT(id);
        *id = glCreateProgram();
    }

    void gl_get_uniform_location(GLuint program, const GLchar* name, GLint* loc) noexcept {
        E2D_ASSERT(loc);
        *loc = glGetUniformLocation(program, name);
    }

    void gl_get_attribute_location(GLuint program, const GLchar* name, GLint* loc) noexcept {
        E2D_ASSERT(loc);
        *loc = glGetAttribLocation(program, name);
    }

    bool process_shader_compilation_result(debug& debug, GLuint shader) noexcept {
        E2D_ASSERT(glIsShader(shader));
        GLint success = GL_FALSE;
        GL_CHECK_CODE(debug, glGetShaderiv(shader, GL_COMPILE_STATUS, &success));
        GLint log_len = 0;
        GL_CHECK_CODE(debug, glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_len));
        if ( log_len > 0 ) {
            GLchar* log_buffer = static_cast<GLchar*>(E2D_ALLOCA(
                sizeof(GLchar) * math::numeric_cast<std::size_t>(log_len)));
            GL_CHECK_CODE(debug, glGetShaderInfoLog(
                shader, log_len, nullptr, log_buffer));
            debug.log(success ? debug::level::warning : debug::level::error,
                "RENDER: shader compilation info:\n--> %0", log_buffer);
        }
        return success == GL_TRUE;
    }

    bool process_program_linking_result(debug& debug, GLuint program) noexcept {
        E2D_ASSERT(glIsProgram(program));
        GLint success = GL_FALSE;
        GL_CHECK_CODE(debug, glGetProgramiv(program, GL_LINK_STATUS, &success));
        GLint log_len = 0;
        GL_CHECK_CODE(debug, glGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_len));
        if ( log_len > 0 ) {
            GLchar* log_buffer = static_cast<GLchar*>(E2D_ALLOCA(
                sizeof(GLchar) * math::numeric_cast<std::size_t>(log_len)));
            GL_CHECK_CODE(debug, glGetProgramInfoLog(
                program, log_len, nullptr, log_buffer));
            debug.log(success ? debug::level::warning : debug::level::error,
                "RENDER: program linking info:\n--> %0", log_buffer);
        }
        return success == GL_TRUE;
    }

    bool process_program_validation_result(debug& debug, GLuint program) noexcept {
        E2D_ASSERT(glIsProgram(program));
        GL_CHECK_CODE(debug, glValidateProgram(program));
        GLint success = GL_FALSE;
        GL_CHECK_CODE(debug, glGetProgramiv(program, GL_VALIDATE_STATUS, &success));
        GLint log_len = 0;
        GL_CHECK_CODE(debug, glGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_len));
        if ( log_len > 0 ) {
            GLchar* log_buffer = static_cast<GLchar*>(E2D_ALLOCA(
                sizeof(GLchar) * math::numeric_cast<std::size_t>(log_len)));
            GL_CHECK_CODE(debug, glGetProgramInfoLog(
                program, log_len, nullptr, log_buffer));
            debug.log(success ? debug::level::warning : debug::level::error,
                "RENDER: program validation info:\n--> %0", log_buffer);
        }
        return success == GL_TRUE;
    }
}

namespace e2d { namespace opengl
{
    //
    // gl_buffer_id
    //

    gl_buffer_id::gl_buffer_id(debug& debug, GLuint id, GLenum target, bool owned) noexcept
    : debug_(debug)
    , id_(id)
    , target_(target)
    , owned_(owned){
        E2D_ASSERT(!id || glIsBuffer(id));
    }

    gl_buffer_id gl_buffer_id::create(debug& debug, GLenum target) noexcept {
        E2D_ASSERT(
            target == GL_ARRAY_BUFFER ||
            target == GL_ELEMENT_ARRAY_BUFFER);
        GLuint id = 0;
        GL_CHECK_CODE(debug, glGenBuffers(1, &id));
        if ( !id ) {
            debug.error("RENDER: Failed to generate buffer id");
            return gl_buffer_id(debug);
        }
        with_gl_bind_buffer(debug, target, id, []() noexcept {});
        return gl_buffer_id(debug, id, target, true);
    }

    gl_buffer_id gl_buffer_id::current(debug& debug, GLenum target) noexcept {
        GLint id = 0;
        GL_CHECK_CODE(debug, glGetIntegerv(gl_target_to_get_target(target), &id));
        return gl_buffer_id(debug, math::numeric_cast<GLuint>(id), target, false);
    }

    gl_buffer_id::gl_buffer_id(debug& debug) noexcept
    : debug_(debug) {}

    gl_buffer_id::~gl_buffer_id() noexcept {
        if ( id_ && owned_ ) {
            GL_CHECK_CODE(debug_, glDeleteBuffers(1, &id_));
        }
    }

    gl_buffer_id::gl_buffer_id(gl_buffer_id&& other) noexcept
    : debug_(other.debug_)
    , id_(other.id_)
    , target_(other.target_){
        other.id_ = 0;
        other.target_ = 0;
    }

    bool gl_buffer_id::empty() const noexcept {
        return id_ == 0;
    }

    GLuint gl_buffer_id::operator*() const noexcept {
        E2D_ASSERT(!empty());
        return id_;
    }

    GLenum gl_buffer_id::target() const noexcept {
        E2D_ASSERT(!empty());
        return target_;
    }

    //
    // gl_shader_id
    //

    gl_shader_id::gl_shader_id(debug& debug, GLuint id, GLenum type, bool owned) noexcept
    : debug_(debug)
    , id_(id)
    , type_(type)
    , owned_(owned){
        E2D_ASSERT(!id || glIsShader(id));
    }

    gl_shader_id gl_shader_id::create(debug& debug, GLenum type) noexcept {
        E2D_ASSERT(
            type == GL_VERTEX_SHADER ||
            type == GL_FRAGMENT_SHADER);
        GLuint id = 0;
        GL_CHECK_CODE(debug, gl_create_shader(type, &id));
        if ( !id ) {
            debug.error("RENDER: Failed to generate shader id");
            return gl_shader_id(debug);
        }
        return gl_shader_id(debug, id, type, true);
    }


    gl_shader_id::gl_shader_id(debug& debug) noexcept
    : debug_(debug) {}

    gl_shader_id::~gl_shader_id() noexcept {
        if ( id_ && owned_ ) {
            GL_CHECK_CODE(debug_, glDeleteShader(id_));
        }
    }

    gl_shader_id::gl_shader_id(gl_shader_id&& other) noexcept
    : debug_(other.debug_)
    , id_(other.id_)
    , type_(other.type_){
        other.id_ = 0;
        other.type_ = 0;
    }

    bool gl_shader_id::empty() const noexcept {
        return id_ == 0;
    }

    GLuint gl_shader_id::operator*() const noexcept {
        E2D_ASSERT(!empty());
        return id_;
    }

    GLenum gl_shader_id::type() const noexcept {
        E2D_ASSERT(!empty());
        return type_;
    }

    //
    // gl_program_id
    //

    gl_program_id::gl_program_id(debug& debug, GLuint id, bool owned) noexcept
    : debug_(debug)
    , id_(id)
    , owned_(owned){
        E2D_ASSERT(!id || glIsProgram(id));
    }

    gl_program_id gl_program_id::create(debug& debug) noexcept {
        GLuint id = 0;
        GL_CHECK_CODE(debug, gl_create_program(&id));
        if ( !id ) {
            debug.error("RENDER: Failed to generate program id");
            return gl_program_id(debug);
        }
        return gl_program_id(debug, id, true);
    }

    gl_program_id gl_program_id::current(debug& debug) noexcept {
        GLint id = 0;
        GL_CHECK_CODE(debug, glGetIntegerv(GL_CURRENT_PROGRAM, &id));
        return gl_program_id(debug, math::numeric_cast<GLuint>(id), false);
    }

    gl_program_id::gl_program_id(debug& debug) noexcept
    : debug_(debug) {}

    gl_program_id::~gl_program_id() noexcept {
        if ( id_ && owned_ ) {
            GL_CHECK_CODE(debug_, glDeleteProgram(id_));
            id_ = 0;
        }
    }

    gl_program_id::gl_program_id(gl_program_id&& other) noexcept
    : debug_(other.debug_)
    , id_(other.id_) {
        other.id_ = 0;
    }

    bool gl_program_id::empty() const noexcept {
        return id_ == 0;
    }

    GLuint gl_program_id::operator*() const noexcept {
        E2D_ASSERT(!empty());
        return id_;
    }

    //
    // gl_texture_id
    //

    gl_texture_id::gl_texture_id(debug& debug, GLuint id, GLenum target, bool owned) noexcept
    : debug_(debug)
    , id_(id)
    , target_(target)
    , owned_(owned){
        E2D_ASSERT(!id || glIsTexture(id));
    }

    gl_texture_id gl_texture_id::create(debug& debug, GLenum target) noexcept {
        E2D_ASSERT(
            target == GL_TEXTURE_2D ||
            target == GL_TEXTURE_CUBE_MAP);
        GLuint id = 0;
        GL_CHECK_CODE(debug, glGenTextures(1, &id));
        if ( !id ) {
            debug.error("RENDER: Failed to generate texture id");
            return gl_texture_id(debug);
        }
        with_gl_bind_texture(debug, target, id, []() noexcept {});
        return gl_texture_id(debug, id, target, true);
    }

    gl_texture_id gl_texture_id::current(debug& debug, GLenum target) noexcept {
        GLint id = 0;
        GL_CHECK_CODE(debug, glGetIntegerv(gl_target_to_get_target(target), &id));
        return gl_texture_id(debug, math::numeric_cast<GLuint>(id), target, false);
    }

    gl_texture_id::gl_texture_id(debug& debug) noexcept
    : debug_(debug) {}

    gl_texture_id::~gl_texture_id() noexcept {
        if ( id_ && owned_ ) {
            GL_CHECK_CODE(debug_, glDeleteTextures(1, &id_));
            id_ = 0;
        }
    }

    gl_texture_id::gl_texture_id(gl_texture_id&& other) noexcept
    : debug_(other.debug_)
    , id_(other.id_)
    , target_(other.target_){
        other.id_ = 0;
        other.target_ = 0;
    }

    bool gl_texture_id::empty() const noexcept {
        return id_ == 0;
    }

    GLuint gl_texture_id::operator*() const noexcept {
        E2D_ASSERT(!empty());
        return id_;
    }

    GLenum gl_texture_id::target() const noexcept {
        E2D_ASSERT(!empty());
        return target_;
    }

    //
    // gl_framebuffer_id
    //

    gl_framebuffer_id::gl_framebuffer_id(debug& debug, GLuint id, GLenum target, bool owned) noexcept
    : debug_(debug)
    , id_(id)
    , target_(target)
    , owned_(owned){
        E2D_ASSERT(!id || glIsFramebuffer(id));
    }

    gl_framebuffer_id gl_framebuffer_id::create(debug& debug, GLenum target) noexcept {
        E2D_ASSERT(
            target == GL_FRAMEBUFFER);
        GLuint id = 0;
        GL_CHECK_CODE(debug, glGenFramebuffers(1, &id));
        if ( !id ) {
            debug.error("RENDER: Failed to generate framebuffer id");
            return gl_framebuffer_id(debug);
        }
        with_gl_bind_framebuffer(debug, target, id, []() noexcept {});
        return gl_framebuffer_id(debug, id, target, true);
    }

    gl_framebuffer_id gl_framebuffer_id::current(debug& debug, GLenum target) noexcept {
        GLint id = 0;
        GL_CHECK_CODE(debug, glGetIntegerv(gl_target_to_get_target(target), &id));
        return gl_framebuffer_id(debug, math::numeric_cast<GLuint>(id), target, false);
    }

    gl_framebuffer_id::gl_framebuffer_id(debug& debug) noexcept
    : debug_(debug) {}

    gl_framebuffer_id::~gl_framebuffer_id() noexcept {
        if ( id_ && owned_ ) {
            GL_CHECK_CODE(debug_, glDeleteFramebuffers(1, &id_));
            id_ = 0;
        }
    }

    gl_framebuffer_id::gl_framebuffer_id(gl_framebuffer_id&& other) noexcept
    : debug_(other.debug_)
    , id_(other.id_)
    , target_(other.target_){
        other.id_ = 0;
        other.target_ = 0;
    }

    bool gl_framebuffer_id::empty() const noexcept {
        return id_ == 0;
    }

    GLuint gl_framebuffer_id::operator*() const noexcept {
        E2D_ASSERT(!empty());
        return id_;
    }

    GLenum gl_framebuffer_id::target() const noexcept {
        E2D_ASSERT(!empty());
        return target_;
    }

    //
    // operators
    //

    bool operator==(const gl_buffer_id& l, const gl_buffer_id& r) noexcept {
        return l.target() == r.target()
            && l.empty() == r.empty()
            && (l.empty() || *l == *r);
    }

    bool operator==(const gl_shader_id& l, const gl_shader_id& r) noexcept {
        return l.type() == r.type()
            && l.empty() == r.empty()
            && (l.empty() || *l == *r);
    }

    bool operator==(const gl_program_id& l, const gl_program_id& r) noexcept {
        return l.empty() == r.empty()
            && (l.empty() || *l == *r);
    }

    bool operator==(const gl_texture_id& l, const gl_texture_id& r) noexcept {
        return l.target() == r.target()
            && l.empty() == r.empty()
            && (l.empty() || *l == *r);
    }

    bool operator==(const gl_framebuffer_id& l, const gl_framebuffer_id& r) noexcept {
        return l.target() == r.target()
            && l.empty() == r.empty()
            && (l.empty() || *l == *r);
    }

    bool operator!=(const gl_buffer_id& l, const gl_buffer_id& r) noexcept {
        return !(l == r);
    }

    bool operator!=(const gl_shader_id& l, const gl_shader_id& r) noexcept {
        return !(l == r);
    }

    bool operator!=(const gl_program_id& l, const gl_program_id& r) noexcept {
        return !(l == r);
    }

    bool operator!=(const gl_texture_id& l, const gl_texture_id& r) noexcept {
        return !(l == r);
    }

    bool operator!=(const gl_framebuffer_id& l, const gl_framebuffer_id& r) noexcept {
        return !(l == r);
    }
}}

namespace e2d { namespace opengl
{
    const char* uniform_type_to_cstr(uniform_type ut) noexcept {
        #define DEFINE_CASE(x) case uniform_type::x: return #x
        switch ( ut ) {
            DEFINE_CASE(signed_integer);
            DEFINE_CASE(floating_point);
            DEFINE_CASE(v2i);
            DEFINE_CASE(v3i);
            DEFINE_CASE(v4i);
            DEFINE_CASE(v2f);
            DEFINE_CASE(v3f);
            DEFINE_CASE(v4f);
            DEFINE_CASE(m2f);
            DEFINE_CASE(m3f);
            DEFINE_CASE(m4f);
            DEFINE_CASE(sampler_2d);
            DEFINE_CASE(sampler_cube);
            DEFINE_CASE(unknown);
            default:
                E2D_ASSERT_MSG(false, "unexpected uniform type");
                return "";
        }
        #undef DEFINE_CASE
    }

    const char* attribute_type_to_cstr(attribute_type at) noexcept {
        #define DEFINE_CASE(x) case attribute_type::x: return #x
        switch ( at ) {
            DEFINE_CASE(floating_point);
            DEFINE_CASE(v2f);
            DEFINE_CASE(v3f);
            DEFINE_CASE(v4f);
            DEFINE_CASE(m2f);
            DEFINE_CASE(m3f);
            DEFINE_CASE(m4f);
            DEFINE_CASE(unknown);
            default:
                E2D_ASSERT_MSG(false, "unexpected attribute type");
                return "";
        }
        #undef DEFINE_CASE
    }
}}

namespace e2d { namespace opengl
{
    GLint convert_format_to_internal_format(image_data_format idf) noexcept {
        #define DEFINE_CASE(x,y) case image_data_format::x: return y;
        switch ( idf ) {
            DEFINE_CASE(g8, GL_ALPHA);
            DEFINE_CASE(ga8, GL_LUMINANCE_ALPHA);
            DEFINE_CASE(rgb8, GL_RGB);
            DEFINE_CASE(rgba8, GL_RGBA);
            default:
                E2D_ASSERT_MSG(false, "unexpected image data format");
                return 0;
        }
        #undef DEFINE_CASE
    }

    GLenum convert_format_to_external_format(image_data_format idf) noexcept {
        #define DEFINE_CASE(x,y) case image_data_format::x: return y;
        switch ( idf ) {
            DEFINE_CASE(g8, GL_ALPHA);
            DEFINE_CASE(ga8, GL_LUMINANCE_ALPHA);
            DEFINE_CASE(rgb8, GL_RGB);
            DEFINE_CASE(rgba8, GL_RGBA);
            default:
                E2D_ASSERT_MSG(false, "unexpected image data format");
                return 0;
        }
        #undef DEFINE_CASE
    }

    GLenum convert_format_to_external_data_type(image_data_format idf) noexcept {
        #define DEFINE_CASE(x,y) case image_data_format::x: return y;
        switch ( idf ) {
            DEFINE_CASE(g8, GL_UNSIGNED_BYTE);
            DEFINE_CASE(ga8, GL_UNSIGNED_BYTE);
            DEFINE_CASE(rgb8, GL_UNSIGNED_BYTE);
            DEFINE_CASE(rgba8, GL_UNSIGNED_BYTE);
            default:
                E2D_ASSERT_MSG(false, "unexpected image data format");
                return 0;
        }
        #undef DEFINE_CASE
    }

    GLenum convert_index_type(index_declaration::index_type it) noexcept {
        #define DEFINE_CASE(x,y) case index_declaration::index_type::x: return y;
        switch ( it ) {
            DEFINE_CASE(unsigned_byte, GL_UNSIGNED_BYTE);
            DEFINE_CASE(unsigned_short, GL_UNSIGNED_SHORT);
            default:
                E2D_ASSERT_MSG(false, "unexpected index type");
                return 0;
        }
        #undef DEFINE_CASE
    }

    GLenum convert_attribute_type(vertex_declaration::attribute_type at) noexcept {
        #define DEFINE_CASE(x,y) case vertex_declaration::attribute_type::x: return y;
        switch ( at ) {
            DEFINE_CASE(signed_byte, GL_BYTE);
            DEFINE_CASE(unsigned_byte, GL_UNSIGNED_BYTE);
            DEFINE_CASE(signed_short, GL_SHORT);
            DEFINE_CASE(unsigned_short, GL_UNSIGNED_SHORT);
            DEFINE_CASE(floating_point, GL_FLOAT);
            default:
                E2D_ASSERT_MSG(false, "unexpected attribute type");
                return 0;
        }
        #undef DEFINE_CASE
    }

    GLint convert_uniform_type(uniform_type ut) noexcept {
        #define DEFINE_CASE(x,y) case uniform_type::x: return y;
        switch ( ut ) {
            DEFINE_CASE(signed_integer, GL_INT);
            DEFINE_CASE(floating_point, GL_FLOAT);

            DEFINE_CASE(v2i, GL_INT_VEC2);
            DEFINE_CASE(v3i, GL_INT_VEC3);
            DEFINE_CASE(v4i, GL_INT_VEC4);

            DEFINE_CASE(v2f, GL_FLOAT_VEC2);
            DEFINE_CASE(v3f, GL_FLOAT_VEC3);
            DEFINE_CASE(v4f, GL_FLOAT_VEC4);

            DEFINE_CASE(m2f, GL_FLOAT_MAT2);
            DEFINE_CASE(m3f, GL_FLOAT_MAT3);
            DEFINE_CASE(m4f, GL_FLOAT_MAT4);

            DEFINE_CASE(sampler_2d, GL_SAMPLER_2D);
            DEFINE_CASE(sampler_cube, GL_SAMPLER_CUBE);
            default:
                E2D_ASSERT_MSG(false, "unexpected uniform type");
                return 0;
        }
        #undef DEFINE_CASE
    }

    GLint convert_attribute_type(attribute_type at) noexcept {
        #define DEFINE_CASE(x,y) case attribute_type::x: return y;
        switch ( at ) {
            DEFINE_CASE(floating_point, GL_FLOAT);

            DEFINE_CASE(v2f, GL_FLOAT_VEC2);
            DEFINE_CASE(v3f, GL_FLOAT_VEC3);
            DEFINE_CASE(v4f, GL_FLOAT_VEC4);

            DEFINE_CASE(m2f, GL_FLOAT_MAT2);
            DEFINE_CASE(m3f, GL_FLOAT_MAT3);
            DEFINE_CASE(m4f, GL_FLOAT_MAT4);
            default:
                E2D_ASSERT_MSG(false, "unexpected attribute type");
                return 0;
        }
        #undef DEFINE_CASE
    }

    GLint convert_sampler_wrap(render::sampler_wrap w) noexcept {
        #define DEFINE_CASE(x,y) case render::sampler_wrap::x: return y;
        switch ( w ) {
            DEFINE_CASE(clamp, GL_CLAMP_TO_EDGE);
            DEFINE_CASE(repeat, GL_REPEAT);
            DEFINE_CASE(mirror, GL_MIRRORED_REPEAT);
            default:
                E2D_ASSERT_MSG(false, "unexpected sampler wrap");
                return 0;
        }
        #undef DEFINE_CASE
    }

    GLint convert_sampler_filter(render::sampler_min_filter f) noexcept {
        #define DEFINE_CASE(x,y) case render::sampler_min_filter::x: return y;
        switch ( f ) {
            DEFINE_CASE(nearest, GL_NEAREST);
            DEFINE_CASE(linear, GL_LINEAR);
            DEFINE_CASE(nearest_mipmap_nearest, GL_NEAREST_MIPMAP_NEAREST);
            DEFINE_CASE(linear_mipmap_nearest, GL_LINEAR_MIPMAP_NEAREST);
            DEFINE_CASE(nearest_mipmap_linear, GL_NEAREST_MIPMAP_LINEAR);
            DEFINE_CASE(linear_mipmap_linear, GL_LINEAR_MIPMAP_LINEAR);
            default:
                E2D_ASSERT_MSG(false, "unexpected sampler min filter");
                return 0;
        }
        #undef DEFINE_CASE
    }

    GLint convert_sampler_filter(render::sampler_mag_filter f) noexcept {
        #define DEFINE_CASE(x,y) case render::sampler_mag_filter::x: return y;
        switch ( f ) {
            DEFINE_CASE(nearest, GL_NEAREST);
            DEFINE_CASE(linear, GL_LINEAR);
            default:
                E2D_ASSERT_MSG(false, "unexpected sampler mag filter");
                return 0;
        }
        #undef DEFINE_CASE
    }

    GLenum convert_buffer_usage(index_buffer::usage u) noexcept {
        #define DEFINE_CASE(x,y) case index_buffer::usage::x: return y;
        switch ( u ) {
            DEFINE_CASE(static_draw, GL_STATIC_DRAW);
            DEFINE_CASE(stream_draw, GL_STREAM_DRAW);
            DEFINE_CASE(dynamic_draw, GL_DYNAMIC_DRAW);
            default:
                E2D_ASSERT_MSG(false, "unexpected index buffer usage");
                return 0;
        }
        #undef DEFINE_CASE
    }

    GLenum convert_buffer_usage(vertex_buffer::usage u) noexcept {
        #define DEFINE_CASE(x,y) case vertex_buffer::usage::x: return y;
        switch ( u ) {
            DEFINE_CASE(static_draw, GL_STATIC_DRAW);
            DEFINE_CASE(stream_draw, GL_STREAM_DRAW);
            DEFINE_CASE(dynamic_draw, GL_DYNAMIC_DRAW);
            default:
                E2D_ASSERT_MSG(false, "unexpected vertex buffer usage");
                return 0;
        }
        #undef DEFINE_CASE
    }

    GLenum convert_topology(render::topology t) noexcept {
        #define DEFINE_CASE(x,y) case render::topology::x: return y;
        switch ( t ) {
            DEFINE_CASE(triangles, GL_TRIANGLES);
            DEFINE_CASE(triangles_fan, GL_TRIANGLE_FAN);
            DEFINE_CASE(triangles_strip, GL_TRIANGLE_STRIP);
            default:
                E2D_ASSERT_MSG(false, "unexpected render topology");
                return 0;
        }
        #undef DEFINE_CASE
    }

    GLenum convert_stencil_op(render::stencil_op sa) noexcept {
        #define DEFINE_CASE(x,y) case render::stencil_op::x: return y;
        switch ( sa ) {
            DEFINE_CASE(keep, GL_KEEP);
            DEFINE_CASE(zero, GL_ZERO);
            DEFINE_CASE(replace, GL_REPLACE);
            DEFINE_CASE(incr, GL_INCR);
            DEFINE_CASE(incr_wrap, GL_INCR_WRAP);
            DEFINE_CASE(decr, GL_DECR);
            DEFINE_CASE(decr_wrap, GL_DECR_WRAP);
            DEFINE_CASE(invert, GL_INVERT);
            default:
                E2D_ASSERT_MSG(false, "unexpected render stencil op");
                return 0;
        }
        #undef DEFINE_CASE
    }

    GLenum convert_compare_func(render::compare_func cf) noexcept {
        #define DEFINE_CASE(x,y) case render::compare_func::x: return y;
        switch ( cf ) {
            DEFINE_CASE(never, GL_NEVER);
            DEFINE_CASE(less, GL_LESS);
            DEFINE_CASE(lequal, GL_LEQUAL);
            DEFINE_CASE(greater, GL_GREATER);
            DEFINE_CASE(gequal, GL_GEQUAL);
            DEFINE_CASE(equal, GL_EQUAL);
            DEFINE_CASE(notequal, GL_NOTEQUAL);
            DEFINE_CASE(always, GL_ALWAYS);
            default:
                E2D_ASSERT_MSG(false, "unexpected render compare func");
                return 0;
        }
        #undef DEFINE_CASE
    }

    GLenum convert_culling_mode(render::culling_mode cm) noexcept {
        #define DEFINE_CASE(x,y) case render::culling_mode::x: return y;
        switch ( cm ) {
            DEFINE_CASE(cw, GL_CW);
            DEFINE_CASE(ccw, GL_CCW);
            default:
                E2D_ASSERT_MSG(false, "unexpected render culling mode");
                return 0;
        }
        #undef DEFINE_CASE
    }

    GLenum convert_culling_face(render::culling_face cf) noexcept {
        #define DEFINE_CASE(x,y) case render::culling_face::x: return y;
        switch ( cf ) {
            DEFINE_CASE(back, GL_BACK);
            DEFINE_CASE(front, GL_FRONT);
            DEFINE_CASE(back_and_front, GL_FRONT_AND_BACK);
            default:
                E2D_ASSERT_MSG(false, "unexpected render culling face");
                return 0;
        }
        #undef DEFINE_CASE
    }

    GLenum convert_blending_factor(render::blending_factor bf) noexcept {
        #define DEFINE_CASE(x,y) case render::blending_factor::x: return y;
        switch ( bf ) {
            DEFINE_CASE(zero, GL_ZERO);
            DEFINE_CASE(one, GL_ONE);
            DEFINE_CASE(src_color, GL_SRC_COLOR);
            DEFINE_CASE(one_minus_src_color, GL_ONE_MINUS_SRC_COLOR);
            DEFINE_CASE(dst_color, GL_DST_COLOR);
            DEFINE_CASE(one_minus_dst_color, GL_ONE_MINUS_DST_COLOR);
            DEFINE_CASE(src_alpha, GL_SRC_ALPHA);
            DEFINE_CASE(one_minus_src_alpha, GL_ONE_MINUS_SRC_ALPHA);
            DEFINE_CASE(dst_alpha, GL_DST_ALPHA);
            DEFINE_CASE(one_minus_dst_alpha, GL_ONE_MINUS_DST_ALPHA);
            DEFINE_CASE(constant_color, GL_CONSTANT_COLOR);
            DEFINE_CASE(one_minus_constant_color, GL_ONE_MINUS_CONSTANT_COLOR);
            DEFINE_CASE(constant_alpha, GL_CONSTANT_ALPHA);
            DEFINE_CASE(one_minus_constant_alpha, GL_ONE_MINUS_CONSTANT_ALPHA);
            DEFINE_CASE(src_alpha_saturate, GL_SRC_ALPHA_SATURATE);
            default:
                E2D_ASSERT_MSG(false, "unexpected render blending factor");
                return 0;
        }
        #undef DEFINE_CASE
    }

    GLenum convert_blending_equation(render::blending_equation be) noexcept {
        #define DEFINE_CASE(x,y) case render::blending_equation::x: return y;
        switch ( be ) {
            DEFINE_CASE(add, GL_FUNC_ADD);
            DEFINE_CASE(subtract, GL_FUNC_SUBTRACT);
            DEFINE_CASE(reverse_subtract, GL_FUNC_REVERSE_SUBTRACT);
            default:
                E2D_ASSERT_MSG(false, "unexpected render blending equation");
                return 0;
        }
        #undef DEFINE_CASE
    }

    uniform_type glsl_type_to_uniform_type(GLenum t) noexcept {
        #define DEFINE_CASE(x,y) case x: return uniform_type::y
        switch ( t ) {
            DEFINE_CASE(GL_INT, signed_integer);
            DEFINE_CASE(GL_FLOAT, floating_point);

            DEFINE_CASE(GL_INT_VEC2, v2i);
            DEFINE_CASE(GL_INT_VEC3, v3i);
            DEFINE_CASE(GL_INT_VEC4, v4i);

            DEFINE_CASE(GL_FLOAT_VEC2, v2f);
            DEFINE_CASE(GL_FLOAT_VEC3, v3f);
            DEFINE_CASE(GL_FLOAT_VEC4, v4f);

            DEFINE_CASE(GL_FLOAT_MAT2, m2f);
            DEFINE_CASE(GL_FLOAT_MAT3, m3f);
            DEFINE_CASE(GL_FLOAT_MAT4, m4f);

            DEFINE_CASE(GL_SAMPLER_2D, sampler_2d);
            DEFINE_CASE(GL_SAMPLER_CUBE, sampler_cube);
            default:
                return uniform_type::unknown;
        }
        #undef DEFINE_CASE
    }

    attribute_type glsl_type_to_attribute_type(GLenum t) noexcept {
        #define DEFINE_CASE(x,y) case x: return attribute_type::y
        switch ( t ) {
            DEFINE_CASE(GL_FLOAT, floating_point);

            DEFINE_CASE(GL_FLOAT_VEC2, v2f);
            DEFINE_CASE(GL_FLOAT_VEC3, v3f);
            DEFINE_CASE(GL_FLOAT_VEC4, v4f);

            DEFINE_CASE(GL_FLOAT_MAT2, m2f);
            DEFINE_CASE(GL_FLOAT_MAT3, m3f);
            DEFINE_CASE(GL_FLOAT_MAT4, m4f);
            default:
                return attribute_type::unknown;
        }
        #undef DEFINE_CASE
    }

    const char* glsl_type_to_cstr(GLenum t) noexcept {
        #define DEFINE_CASE(x) case x: return #x
        switch ( t ) {
            DEFINE_CASE(GL_INT);
            DEFINE_CASE(GL_FLOAT);

            DEFINE_CASE(GL_INT_VEC2);
            DEFINE_CASE(GL_INT_VEC3);
            DEFINE_CASE(GL_INT_VEC4);

            DEFINE_CASE(GL_FLOAT_VEC2);
            DEFINE_CASE(GL_FLOAT_VEC3);
            DEFINE_CASE(GL_FLOAT_VEC4);

            DEFINE_CASE(GL_FLOAT_MAT2);
            DEFINE_CASE(GL_FLOAT_MAT3);
            DEFINE_CASE(GL_FLOAT_MAT4);

            DEFINE_CASE(GL_SAMPLER_2D);
            DEFINE_CASE(GL_SAMPLER_CUBE);
            default:
                return "GL_UNKNOWN";
        }
        #undef DEFINE_CASE
    }

    const char* gl_error_code_to_cstr(GLenum e) noexcept {
        #define DEFINE_CASE(x) case x: return #x;
        switch ( e ) {
            DEFINE_CASE(GL_INVALID_ENUM);
            DEFINE_CASE(GL_INVALID_VALUE);
            DEFINE_CASE(GL_INVALID_OPERATION);
            DEFINE_CASE(GL_INVALID_FRAMEBUFFER_OPERATION);
            DEFINE_CASE(GL_OUT_OF_MEMORY);
            default:
                return "GL_UNKNOWN";
        }
        #undef DEFINE_CASE
    }

    GLenum gl_target_to_get_target(GLenum t) noexcept {
        #define DEFINE_CASE(x,y) case x: return y
        switch ( t ) {
            DEFINE_CASE(GL_ARRAY_BUFFER, GL_ARRAY_BUFFER_BINDING);
            DEFINE_CASE(GL_ELEMENT_ARRAY_BUFFER, GL_ELEMENT_ARRAY_BUFFER_BINDING);
            DEFINE_CASE(GL_TEXTURE_2D, GL_TEXTURE_BINDING_2D);
            DEFINE_CASE(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_BINDING_CUBE_MAP);
            DEFINE_CASE(GL_FRAMEBUFFER, GL_FRAMEBUFFER_BINDING);
            default:
                E2D_ASSERT_MSG(false, "unexpected gl target type");
                return 0;
        }
        #undef DEFINE_CASE
    }
}}

namespace e2d { namespace opengl
{
    void gl_trace_info(debug& debug) noexcept {
        const char* vendor = nullptr;
        GL_CHECK_CODE(debug, gl_get_string(GL_VENDOR, &vendor));
        const char* renderer = nullptr;
        GL_CHECK_CODE(debug, gl_get_string(GL_RENDERER, &renderer));
        const char* version = nullptr;
        GL_CHECK_CODE(debug, gl_get_string(GL_VERSION, &version));
        const char* language = nullptr;
        GL_CHECK_CODE(debug, gl_get_string(GL_SHADING_LANGUAGE_VERSION, &language));
        const char* extensions = nullptr;
        GL_CHECK_CODE(debug, gl_get_string(GL_EXTENSIONS, &extensions));
        const auto cstr_or_undefined = [](const char* cstr) noexcept {
            return (cstr && *cstr)
                ? cstr
                : "(undefined)";
        };
        debug.trace("RENDER: opengl info:\n"
            "--> GL_VENDOR: %0\n"
            "--> GL_RENDERER: %1\n"
            "--> GL_VERSION: %2\n"
            "--> GL_SHADING_LANGUAGE_VERSION: %3\n"
            "--> GL_EXTENSIONS: %4",
            cstr_or_undefined(vendor),
            cstr_or_undefined(renderer),
            cstr_or_undefined(version),
            cstr_or_undefined(language),
            cstr_or_undefined(extensions));
    }

    void gl_trace_limits(debug& debug) noexcept {
        GLint max_texture_size = 0;
        GL_CHECK_CODE(debug, glGetIntegerv(
            GL_MAX_TEXTURE_SIZE, &max_texture_size));
        GLint max_renderbuffer_size = 0;
        GL_CHECK_CODE(debug, glGetIntegerv(
            GL_MAX_RENDERBUFFER_SIZE, &max_renderbuffer_size));
        GLint max_cube_map_texture_size = 0;
        GL_CHECK_CODE(debug, glGetIntegerv(
            GL_MAX_CUBE_MAP_TEXTURE_SIZE, &max_cube_map_texture_size));
        GLint max_texture_image_units = 0;
        GL_CHECK_CODE(debug, glGetIntegerv(
            GL_MAX_TEXTURE_IMAGE_UNITS, &max_texture_image_units));
        GLint max_vertex_texture_image_units = 0;
        GL_CHECK_CODE(debug, glGetIntegerv(
            GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS, &max_vertex_texture_image_units));
        GLint max_combined_texture_image_units = 0;
        GL_CHECK_CODE(debug, glGetIntegerv(
            GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &max_combined_texture_image_units));
        debug.trace("RENDER: opengl limits:\n"
            "--> GL_MAX_TEXTURE_SIZE: %0\n"
            "--> GL_MAX_RENDERBUFFER_SIZE: %1\n"
            "--> GL_MAX_CUBE_MAP_TEXTURE_SIZE: %2\n"
            "--> GL_MAX_TEXTURE_IMAGE_UNITS: %3\n"
            "--> GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS: %4\n"
            "--> GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS: %5",
            max_texture_size,
            max_renderbuffer_size,
            max_cube_map_texture_size,
            max_texture_image_units,
            max_vertex_texture_image_units,
            max_combined_texture_image_units);
    }

    gl_shader_id gl_compile_shader(debug& debug, const str& source, GLenum type) noexcept {
        gl_shader_id id = gl_shader_id::create(debug, type);
        if ( id.empty() ) {
            return id;
        }
        const char* source_cstr = source.c_str();
        GL_CHECK_CODE(debug, glShaderSource(*id, 1, &source_cstr, nullptr));
        GL_CHECK_CODE(debug, glCompileShader(*id));
        return process_shader_compilation_result(debug, *id)
            ? std::move(id)
            : gl_shader_id(debug);
    }

    gl_program_id gl_link_program(debug& debug, gl_shader_id vs, gl_shader_id fs) noexcept {
        E2D_ASSERT(!vs.empty() && !fs.empty());
        gl_program_id id = gl_program_id::create(debug);
        if ( id.empty() ) {
            return id;
        }
        GL_CHECK_CODE(debug, glAttachShader(*id, *vs));
        GL_CHECK_CODE(debug, glAttachShader(*id, *fs));
        GL_CHECK_CODE(debug, glLinkProgram(*id));
        return process_program_linking_result(debug, *id)
            && process_program_validation_result(debug, *id)
            ? std::move(id)
            : gl_program_id(debug);
    }

    gl_texture_id gl_compile_texture(debug& debug, const image& image) {
        gl_texture_id id = gl_texture_id::create(
            debug, GL_TEXTURE_2D);
        if ( id.empty() ) {
            return id;
        }
        with_gl_bind_texture(debug, id, [&debug, &id, &image]() noexcept {
            GL_CHECK_CODE(debug, glTexImage2D(
                id.target(),
                0,
                convert_format_to_internal_format(image.format()),
                math::numeric_cast<GLsizei>(image.size().x),
                math::numeric_cast<GLsizei>(image.size().y),
                0,
                convert_format_to_external_format(image.format()),
                convert_format_to_external_data_type(image.format()),
                image.data().data()));
            #if !defined(GL_ES_VERSION_2_0)
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
            #endif
        });
        return id;
    }

    gl_buffer_id gl_compile_index_buffer(
        debug& debug, const buffer& indices, index_buffer::usage usage)
    {
        gl_buffer_id id = gl_buffer_id::create(
            debug, GL_ELEMENT_ARRAY_BUFFER);
        if ( id.empty() ) {
            return id;
        }
        with_gl_bind_buffer(debug, id, [&debug, &id, &indices, &usage]() {
            GL_CHECK_CODE(debug, glBufferData(
                id.target(),
                math::numeric_cast<GLsizeiptr>(indices.size()),
                indices.data(),
                convert_buffer_usage(usage)));
        });
        return id;
    }

    gl_buffer_id gl_compile_vertex_buffer(
        debug& debug, const buffer& vertices, vertex_buffer::usage usage)
    {
        gl_buffer_id id = gl_buffer_id::create(
            debug, GL_ARRAY_BUFFER);
        if ( id.empty() ) {
            return id;
        }
        with_gl_bind_buffer(debug, id, [&debug, &id, &vertices, &usage]() {
            GL_CHECK_CODE(debug, glBufferData(
                id.target(),
                math::numeric_cast<GLsizeiptr>(vertices.size()),
                vertices.data(),
                convert_buffer_usage(usage)));
        });
        return id;
    }
}}

namespace e2d { namespace opengl
{
    void grab_program_uniforms(
        debug& debug,
        GLuint program,
        vector<uniform_info>& uniforms)
    {
        E2D_ASSERT(program && glIsProgram(program));

        GLint uniform_count = 0;
        GL_CHECK_CODE(debug, glGetProgramiv(
            program, GL_ACTIVE_UNIFORMS, &uniform_count));

        GLint uniform_max_len = 0;
        GL_CHECK_CODE(debug, glGetProgramiv(
            program, GL_ACTIVE_UNIFORM_MAX_LENGTH, &uniform_max_len));

        GLchar* name_buffer = static_cast<GLchar*>(E2D_ALLOCA(
            sizeof(GLchar) * math::numeric_cast<std::size_t>(uniform_max_len)));

        uniforms.reserve(uniforms.size() +
            math::numeric_cast<std::size_t>(uniform_count));

        for ( GLuint i = 0; i < math::numeric_cast<GLuint>(uniform_count); ++i ) {
            GLint size = 0;
            GLenum type = 0;
            GL_CHECK_CODE(debug, glGetActiveUniform(
                program, i, uniform_max_len,
                nullptr, &size, &type, name_buffer));
            GLint location = 0;
            GL_CHECK_CODE(debug, gl_get_uniform_location(
                program, name_buffer, &location));
            uniforms.emplace_back(
                name_buffer,
                size,
                location,
                glsl_type_to_uniform_type(type));
        }
    }

    void grab_program_attributes(
        debug& debug,
        GLuint program,
        vector<attribute_info>& attributes)
    {
        E2D_ASSERT(program && glIsProgram(program));

        GLint attribute_count = 0;
        GL_CHECK_CODE(debug, glGetProgramiv(
            program, GL_ACTIVE_ATTRIBUTES, &attribute_count));

        GLint attribute_max_len = 0;
        GL_CHECK_CODE(debug, glGetProgramiv(
            program, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH, &attribute_max_len));

        GLchar* name_buffer = static_cast<GLchar*>(E2D_ALLOCA(
            sizeof(GLchar) * math::numeric_cast<std::size_t>(attribute_max_len)));

        attributes.reserve(attributes.size() +
            math::numeric_cast<std::size_t>(attribute_count));

        for ( GLuint i = 0; i < math::numeric_cast<GLuint>(attribute_count); ++i ) {
            GLint size = 0;
            GLenum type = 0;
            GL_CHECK_CODE(debug, glGetActiveAttrib(
                program, i, attribute_max_len,
                nullptr, &size, &type, name_buffer));
            GLint location = 0;
            GL_CHECK_CODE(debug, gl_get_attribute_location(
                program, name_buffer, &location));
            attributes.emplace_back(
                name_buffer,
                size,
                location,
                glsl_type_to_attribute_type(type));
        }
    }
}}

#endif
