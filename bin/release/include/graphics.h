#pragma once

#include <string>
#include <memory>
#include <array>
#include <vector>

#ifdef DrawText
#undef DrawText
#endif

namespace Ume::Graphics {

// ─────────────────────────────────────────────────────────────
// Input Handling
// ─────────────────────────────────────────────────────────────
enum class Key {
    // Letter keys
    A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
    // Number keys
    Key0, Key1, Key2, Key3, Key4, Key5, Key6, Key7, Key8, Key9,
    // Special keys
    Space, Enter, Escape, Backspace, Tab, Delete,
    Left, Right, Up, Down, Home, End,
    // Function keys
    F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
    LeftShift, RightShift, LeftControl, RightControl, LeftAlt, RightAlt
};

enum class MouseButton {
    Left, Right, Middle
};

// ─────────────────────────────────────────────────────────────
// Texture
// ─────────────────────────────────────────────────────────────
class Texture {
public:
    static Texture* Create(int width, int height, const unsigned char* pixels, int channels = 4);
    // Load a texture from an image file (PNG, JPEG, etc. via stb_image if available).
    // Returns nullptr if the file cannot be loaded or stb_image is not compiled in.
    static Texture* Load(const std::string& path);
    ~Texture();

    void Bind(unsigned int slot = 0) const;
    unsigned int GetID() const { return id_; }
    int GetWidth() const { return width_; }
    int GetHeight() const { return height_; }

    // ── camelCase wrappers (for Ume compiler compatibility) ──
    void bind(unsigned int slot = 0) const { Bind(slot); }
    unsigned int getID() const { return GetID(); }
    int getWidth() const { return GetWidth(); }
    int getHeight() const { return GetHeight(); }

private:
    Texture(unsigned int id, int width, int height);
    unsigned int id_;
    int width_;
    int height_;
};

// ─────────────────────────────────────────────────────────────
// Shader
// ─────────────────────────────────────────────────────────────
class Shader {
public:
    static Shader* Create(const std::string& vertexSource, const std::string& fragmentSource);
    ~Shader();

    void Use() const;
    void SetFloat(const std::string& name, float value);
    void SetInt(const std::string& name, int value);
    void SetVec2(const std::string& name, float x, float y);
    void SetVec3(const std::string& name, float x, float y, float z);
    void SetVec4(const std::string& name, float x, float y, float z, float w);
    void SetMat4(const std::string& name, const float* matrix);
    void SetTexture(const std::string& name, const Texture* texture, int slot = 0);

    unsigned int GetID() const { return program_id_; }

    // ── camelCase wrappers (for Ume compiler compatibility) ──
    void use() const { Use(); }
    void setFloat(const std::string& name, float value) { SetFloat(name, value); }
    void setInt(const std::string& name, int value) { SetInt(name, value); }
    void setVec2(const std::string& name, float x, float y) { SetVec2(name, x, y); }
    void setVec3(const std::string& name, float x, float y, float z) { SetVec3(name, x, y, z); }
    void setVec4(const std::string& name, float x, float y, float z, float w) { SetVec4(name, x, y, z, w); }
    void setMat4(const std::string& name, const float* matrix) { SetMat4(name, matrix); }
    void setMat4(const std::string& name, const std::vector<float>& matrix) {
        if (matrix.size() >= 16) SetMat4(name, matrix.data());
    }
    void setTexture(const std::string& name, const Texture* texture, int slot = 0) {
        SetTexture(name, texture, slot);
    }
    void setTexture(const std::string& name, std::shared_ptr<Texture> texture, int slot = 0) {
        SetTexture(name, texture.get(), slot);
    }
    unsigned int getID() const { return GetID(); }

private:
    Shader(unsigned int program_id);
    
    unsigned int program_id_;
    
    static unsigned int CompileShader(const std::string& source, unsigned int type);
};

// ─────────────────────────────────────────────────────────────
// Vertex Array Object (VAO) + VBO management
// ─────────────────────────────────────────────────────────────
class Mesh {
public:
    static Mesh* CreateQuad();
    static Mesh* CreateTriangle();
    static Mesh* CreateCube();
    static Mesh* Create(const float* vertices, size_t floatCount, const unsigned int* indices = nullptr, size_t indexCount = 0, int stride = 6);
    ~Mesh();

    void UpdateData(const float* vertices, size_t floatCount, const unsigned int* indices = nullptr, size_t indexCount = 0);
    void Bind() const;
    void Draw() const;
    
    unsigned int GetVertexCount() const { return vertex_count_; }
    int GetStride() const { return stride_; }

    // ── camelCase wrappers (for Ume compiler compatibility) ──
    void updateData(const float* vertices, size_t floatCount, const unsigned int* indices = nullptr, size_t indexCount = 0) {
        UpdateData(vertices, floatCount, indices, indexCount);
    }
    void updateData(const std::vector<float>& verts, const std::vector<unsigned int>& inds) {
        UpdateData(verts.data(), verts.size(), inds.data(), inds.size());
    }
    void updateData(const std::vector<float>& verts, const std::vector<int>& inds) {
        std::vector<unsigned int> uinds(inds.begin(), inds.end());
        UpdateData(verts.data(), verts.size(), uinds.data(), uinds.size());
    }
    // 4-arg variant with explicit counts
    void updateData(const std::vector<float>& verts, const std::vector<int>& inds, int vertCount, int indCount) {
        std::vector<unsigned int> uinds(inds.begin(), inds.end());
        size_t fc = (vertCount >= 0) ? (size_t)vertCount : verts.size();
        size_t ic = (indCount >= 0) ? (size_t)indCount : uinds.size();
        if (fc > verts.size()) fc = verts.size();
        if (ic > uinds.size()) ic = uinds.size();
        UpdateData(verts.data(), fc, uinds.data(), ic);
    }
    void updateData(const std::vector<float>& verts, const std::vector<unsigned int>& inds, int vertCount, int indCount) {
        size_t fc = (vertCount >= 0) ? (size_t)vertCount : verts.size();
        size_t ic = (indCount >= 0) ? (size_t)indCount : inds.size();
        if (fc > verts.size()) fc = verts.size();
        if (ic > inds.size()) ic = inds.size();
        UpdateData(verts.data(), fc, inds.data(), ic);
    }
    void bind() const { Bind(); }
    void draw() const { Draw(); }
    unsigned int getVertexCount() const { return GetVertexCount(); }
    int getStride() const { return GetStride(); }

private:
    Mesh();
    
    unsigned int vao_, vbo_, ebo_;
    unsigned int vertex_count_;
    int stride_;
    
    void SetupVertexAttribs(int stride = 6);
};

// ─────────────────────────────────────────────────────────────
// Font & Text Rendering
// ─────────────────────────────────────────────────────────────
struct GlyphQuad {
    float x0, y0, s0, t0;
    float x1, y1, s1, t1;
    float xadvance;
};

class Font {
public:
    static Font* Load(const std::string& fontPath, float pixelHeight = 18.0f);
    static Font* CreateDefault(float pixelHeight = 18.0f);
    ~Font();

    Texture* GetTexture() const { return atlas_texture_; }
    float GetPixelHeight() const { return pixel_height_; }

    float GetTextWidth(const std::string& text, float scale = 1.0f) const;
    float GetTextHeight(const std::string& text, float scale = 1.0f) const;

    bool GetCharacterQuad(char c, float& x, float& y, GlyphQuad& quad) const;
    Mesh* CreateTextMesh(const std::string& text, float scale = 1.0f) const;

    // ── camelCase wrappers ──
    Texture* getTexture() const { return GetTexture(); }
    float getPixelHeight() const { return GetPixelHeight(); }
    float getTextWidth(const std::string& text, float scale = 1.0f) const { return GetTextWidth(text, scale); }
    float getTextHeight(const std::string& text, float scale = 1.0f) const { return GetTextHeight(text, scale); }
    Mesh* createTextMesh(const std::string& text, float scale = 1.0f) const { return CreateTextMesh(text, scale); }

private:
    Font(Texture* atlas, void* chardata, float pixelHeight);

    Texture* atlas_texture_;
    void* cdata_; // stbtt_bakedchar[96]
    float pixel_height_;
};

// ─────────────────────────────────────────────────────────────
// Window with Input and Rendering
// ─────────────────────────────────────────────────────────────
class Window {
public:
    static Window* Create(int width, int height, const std::string& title);
    ~Window();

    // Window state
    bool IsOpen() const;
    void Close();
    
    // Rendering
    void Clear(float r, float g, float b, float a = 1.0f);
    void SwapBuffers();
    void PollEvents();
    
    // Drawing
    void DrawMesh(Mesh* mesh, Shader* shader);
    void DrawLine(float x1, float y1, float x2, float y2, float r, float g, float b);
    void DrawRect(float x, float y, float width, float height, float r, float g, float b);
    void DrawCircle(float x, float y, float radius, float r, float g, float b, int segments = 32);
    void DrawText(Font* font, const std::string& text, float x, float y, float r, float g, float b, float a = 1.0f, float scale = 1.0f);
    void DrawTextDefault(const std::string& text, float x, float y, float r = 1.0f, float g = 1.0f, float b = 1.0f, float a = 1.0f, float fontSize = 18.0f);

    // Input
    bool IsKeyPressed(Key key) const;
    bool IsMouseButtonPressed(MouseButton button) const;
    void GetMousePosition(float& x, float& y) const;
    void SetCursorGrabbed(bool grabbed);
    void GetMouseDelta(float& dx, float& dy);
    void EnableDepthTest(bool enabled);
    void EnableCullFace(bool enabled);
    void SetFullscreen(bool fullscreen);
    void SetMaximized(bool maximized);
    
    // Properties
    int GetWidth() const { return width_; }
    int GetHeight() const { return height_; }
    float GetAspectRatio() const { return (float)width_ / (float)height_; }

    // ── camelCase wrappers (for Ume compiler compatibility) ──
    // These allow Ume code to call window.isOpen(), window.isKeyPressed(), etc.
    // directly on Window instances (shared_ptr<NativeWindow>).
    bool isOpen() const { return IsOpen(); }
    void close() { Close(); }
    void clearRGB(float r, float g, float b) { Clear(r, g, b, 1.0f); }
    void clearRGBA(float r, float g, float b, float a) { Clear(r, g, b, a); }
    void swapBuffers() { SwapBuffers(); }
    void pollEvents() { PollEvents(); }

    void drawMesh(Mesh* mesh, Shader* shader) { DrawMesh(mesh, shader); }
    void drawMesh(std::shared_ptr<Mesh> mesh, std::shared_ptr<Shader> shader) {
        DrawMesh(mesh.get(), shader.get());
    }
    void drawLine(float x1, float y1, float x2, float y2, float r, float g, float b) {
        DrawLine(x1, y1, x2, y2, r, g, b);
    }
    void drawRect(float x, float y, float w, float h, float r, float g, float b) {
        DrawRect(x, y, w, h, r, g, b);
    }
    void drawCircle(float x, float y, float radius, float r, float g, float b, int segments = 32) {
        DrawCircle(x, y, radius, r, g, b, segments);
    }
    void drawText(Font* font, const std::string& text, float x, float y, float r, float g, float b, float a = 1.0f, float scale = 1.0f) {
        DrawText(font, text, x, y, r, g, b, a, scale);
    }
    void drawText(std::shared_ptr<Font> font, const std::string& text, float x, float y, float r, float g, float b, float a = 1.0f, float scale = 1.0f) {
        DrawText(font.get(), text, x, y, r, g, b, a, scale);
    }
    void drawTextDefault(const std::string& text, float x, float y, float r = 1.0f, float g = 1.0f, float b = 1.0f, float a = 1.0f, float fontSize = 18.0f) {
        DrawTextDefault(text, x, y, r, g, b, a, fontSize);
    }

    bool isKeyPressed(Key key) const { return IsKeyPressed(key); }
    bool isKeyPressed(int key) const { return IsKeyPressed(static_cast<Key>(key)); }
    bool isMouseButtonPressed(MouseButton button) const { return IsMouseButtonPressed(button); }
    bool isMouseButtonPressed(int button) const { return IsMouseButtonPressed(static_cast<MouseButton>(button)); }

    void getMousePosition(float& x, float& y) const { GetMousePosition(x, y); }
    std::vector<float> getMousePosition() {
        float x = 0, y = 0;
        GetMousePosition(x, y);
        return { x, y };
    }
    void setCursorGrabbed(bool grabbed) { SetCursorGrabbed(grabbed); }
    void getMouseDelta(float& dx, float& dy) { GetMouseDelta(dx, dy); }
    std::vector<float> getMouseDelta() {
        float dx = 0, dy = 0;
        GetMouseDelta(dx, dy);
        return { dx, dy };
    }
    void enableDepthTest(bool enabled) { EnableDepthTest(enabled); }
    void enableCullFace(bool enabled) { EnableCullFace(enabled); }
    void setFullscreen(bool fullscreen) { SetFullscreen(fullscreen); }
    void setMaximized(bool maximized) { SetMaximized(maximized); }

    int getWidth() const { return GetWidth(); }
    int getHeight() const { return GetHeight(); }
    float getAspectRatio() const { return GetAspectRatio(); }

private:
    Window(int width, int height, const std::string& title);
    
    int width_;
    int height_;
    std::string title_;
    void* native_window_;  // GLFWwindow*
    bool is_open_;

    Mesh* default_quad_;
    Shader* default_shader_;

    bool InitOpenGL();
};

} // namespace Ume::Graphics
