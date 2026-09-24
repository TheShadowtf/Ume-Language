#include "../include/graphics.h"
#define GLFW_INCLUDE_NONE
#include "../../vendor/glfw/include/GLFW/glfw3.h"
#include "../../vendor/glad/include/glad/glad.h"
#include <stdexcept>
#include <stdio.h>
#include <windows.h>
#ifdef DrawText
#undef DrawText
#endif
#include <cmath>
#include <fstream>
#include <unordered_map>
#define STB_TRUETYPE_IMPLEMENTATION
#include "../include/stb_truetype.h"

namespace Ume::Graphics {

unsigned int Shader::CompileShader(const std::string& source, unsigned int type) {
    const char* src = source.c_str();
    unsigned int id = glCreateShader(type);
    glShaderSource(id, 1, &src, nullptr);
    glCompileShader(id);

    int result;
    glGetShaderiv(id, GL_COMPILE_STATUS, &result);
    if (result == GL_FALSE) {
        int length;
        glGetShaderiv(id, GL_INFO_LOG_LENGTH, &length);
        char* message = new char[length];
        glGetShaderInfoLog(id, length, &length, message);
        
        fprintf(stderr, "Shader compilation failed:\n%s\n", message);
        delete[] message;
        glDeleteShader(id);
        return 0;
    }

    return id;
}

Shader::Shader(unsigned int program_id) : program_id_(program_id) {}

Shader* Shader::Create(const std::string& vertexSource, const std::string& fragmentSource) {
    unsigned int vs = CompileShader(vertexSource, GL_VERTEX_SHADER);
    unsigned int fs = CompileShader(fragmentSource, GL_FRAGMENT_SHADER);

    if (vs == 0 || fs == 0) {
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        return nullptr;
    }

    unsigned int program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    int result;
    glGetProgramiv(program, GL_LINK_STATUS, &result);
    if (result == GL_FALSE) {
        int length;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
        char* message = new char[length];
        glGetProgramInfoLog(program, length, &length, message);
        fprintf(stderr, "Program linking failed:\n%s\n", message);
        delete[] message;
        glDeleteProgram(program);
        return nullptr;
    }

    glDeleteShader(vs);
    glDeleteShader(fs);

    return new Shader(program);
}

Shader::~Shader() {
    glDeleteProgram(program_id_);
}

void Shader::Use() const {
    glUseProgram(program_id_);
}

void Shader::SetFloat(const std::string& name, float value) {
    int location = glGetUniformLocation(program_id_, name.c_str());
    glUniform1f(location, value);
}

void Shader::SetInt(const std::string& name, int value) {
    int location = glGetUniformLocation(program_id_, name.c_str());
    glUniform1i(location, value);
}

void Shader::SetVec2(const std::string& name, float x, float y) {
    int location = glGetUniformLocation(program_id_, name.c_str());
    glUniform2f(location, x, y);
}

void Shader::SetVec3(const std::string& name, float x, float y, float z) {
    int location = glGetUniformLocation(program_id_, name.c_str());
    glUniform3f(location, x, y, z);
}

void Shader::SetVec4(const std::string& name, float x, float y, float z, float w) {
    int location = glGetUniformLocation(program_id_, name.c_str());
    glUniform4f(location, x, y, z, w);
}

void Shader::SetMat4(const std::string& name, const float* matrix) {
    int location = glGetUniformLocation(program_id_, name.c_str());
    glUniformMatrix4fv(location, 1, GL_FALSE, matrix);
}

void Shader::SetTexture(const std::string& name, const Texture* texture, int slot) {
    if (!texture) return;
    texture->Bind(slot);
    SetInt(name, slot);
}

Texture::Texture(unsigned int id, int width, int height) : id_(id), width_(width), height_(height) {}

Texture* Texture::Create(int width, int height, const unsigned char* pixels, int channels) {
    if (!pixels || width <= 0 || height <= 0) return nullptr;
    unsigned int id;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);

    GLenum format = GL_RGBA;
    if (channels == 3) format = GL_RGB;
    else if (channels == 1) format = GL_RED;

    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glBindTexture(GL_TEXTURE_2D, 0);
    return new Texture(id, width, height);
}

Texture::~Texture() {
    if (id_) glDeleteTextures(1, &id_);
}

// Texture::Load — loads an image file using stb_image if available.
// Tries multiple common include paths for stb_image.h.
#if defined(__has_include)
#  if __has_include(<stb_image.h>)
#    define UME_HAS_STB 1
#    define STB_IMAGE_IMPLEMENTATION
#    include <stb_image.h>
#  elif __has_include("stb_image.h")
#    define UME_HAS_STB 1
#    define STB_IMAGE_IMPLEMENTATION
#    include "stb_image.h"
#  elif __has_include(<vendor/stb/stb_image.h>)
#    define UME_HAS_STB 1
#    define STB_IMAGE_IMPLEMENTATION
#    include <vendor/stb/stb_image.h>
#  endif
#endif

#ifndef UME_HAS_STB
#  define UME_HAS_STB 0
#endif

Texture* Texture::Load(const std::string& path) {
#if UME_HAS_STB
    int w = 0, h = 0, channels = 0;
    stbi_set_flip_vertically_on_load(1);
    unsigned char* data = stbi_load(path.c_str(), &w, &h, &channels, 4);
    if (!data) return nullptr;
    Texture* tex = Texture::Create(w, h, data, 4);
    stbi_image_free(data);
    return tex;
#else
    // stb_image not available — return nullptr
    (void)path;
    return nullptr;
#endif
}

std::vector<int> LoadImagePixels(const std::string& path) {
    std::vector<int> result;
#if UME_HAS_STB
    int w = 0, h = 0, channels = 0;
    stbi_set_flip_vertically_on_load(0);  // Atlas building expects top-to-bottom layout
    unsigned char* data = stbi_load(path.c_str(), &w, &h, &channels, 4);
    if (!data) return result;
    result.reserve(2 + w * h * 4);
    result.push_back(w);
    result.push_back(h);
    for (int i = 0; i < w * h * 4; i++) {
        result.push_back((int)data[i]);
    }
    stbi_image_free(data);
#else
    (void)path;
#endif
    return result;
}

void Texture::Bind(unsigned int slot) const {
    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(GL_TEXTURE_2D, id_);
}

Mesh::Mesh() : vao_(0), vbo_(0), ebo_(0), vertex_count_(0), stride_(6) {}

Mesh* Mesh::CreateQuad() {
    Mesh* mesh = new Mesh();
    
    // Quad vertices (x, y, z, r, g, b)
    float vertices[] = {
        // Position          // Color
        -0.5f, -0.5f, 0.0f,  1.0f, 0.0f, 0.0f,
         0.5f, -0.5f, 0.0f,  0.0f, 1.0f, 0.0f,
         0.5f,  0.5f, 0.0f,  0.0f, 0.0f, 1.0f,
        -0.5f,  0.5f, 0.0f,  1.0f, 1.0f, 0.0f,
    };

    unsigned int indices[] = {
        0, 1, 2,
        2, 3, 0
    };

    glGenVertexArrays(1, &mesh->vao_);
    glGenBuffers(1, &mesh->vbo_);
    glGenBuffers(1, &mesh->ebo_);

    glBindVertexArray(mesh->vao_);
    
    glBindBuffer(GL_ARRAY_BUFFER, mesh->vbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->ebo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    mesh->SetupVertexAttribs(6);
    mesh->vertex_count_ = 6;

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    return mesh;
}

Mesh* Mesh::CreateTriangle() {
    Mesh* mesh = new Mesh();
    
    float vertices[] = {
        -0.5f, -0.5f, 0.0f,  1.0f, 0.0f, 0.0f,
         0.5f, -0.5f, 0.0f,  0.0f, 1.0f, 0.0f,
         0.0f,  0.5f, 0.0f,  0.0f, 0.0f, 1.0f,
    };

    glGenVertexArrays(1, &mesh->vao_);
    glGenBuffers(1, &mesh->vbo_);

    glBindVertexArray(mesh->vao_);
    
    glBindBuffer(GL_ARRAY_BUFFER, mesh->vbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    mesh->SetupVertexAttribs(6);
    mesh->vertex_count_ = 3;

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    return mesh;
}

Mesh* Mesh::CreateCube() {
    Mesh* mesh = new Mesh();
    
    float vertices[] = {
        // Front face (+Z)
        -0.5f, -0.5f,  0.5f,   0.0f,  0.0f,  1.0f,
         0.5f, -0.5f,  0.5f,   0.0f,  0.0f,  1.0f,
         0.5f,  0.5f,  0.5f,   0.0f,  0.0f,  1.0f,
        -0.5f,  0.5f,  0.5f,   0.0f,  0.0f,  1.0f,

        // Back face (-Z)
         0.5f, -0.5f, -0.5f,   0.0f,  0.0f, -1.0f,
        -0.5f, -0.5f, -0.5f,   0.0f,  0.0f, -1.0f,
        -0.5f,  0.5f, -0.5f,   0.0f,  0.0f, -1.0f,
         0.5f,  0.5f, -0.5f,   0.0f,  0.0f, -1.0f,

        // Top face (+Y)
        -0.5f,  0.5f,  0.5f,   0.0f,  1.0f,  0.0f,
         0.5f,  0.5f,  0.5f,   0.0f,  1.0f,  0.0f,
         0.5f,  0.5f, -0.5f,   0.0f,  1.0f,  0.0f,
        -0.5f,  0.5f, -0.5f,   0.0f,  1.0f,  0.0f,

        // Bottom face (-Y)
        -0.5f, -0.5f, -0.5f,   0.0f, -1.0f,  0.0f,
         0.5f, -0.5f, -0.5f,   0.0f, -1.0f,  0.0f,
         0.5f, -0.5f,  0.5f,   0.0f, -1.0f,  0.0f,
        -0.5f, -0.5f,  0.5f,   0.0f, -1.0f,  0.0f,

        // Right face (+X)
         0.5f, -0.5f,  0.5f,   1.0f,  0.0f,  0.0f,
         0.5f, -0.5f, -0.5f,   1.0f,  0.0f,  0.0f,
         0.5f,  0.5f, -0.5f,   1.0f,  0.0f,  0.0f,
         0.5f,  0.5f,  0.5f,   1.0f,  0.0f,  0.0f,

        // Left face (-X)
        -0.5f, -0.5f, -0.5f,  -1.0f,  0.0f,  0.0f,
        -0.5f, -0.5f,  0.5f,  -1.0f,  0.0f,  0.0f,
        -0.5f,  0.5f,  0.5f,  -1.0f,  0.0f,  0.0f,
        -0.5f,  0.5f, -0.5f,  -1.0f,  0.0f,  0.0f
    };

    unsigned int indices[] = {
        0,  1,  2,   2,  3,  0,   // Front
        4,  5,  6,   6,  7,  4,   // Back
        8,  9, 10,  10, 11,  8,   // Top
       12, 13, 14,  14, 15, 12,   // Bottom
       16, 17, 18,  18, 19, 16,   // Right
       20, 21, 22,  22, 23, 20    // Left
    };

    glGenVertexArrays(1, &mesh->vao_);
    glGenBuffers(1, &mesh->vbo_);
    glGenBuffers(1, &mesh->ebo_);

    glBindVertexArray(mesh->vao_);
    
    glBindBuffer(GL_ARRAY_BUFFER, mesh->vbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->ebo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    mesh->SetupVertexAttribs(6);
    mesh->vertex_count_ = 36;

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    return mesh;
}

Mesh* Mesh::Create(const float* vertices, size_t floatCount, const unsigned int* indices, size_t indexCount, int stride) {
    if (!vertices || floatCount == 0) return nullptr;
    Mesh* mesh = new Mesh();
    mesh->stride_ = stride > 0 ? stride : 6;
    glGenVertexArrays(1, &mesh->vao_);
    glGenBuffers(1, &mesh->vbo_);
    glBindVertexArray(mesh->vao_);
    glBindBuffer(GL_ARRAY_BUFFER, mesh->vbo_);
    glBufferData(GL_ARRAY_BUFFER, floatCount * sizeof(float), vertices, GL_DYNAMIC_DRAW);

    if (indices && indexCount > 0) {
        glGenBuffers(1, &mesh->ebo_);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->ebo_);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indexCount * sizeof(unsigned int), indices, GL_DYNAMIC_DRAW);
        mesh->vertex_count_ = (unsigned int)indexCount;
    } else {
        mesh->vertex_count_ = (unsigned int)(floatCount / mesh->stride_);
    }

    mesh->SetupVertexAttribs(mesh->stride_);
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    return mesh;
}

void Mesh::UpdateData(const float* vertices, size_t floatCount, const unsigned int* indices, size_t indexCount) {
    if (!vertices || floatCount == 0 || !vao_ || !vbo_) return;
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, floatCount * sizeof(float), vertices, GL_DYNAMIC_DRAW);

    if (indices && indexCount > 0) {
        if (!ebo_) {
            glGenBuffers(1, &ebo_);
        }
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indexCount * sizeof(unsigned int), indices, GL_DYNAMIC_DRAW);
        vertex_count_ = (unsigned int)indexCount;
    } else {
        vertex_count_ = (unsigned int)(floatCount / (stride_ > 0 ? stride_ : 6));
    }
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void Mesh::SetupVertexAttribs(int stride) {
    stride_ = stride;
    if (stride == 8) {
        // (x, y, z, nx, ny, nz, u, v)
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
        glEnableVertexAttribArray(2);
    } else if (stride == 5) {
        // (x, y, z, u, v)
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);
    } else {
        // Standard 6 floats: (x, y, z, r/nx, g/ny, b/nz)
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);
    }
}

Mesh::~Mesh() {
    glDeleteVertexArrays(1, &vao_);
    glDeleteBuffers(1, &vbo_);
    if (ebo_) glDeleteBuffers(1, &ebo_);
}

void Mesh::Bind() const {
    glBindVertexArray(vao_);
}

void Mesh::Draw() const {
    glBindVertexArray(vao_);
    if (ebo_) {
        glDrawElements(GL_TRIANGLES, vertex_count_, GL_UNSIGNED_INT, 0);
    } else {
        glDrawArrays(GL_TRIANGLES, 0, vertex_count_);
    }
    glBindVertexArray(0);
}

static bool glfw_initialized = false;

Window::Window(int width, int height, const std::string& title)
    : width_(width), height_(height), title_(title), native_window_(nullptr), is_open_(false),
      default_quad_(nullptr), default_shader_(nullptr) {
}

Window* Window::Create(int width, int height, const std::string& title) {
    if (!glfw_initialized) {
        if (!glfwInit()) {
            throw std::runtime_error("Failed to initialize GLFW");
        }
        glfw_initialized = true;
    }

    Window* window = new Window(width, height, title);
    if (!window->InitOpenGL()) {
        delete window;
        return nullptr;
    }

    window->is_open_ = true;
    return window;
}

bool Window::InitOpenGL() {
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    native_window_ = glfwCreateWindow(width_, height_, title_.c_str(), nullptr, nullptr);
    if (!native_window_) {
        fprintf(stderr, "Failed to create GLFW window\n");
        return false;
    }

    glfwMakeContextCurrent((GLFWwindow*)native_window_);
    glfwSwapInterval(1);
    glfwSetInputMode((GLFWwindow*)native_window_, GLFW_STICKY_KEYS, GLFW_TRUE);
    glfwSetInputMode((GLFWwindow*)native_window_, GLFW_STICKY_MOUSE_BUTTONS, GLFW_FALSE);

    if (!gladLoadGLLoader((void*(*)(const char*))glfwGetProcAddress)) {
        fprintf(stderr, "Failed to load OpenGL functions\n");
        glfwDestroyWindow((GLFWwindow*)native_window_);
        return false;
    }

    glViewport(0, 0, width_, height_);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    // Initialize default rendering resources
    const std::string defaultVertexSrc = R"(
#version 410 core
layout (location = 0) in vec3 aPos;
uniform vec2 uResolution;
uniform vec2 uPos;
uniform vec2 uSize;
void main() {
    vec2 offset = aPos.xy + vec2(0.5, -0.5); // Top-left becomes (0,0)
    vec2 pixelPos = uPos + vec2(offset.x * uSize.x, -offset.y * uSize.y);
    vec2 ndcPos = (pixelPos / uResolution) * 2.0 - 1.0;
    ndcPos.y = -ndcPos.y;
    gl_Position = vec4(ndcPos, 0.0, 1.0);
}
    )";

    const std::string defaultFragmentSrc = R"(
#version 410 core
out vec4 FragColor;
uniform vec4 uColor;
void main() {
    FragColor = uColor;
}
    )";

    default_shader_ = Shader::Create(defaultVertexSrc, defaultFragmentSrc);
    default_quad_ = Mesh::CreateQuad();

    return true;
}

Window::~Window() {
    if (default_quad_) delete default_quad_;
    if (default_shader_) delete default_shader_;
    
    if (native_window_) {
        glfwDestroyWindow((GLFWwindow*)native_window_);
        native_window_ = nullptr;
    }
}

bool Window::IsOpen() const {
    if (!native_window_) return false;
    return !glfwWindowShouldClose((GLFWwindow*)native_window_);
}

void Window::Close() {
    if (native_window_) {
        glfwSetWindowShouldClose((GLFWwindow*)native_window_, 1);
    }
    is_open_ = false;
}

void Window::Clear(float r, float g, float b, float a) {
    if (!native_window_) return;
    glClearColor(r, g, b, a);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void Window::SwapBuffers() {
    if (!native_window_) return;
    glfwSwapBuffers((GLFWwindow*)native_window_);
}

void Window::PollEvents() {
    glfwPollEvents();
    if (native_window_) {
        int w, h;
        glfwGetFramebufferSize((GLFWwindow*)native_window_, &w, &h);
        if (w > 0 && h > 0 && (w != width_ || h != height_)) {
            width_ = w;
            height_ = h;
            glViewport(0, 0, width_, height_);
        }
    }
}

void Window::SetFullscreen(bool fullscreen) {
    if (!native_window_) return;
    GLFWwindow* win = (GLFWwindow*)native_window_;
    if (fullscreen) {
        GLFWmonitor* monitor = glfwGetPrimaryMonitor();
        if (monitor) {
            const GLFWvidmode* mode = glfwGetVideoMode(monitor);
            if (mode) {
                glfwSetWindowMonitor(win, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
            }
        }
    } else {
        glfwSetWindowMonitor(win, nullptr, 100, 100, 960, 720, 0);
    }
}

void Window::SetMaximized(bool maximized) {
    if (!native_window_) return;
    if (maximized) {
        glfwMaximizeWindow((GLFWwindow*)native_window_);
    } else {
        glfwRestoreWindow((GLFWwindow*)native_window_);
    }
}

void Window::DrawMesh(Mesh* mesh, Shader* shader) {
    if (!mesh || !shader) return;
    shader->Use();
    mesh->Draw();
}

static Shader* GetPrimitiveShader() {
    static Shader* s_primitiveShader = nullptr;
    if (!s_primitiveShader) {
        const std::string vs = R"(
#version 410 core
layout (location = 0) in vec3 aPos;
uniform vec2 uResolution;
void main() {
    vec2 ndcPos = (aPos.xy / uResolution) * 2.0 - 1.0;
    ndcPos.y = -ndcPos.y;
    gl_Position = vec4(ndcPos, 0.0, 1.0);
}
        )";
        const std::string fs = R"(
#version 410 core
out vec4 FragColor;
uniform vec4 uColor;
void main() { FragColor = uColor; }
        )";
        s_primitiveShader = Shader::Create(vs, fs);
    }
    return s_primitiveShader;
}

void Window::DrawLine(float x1, float y1, float x2, float y2, float r, float g, float b, float a) {
    if (!native_window_) return;
    
    static unsigned int s_lineVAO = 0;
    static unsigned int s_lineVBO = 0;
    if (s_lineVAO == 0) {
        glGenVertexArrays(1, &s_lineVAO);
        glGenBuffers(1, &s_lineVBO);
        glBindVertexArray(s_lineVAO);
        glBindBuffer(GL_ARRAY_BUFFER, s_lineVBO);
        glBufferData(GL_ARRAY_BUFFER, 6 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
    
    float vertices[] = {
        x1, y1, 0.0f,
        x2, y2, 0.0f
    };
    
    glBindBuffer(GL_ARRAY_BUFFER, s_lineVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    
    Shader* shader = GetPrimitiveShader();
    if (!shader) return;
    
    shader->Use();
    shader->SetVec4("uColor", r, g, b, a);
    shader->SetVec2("uResolution", (float)width_, (float)height_);
    
    glBindVertexArray(s_lineVAO);
    glDrawArrays(GL_LINES, 0, 2);
    glBindVertexArray(0);
}

void Window::DrawRect(float x, float y, float width, float height, float r, float g, float b, float a) {
    if (!native_window_ || !default_shader_ || !default_quad_) return;
    
    default_shader_->Use();
    
    // Set uniforms
    int locRes = glGetUniformLocation(default_shader_->GetID(), "uResolution");
    glUniform2f(locRes, (float)width_, (float)height_);
    
    int locPos = glGetUniformLocation(default_shader_->GetID(), "uPos");
    glUniform2f(locPos, x, y);
    
    int locSize = glGetUniformLocation(default_shader_->GetID(), "uSize");
    glUniform2f(locSize, width, height);
    
    int locColor = glGetUniformLocation(default_shader_->GetID(), "uColor");
    glUniform4f(locColor, r, g, b, a);
    
    default_quad_->Draw();
}

void Window::DrawCircle(float x, float y, float radius, float r, float g, float b, float a, int segments) {
    if (!native_window_ || segments < 3) return;
    
    static unsigned int s_circleVAO = 0;
    static unsigned int s_circleVBO = 0;
    static size_t s_circleVBOCapacity = 0;
    
    if (s_circleVAO == 0) {
        glGenVertexArrays(1, &s_circleVAO);
        glGenBuffers(1, &s_circleVBO);
        glBindVertexArray(s_circleVAO);
        glBindBuffer(GL_ARRAY_BUFFER, s_circleVBO);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
    
    static std::vector<float> vertices;
    vertices.clear();
    vertices.reserve((segments + 2) * 3);
    vertices.push_back(x);
    vertices.push_back(y);
    vertices.push_back(0.0f);
    
    for (int i = 0; i <= segments; ++i) {
        float angle = 2.0f * 3.14159265358979323846f * (float)i / (float)segments;
        vertices.push_back(x + std::cos(angle) * radius);
        vertices.push_back(y + std::sin(angle) * radius);
        vertices.push_back(0.0f);
    }
    
    glBindBuffer(GL_ARRAY_BUFFER, s_circleVBO);
    size_t neededBytes = vertices.size() * sizeof(float);
    if (neededBytes > s_circleVBOCapacity) {
        glBufferData(GL_ARRAY_BUFFER, neededBytes, vertices.data(), GL_DYNAMIC_DRAW);
        s_circleVBOCapacity = neededBytes;
    } else {
        glBufferSubData(GL_ARRAY_BUFFER, 0, neededBytes, vertices.data());
    }
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    
    Shader* shader = GetPrimitiveShader();
    if (!shader) return;
    
    shader->Use();
    shader->SetVec4("uColor", r, g, b, a);
    shader->SetVec2("uResolution", (float)width_, (float)height_);
    
    glBindVertexArray(s_circleVAO);
    glDrawArrays(GL_TRIANGLE_FAN, 0, (GLsizei)(segments + 2));
    glBindVertexArray(0);
}

void Window::DrawCircle(float x, float y, float radius, float r, float g, float b, int segments) {
    DrawCircle(x, y, radius, r, g, b, 1.0f, segments);
}

bool Window::IsKeyPressed(Key key) const {
    if (!native_window_) return false;
    
    int glfwKey = 0;
    switch (key) {
        case Key::A: glfwKey = GLFW_KEY_A; break;
        case Key::B: glfwKey = GLFW_KEY_B; break;
        case Key::C: glfwKey = GLFW_KEY_C; break;
        case Key::D: glfwKey = GLFW_KEY_D; break;
        case Key::E: glfwKey = GLFW_KEY_E; break;
        case Key::F: glfwKey = GLFW_KEY_F; break;
        case Key::G: glfwKey = GLFW_KEY_G; break;
        case Key::H: glfwKey = GLFW_KEY_H; break;
        case Key::I: glfwKey = GLFW_KEY_I; break;
        case Key::J: glfwKey = GLFW_KEY_J; break;
        case Key::K: glfwKey = GLFW_KEY_K; break;
        case Key::L: glfwKey = GLFW_KEY_L; break;
        case Key::M: glfwKey = GLFW_KEY_M; break;
        case Key::N: glfwKey = GLFW_KEY_N; break;
        case Key::O: glfwKey = GLFW_KEY_O; break;
        case Key::P: glfwKey = GLFW_KEY_P; break;
        case Key::Q: glfwKey = GLFW_KEY_Q; break;
        case Key::R: glfwKey = GLFW_KEY_R; break;
        case Key::S: glfwKey = GLFW_KEY_S; break;
        case Key::T: glfwKey = GLFW_KEY_T; break;
        case Key::U: glfwKey = GLFW_KEY_U; break;
        case Key::V: glfwKey = GLFW_KEY_V; break;
        case Key::W: glfwKey = GLFW_KEY_W; break;
        case Key::X: glfwKey = GLFW_KEY_X; break;
        case Key::Y: glfwKey = GLFW_KEY_Y; break;
        case Key::Z: glfwKey = GLFW_KEY_Z; break;
        case Key::Key0: glfwKey = GLFW_KEY_0; break;
        case Key::Key1: glfwKey = GLFW_KEY_1; break;
        case Key::Key2: glfwKey = GLFW_KEY_2; break;
        case Key::Key3: glfwKey = GLFW_KEY_3; break;
        case Key::Key4: glfwKey = GLFW_KEY_4; break;
        case Key::Key5: glfwKey = GLFW_KEY_5; break;
        case Key::Key6: glfwKey = GLFW_KEY_6; break;
        case Key::Key7: glfwKey = GLFW_KEY_7; break;
        case Key::Key8: glfwKey = GLFW_KEY_8; break;
        case Key::Key9: glfwKey = GLFW_KEY_9; break;
        case Key::Space: glfwKey = GLFW_KEY_SPACE; break;
        case Key::Enter: glfwKey = GLFW_KEY_ENTER; break;
        case Key::Escape: glfwKey = GLFW_KEY_ESCAPE; break;
        case Key::Backspace: glfwKey = GLFW_KEY_BACKSPACE; break;
        case Key::Tab: glfwKey = GLFW_KEY_TAB; break;
        case Key::Delete: glfwKey = GLFW_KEY_DELETE; break;
        case Key::Left: glfwKey = GLFW_KEY_LEFT; break;
        case Key::Right: glfwKey = GLFW_KEY_RIGHT; break;
        case Key::Up: glfwKey = GLFW_KEY_UP; break;
        case Key::Down: glfwKey = GLFW_KEY_DOWN; break;
        case Key::Home: glfwKey = GLFW_KEY_HOME; break;
        case Key::End: glfwKey = GLFW_KEY_END; break;
        case Key::F1: glfwKey = GLFW_KEY_F1; break;
        case Key::F2: glfwKey = GLFW_KEY_F2; break;
        case Key::F3: glfwKey = GLFW_KEY_F3; break;
        case Key::F4: glfwKey = GLFW_KEY_F4; break;
        case Key::F5: glfwKey = GLFW_KEY_F5; break;
        case Key::F6: glfwKey = GLFW_KEY_F6; break;
        case Key::F7: glfwKey = GLFW_KEY_F7; break;
        case Key::F8: glfwKey = GLFW_KEY_F8; break;
        case Key::F9: glfwKey = GLFW_KEY_F9; break;
        case Key::F10: glfwKey = GLFW_KEY_F10; break;
        case Key::F11: glfwKey = GLFW_KEY_F11; break;
        case Key::F12: glfwKey = GLFW_KEY_F12; break;
        case Key::LeftShift: glfwKey = GLFW_KEY_LEFT_SHIFT; break;
        case Key::RightShift: glfwKey = GLFW_KEY_RIGHT_SHIFT; break;
        case Key::LeftControl: glfwKey = GLFW_KEY_LEFT_CONTROL; break;
        case Key::RightControl: glfwKey = GLFW_KEY_RIGHT_CONTROL; break;
        case Key::LeftAlt: glfwKey = GLFW_KEY_LEFT_ALT; break;
        case Key::RightAlt: glfwKey = GLFW_KEY_RIGHT_ALT; break;
        case Key::Apostrophe: glfwKey = GLFW_KEY_APOSTROPHE; break;
        case Key::Comma: glfwKey = GLFW_KEY_COMMA; break;
        case Key::Minus: glfwKey = GLFW_KEY_MINUS; break;
        case Key::Period: glfwKey = GLFW_KEY_PERIOD; break;
        case Key::Slash: glfwKey = GLFW_KEY_SLASH; break;
        case Key::Semicolon: glfwKey = GLFW_KEY_SEMICOLON; break;
        case Key::Equal: glfwKey = GLFW_KEY_EQUAL; break;
        case Key::LeftBracket: glfwKey = GLFW_KEY_LEFT_BRACKET; break;
        case Key::Backslash: glfwKey = GLFW_KEY_BACKSLASH; break;
        case Key::RightBracket: glfwKey = GLFW_KEY_RIGHT_BRACKET; break;
        case Key::GraveAccent: glfwKey = GLFW_KEY_GRAVE_ACCENT; break;
        case Key::NumLock: glfwKey = GLFW_KEY_NUM_LOCK; break;
        case Key::Insert: glfwKey = GLFW_KEY_INSERT; break;
        case Key::PageUp: glfwKey = GLFW_KEY_PAGE_UP; break;
        case Key::PageDown: glfwKey = GLFW_KEY_PAGE_DOWN; break;
        case Key::CapsLock: glfwKey = GLFW_KEY_CAPS_LOCK; break;
        case Key::ScrollLock: glfwKey = GLFW_KEY_SCROLL_LOCK; break;
        case Key::PrintScreen: glfwKey = GLFW_KEY_PRINT_SCREEN; break;
        case Key::Pause: glfwKey = GLFW_KEY_PAUSE; break;
        case Key::F13: glfwKey = GLFW_KEY_F13; break;
        case Key::F14: glfwKey = GLFW_KEY_F14; break;
        case Key::F15: glfwKey = GLFW_KEY_F15; break;
        case Key::F16: glfwKey = GLFW_KEY_F16; break;
        case Key::F17: glfwKey = GLFW_KEY_F17; break;
        case Key::F18: glfwKey = GLFW_KEY_F18; break;
        case Key::F19: glfwKey = GLFW_KEY_F19; break;
        case Key::F20: glfwKey = GLFW_KEY_F20; break;
        case Key::F21: glfwKey = GLFW_KEY_F21; break;
        case Key::F22: glfwKey = GLFW_KEY_F22; break;
        case Key::F23: glfwKey = GLFW_KEY_F23; break;
        case Key::F24: glfwKey = GLFW_KEY_F24; break;
        case Key::F25: glfwKey = GLFW_KEY_F25; break;
        case Key::Kp0: glfwKey = GLFW_KEY_KP_0; break;
        case Key::Kp1: glfwKey = GLFW_KEY_KP_1; break;
        case Key::Kp2: glfwKey = GLFW_KEY_KP_2; break;
        case Key::Kp3: glfwKey = GLFW_KEY_KP_3; break;
        case Key::Kp4: glfwKey = GLFW_KEY_KP_4; break;
        case Key::Kp5: glfwKey = GLFW_KEY_KP_5; break;
        case Key::Kp6: glfwKey = GLFW_KEY_KP_6; break;
        case Key::Kp7: glfwKey = GLFW_KEY_KP_7; break;
        case Key::Kp8: glfwKey = GLFW_KEY_KP_8; break;
        case Key::Kp9: glfwKey = GLFW_KEY_KP_9; break;
        case Key::KpDecimal: glfwKey = GLFW_KEY_KP_DECIMAL; break;
        case Key::KpDivide: glfwKey = GLFW_KEY_KP_DIVIDE; break;
        case Key::KpMultiply: glfwKey = GLFW_KEY_KP_MULTIPLY; break;
        case Key::KpSubtract: glfwKey = GLFW_KEY_KP_SUBTRACT; break;
        case Key::KpAdd: glfwKey = GLFW_KEY_KP_ADD; break;
        case Key::KpEnter: glfwKey = GLFW_KEY_KP_ENTER; break;
        case Key::KpEqual: glfwKey = GLFW_KEY_KP_EQUAL; break;
        case Key::LeftSuper: glfwKey = GLFW_KEY_LEFT_SUPER; break;
        case Key::RightSuper: glfwKey = GLFW_KEY_RIGHT_SUPER; break;
        case Key::Menu: glfwKey = GLFW_KEY_MENU; break;
        case Key::World1: glfwKey = GLFW_KEY_WORLD_1; break;
        case Key::World2: glfwKey = GLFW_KEY_WORLD_2; break;
        case Key::Unknown: glfwKey = GLFW_KEY_UNKNOWN; break;

        default: return false;
    }
    
    return glfwGetKey((GLFWwindow*)native_window_, glfwKey) == GLFW_PRESS;
}

bool Window::IsMouseButtonPressed(MouseButton button) const {
    if (!native_window_) return false;
    
    int glfwButton = 0;
    switch (button) {
        case MouseButton::Left: glfwButton = GLFW_MOUSE_BUTTON_LEFT; break;
        case MouseButton::Right: glfwButton = GLFW_MOUSE_BUTTON_RIGHT; break;
        case MouseButton::Middle: glfwButton = GLFW_MOUSE_BUTTON_MIDDLE; break;
    }
    
    return glfwGetMouseButton((GLFWwindow*)native_window_, glfwButton) == GLFW_PRESS;
}

void Window::GetContentScale(float& sx, float& sy) const {
    if (!native_window_) { sx = sy = 1.0f; return; }
    int winW = 0, winH = 0, fbW = 0, fbH = 0;
    glfwGetWindowSize((GLFWwindow*)native_window_, &winW, &winH);
    glfwGetFramebufferSize((GLFWwindow*)native_window_, &fbW, &fbH);
    sx = (winW > 0 && fbW > 0) ? ((float)fbW / (float)winW) : 1.0f;
    sy = (winH > 0 && fbH > 0) ? ((float)fbH / (float)winH) : 1.0f;
}

void Window::GetWindowSize(int& w, int& h) const {
    if (!native_window_) { w = h = 0; return; }
    glfwGetWindowSize((GLFWwindow*)native_window_, &w, &h);
}

int Window::GetWindowWidth() const {
    int w = 0, h = 0;
    GetWindowSize(w, h);
    return w;
}

int Window::GetWindowHeight() const {
    int w = 0, h = 0;
    GetWindowSize(w, h);
    return h;
}

void Window::GetMousePosition(float& x, float& y) const {
    if (!native_window_) {
        x = y = 0;
        return;
    }
    
    double dx, dy;
    glfwGetCursorPos((GLFWwindow*)native_window_, &dx, &dy);
    float sx = 1.0f, sy = 1.0f;
    GetContentScale(sx, sy);
    x = (float)(dx * sx);
    y = (float)(dy * sy);
}

static bool g_resetMousePosition = true;

void Window::SetCursorGrabbed(bool grabbed) {
    if (!native_window_) return;
    if (grabbed) {
        glfwSetInputMode((GLFWwindow*)native_window_, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    } else {
        glfwSetInputMode((GLFWwindow*)native_window_, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
    g_resetMousePosition = true;
}

void Window::GetMouseDelta(float& dx, float& dy) {
    if (!native_window_) { dx = dy = 0; return; }
    double x, y;
    glfwGetCursorPos((GLFWwindow*)native_window_, &x, &y);
    static double lastX = 0, lastY = 0;
    if (g_resetMousePosition) {
        lastX = x; lastY = y;
        g_resetMousePosition = false;
        dx = 0; dy = 0;
        return;
    }
    float sx = 1.0f, sy = 1.0f;
    GetContentScale(sx, sy);
    dx = (float)((x - lastX) * sx);
    dy = (float)((y - lastY) * sy);
    lastX = x;
    lastY = y;
}

void Window::EnableDepthTest(bool enabled) {
    if (enabled) {
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
    } else {
        glDisable(GL_DEPTH_TEST);
    }
}

void Window::EnableCullFace(bool enabled) {
    if (enabled) {
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
    } else {
        glDisable(GL_CULL_FACE);
    }
}

Font::Font(Texture* atlas, void* chardata, float pixelHeight)
    : atlas_texture_(atlas), cdata_(chardata), pixel_height_(pixelHeight) {}

Font::~Font() {
    if (atlas_texture_) delete atlas_texture_;
    if (cdata_) delete[] static_cast<stbtt_bakedchar*>(cdata_);
}

Font* Font::Load(const std::string& fontPath, float pixelHeight) {
    std::ifstream file(fontPath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return CreateDefault(pixelHeight);

    std::streamsize size = file.tellg();
    if (size <= 0) return CreateDefault(pixelHeight);
    file.seekg(0, std::ios::beg);
    std::vector<unsigned char> fontBuffer(size);
    if (!file.read((char*)fontBuffer.data(), size)) return CreateDefault(pixelHeight);

    stbtt_bakedchar* cdata = new stbtt_bakedchar[96];
    int pw = 512, ph = 512;
    std::vector<unsigned char> tempBitmap(pw * ph, 0);

    int res = stbtt_BakeFontBitmap(fontBuffer.data(), 0, pixelHeight, tempBitmap.data(), pw, ph, 32, 96, cdata);
    if (res <= 0) {
        pw = 1024; ph = 1024;
        tempBitmap.assign(pw * ph, 0);
        stbtt_BakeFontBitmap(fontBuffer.data(), 0, pixelHeight, tempBitmap.data(), pw, ph, 32, 96, cdata);
    }

    Texture* tex = Texture::Create(pw, ph, tempBitmap.data(), 1);
    return new Font(tex, cdata, pixelHeight);
}

Font* Font::CreateDefault(float pixelHeight) {
    int pw = 512, ph = 256;
    std::vector<unsigned char> bitmap(pw * ph, 0);
    stbtt_bakedchar* cdata = new stbtt_bakedchar[96];

    static const unsigned char font8x16_basic[96][16] = {
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}, // ' ' (32)
        {0,0,0,16,16,16,16,16,16,0,16,16,0,0,0,0}, // '!'
        {0,0,36,36,36,0,0,0,0,0,0,0,0,0,0,0}, // '"'
        {0,0,0,36,36,127,36,36,127,36,36,0,0,0,0,0}, // '#'
        {0,0,16,62,80,60,18,124,16,0,0,0,0,0,0,0}, // '$'
        {0,0,67,99,19,8,12,48,98,99,0,0,0,0,0,0}, // '%'
        {0,0,28,34,34,20,40,74,68,58,0,0,0,0,0,0}, // '&'
        {0,0,24,24,16,32,0,0,0,0,0,0,0,0,0,0}, // '\''
        {0,0,12,24,32,32,32,32,24,12,0,0,0,0,0,0}, // '('
        {0,0,48,24,8,8,8,8,24,48,0,0,0,0,0,0}, // ')'
        {0,0,0,0,42,28,127,28,42,0,0,0,0,0,0,0}, // '*'
        {0,0,0,8,8,62,8,8,0,0,0,0,0,0,0,0}, // '+'
        {0,0,0,0,0,0,0,0,24,24,16,32,0,0,0,0}, // ','
        {0,0,0,0,0,62,0,0,0,0,0,0,0,0,0,0}, // '-'
        {0,0,0,0,0,0,0,0,24,24,0,0,0,0,0,0}, // '.'
        {0,0,0,2,4,8,16,32,64,0,0,0,0,0,0,0}, // '/'
        {0,0,60,66,66,66,66,66,66,60,0,0,0,0,0,0}, // '0'
        {0,0,16,48,16,16,16,16,16,56,0,0,0,0,0,0}, // '1'
        {0,0,60,66,2,4,24,32,64,126,0,0,0,0,0,0}, // '2'
        {0,0,60,66,2,28,2,2,66,60,0,0,0,0,0,0}, // '3'
        {0,0,8,24,40,72,126,8,8,8,0,0,0,0,0,0}, // '4'
        {0,0,126,64,64,124,2,2,66,60,0,0,0,0,0,0}, // '5'
        {0,0,60,66,64,124,66,66,66,60,0,0,0,0,0,0}, // '6'
        {0,0,126,2,4,8,16,16,16,16,0,0,0,0,0,0}, // '7'
        {0,0,60,66,66,60,66,66,66,60,0,0,0,0,0,0}, // '8'
        {0,0,60,66,66,66,62,2,66,60,0,0,0,0,0,0}, // '9'
        {0,0,0,24,24,0,0,24,24,0,0,0,0,0,0,0}, // ':'
        {0,0,0,24,24,0,0,24,24,16,32,0,0,0,0,0}, // ';'
        {0,0,6,12,24,48,24,12,6,0,0,0,0,0,0,0}, // '<'
        {0,0,0,0,126,0,126,0,0,0,0,0,0,0,0,0}, // '='
        {0,0,48,24,12,6,12,24,48,0,0,0,0,0,0,0}, // '>'
        {0,0,60,66,4,8,16,16,0,16,0,0,0,0,0,0}, // '?'
        {0,0,60,66,74,86,78,64,66,60,0,0,0,0,0,0}, // '@'
        {0,0,24,36,66,66,126,66,66,66,0,0,0,0,0,0}, // 'A'
        {0,0,124,66,66,124,66,66,66,124,0,0,0,0,0,0}, // 'B'
        {0,0,60,66,64,64,64,64,66,60,0,0,0,0,0,0}, // 'C'
        {0,0,120,68,66,66,66,66,68,120,0,0,0,0,0,0}, // 'D'
        {0,0,126,64,64,120,64,64,64,126,0,0,0,0,0,0}, // 'E'
        {0,0,126,64,64,120,64,64,64,64,0,0,0,0,0,0}, // 'F'
        {0,0,60,66,64,78,66,66,66,60,0,0,0,0,0,0}, // 'G'
        {0,0,66,66,66,126,66,66,66,66,0,0,0,0,0,0}, // 'H'
        {0,0,60,16,16,16,16,16,16,60,0,0,0,0,0,0}, // 'I'
        {0,0,30,8,8,8,8,8,72,48,0,0,0,0,0,0}, // 'J'
        {0,0,66,68,72,112,72,68,66,66,0,0,0,0,0,0}, // 'K'
        {0,0,64,64,64,64,64,64,64,126,0,0,0,0,0,0}, // 'L'
        {0,0,66,102,90,66,66,66,66,66,0,0,0,0,0,0}, // 'M'
        {0,0,66,98,82,74,74,70,66,66,0,0,0,0,0,0}, // 'N'
        {0,0,60,66,66,66,66,66,66,60,0,0,0,0,0,0}, // 'O'
        {0,0,124,66,66,124,64,64,64,64,0,0,0,0,0,0}, // 'P'
        {0,0,60,66,66,66,66,74,68,58,0,0,0,0,0,0}, // 'Q'
        {0,0,124,66,66,124,72,68,66,66,0,0,0,0,0,0}, // 'R'
        {0,0,60,66,64,60,2,2,66,60,0,0,0,0,0,0}, // 'S'
        {0,0,126,16,16,16,16,16,16,16,0,0,0,0,0,0}, // 'T'
        {0,0,66,66,66,66,66,66,66,60,0,0,0,0,0,0}, // 'U'
        {0,0,66,66,66,66,66,36,36,24,0,0,0,0,0,0}, // 'V'
        {0,0,66,66,66,66,66,90,102,66,0,0,0,0,0,0}, // 'W'
        {0,0,66,66,36,24,24,36,66,66,0,0,0,0,0,0}, // 'X'
        {0,0,66,66,66,36,24,16,16,16,0,0,0,0,0,0}, // 'Y'
        {0,0,126,2,4,8,16,32,64,126,0,0,0,0,0,0}, // 'Z'
        {0,0,30,16,16,16,16,16,16,30,0,0,0,0,0,0}, // '['
        {0,0,0,64,32,16,8,4,2,0,0,0,0,0,0,0}, // '\'
        {0,0,120,8,8,8,8,8,8,120,0,0,0,0,0,0}, // ']'
        {0,0,16,40,68,0,0,0,0,0,0,0,0,0,0,0}, // '^'
        {0,0,0,0,0,0,0,0,0,0,127,0,0,0,0,0}, // '_'
        {0,0,32,16,8,0,0,0,0,0,0,0,0,0,0,0}, // '`'
        {0,0,0,0,60,2,62,66,66,62,0,0,0,0,0,0}, // 'a'
        {0,0,64,64,124,66,66,66,66,124,0,0,0,0,0,0}, // 'b'
        {0,0,0,0,60,66,64,64,66,60,0,0,0,0,0,0}, // 'c'
        {0,0,2,2,62,66,66,66,66,62,0,0,0,0,0,0}, // 'd'
        {0,0,0,0,60,66,126,64,66,60,0,0,0,0,0,0}, // 'e'
        {0,0,28,34,32,120,32,32,32,32,0,0,0,0,0,0}, // 'f'
        {0,0,0,0,62,66,66,66,62,2,60,0,0,0,0,0}, // 'g'
        {0,0,64,64,124,66,66,66,66,66,0,0,0,0,0,0}, // 'h'
        {0,0,16,0,48,16,16,16,16,56,0,0,0,0,0,0}, // 'i'
        {0,0,8,0,24,8,8,8,8,8,72,48,0,0,0,0}, // 'j'
        {0,0,64,64,68,72,112,72,68,66,0,0,0,0,0,0}, // 'k'
        {0,0,48,16,16,16,16,16,16,56,0,0,0,0,0,0}, // 'l'
        {0,0,0,0,108,146,146,146,146,146,0,0,0,0,0,0}, // 'm'
        {0,0,0,0,124,66,66,66,66,66,0,0,0,0,0,0}, // 'n'
        {0,0,0,0,60,66,66,66,66,60,0,0,0,0,0,0}, // 'o'
        {0,0,0,0,124,66,66,66,124,64,64,0,0,0,0,0}, // 'p'
        {0,0,0,0,62,66,66,66,62,2,2,0,0,0,0,0}, // 'q'
        {0,0,0,0,108,70,64,64,64,64,0,0,0,0,0,0}, // 'r'
        {0,0,0,0,62,64,60,2,66,60,0,0,0,0,0,0}, // 's'
        {0,0,32,32,112,32,32,32,34,28,0,0,0,0,0,0}, // 't'
        {0,0,0,0,66,66,66,66,66,62,0,0,0,0,0,0}, // 'u'
        {0,0,0,0,66,66,66,36,36,24,0,0,0,0,0,0}, // 'v'
        {0,0,0,0,66,66,146,146,108,66,0,0,0,0,0,0}, // 'w'
        {0,0,0,0,66,36,24,24,36,66,0,0,0,0,0,0}, // 'x'
        {0,0,0,0,66,66,66,62,2,60,0,0,0,0,0}, // 'y'
        {0,0,0,0,126,4,8,16,32,126,0,0,0,0,0,0}, // 'z'
        {0,0,14,24,24,112,24,24,14,0,0,0,0,0,0,0}, // '{'
        {0,0,16,16,16,16,16,16,16,16,0,0,0,0,0,0}, // '|'
        {0,0,112,24,24,14,24,24,112,0,0,0,0,0,0,0}, // '}'
        {0,0,40,68,0,0,0,0,0,0,0,0,0,0,0,0}, // '~'
        {0,0,127,127,127,127,127,127,127,127,0,0,0,0,0,0} // DEL
    };

    for (int i = 0; i < 96; i++) {
        int col = i % 16;
        int row = i / 16;
        int gx = col * 32;
        int gy = row * 32;

        for (int r = 0; r < 16; r++) {
            unsigned char b = font8x16_basic[i][r];
            for (int c = 0; c < 8; c++) {
                if (b & (1 << (7 - c))) {
                    int px_x = gx + c * 2;
                    int px_y = gy + r * 2;
                    for (int dy = 0; dy < 2; dy++) {
                        for (int dx = 0; dx < 2; dx++) {
                            int x_pos = px_x + dx;
                            int y_pos = px_y + dy;
                            if (x_pos < pw && y_pos < ph) {
                                bitmap[y_pos * pw + x_pos] = 255;
                            }
                        }
                    }
                }
            }
        }

        cdata[i].x0 = (unsigned short)gx;
        cdata[i].y0 = (unsigned short)gy;
        cdata[i].x1 = (unsigned short)(gx + 16);
        cdata[i].y1 = (unsigned short)(gy + 32);
        cdata[i].xoff = 0.0f;
        cdata[i].yoff = -14.0f * (pixelHeight / 16.0f);
        cdata[i].xadvance = 16.0f * (pixelHeight / 16.0f);
    }

    Texture* tex = Texture::Create(pw, ph, bitmap.data(), 1);
    return new Font(tex, cdata, pixelHeight);
}

float Font::GetTextWidth(const std::string& text, float scale) const {
    if (!cdata_) return 0.0f;
    const stbtt_bakedchar* chardata = static_cast<const stbtt_bakedchar*>(cdata_);
    float w = 0.0f;
    for (char c : text) {
        if (c >= 32 && c < 128) {
            w += chardata[c - 32].xadvance * scale;
        }
    }
    return w;
}

float Font::GetTextHeight(const std::string& text, float scale) const {
    (void)text;
    return pixel_height_ * scale;
}

bool Font::GetCharacterQuad(char c, float& x, float& y, GlyphQuad& quad) const {
    if (!cdata_ || c < 32 || c >= 128) return false;
    const stbtt_bakedchar* chardata = static_cast<const stbtt_bakedchar*>(cdata_);
    stbtt_aligned_quad q;
    int pw = atlas_texture_ ? atlas_texture_->GetWidth() : 512;
    int ph = atlas_texture_ ? atlas_texture_->GetHeight() : 512;
    stbtt_GetBakedQuad(const_cast<stbtt_bakedchar*>(chardata), pw, ph, c - 32, &x, &y, &q, 1);
    quad.x0 = q.x0; quad.y0 = q.y0; quad.s0 = q.s0; quad.t0 = q.t0;
    quad.x1 = q.x1; quad.y1 = q.y1; quad.s1 = q.s1; quad.t1 = q.t1;
    quad.xadvance = chardata[c - 32].xadvance;
    return true;
}

bool Font::BuildTextMesh(const std::string& text, float scale, std::vector<float>& outVerts, std::vector<unsigned int>& outInds) const {
    if (!cdata_ || text.empty()) return false;
    const stbtt_bakedchar* chardata = static_cast<const stbtt_bakedchar*>(cdata_);
    outVerts.clear();
    outInds.clear();

    float xpos = 0.0f;
    float ypos = 0.0f;
    unsigned int quadCount = 0;

    int pw = atlas_texture_ ? atlas_texture_->GetWidth() : 512;
    int ph = atlas_texture_ ? atlas_texture_->GetHeight() : 512;

    for (char c : text) {
        if (c < 32 || c >= 128) {
            if (c == '\n') {
                xpos = 0.0f;
                ypos += pixel_height_ * 1.2f;
            }
            continue;
        }
        stbtt_aligned_quad q;
        stbtt_GetBakedQuad(const_cast<stbtt_bakedchar*>(chardata), pw, ph, c - 32, &xpos, &ypos, &q, 1);

        float x0 = q.x0 * scale; float y0 = q.y0 * scale;
        float x1 = q.x1 * scale; float y1 = q.y1 * scale;
        float s0 = q.s0; float t0 = q.t0;
        float s1 = q.s1; float t1 = q.t1;

        outVerts.insert(outVerts.end(), {
            x0, y0, 0.0f, s0, t0, 0.0f,
            x1, y0, 0.0f, s1, t0, 0.0f,
            x1, y1, 0.0f, s1, t1, 0.0f,
            x0, y1, 0.0f, s0, t1, 0.0f
        });

        unsigned int base = quadCount * 4;
        outInds.insert(outInds.end(), { base, base + 1, base + 2, base + 2, base + 3, base });
        quadCount++;
    }

    return !outVerts.empty();
}

Mesh* Font::CreateTextMesh(const std::string& text, float scale) const {
    std::vector<float> verts;
    std::vector<unsigned int> inds;
    if (!BuildTextMesh(text, scale, verts, inds)) {
        return Mesh::CreateQuad();
    }
    return Mesh::Create(verts.data(), verts.size(), inds.data(), inds.size(), 6);
}

static Shader* g_text_shader = nullptr;

void Window::DrawText(Font* font, const std::string& text, float x, float y, float r, float g, float b, float a, float scale) {
    if (text.empty() || !native_window_) return;
    if (!font) font = Font::CreateDefault(18.0f);
    if (!font) return;

    if (!g_text_shader) {
        const std::string vs = R"(
#version 410 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aTexCoord;
uniform vec2 uScreenSize;
uniform vec2 uPos;
out vec2 TexCoord;
void main() {
    vec2 pixelPos = uPos + vec2(aPos.x, aPos.y);
    vec2 ndcPos = (pixelPos / uScreenSize) * 2.0 - 1.0;
    ndcPos.y = -ndcPos.y;
    gl_Position = vec4(ndcPos, 0.0, 1.0);
    TexCoord = aTexCoord.xy;
}
)";
        const std::string fs = R"(
#version 410 core
in vec2 TexCoord;
uniform sampler2D uFontTexture;
uniform vec4 uColor;
out vec4 FragColor;
void main() {
    float alpha = texture(uFontTexture, TexCoord).r;
    if (alpha < 0.01) discard;
    FragColor = vec4(uColor.rgb, uColor.a * alpha);
}
)";
        g_text_shader = Shader::Create(vs, fs);
    }

    static Mesh* s_reusableTextMesh = nullptr;
    static std::vector<float> s_textVerts;
    static std::vector<unsigned int> s_textInds;

    if (!font->BuildTextMesh(text, scale, s_textVerts, s_textInds)) return;

    if (!s_reusableTextMesh) {
        s_reusableTextMesh = Mesh::Create(s_textVerts.data(), s_textVerts.size(), s_textInds.data(), s_textInds.size(), 6);
    } else {
        s_reusableTextMesh->UpdateData(s_textVerts.data(), s_textVerts.size(), s_textInds.data(), s_textInds.size());
    }

    if (!s_reusableTextMesh) return;

    g_text_shader->Use();
    g_text_shader->SetVec2("uScreenSize", (float)width_, (float)height_);
    g_text_shader->SetVec2("uPos", x, y + font->GetPixelHeight() * scale);
    g_text_shader->SetVec4("uColor", r, g, b, a);

    if (font->GetTexture()) {
        font->GetTexture()->Bind(0);
        g_text_shader->SetInt("uFontTexture", 0);
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);

    s_reusableTextMesh->Draw();
}

void Window::DrawTextDefault(const std::string& text, float x, float y, float r, float g, float b, float a, float fontSize) {
    static std::unordered_map<int, Font*> s_fontCache;
    int key = static_cast<int>(std::round(fontSize * 10.0f));
    auto it = s_fontCache.find(key);
    Font* font = nullptr;
    if (it == s_fontCache.end()) {
        font = Font::CreateDefault(fontSize);
        s_fontCache[key] = font;
    } else {
        font = it->second;
    }
    DrawText(font, text, x, y, r, g, b, a, 1.0f);
}

}
