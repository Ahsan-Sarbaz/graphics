#include "opengl.h"
#include <iostream>

namespace ogl {

    bool init() {
        if (glewInit() != GLEW_OK) {
            return false;
        }

        return true;
    }

    Shader create_shader(GLenum type, const char* source) {
        Shader shader;
        shader.id = glCreateShader(type);
        glShaderSource(shader.id, 1, &source, nullptr);
        glCompileShader(shader.id);

        int success;
        char info_log[512];
        glGetShaderiv(shader.id, GL_COMPILE_STATUS, &success);
        if (!success) {
            int info_log_length;
            glGetShaderiv(shader.id, GL_INFO_LOG_LENGTH, &info_log_length);
            glGetShaderInfoLog(shader.id, info_log_length, NULL, info_log);
            if (type == GL_VERTEX_SHADER) {
                std::cerr << "Vertex shader compilation failed\n"
                    << info_log << std::endl;
            }
            else if (type == GL_FRAGMENT_SHADER) {
                std::cerr << "Fragment shader compilation failed\n"
                    << info_log << std::endl;
            }
            else if (type == GL_COMPUTE_SHADER) {
                std::cerr << "Compute shader compilation failed\n"
                    << info_log << std::endl;
            }
            else {
                std::cerr << "Shader compilation failed\n" << info_log << std::endl;
            }
        }

        if (!success) {
            return {};
        }

        return shader;
    }

    Program create_program(const std::vector<Shader>& shaders) {
        Program program;

        program.id = glCreateProgram();

        for (Shader shader : shaders) {
            glAttachShader(program.id, shader.id);
        }

        glLinkProgram(program.id);

        int success;
        char info_log[512];
        glGetProgramiv(program.id, GL_LINK_STATUS, &success);
        if (!success) {
            int info_log_length;
            glGetProgramiv(program.id, GL_INFO_LOG_LENGTH, &info_log_length);
            glGetProgramInfoLog(program.id, info_log_length, NULL, info_log);
            std::cerr << "Program linking failed\n" << info_log << std::endl;
            return {};
        }

        for (Shader shader : shaders) {
            glDetachShader(program.id, shader.id);
            glDeleteShader(shader.id);
        }

        return program;
    }

    void use_program(Program program) { glUseProgram(program.id); }

    VertexArray create_vertex_array() {
        VertexArray vertex_array;
        glCreateVertexArrays(1, &vertex_array.id);
        return vertex_array;
    }

    void bind_vertex_array(VertexArray vertex_array) {
        glBindVertexArray(vertex_array.id);
    }

    void delete_vertex_array(VertexArray vertex_array) {
        glDeleteVertexArrays(1, &vertex_array.id);
    }

    void set_index_buffer(VertexArray vao, Buffer buffer) {
        glVertexArrayElementBuffer(vao.id, buffer.id);
    }

    Buffer create_buffer(void* data, size_t size, bool dynamic) {
        Buffer buffer;
        glCreateBuffers(1, &buffer.id);

        GLbitfield flags = 0;
        if (dynamic) {
            flags = GL_DYNAMIC_STORAGE_BIT;
        }

        glNamedBufferStorage(buffer.id, size, data, flags);

        return buffer;
    }

    void delete_buffer(Buffer buffer) { glDeleteBuffers(1, &buffer.id); }

    void bind_buffer_as_ssbo(Buffer buffer, int binding) {
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, binding, buffer.id);
    }

    void bind_buffer_as_ubo(Buffer buffer, int binding) {
        glBindBufferBase(GL_UNIFORM_BUFFER, binding, buffer.id);
    }

    void bind_buffer_as_vbo(Buffer buffer) {
        glBindBuffer(GL_ARRAY_BUFFER, buffer.id);
    }

    void bind_buffer_as_ebo(Buffer buffer) {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffer.id);
    }

    void buffer_subdata(Buffer buffer, void* data, size_t size, size_t offset) {
        glNamedBufferSubData(buffer.id, offset, size, data);
    }

    Framebuffer create_framebuffer(int width, int height)
    {
        Framebuffer framebuffer = {};
		glCreateFramebuffers(1, &framebuffer.id);
        framebuffer.width = width;
		framebuffer.height = height;
		return framebuffer;
    }

	void bind_framebuffer(Framebuffer& framebuffer) 
    {
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer.id);
	}

    void bind_default_framebuffer()
	{
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	void delete_framebuffer(Framebuffer& framebuffer) 
    {
		glDeleteFramebuffers(1, &framebuffer.id);
	}

    void framebuffer_color_attachment(Framebuffer& framebuffer, FramebufferAttachment& attachment, int index)
    {
        glNamedFramebufferTexture(framebuffer.id, GL_COLOR_ATTACHMENT0 + index, attachment.id, 0);
        attachment.index = index;
        framebuffer.color_attachments.push_back(attachment);
	}

    void framebuffer_depth_attachment(Framebuffer& framebuffer, FramebufferAttachment& attachment) {
		glNamedFramebufferTexture(framebuffer.id, GL_DEPTH_ATTACHMENT, attachment.id, 0);
		framebuffer.depth_attachment = attachment;
    }

    FramebufferAttachment create_framebuffer_attachment(Framebuffer& framebuffer, int format, bool draw) {
        FramebufferAttachment attachment = {};
		attachment.format = format;
		attachment.draw = draw;
        glCreateTextures(GL_TEXTURE_2D, 1, &attachment.id);
        glTextureParameteri(attachment.id, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTextureParameteri(attachment.id, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTextureParameteri(attachment.id, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTextureParameteri(attachment.id, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTextureStorage2D(attachment.id, 1, format, framebuffer.width, framebuffer.height);
        return attachment;
    }

	void delete_framebuffer_attachment(FramebufferAttachment attachment) {
		glDeleteTextures(1, &attachment.id);
	}

    void framebuffer_draw_attachments(Framebuffer& framebuffer) {
        std::vector<uint32_t> attachments;
        for (FramebufferAttachment attachment : framebuffer.color_attachments) {
            if (attachment.draw) {
			    attachments.push_back(GL_COLOR_ATTACHMENT0 + attachment.index);
            }
		}

        if (framebuffer.depth_attachment.draw) {
			attachments.push_back(GL_DEPTH_ATTACHMENT);
		}

        glFramebufferDrawBuffersEXT(framebuffer.id, attachments.size(), attachments.data());
    }

    void framebuffer_resize(Framebuffer& framebuffer, int width, int height) {
		framebuffer.width = width;
		framebuffer.height = height;

        std::vector<FramebufferAttachment> attachments;

        for (FramebufferAttachment attachment : framebuffer.color_attachments) {
            auto a = create_framebuffer_attachment(framebuffer, attachment.format, attachment.draw);
            a.index = attachment.index;
            attachments.push_back(a);
		}

        FramebufferAttachment depth_attachment = {};
        if (framebuffer.depth_attachment.id != 0) {
            depth_attachment = create_framebuffer_attachment(framebuffer, framebuffer.depth_attachment.format, framebuffer.depth_attachment.draw);
        }

        std::vector<FramebufferAttachment> old_attachments = framebuffer.color_attachments;
        framebuffer.color_attachments.clear();

        for (FramebufferAttachment attachment : attachments) {
			framebuffer_color_attachment(framebuffer, attachment, attachment.index);
        }

        FramebufferAttachment old_depth_attachment = {};
        if (depth_attachment.id != 0) {
            old_depth_attachment = framebuffer.depth_attachment;
            framebuffer.depth_attachment = {};
        }

        framebuffer_depth_attachment(framebuffer, depth_attachment);

        framebuffer_draw_attachments(framebuffer);

		for (FramebufferAttachment attachment : old_attachments) {
			delete_framebuffer_attachment(attachment);
		}

        if (depth_attachment.id != 0) {
            delete_framebuffer_attachment(old_depth_attachment);
        }
    }



    Texture2D create_texture(int width, int height, int format)
    {
		Texture2D texture;
		glCreateTextures(GL_TEXTURE_2D, 1, &texture.id);
		glTextureStorage2D(texture.id, 1, format, width, height);
		return texture;
	}

    Texture2D create_texture_from_bytes(void* data, int size, int width, int height, int channels, bool srgb, bool compressed, int internal_format, int pixel_format) {
        Texture2D texture;

        glCreateTextures(GL_TEXTURE_2D, 1, &texture.id);

        if (internal_format == 0) {
            switch (channels) {
            case 1:
                internal_format = GL_R8;
                break;
            case 2:
                internal_format = GL_RG8;
                break;
            case 3:
                internal_format = srgb ? GL_SRGB8 : GL_RGB8;
                break;
            case 4:
                internal_format = srgb ? GL_SRGB8_ALPHA8 : GL_RGBA8;
                break;
            default:
                std::cerr << "Invalid number of channels " << channels << std::endl;
                return {};
            }
        }
        if (pixel_format == 0 && !compressed) {
            switch (channels) {
            case 1:
                pixel_format = GL_RED;
                break;
            case 2:
                pixel_format = GL_RG;
                break;
            case 3:
                pixel_format = GL_RGB;
                break;
            case 4:
                pixel_format = GL_RGBA;
                break;
            default:
                std::cerr << "Invalid number of channels " << channels << std::endl;
                return {};
            }
        }

        glTextureStorage2D(texture.id, 1, internal_format, width, height);

        glTextureParameteri(texture.id, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTextureParameteri(texture.id, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTextureParameteri(texture.id, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTextureParameteri(texture.id, GL_TEXTURE_WRAP_T, GL_REPEAT);

        if (compressed) {
            glCompressedTextureSubImage2D(texture.id, 0, 0, 0, width, height, pixel_format, size, data);
        }
        else {
            glTextureSubImage2D(texture.id, 0, 0, 0, width, height, pixel_format, GL_UNSIGNED_BYTE, data);
            glGenerateTextureMipmap(texture.id);
        }

        return texture;
    }

    void delete_texture(Texture2D texture) {
        glDeleteTextures(1, &texture.id);
    }

    void bind_texture(Texture2D texture, int unit) {
        glBindTextureUnit(unit, texture.id);
    }


} // namespace ogl