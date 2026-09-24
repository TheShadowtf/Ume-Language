#pragma once
// graphics_compiler_bindings.h — Graphics runtime bindings for the Ume compiler.
//
// This header is included by the generated C++ output (inside the global scope,
// after the _ume_rt namespace and `using namespace _ume_rt;`). It defines:
//   - Type aliases: NativeWindow, NativeShader, NativeMesh, NativeTexture
//   - The _ume_rt::Graphics struct with static methods for all graphics ops
//   - Keys / MouseButtons type aliases (so Graphics::Keys::A works)
//
// Methods accept std::shared_ptr<NativeX> (primary) AND const Any& (fallback)
// so that both typed variables (Window w = ...) and dynamic ones (any w = ...)
// compile correctly.
#include <memory>
#include <string>
#include <vector>
#include <any>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "graphics.h"
#ifdef near
#undef near
#endif
#ifdef far
#undef far
#endif
#ifdef DrawText
#undef DrawText
#endif

namespace _ume_rt {

using NativeWindow  = Ume::Graphics::Window;
using NativeShader  = Ume::Graphics::Shader;
using NativeMesh    = Ume::Graphics::Mesh;
using NativeTexture = Ume::Graphics::Texture;
using NativeFont    = Ume::Graphics::Font;

// ── Pointer extraction helpers (handle both shared_ptr and Any)
inline std::shared_ptr<NativeFont> _extractFont(const std::shared_ptr<NativeFont>& f) { return f; }
inline std::shared_ptr<NativeFont> _extractFont(const Any& a) {
    if (!a.value_.has_value()) return nullptr;
    if (auto p = std::any_cast<std::shared_ptr<NativeFont>>(&a.value_)) return *p;
    if (auto p = std::any_cast<NativeFont*>(&a.value_)) return std::shared_ptr<NativeFont>(*p, [](NativeFont*){});
    return nullptr;
}
inline std::shared_ptr<NativeWindow> _extractWindow(const std::shared_ptr<NativeWindow>& w) { return w; }
inline std::shared_ptr<NativeWindow> _extractWindow(const Any& a) {
    if (!a.value_.has_value()) return nullptr;
    if (auto p = std::any_cast<std::shared_ptr<NativeWindow>>(&a.value_)) return *p;
    if (auto p = std::any_cast<NativeWindow*>(&a.value_)) return std::shared_ptr<NativeWindow>(*p, [](NativeWindow*){});
    return nullptr;
}
inline std::shared_ptr<NativeShader> _extractShader(const std::shared_ptr<NativeShader>& s) { return s; }
inline std::shared_ptr<NativeShader> _extractShader(const Any& a) {
    if (!a.value_.has_value()) return nullptr;
    if (auto p = std::any_cast<std::shared_ptr<NativeShader>>(&a.value_)) return *p;
    if (auto p = std::any_cast<NativeShader*>(&a.value_)) return std::shared_ptr<NativeShader>(*p, [](NativeShader*){});
    return nullptr;
}
inline std::shared_ptr<NativeMesh> _extractMesh(const std::shared_ptr<NativeMesh>& m) { return m; }
inline std::shared_ptr<NativeMesh> _extractMesh(const Any& a) {
    if (!a.value_.has_value()) return nullptr;
    if (auto p = std::any_cast<std::shared_ptr<NativeMesh>>(&a.value_)) return *p;
    if (auto p = std::any_cast<NativeMesh*>(&a.value_)) return std::shared_ptr<NativeMesh>(*p, [](NativeMesh*){});
    return nullptr;
}
inline std::shared_ptr<NativeTexture> _extractTexture(const std::shared_ptr<NativeTexture>& t) { return t; }
inline std::shared_ptr<NativeTexture> _extractTexture(const Any& a) {
    if (!a.value_.has_value()) return nullptr;
    if (auto p = std::any_cast<std::shared_ptr<NativeTexture>>(&a.value_)) return *p;
    if (auto p = std::any_cast<NativeTexture*>(&a.value_)) return std::shared_ptr<NativeTexture>(*p, [](NativeTexture*){});
    return nullptr;
}

struct Graphics {
    // Static Any fields so that `Graphics.Keys` and `Graphics.MouseButtons`
    // (as value accesses in stdlib getKeys()/getMouseButtons()) compile.
    // The actual key/mousebutton values are accessed via the Key/MouseButton
    // enums defined in stdlib graphics.ume, not through Graphics.Keys.
    static inline Any Keys{};
    static inline Any MouseButtons{};

    // ── Window creation
    static std::shared_ptr<NativeWindow> createWindow(Int width, Int height, UmeString title) {
        auto w = NativeWindow::Create(width, height, title.c_str());
        return std::shared_ptr<NativeWindow>(w);
    }

    // ── Window state (templated to accept both shared_ptr and Any) ──
    template<typename W>
    static bool windowIsOpen(const W& w) {
        auto ptr = _extractWindow(w);
        return ptr ? ptr->IsOpen() : false;
    }
    template<typename W>
    static void windowClose(const W& w) {
        if (auto ptr = _extractWindow(w)) ptr->Close();
    }
    static void windowPollEvents() {
        // Poll events is handled per-window in SwapBuffers
    }

    // ── Clearing
    // windowClear(window, r, g, b) and windowClear(window, r, g, b, a) —
    // the stdlib Window class calls Graphics.windowClear with 4 or 5 args.
    template<typename W>
    static void windowClear(const W& w, Float r, Float g, Float b, Float a = 1.0f) {
        if (auto ptr = _extractWindow(w)) ptr->Clear(r, g, b, a);
    }
    template<typename W>
    static void windowClearRGB(const W& w, Float r, Float g, Float b) {
        if (auto ptr = _extractWindow(w)) ptr->Clear(r, g, b, 1.0f);
    }
    template<typename W>
    static void windowClearRGBA(const W& w, Float r, Float g, Float b, Float a) {
        if (auto ptr = _extractWindow(w)) ptr->Clear(r, g, b, a);
    }

    // ── Swap buffers
    template<typename W>
    static void windowSwapBuffers(const W& w) {
        if (auto ptr = _extractWindow(w)) {
            ptr->SwapBuffers();
            ptr->PollEvents();
        }
    }
    template<typename W>
    static void windowPollEvents(const W& w) {
        if (auto ptr = _extractWindow(w)) ptr->PollEvents();
    }

    // ── Input
    // Key/button parameter is templated so both Int and enum-class Key values
    // work without ambiguity (enum class doesn't implicitly convert to Int).
    template<typename W, typename K>
    static bool windowIsKeyPressed(const W& w, K key) {
        auto ptr = _extractWindow(w);
        return ptr ? ptr->IsKeyPressed(static_cast<Ume::Graphics::Key>(key)) : false;
    }
    template<typename W, typename B>
    static bool windowIsMouseButtonPressed(const W& w, B button) {
        auto ptr = _extractWindow(w);
        return ptr ? ptr->IsMouseButtonPressed(static_cast<Ume::Graphics::MouseButton>(button)) : false;
    }
    template<typename W>
    static std::vector<Float> windowGetMousePosition(const W& w) {
        auto ptr = _extractWindow(w);
        float x = 0, y = 0;
        if (ptr) ptr->GetMousePosition(x, y);
        return { x, y };
    }
    template<typename W>
    static void windowSetCursorGrabbed(const W& w, bool grabbed) {
        if (auto ptr = _extractWindow(w)) ptr->SetCursorGrabbed(grabbed);
    }
    template<typename W>
    static std::vector<Float> windowGetMouseDelta(const W& w) {
        auto ptr = _extractWindow(w);
        float dx = 0, dy = 0;
        if (ptr) ptr->GetMouseDelta(dx, dy);
        return { dx, dy };
    }
    template<typename W>
    static void windowEnableDepthTest(const W& w, bool enabled) {
        if (auto ptr = _extractWindow(w)) ptr->EnableDepthTest(enabled);
    }
    template<typename W>
    static void windowEnableCullFace(const W& w, bool enabled) {
        if (auto ptr = _extractWindow(w)) ptr->EnableCullFace(enabled);
    }
    template<typename W>
    static void windowSetFullscreen(const W& w, bool fullscreen) {
        if (auto ptr = _extractWindow(w)) ptr->SetFullscreen(fullscreen);
    }
    template<typename W>
    static void windowSetMaximized(const W& w, bool maximized) {
        if (auto ptr = _extractWindow(w)) ptr->SetMaximized(maximized);
    }
    template<typename W>
    static Int windowGetWidth(const W& w) {
        auto ptr = _extractWindow(w);
        return ptr ? ptr->GetWidth() : 0;
    }
    template<typename W>
    static Int windowGetHeight(const W& w) {
        auto ptr = _extractWindow(w);
        return ptr ? ptr->GetHeight() : 0;
    }
    template<typename W>
    static Float windowGetAspectRatio(const W& w) {
        auto ptr = _extractWindow(w);
        return ptr ? ptr->GetAspectRatio() : 1.0f;
    }
    template<typename W>
    static std::vector<Float> windowGetContentScale(const W& w) {
        auto ptr = _extractWindow(w);
        float sx = 1.0f, sy = 1.0f;
        if (ptr) ptr->GetContentScale(sx, sy);
        return { sx, sy };
    }
    template<typename W>
    static Int windowGetWindowWidth(const W& w) {
        auto ptr = _extractWindow(w);
        return ptr ? ptr->GetWindowWidth() : 0;
    }
    template<typename W>
    static Int windowGetWindowHeight(const W& w) {
        auto ptr = _extractWindow(w);
        return ptr ? ptr->GetWindowHeight() : 0;
    }

    // ── Drawing primitives
    template<typename W>
    static void windowDrawLine(const W& w, Float x1, Float y1, Float x2, Float y2, Float r, Float g, Float b, Float a = 1.0f) {
        if (auto ptr = _extractWindow(w)) ptr->DrawLine(x1, y1, x2, y2, r, g, b, a);
    }
    template<typename W>
    static void windowDrawRect(const W& w, Float x, Float y, Float width, Float height, Float r, Float g, Float b, Float a = 1.0f) {
        if (auto ptr = _extractWindow(w)) ptr->DrawRect(x, y, width, height, r, g, b, a);
    }
    template<typename W>
    static void windowDrawCircle(const W& w, Float x, Float y, Float radius, Float r, Float g, Float b, Int segments = 32) {
        if (auto ptr = _extractWindow(w)) ptr->DrawCircle(x, y, radius, r, g, b, (int)segments);
    }
    template<typename W>
    static void windowDrawCircle(const W& w, Float x, Float y, Float radius, Float r, Float g, Float b, Float a, Int segments = 32) {
        if (auto ptr = _extractWindow(w)) ptr->DrawCircle(x, y, radius, r, g, b, a, (int)segments);
    }

    // ── Shader
    static std::shared_ptr<NativeShader> createShader(UmeString vs, UmeString fs) {
        auto s = NativeShader::Create(vs.c_str(), fs.c_str());
        return std::shared_ptr<NativeShader>(s);
    }
    template<typename S>
    static void shaderUse(const S& s) {
        if (auto ptr = _extractShader(s)) ptr->Use();
    }
    template<typename S>
    static void shaderSetInt(const S& s, UmeString name, Int val) {
        if (auto ptr = _extractShader(s)) ptr->SetInt(name.c_str(), val);
    }
    template<typename S>
    static void shaderSetFloat(const S& s, UmeString name, Float val) {
        if (auto ptr = _extractShader(s)) ptr->SetFloat(name.c_str(), val);
    }
    template<typename S>
    static void shaderSetVec2(const S& s, UmeString name, Float x, Float y) {
        if (auto ptr = _extractShader(s)) ptr->SetVec2(name.c_str(), x, y);
    }
    template<typename S>
    static void shaderSetVec3(const S& s, UmeString name, Float x, Float y, Float z) {
        if (auto ptr = _extractShader(s)) ptr->SetVec3(name.c_str(), x, y, z);
    }
    template<typename S>
    static void shaderSetVec4(const S& s, UmeString name, Float x, Float y, Float z, Float w) {
        if (auto ptr = _extractShader(s)) ptr->SetVec4(name.c_str(), x, y, z, w);
    }
    template<typename S>
    static void shaderSetMat4(const S& s, UmeString name, const std::vector<Float>& matrix) {
        if (auto ptr = _extractShader(s); ptr && matrix.size() >= 16) {
            ptr->SetMat4(name.c_str(), matrix.data());
        }
    }
    template<typename S, typename T>
    static void shaderSetTexture(const S& s, UmeString name, const T& tex, Int slot) {
        auto sptr = _extractShader(s);
        auto tptr = _extractTexture(tex);
        if (sptr && tptr) sptr->SetTexture(name.c_str(), tptr.get(), slot);
    }

    // ── Texture
    static std::shared_ptr<NativeTexture> createTexture(UmeString path) {
        // Load a texture from file via Texture::Load (implemented in graphics lib)
        auto t = NativeTexture::Load(path.std::string::c_str());
        return std::shared_ptr<NativeTexture>(t);
    }
    static std::shared_ptr<NativeTexture> createTexture(Int w, Int h, const std::vector<Int>& pixels, Int channels = 4) {
        std::vector<unsigned char> data;
        data.reserve(pixels.size());
        for (auto p : pixels) data.push_back((unsigned char)p);
        auto t = NativeTexture::Create(w, h, data.data(), (int)channels);
        return std::shared_ptr<NativeTexture>(t);
    }
    template<typename T>
    static void textureBind(const T& tex, Int slot) {
        if (auto ptr = _extractTexture(tex)) ptr->Bind(slot);
    }

    // loadImagePixels(path) → returns vector<Int> of [width, height, R,G,B,A, ...]
    // Allows Ume code to read PNG pixel data for atlas construction.
    static std::vector<Int> loadImagePixels(UmeString path) {
        std::vector<int> raw = Ume::Graphics::LoadImagePixels(std::string(path));
        return std::vector<Int>(raw.begin(), raw.end());
    }

    // ── Mesh
    static std::shared_ptr<NativeMesh> createCustomMesh(const std::vector<Float>& verts, const std::vector<Int>& inds, Int stride = 6) {
        std::vector<unsigned int> uinds(inds.begin(), inds.end());
        auto m = NativeMesh::Create(verts.data(), verts.size(), uinds.data(), uinds.size(), stride);
        return std::shared_ptr<NativeMesh>(m);
    }
    static std::shared_ptr<NativeMesh> createQuad() {
        auto m = NativeMesh::CreateQuad();
        return std::shared_ptr<NativeMesh>(m);
    }
    static std::shared_ptr<NativeMesh> createTriangle() {
        auto m = NativeMesh::CreateTriangle();
        return std::shared_ptr<NativeMesh>(m);
    }
    static std::shared_ptr<NativeMesh> createCube() {
        auto m = NativeMesh::CreateCube();
        return std::shared_ptr<NativeMesh>(m);
    }
    template<typename M>
    static void meshDraw(const M& m) {
        if (auto ptr = _extractMesh(m)) ptr->Draw();
    }
    template<typename M>
    static void meshUpdateData(const M& m, const std::vector<Float>& verts, const std::vector<Int>& inds, Int vertCount = -1, Int indCount = -1) {
        auto ptr = _extractMesh(m);
        if (!ptr) return;
        std::vector<unsigned int> uinds(inds.begin(), inds.end());
        size_t floatCount = (vertCount >= 0) ? (size_t)vertCount : verts.size();
        size_t indexCount = (indCount >= 0) ? (size_t)indCount : uinds.size();
        if (floatCount > verts.size()) floatCount = verts.size();
        if (indexCount > uinds.size()) indexCount = uinds.size();
        ptr->UpdateData(verts.data(), floatCount, uinds.data(), indexCount);
    }

    // ── Draw mesh with shader
    template<typename W, typename M, typename S>
    static void windowDrawMesh(const W& w, const M& m, const S& s) {
        auto wptr = _extractWindow(w);
        auto mptr = _extractMesh(m);
        auto sptr = _extractShader(s);
        if (wptr && mptr && sptr) wptr->DrawMesh(mptr.get(), sptr.get());
    }

    // ── Font & Text
    static std::shared_ptr<NativeFont> createFont(UmeString path, Float fontSize = 18.0f) {
        auto f = NativeFont::Load(path.std::string::c_str(), fontSize);
        return std::shared_ptr<NativeFont>(f);
    }
    static std::shared_ptr<NativeFont> createDefaultFont(Float fontSize = 18.0f) {
        auto f = NativeFont::CreateDefault(fontSize);
        return std::shared_ptr<NativeFont>(f);
    }
    template<typename F>
    static Float fontGetTextWidth(const F& f, UmeString text) {
        if (auto ptr = _extractFont(f)) return ptr->GetTextWidth(text.std::string::c_str());
        return 0.0f;
    }
    template<typename F>
    static Float fontGetTextHeight(const F& f, UmeString text) {
        if (auto ptr = _extractFont(f)) return ptr->GetTextHeight(text.std::string::c_str());
        return 0.0f;
    }
    template<typename F>
    static std::shared_ptr<NativeTexture> fontGetTexture(const F& f) {
        if (auto ptr = _extractFont(f)) {
            return std::shared_ptr<NativeTexture>(ptr->GetTexture(), [](NativeTexture*){});
        }
        return nullptr;
    }
    template<typename F>
    static std::shared_ptr<NativeMesh> fontCreateTextMesh(const F& f, UmeString text, Float scale = 1.0f) {
        if (auto ptr = _extractFont(f)) {
            auto m = ptr->CreateTextMesh(text.std::string::c_str(), scale);
            return std::shared_ptr<NativeMesh>(m);
        }
        return nullptr;
    }
    template<typename W, typename F>
    static void windowDrawText(const W& w, const F& f, UmeString text, Float x, Float y, Float r, Float g, Float b, Float a = 1.0f, Float scale = 1.0f) {
        auto wptr = _extractWindow(w);
        auto fptr = _extractFont(f);
        if (wptr) wptr->DrawText(fptr.get(), text.std::string::c_str(), x, y, r, g, b, a, scale);
    }
    template<typename W>
    static void windowDrawTextDefault(const W& w, UmeString text, Float x, Float y, Float r = 1.0f, Float g = 1.0f, Float b = 1.0f, Float a = 1.0f, Float fontSize = 18.0f) {
        if (auto wptr = _extractWindow(w)) wptr->DrawTextDefault(text.std::string::c_str(), x, y, r, g, b, a, fontSize);
    }
};

}
