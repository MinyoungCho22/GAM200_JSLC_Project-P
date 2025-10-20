#pragma once

#include <GL/glew.h>

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
}