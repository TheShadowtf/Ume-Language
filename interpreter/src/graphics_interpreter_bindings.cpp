#include "../include/evaluator.h"
#include "../../graphics/include/graphics.h"
#include <memory>
#include <cstdio>
#include <unordered_map>

namespace Ume {

// Store pointers in global registries so we can retrieve them by ID
static std::unordered_map<int, Ume::Graphics::Window*> g_windows;
static std::unordered_map<int, Ume::Graphics::Shader*> g_shaders;
static std::unordered_map<int, Ume::Graphics::Mesh*> g_meshes;
static std::unordered_map<int, Ume::Graphics::Texture*> g_textures;
static std::unordered_map<int, Ume::Graphics::Font*> g_fonts;
static int g_next_window_id = 1;
static int g_next_shader_id = 1;
static int g_next_mesh_id = 1;
static int g_next_texture_id = 1;
static int g_next_font_id = 1;

class GraphicsWrapper {
public:
    static Ume::Graphics::Window* GetWindowFromID(int id) {
        auto it = g_windows.find(id);
        if (it == g_windows.end()) return nullptr;
        return it->second;
    }
    
    static int RegisterWindow(Ume::Graphics::Window* win) {
        int id = g_next_window_id++;
        g_windows[id] = win;
        return id;
    }

    static Ume::Graphics::Shader* GetShaderFromID(int id) {
        auto it = g_shaders.find(id);
        if (it == g_shaders.end()) return nullptr;
        return it->second;
    }
    
    static int RegisterShader(Ume::Graphics::Shader* shader) {
        int id = g_next_shader_id++;
        g_shaders[id] = shader;
        return id;
    }

    static Ume::Graphics::Mesh* GetMeshFromID(int id) {
        auto it = g_meshes.find(id);
        if (it == g_meshes.end()) return nullptr;
        return it->second;
    }
    
    static int RegisterMesh(Ume::Graphics::Mesh* mesh) {
        int id = g_next_mesh_id++;
        g_meshes[id] = mesh;
        return id;
    }

    static Ume::Graphics::Texture* GetTextureFromID(int id) {
        auto it = g_textures.find(id);
        if (it == g_textures.end()) return nullptr;
        return it->second;
    }

    static int RegisterTexture(Ume::Graphics::Texture* tex) {
        int id = g_next_texture_id++;
        g_textures[id] = tex;
        return id;
    }

    static Ume::Graphics::Font* GetFontFromID(int id) {
        auto it = g_fonts.find(id);
        if (it == g_fonts.end()) return nullptr;
        return it->second;
    }

    static int RegisterFont(Ume::Graphics::Font* font) {
        int id = g_next_font_id++;
        g_fonts[id] = font;
        return id;
    }
};

void Evaluator::registerGraphics(std::shared_ptr<Environment> env) {
    auto graphicsObj = std::make_shared<ObjectInstance>();
    graphicsObj->className = "Graphics";

    auto mkFn = [](const std::string& name, std::function<Value(std::vector<Value>)> fn) {
        auto fi = std::make_shared<FunctionInstance>();
        fi->name = name; 
        fi->native = std::move(fn);
        return Value::makeFunction(fi);
    };

    // Window functions
    graphicsObj->fields["createWindow"] = mkFn("createWindow", [](std::vector<Value> args) {
        if (args.size() < 3) throw UmeRuntimeException(Value::makeString("Window requires 3 arguments"), 0, 0);
        auto window = Ume::Graphics::Window::Create((int)args[0].intVal, (int)args[1].intVal, args[2].toString());
        if (!window) throw UmeRuntimeException(Value::makeString("Failed to create window"), 0, 0);
        return Value::makeInt(GraphicsWrapper::RegisterWindow(window));
    });

    graphicsObj->fields["windowIsOpen"] = mkFn("windowIsOpen", [](std::vector<Value> args) {
        auto w = args.empty() ? nullptr : GraphicsWrapper::GetWindowFromID((int)args[0].intVal);
        return Value::makeBool(w && w->IsOpen());
    });
    
    graphicsObj->fields["windowClose"] = mkFn("windowClose", [](std::vector<Value> args) {
        auto w = args.empty() ? nullptr : GraphicsWrapper::GetWindowFromID((int)args[0].intVal);
        if (w) w->Close();
        return Value::makeNull();
    });

    graphicsObj->fields["windowClear"] = mkFn("windowClear", [](std::vector<Value> args) {
        auto w = args.empty() ? nullptr : GraphicsWrapper::GetWindowFromID((int)args[0].intVal);
        if (w && args.size() >= 4) {
            float r = (float)args[1].toDouble(), g = (float)args[2].toDouble(), b = (float)args[3].toDouble();
            float a = args.size() >= 5 ? (float)args[4].toDouble() : 1.0f;
            w->Clear(r, g, b, a);
        }
        return Value::makeNull();
    });

    graphicsObj->fields["windowSwapBuffers"] = mkFn("windowSwapBuffers", [](std::vector<Value> args) {
        auto w = args.empty() ? nullptr : GraphicsWrapper::GetWindowFromID((int)args[0].intVal);
        if (w) w->SwapBuffers();
        return Value::makeNull();
    });

    graphicsObj->fields["windowPollEvents"] = mkFn("windowPollEvents", [](std::vector<Value> args) {
        auto w = args.empty() ? nullptr : GraphicsWrapper::GetWindowFromID((int)args[0].intVal);
        if (w) w->PollEvents();
        return Value::makeNull();
    });

    graphicsObj->fields["windowDrawLine"] = mkFn("windowDrawLine", [](std::vector<Value> args) {
        auto w = args.empty() ? nullptr : GraphicsWrapper::GetWindowFromID((int)args[0].intVal);
        if (w && args.size() >= 8) {
            w->DrawLine((float)args[1].toDouble(), (float)args[2].toDouble(), (float)args[3].toDouble(), (float)args[4].toDouble(),
                        (float)args[5].toDouble(), (float)args[6].toDouble(), (float)args[7].toDouble());
        }
        return Value::makeNull();
    });

    graphicsObj->fields["windowDrawRect"] = mkFn("windowDrawRect", [](std::vector<Value> args) {
        auto w = args.empty() ? nullptr : GraphicsWrapper::GetWindowFromID((int)args[0].intVal);
        if (w && args.size() >= 8) {
            w->DrawRect((float)args[1].toDouble(), (float)args[2].toDouble(), (float)args[3].toDouble(), (float)args[4].toDouble(),
                        (float)args[5].toDouble(), (float)args[6].toDouble(), (float)args[7].toDouble());
        }
        return Value::makeNull();
    });

    graphicsObj->fields["windowDrawCircle"] = mkFn("windowDrawCircle", [](std::vector<Value> args) {
        auto w = args.empty() ? nullptr : GraphicsWrapper::GetWindowFromID((int)args[0].intVal);
        if (w && args.size() >= 7) {
            int seg = args.size() >= 8 ? (int)args[7].intVal : 32;
            w->DrawCircle((float)args[1].toDouble(), (float)args[2].toDouble(), (float)args[3].toDouble(),
                          (float)args[4].toDouble(), (float)args[5].toDouble(), (float)args[6].toDouble(), seg);
        }
        return Value::makeNull();
    });

    graphicsObj->fields["windowIsKeyPressed"] = mkFn("windowIsKeyPressed", [](std::vector<Value> args) {
        auto w = args.empty() ? nullptr : GraphicsWrapper::GetWindowFromID((int)args[0].intVal);
        if (!w || args.size() < 2) return Value::makeBool(false);
        int keyIndex = -1;
        if (args[1].isInt()) {
            keyIndex = (int)args[1].intVal;
        } else if (args[1].isString()) {
            std::string kStr = args[1].strVal;
            size_t dot = kStr.rfind('.');
            if (dot != std::string::npos) kStr = kStr.substr(dot + 1);
            static const std::unordered_map<std::string, int> kMap = {
                {"A",0},{"B",1},{"C",2},{"D",3},{"E",4},{"F",5},{"G",6},{"H",7},{"I",8},{"J",9},
                {"K",10},{"L",11},{"M",12},{"N",13},{"O",14},{"P",15},{"Q",16},{"R",17},{"S",18},
                {"T",19},{"U",20},{"V",21},{"W",22},{"X",23},{"Y",24},{"Z",25},
                {"Key0",26},{"Key1",27},{"Key2",28},{"Key3",29},{"Key4",30},{"Key5",31},{"Key6",32},{"Key7",33},{"Key8",34},{"Key9",35},
                {"Space",36},{"Enter",37},{"Escape",38},{"Backspace",39},{"Tab",40},{"Delete",41},
                {"Left",42},{"Right",43},{"Up",44},{"Down",45},{"Home",46},{"End",47},
                {"F1",48},{"F2",49},{"F3",50},{"F4",51},{"F5",52},{"F6",53},{"F7",54},{"F8",55},{"F9",56},{"F10",57},{"F11",58},{"F12",59},
                {"LeftShift",60},{"RightShift",61},{"LeftControl",62},{"RightControl",63},{"LeftAlt",64},{"RightAlt",65}
            };
            auto it = kMap.find(kStr);
            if (it != kMap.end()) keyIndex = it->second;
        } else if (args[1].isObject() && args[1].objVal) {
            auto itVal = args[1].objVal->fields.find("__enumValue");
            if (itVal != args[1].objVal->fields.end() && itVal->second.isInt()) {
                keyIndex = (int)itVal->second.intVal;
            } else {
                auto itName = args[1].objVal->fields.find("__enumName");
                if (itName != args[1].objVal->fields.end() && itName->second.isString()) {
                    std::string kStr = itName->second.strVal;
                    static const std::unordered_map<std::string, int> kMap = {
                        {"A",0},{"B",1},{"C",2},{"D",3},{"E",4},{"F",5},{"G",6},{"H",7},{"I",8},{"J",9},
                        {"K",10},{"L",11},{"M",12},{"N",13},{"O",14},{"P",15},{"Q",16},{"R",17},{"S",18},
                        {"T",19},{"U",20},{"V",21},{"W",22},{"X",23},{"Y",24},{"Z",25},
                        {"Key0",26},{"Key1",27},{"Key2",28},{"Key3",29},{"Key4",30},{"Key5",31},{"Key6",32},{"Key7",33},{"Key8",34},{"Key9",35},
                        {"Space",36},{"Enter",37},{"Escape",38},{"Backspace",39},{"Tab",40},{"Delete",41},
                        {"Left",42},{"Right",43},{"Up",44},{"Down",45},{"Home",46},{"End",47},
                        {"F1",48},{"F2",49},{"F3",50},{"F4",51},{"F5",52},{"F6",53},{"F7",54},{"F8",55},{"F9",56},{"F10",57},{"F11",58},{"F12",59},
                        {"LeftShift",60},{"RightShift",61},{"LeftControl",62},{"RightControl",63},{"LeftAlt",64},{"RightAlt",65}
                    };
                    auto it = kMap.find(kStr);
                    if (it != kMap.end()) keyIndex = it->second;
                }
            }
        }
        if (keyIndex < 0) return Value::makeBool(false);
        return Value::makeBool(w->IsKeyPressed((Ume::Graphics::Key)keyIndex));
    });

    graphicsObj->fields["windowIsMouseButtonPressed"] = mkFn("windowIsMouseButtonPressed", [](std::vector<Value> args) {
        auto w = args.empty() ? nullptr : GraphicsWrapper::GetWindowFromID((int)args[0].intVal);
        if (!w || args.size() < 2) return Value::makeBool(false);
        int btnIndex = -1;
        if (args[1].isInt()) {
            btnIndex = (int)args[1].intVal;
        } else if (args[1].isObject() && args[1].objVal) {
            auto itVal = args[1].objVal->fields.find("__enumValue");
            if (itVal != args[1].objVal->fields.end() && itVal->second.isInt()) {
                btnIndex = (int)itVal->second.intVal;
            } else {
                auto itName = args[1].objVal->fields.find("__enumName");
                if (itName != args[1].objVal->fields.end() && itName->second.isString()) {
                    if (itName->second.strVal == "Left") btnIndex = 0;
                    else if (itName->second.strVal == "Right") btnIndex = 1;
                    else if (itName->second.strVal == "Middle") btnIndex = 2;
                }
            }
        }
        if (btnIndex < 0) return Value::makeBool(false);
        return Value::makeBool(w->IsMouseButtonPressed((Ume::Graphics::MouseButton)btnIndex));
    });

    graphicsObj->fields["windowSetCursorGrabbed"] = mkFn("windowSetCursorGrabbed", [](std::vector<Value> args) -> Value {
        auto w = args.empty() ? nullptr : GraphicsWrapper::GetWindowFromID((int)args[0].intVal);
        if (w) w->SetCursorGrabbed(args.size() > 1 ? args[1].toBool() : true);
        return Value::makeNull();
    });

    graphicsObj->fields["windowGetMouseDelta"] = mkFn("windowGetMouseDelta", [](std::vector<Value> args) -> Value {
        auto w = args.empty() ? nullptr : GraphicsWrapper::GetWindowFromID((int)args[0].intVal);
        float dx = 0, dy = 0;
        if (w) w->GetMouseDelta(dx, dy);
        auto arr = std::make_shared<ArrayInstance>();
        arr->elements.push_back(Value::makeFloat(dx));
        arr->elements.push_back(Value::makeFloat(dy));
        return Value::makeArray(arr);
    });

    graphicsObj->fields["windowEnableDepthTest"] = mkFn("windowEnableDepthTest", [](std::vector<Value> args) -> Value {
        auto w = args.empty() ? nullptr : GraphicsWrapper::GetWindowFromID((int)args[0].intVal);
        if (w) w->EnableDepthTest(args.size() > 1 ? args[1].toBool() : true);
        return Value::makeNull();
    });

    graphicsObj->fields["windowEnableCullFace"] = mkFn("windowEnableCullFace", [](std::vector<Value> args) -> Value {
        auto w = args.empty() ? nullptr : GraphicsWrapper::GetWindowFromID((int)args[0].intVal);
        if (w) w->EnableCullFace(args.size() > 1 ? args[1].toBool() : true);
        return Value::makeNull();
    });

    graphicsObj->fields["windowSetFullscreen"] = mkFn("windowSetFullscreen", [](std::vector<Value> args) -> Value {
        auto w = args.empty() ? nullptr : GraphicsWrapper::GetWindowFromID((int)args[0].intVal);
        if (w) w->SetFullscreen(args.size() > 1 ? args[1].toBool() : true);
        return Value::makeNull();
    });

    graphicsObj->fields["windowSetMaximized"] = mkFn("windowSetMaximized", [](std::vector<Value> args) -> Value {
        auto w = args.empty() ? nullptr : GraphicsWrapper::GetWindowFromID((int)args[0].intVal);
        if (w) w->SetMaximized(args.size() > 1 ? args[1].toBool() : true);
        return Value::makeNull();
    });

    graphicsObj->fields["windowGetMousePosition"] = mkFn("windowGetMousePosition", [](std::vector<Value> args) -> Value {
        auto w = args.empty() ? nullptr : GraphicsWrapper::GetWindowFromID((int)args[0].intVal);
        float x = 0, y = 0;
        if (w) w->GetMousePosition(x, y);
        auto arr = std::make_shared<ArrayInstance>();
        arr->elements.push_back(Value::makeFloat(x));
        arr->elements.push_back(Value::makeFloat(y));
        return Value::makeArray(arr);
    });

    graphicsObj->fields["windowGetWidth"] = mkFn("windowGetWidth", [](std::vector<Value> args) {
        auto w = args.empty() ? nullptr : GraphicsWrapper::GetWindowFromID((int)args[0].intVal);
        return Value::makeInt(w ? w->GetWidth() : 0);
    });
    
    graphicsObj->fields["windowGetHeight"] = mkFn("windowGetHeight", [](std::vector<Value> args) {
        auto w = args.empty() ? nullptr : GraphicsWrapper::GetWindowFromID((int)args[0].intVal);
        return Value::makeInt(w ? w->GetHeight() : 0);
    });

    graphicsObj->fields["windowGetAspectRatio"] = mkFn("windowGetAspectRatio", [](std::vector<Value> args) {
        auto w = args.empty() ? nullptr : GraphicsWrapper::GetWindowFromID((int)args[0].intVal);
        return Value::makeFloat(w ? w->GetAspectRatio() : 1.0f);
    });

    // Mesh functions
    graphicsObj->fields["createQuad"] = mkFn("createQuad", [](std::vector<Value>) {
        auto mesh = Ume::Graphics::Mesh::CreateQuad();
        return Value::makeInt(mesh ? GraphicsWrapper::RegisterMesh(mesh) : 0);
    });
    
    graphicsObj->fields["createTriangle"] = mkFn("createTriangle", [](std::vector<Value>) {
        auto mesh = Ume::Graphics::Mesh::CreateTriangle();
        return Value::makeInt(mesh ? GraphicsWrapper::RegisterMesh(mesh) : 0);
    });
    
    graphicsObj->fields["createCube"] = mkFn("createCube", [](std::vector<Value>) {
        auto mesh = Ume::Graphics::Mesh::CreateCube();
        return Value::makeInt(mesh ? GraphicsWrapper::RegisterMesh(mesh) : 0);
    });

    graphicsObj->fields["createCustomMesh"] = mkFn("createCustomMesh", [](std::vector<Value> args) -> Value {
        if (args.size() < 2) return Value::makeInt(0);
        std::vector<float> verts;
        if (args[0].isArray() && args[0].arrVal) {
            for (auto& el : args[0].arrVal->elements) {
                verts.push_back((float)el.toDouble());
            }
        }
        std::vector<unsigned int> inds;
        if (args[1].isArray() && args[1].arrVal) {
            for (auto& el : args[1].arrVal->elements) {
                inds.push_back((unsigned int)el.intVal);
            }
        }
        int stride = (args.size() >= 3) ? (int)args[2].intVal : 6;
        auto mesh = Ume::Graphics::Mesh::Create(verts.data(), verts.size(), inds.data(), inds.size(), stride);
        return Value::makeInt(mesh ? GraphicsWrapper::RegisterMesh(mesh) : 0);
    });

    graphicsObj->fields["meshUpdateData"] = mkFn("meshUpdateData", [](std::vector<Value> args) -> Value {
        if (args.size() < 3) return Value::makeNull();
        auto m = GraphicsWrapper::GetMeshFromID((int)args[0].intVal);
        if (!m) return Value::makeNull();

        std::vector<float> verts;
        if (args[1].isArray() && args[1].arrVal) {
            for (auto& el : args[1].arrVal->elements) {
                verts.push_back((float)el.toDouble());
            }
        }
        std::vector<unsigned int> inds;
        if (args[2].isArray() && args[2].arrVal) {
            for (auto& el : args[2].arrVal->elements) {
                inds.push_back((unsigned int)el.intVal);
            }
        }
        size_t floatCount = (args.size() >= 4 && args[3].isInt() && args[3].intVal >= 0) ? (size_t)args[3].intVal : verts.size();
        size_t indexCount = (args.size() >= 5 && args[4].isInt() && args[4].intVal >= 0) ? (size_t)args[4].intVal : inds.size();
        if (floatCount > verts.size()) floatCount = verts.size();
        if (indexCount > inds.size()) indexCount = inds.size();

        m->UpdateData(verts.data(), floatCount, inds.data(), indexCount);
        return Value::makeNull();
    });

    // Texture functions
    graphicsObj->fields["createTexture"] = mkFn("createTexture", [](std::vector<Value> args) -> Value {
        if (args.size() < 3) return Value::makeInt(0);
        int w = (int)args[0].intVal;
        int h = (int)args[1].intVal;
        std::vector<unsigned char> pixels;
        if (args[2].isArray() && args[2].arrVal) {
            for (auto& el : args[2].arrVal->elements) {
                pixels.push_back((unsigned char)el.intVal);
            }
        }
        int channels = (args.size() >= 4) ? (int)args[3].intVal : 4;
        auto tex = Ume::Graphics::Texture::Create(w, h, pixels.data(), channels);
        return Value::makeInt(tex ? GraphicsWrapper::RegisterTexture(tex) : 0);
    });

    // loadImagePixels(path) → returns int[] of [width, height, R,G,B,A, R,G,B,A, ...]
    // Uses stb_image to load a PNG/JPG/BMP file and returns pixel data as a Ume int array.
    // Returns an empty array if the file cannot be loaded.
    graphicsObj->fields["loadImagePixels"] = mkFn("loadImagePixels", [](std::vector<Value> args) -> Value {
        auto result = std::make_shared<ArrayInstance>();
        if (args.empty()) return Value::makeArray(result);
        std::string path = args[0].toString();
#if UME_HAS_STB
        int w = 0, h = 0, channels = 0;
        stbi_set_flip_vertically_on_load(0);  // Don't flip for atlas building
        unsigned char* data = stbi_load(path.c_str(), &w, &h, &channels, 4);
        if (!data) return Value::makeArray(result);
        result->elements.reserve(2 + w * h * 4);
        result->elements.push_back(Value::makeInt(w));
        result->elements.push_back(Value::makeInt(h));
        for (int i = 0; i < w * h * 4; i++) {
            result->elements.push_back(Value::makeInt((int)data[i]));
        }
        stbi_image_free(data);
#else
        (void)path;
#endif
        return Value::makeArray(result);
    });

    graphicsObj->fields["textureBind"] = mkFn("textureBind", [](std::vector<Value> args) -> Value {
        if (args.size() < 1) return Value::makeNull();
        auto t = GraphicsWrapper::GetTextureFromID((int)args[0].intVal);
        int slot = (args.size() >= 2) ? (int)args[1].intVal : 0;
        if (t) t->Bind(slot);
        return Value::makeNull();
    });

    // Font & Text functions
    graphicsObj->fields["createFont"] = mkFn("createFont", [](std::vector<Value> args) -> Value {
        if (args.empty()) return Value::makeInt(0);
        std::string path = args[0].toString();
        float fontSize = (args.size() >= 2) ? (float)args[1].toDouble() : 18.0f;
        auto f = Ume::Graphics::Font::Load(path, fontSize);
        return Value::makeInt(f ? GraphicsWrapper::RegisterFont(f) : 0);
    });

    graphicsObj->fields["createDefaultFont"] = mkFn("createDefaultFont", [](std::vector<Value> args) -> Value {
        float fontSize = (!args.empty()) ? (float)args[0].toDouble() : 18.0f;
        auto f = Ume::Graphics::Font::CreateDefault(fontSize);
        return Value::makeInt(f ? GraphicsWrapper::RegisterFont(f) : 0);
    });

    graphicsObj->fields["fontGetTextWidth"] = mkFn("fontGetTextWidth", [](std::vector<Value> args) -> Value {
        if (args.size() < 2) return Value::makeFloat(0.0);
        auto f = GraphicsWrapper::GetFontFromID((int)args[0].intVal);
        if (!f) return Value::makeFloat(0.0);
        return Value::makeFloat(f->GetTextWidth(args[1].toString()));
    });

    graphicsObj->fields["fontGetTextHeight"] = mkFn("fontGetTextHeight", [](std::vector<Value> args) -> Value {
        if (args.size() < 2) return Value::makeFloat(0.0);
        auto f = GraphicsWrapper::GetFontFromID((int)args[0].intVal);
        if (!f) return Value::makeFloat(0.0);
        return Value::makeFloat(f->GetTextHeight(args[1].toString()));
    });

    graphicsObj->fields["fontGetTexture"] = mkFn("fontGetTexture", [](std::vector<Value> args) -> Value {
        if (args.empty()) return Value::makeInt(0);
        auto f = GraphicsWrapper::GetFontFromID((int)args[0].intVal);
        if (!f || !f->GetTexture()) return Value::makeInt(0);
        return Value::makeInt(GraphicsWrapper::RegisterTexture(f->GetTexture()));
    });

    graphicsObj->fields["fontCreateTextMesh"] = mkFn("fontCreateTextMesh", [](std::vector<Value> args) -> Value {
        if (args.size() < 2) return Value::makeInt(0);
        auto f = GraphicsWrapper::GetFontFromID((int)args[0].intVal);
        if (!f) return Value::makeInt(0);
        float scale = (args.size() >= 3) ? (float)args[2].toDouble() : 1.0f;
        auto mesh = f->CreateTextMesh(args[1].toString(), scale);
        return Value::makeInt(mesh ? GraphicsWrapper::RegisterMesh(mesh) : 0);
    });

    graphicsObj->fields["windowDrawText"] = mkFn("windowDrawText", [](std::vector<Value> args) -> Value {
        if (args.size() < 8) return Value::makeNull();
        auto w = GraphicsWrapper::GetWindowFromID((int)args[0].intVal);
        auto f = GraphicsWrapper::GetFontFromID((int)args[1].intVal);
        if (w && f) {
            std::string text = args[2].toString();
            float x = (float)args[3].toDouble();
            float y = (float)args[4].toDouble();
            float r = (float)args[5].toDouble();
            float g = (float)args[6].toDouble();
            float b = (float)args[7].toDouble();
            float a = (args.size() >= 9) ? (float)args[8].toDouble() : 1.0f;
            float scale = (args.size() >= 10) ? (float)args[9].toDouble() : 1.0f;
            w->DrawText(f, text, x, y, r, g, b, a, scale);
        }
        return Value::makeNull();
    });

    graphicsObj->fields["windowDrawTextDefault"] = mkFn("windowDrawTextDefault", [](std::vector<Value> args) -> Value {
        if (args.size() < 7) return Value::makeNull();
        auto w = GraphicsWrapper::GetWindowFromID((int)args[0].intVal);
        if (w) {
            std::string text = args[1].toString();
            float x = (float)args[2].toDouble();
            float y = (float)args[3].toDouble();
            float r = (float)args[4].toDouble();
            float g = (float)args[5].toDouble();
            float b = (float)args[6].toDouble();
            float a = (args.size() >= 8) ? (float)args[7].toDouble() : 1.0f;
            float fontSize = (args.size() >= 9) ? (float)args[8].toDouble() : 18.0f;
            w->DrawTextDefault(text, x, y, r, g, b, a, fontSize);
        }
        return Value::makeNull();
    });

    // Shader functions
    graphicsObj->fields["createShader"] = mkFn("createShader", [](std::vector<Value> args) {
        if (args.size() < 2) return Value::makeInt(0);
        auto shader = Ume::Graphics::Shader::Create(args[0].toString(), args[1].toString());
        return Value::makeInt(shader ? GraphicsWrapper::RegisterShader(shader) : 0);
    });

    graphicsObj->fields["shaderUse"] = mkFn("shaderUse", [](std::vector<Value> args) -> Value {
        if (auto s = GraphicsWrapper::GetShaderFromID((int)args[0].intVal)) s->Use();
        return Value::makeNull();
    });
    
    graphicsObj->fields["shaderSetFloat"] = mkFn("shaderSetFloat", [](std::vector<Value> args) -> Value {
        if (auto s = GraphicsWrapper::GetShaderFromID((int)args[0].intVal)) s->SetFloat(args[1].toString(), (float)args[2].toDouble());
        return Value::makeNull();
    });
    
    graphicsObj->fields["shaderSetInt"] = mkFn("shaderSetInt", [](std::vector<Value> args) -> Value {
        if (auto s = GraphicsWrapper::GetShaderFromID((int)args[0].intVal)) s->SetInt(args[1].toString(), (int)args[2].intVal);
        return Value::makeNull();
    });
    
    graphicsObj->fields["shaderSetVec3"] = mkFn("shaderSetVec3", [](std::vector<Value> args) -> Value {
        if (auto s = GraphicsWrapper::GetShaderFromID((int)args[0].intVal)) s->SetVec3(args[1].toString(), (float)args[2].toDouble(), (float)args[3].toDouble(), (float)args[4].toDouble());
        return Value::makeNull();
    });
    
    graphicsObj->fields["shaderSetVec4"] = mkFn("shaderSetVec4", [](std::vector<Value> args) -> Value {
        if (auto s = GraphicsWrapper::GetShaderFromID((int)args[0].intVal)) s->SetVec4(args[1].toString(), (float)args[2].toDouble(), (float)args[3].toDouble(), (float)args[4].toDouble(), (float)args[5].toDouble());
        return Value::makeNull();
    });

    graphicsObj->fields["shaderSetMat4"] = mkFn("shaderSetMat4", [](std::vector<Value> args) -> Value {
        if (args.size() < 3) return Value::makeNull();
        auto s = GraphicsWrapper::GetShaderFromID((int)args[0].intVal);
        if (!s) return Value::makeNull();
        float mat[16];
        if (args[2].isArray()) {
            auto& elems = args[2].arrVal->elements;
            for (int i = 0; i < 16 && i < (int)elems.size(); i++)
                mat[i] = (float)elems[i].toDouble();
        }
        s->SetMat4(args[1].toString(), mat);
        return Value::makeNull();
    });

    graphicsObj->fields["shaderSetTexture"] = mkFn("shaderSetTexture", [](std::vector<Value> args) -> Value {
        if (args.size() < 3) return Value::makeNull();
        auto s = GraphicsWrapper::GetShaderFromID((int)args[0].intVal);
        auto t = GraphicsWrapper::GetTextureFromID((int)args[2].intVal);
        int slot = (args.size() >= 4) ? (int)args[3].intVal : 0;
        if (s && t) s->SetTexture(args[1].toString(), t, slot);
        return Value::makeNull();
    });

    graphicsObj->fields["meshDraw"] = mkFn("meshDraw", [](std::vector<Value> args) -> Value {
        if (auto m = GraphicsWrapper::GetMeshFromID((int)args[0].intVal)) m->Draw();
        return Value::makeNull();
    });
    
    graphicsObj->fields["windowDrawMesh"] = mkFn("windowDrawMesh", [](std::vector<Value> args) -> Value {
        if (auto w = GraphicsWrapper::GetWindowFromID((int)args[0].intVal)) {
            w->DrawMesh(GraphicsWrapper::GetMeshFromID((int)args[1].intVal), GraphicsWrapper::GetShaderFromID((int)args[2].intVal));
        }
        return Value::makeNull();
    });

    // Keys and MouseButtons objects
    auto keysObj = std::make_shared<ObjectInstance>();
    keysObj->className = "Keys";
    keysObj->fields["A"] = Value::makeInt((int)Ume::Graphics::Key::A);
    keysObj->fields["B"] = Value::makeInt((int)Ume::Graphics::Key::B);
    keysObj->fields["C"] = Value::makeInt((int)Ume::Graphics::Key::C);
    keysObj->fields["D"] = Value::makeInt((int)Ume::Graphics::Key::D);
    keysObj->fields["E"] = Value::makeInt((int)Ume::Graphics::Key::E);
    keysObj->fields["F"] = Value::makeInt((int)Ume::Graphics::Key::F);
    keysObj->fields["G"] = Value::makeInt((int)Ume::Graphics::Key::G);
    keysObj->fields["H"] = Value::makeInt((int)Ume::Graphics::Key::H);
    keysObj->fields["I"] = Value::makeInt((int)Ume::Graphics::Key::I);
    keysObj->fields["J"] = Value::makeInt((int)Ume::Graphics::Key::J);
    keysObj->fields["K"] = Value::makeInt((int)Ume::Graphics::Key::K);
    keysObj->fields["L"] = Value::makeInt((int)Ume::Graphics::Key::L);
    keysObj->fields["M"] = Value::makeInt((int)Ume::Graphics::Key::M);
    keysObj->fields["N"] = Value::makeInt((int)Ume::Graphics::Key::N);
    keysObj->fields["O"] = Value::makeInt((int)Ume::Graphics::Key::O);
    keysObj->fields["P"] = Value::makeInt((int)Ume::Graphics::Key::P);
    keysObj->fields["Q"] = Value::makeInt((int)Ume::Graphics::Key::Q);
    keysObj->fields["R"] = Value::makeInt((int)Ume::Graphics::Key::R);
    keysObj->fields["S"] = Value::makeInt((int)Ume::Graphics::Key::S);
    keysObj->fields["T"] = Value::makeInt((int)Ume::Graphics::Key::T);
    keysObj->fields["U"] = Value::makeInt((int)Ume::Graphics::Key::U);
    keysObj->fields["V"] = Value::makeInt((int)Ume::Graphics::Key::V);
    keysObj->fields["W"] = Value::makeInt((int)Ume::Graphics::Key::W);
    keysObj->fields["X"] = Value::makeInt((int)Ume::Graphics::Key::X);
    keysObj->fields["Y"] = Value::makeInt((int)Ume::Graphics::Key::Y);
    keysObj->fields["Z"] = Value::makeInt((int)Ume::Graphics::Key::Z);

    keysObj->fields["Key0"] = Value::makeInt((int)Ume::Graphics::Key::Key0);
    keysObj->fields["Key1"] = Value::makeInt((int)Ume::Graphics::Key::Key1);
    keysObj->fields["Key2"] = Value::makeInt((int)Ume::Graphics::Key::Key2);
    keysObj->fields["Key3"] = Value::makeInt((int)Ume::Graphics::Key::Key3);
    keysObj->fields["Key4"] = Value::makeInt((int)Ume::Graphics::Key::Key4);
    keysObj->fields["Key5"] = Value::makeInt((int)Ume::Graphics::Key::Key5);
    keysObj->fields["Key6"] = Value::makeInt((int)Ume::Graphics::Key::Key6);
    keysObj->fields["Key7"] = Value::makeInt((int)Ume::Graphics::Key::Key7);
    keysObj->fields["Key8"] = Value::makeInt((int)Ume::Graphics::Key::Key8);
    keysObj->fields["Key9"] = Value::makeInt((int)Ume::Graphics::Key::Key9);

    keysObj->fields["Space"] = Value::makeInt((int)Ume::Graphics::Key::Space);
    keysObj->fields["Escape"] = Value::makeInt((int)Ume::Graphics::Key::Escape);
    keysObj->fields["Tab"] = Value::makeInt((int)Ume::Graphics::Key::Tab);
    keysObj->fields["LeftShift"] = Value::makeInt((int)Ume::Graphics::Key::LeftShift);
    keysObj->fields["RightShift"] = Value::makeInt((int)Ume::Graphics::Key::RightShift);
    keysObj->fields["Left"] = Value::makeInt((int)Ume::Graphics::Key::Left);
    keysObj->fields["Right"] = Value::makeInt((int)Ume::Graphics::Key::Right);
    keysObj->fields["Up"] = Value::makeInt((int)Ume::Graphics::Key::Up);
    keysObj->fields["Down"] = Value::makeInt((int)Ume::Graphics::Key::Down);

    auto mouseObj = std::make_shared<ObjectInstance>();
    mouseObj->className = "MouseButtons";
    mouseObj->fields["Left"] = Value::makeInt((int)Ume::Graphics::MouseButton::Left);
    mouseObj->fields["Right"] = Value::makeInt((int)Ume::Graphics::MouseButton::Right);
    mouseObj->fields["Middle"] = Value::makeInt((int)Ume::Graphics::MouseButton::Middle);

    graphicsObj->fields["Keys"] = Value::makeObject(keysObj);
    graphicsObj->fields["MouseButtons"] = Value::makeObject(mouseObj);

    env->declare("Graphics", Value::makeObject(graphicsObj));
}

} // namespace Ume
