#pragma once

#include <GL/glew.h>
#include <vector>

namespace ogl {

    struct Buffer {
        GLuint id;
    };

    struct Shader {
        GLuint id;
    };

    struct Program {
        GLuint id;
    };

    struct VertexArray {
        GLuint id;
    };

    struct Texture2D {
        GLuint id;
    };

    struct FramebufferAttachment {
		GLuint id;
        int format;
        int index;
	};

    struct Framebuffer {
        GLuint id;
        int width, height;
        std::vector<FramebufferAttachment> color_attachments;
        FramebufferAttachment depth_attachment;
    };

    bool init();

    Framebuffer create_framebuffer(int width, int height);
    void bind_framebuffer(Framebuffer& framebuffer);
    void bind_default_framebuffer();
    void delete_framebuffer(Framebuffer& framebuffer);
    void framebuffer_color_attachment(Framebuffer& framebuffer, FramebufferAttachment& attachment, int index);
    void framebuffer_depth_attachment(Framebuffer& framebuffer, FramebufferAttachment& attachment);
    void delete_framebuffer_attachment(FramebufferAttachment attachment);

    void framebuffer_resize(Framebuffer& framebuffer, int width, int height);

    FramebufferAttachment create_framebuffer_attachment(Framebuffer& framebuffer, int format);

    Texture2D create_texture(int width, int height, int format);

    Texture2D create_texture_from_bytes(void* data, int width, int height, int channels, bool srgb);

    void delete_texture(Texture2D texture);

    void bind_texture(Texture2D texture, int unit);

    VertexArray create_vertex_array();

    void bind_vertex_array(VertexArray vertex_array);

    Shader create_shader(GLenum type, const char* source);

    Program create_program(const std::vector<Shader>& shaders);

    void use_program(Program program);

    Buffer create_buffer(void* data, size_t size, bool dynamic = false);

    void bind_buffer_as_ssbo(Buffer buffer, int binding);

    void bind_buffer_as_ebo(Buffer buffer);

    void bind_buffer_as_vbo(Buffer buffer);

    void bind_buffer_as_ubo(Buffer buffer, int binding);

    void delete_vertex_array(VertexArray vertex_array);

    void set_index_buffer(VertexArray vao, Buffer buffer);

    void delete_buffer(Buffer buffer);

    void buffer_subdata(Buffer buffer, void* data, size_t size, size_t offset);

}