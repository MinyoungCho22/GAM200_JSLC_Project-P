#pragma once

#ifdef __INTELLISENSE__
    #include <cstddef>
    // IntelliSense용 타입 정의
    typedef unsigned int GLuint;
    typedef int GLint;
    typedef int GLsizei;
    typedef unsigned int GLenum;
    typedef unsigned char GLboolean;
    typedef float GLfloat;
    typedef unsigned int GLbitfield;
    typedef unsigned char GLubyte;
    typedef const char GLchar;
    typedef void GLvoid;
    typedef ptrdiff_t GLsizeiptr;
    
    // IntelliSense용 상수 정의
    #define GL_ARRAY_BUFFER 0x8892
    #define GL_STATIC_DRAW 0x88E4
    #define GL_FLOAT 0x1406
    #define GL_FALSE 0
    #define GL_TRIANGLES 0x0004
    #define GL_COLOR_BUFFER_BIT 0x00004000
    #define GL_BLEND 0x0BE2
    #define GL_SRC_ALPHA 0x0302
    #define GL_ONE_MINUS_SRC_ALPHA 0x0303
    #define GL_TEXTURE_2D 0x0DE1
    #define GL_RGBA 0x1908
    #define GL_RGB 0x1907
    #define GL_UNSIGNED_BYTE 0x1401
    #define GL_TEXTURE_WRAP_S 0x2802
    #define GL_TEXTURE_WRAP_T 0x2803
    #define GL_CLAMP_TO_EDGE 0x812F
    #define GL_TEXTURE_MIN_FILTER 0x2801
    #define GL_TEXTURE_MAG_FILTER 0x2800
    #define GL_LINEAR 0x2601
    #define GL_NEAREST 0x2600
    #define GL_FRAMEBUFFER 0x8D40
    #define GL_COLOR_ATTACHMENT0 0x8CE0
    #define GL_FRAMEBUFFER_COMPLETE 0x8CD5
    #define GL_VERTEX_SHADER 0x8B31
    #define GL_FRAGMENT_SHADER 0x8B30
    #define GL_COMPILE_STATUS 0x8B81
    #define GL_LINK_STATUS 0x8B82
    #define GL_INFO_LOG_LENGTH 0x8B84
    #define GL_TEXTURE0 0x84C0
    #define GL_VERSION 0x1F02
#else
    #include <GL/glew.h>
#endif

namespace GL
{
    // Buffer & Vertex Array
    static inline void GenVertexArrays(GLsizei n, GLuint* arrays) { glGenVertexArrays(n, arrays); }
    static inline void GenBuffers(GLsizei n, GLuint* buffers) { glGenBuffers(n, buffers); }
    static inline void BindVertexArray(GLuint array) { glBindVertexArray(array); }
    static inline void BindBuffer(GLenum target, GLuint buffer) { glBindBuffer(target, buffer); }
    static inline void BufferData(GLenum target, GLsizeiptr size, const void* data, GLenum usage) { glBufferData(target, size, data, usage); }
    static inline void VertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void* pointer) { glVertexAttribPointer(index, size, type, normalized, stride, pointer); }
    static inline void EnableVertexAttribArray(GLuint index) { glEnableVertexAttribArray(index); }
    static inline void DeleteVertexArrays(GLsizei n, const GLuint* arrays) { glDeleteVertexArrays(n, arrays); }
    static inline void DeleteBuffers(GLsizei n, const GLuint* buffers) { glDeleteBuffers(n, buffers); }

    // Drawing
    static inline void DrawArrays(GLenum mode, GLint first, GLsizei count) { glDrawArrays(mode, first, count); }
    static inline void DrawElements(GLenum mode, GLsizei count, GLenum type, const void* indices) { glDrawElements(mode, count, type, indices); }

    // State & Context
    static inline void Enable(GLenum cap) { glEnable(cap); }
    // [추가됨] glDisable 래퍼 함수
    static inline void Disable(GLenum cap) { glDisable(cap); }
    static inline void BlendFunc(GLenum sfactor, GLenum dfactor) { glBlendFunc(sfactor, dfactor); }
    static inline void ClearColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha) { glClearColor(red, green, blue, alpha); }
    static inline void Clear(GLbitfield mask) { glClear(mask); }
    static inline const GLubyte* GetString(GLenum name) { return glGetString(name); }
    static inline void Viewport(GLint x, GLint y, GLsizei width, GLsizei height) { glViewport(x, y, width, height); }

    // Textures
    static inline void GenTextures(GLsizei n, GLuint* textures) { glGenTextures(n, textures); }
    static inline void BindTexture(GLenum target, GLuint texture) { glBindTexture(target, texture); }
    static inline void TexParameteri(GLenum target, GLenum pname, GLint param) { glTexParameteri(target, pname, param); }
    static inline void TexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void* pixels) { glTexImage2D(target, level, internalformat, width, height, border, format, type, pixels); }
    static inline void GenerateMipmap(GLenum target) { glGenerateMipmap(target); }
    static inline void ActiveTexture(GLenum texture) { glActiveTexture(texture); }
    static inline void DeleteTextures(GLsizei n, const GLuint* textures) { glDeleteTextures(n, textures); }

    // Framebuffers (FBO)
    static inline void GenFramebuffers(GLsizei n, GLuint* framebuffers) { glGenFramebuffers(n, framebuffers); }
    static inline void BindFramebuffer(GLenum target, GLuint framebuffer) { glBindFramebuffer(target, framebuffer); }
    static inline void FramebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level) { glFramebufferTexture2D(target, attachment, textarget, texture, level); }
    static inline GLenum CheckFramebufferStatus(GLenum target) { return glCheckFramebufferStatus(target); }
    static inline void DeleteFramebuffers(GLsizei n, const GLuint* framebuffers) { glDeleteFramebuffers(n, framebuffers); }

    // Shaders & Programs
    static inline GLuint CreateShader(GLenum type) { return glCreateShader(type); }
    static inline void ShaderSource(GLuint shader, GLsizei count, const GLchar* const* string, const GLint* length) { glShaderSource(shader, count, string, length); }
    static inline void CompileShader(GLuint shader) { glCompileShader(shader); }
    static inline void GetShaderiv(GLuint shader, GLenum pname, GLint* params) { glGetShaderiv(shader, pname, params); }
    static inline void GetShaderInfoLog(GLuint shader, GLsizei bufSize, GLsizei* length, GLchar* infoLog) { glGetShaderInfoLog(shader, bufSize, length, infoLog); }
    static inline GLuint CreateProgram() { return glCreateProgram(); }
    static inline void AttachShader(GLuint program, GLuint shader) { glAttachShader(program, shader); }
    static inline void LinkProgram(GLuint program) { glLinkProgram(program); }
    static inline void GetProgramiv(GLuint program, GLenum pname, GLint* params) { glGetProgramiv(program, pname, params); }
    static inline void GetProgramInfoLog(GLuint program, GLsizei bufSize, GLsizei* length, GLchar* infoLog) { glGetProgramInfoLog(program, bufSize, length, infoLog); }
    static inline void DeleteShader(GLuint shader) { glDeleteShader(shader); }
    static inline void DeleteProgram(GLuint program) { glDeleteProgram(program); }
    static inline void UseProgram(GLuint program) { glUseProgram(program); }
    static inline GLint GetUniformLocation(GLuint program, const GLchar* name) { return glGetUniformLocation(program, name); }
    static inline void Uniform1i(GLint location, GLint v0) { glUniform1i(location, v0); }
    static inline void Uniform3f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2) { glUniform3f(location, v0, v1, v2); }
    static inline void UniformMatrix4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) { glUniformMatrix4fv(location, count, transpose, value); }
    static inline void Uniform4f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3) { glUniform4f(location, v0, v1, v2, v3); }
    static inline void Uniform1f(GLint location, GLfloat v0) { glUniform1f(location, v0); }
}
