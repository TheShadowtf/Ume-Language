// codegen.cpp — C++ Transpiler for the Ume Language
#include "../include/codegen.h"
#include "../include/lexer.h"
#include <sstream>
#include <stdexcept>
#include <cassert>
#include <unordered_set>
#include <cctype>
#include <functional>

namespace Ume {

// ─────────────────────────────────────────────────────────────
// Runtime header embedded as a string (prepended to all output)
// ─────────────────────────────────────────────────────────────
static const char* kRuntimeHeader = R"CPP(
// ── Ume Runtime v2.1 (2025-01-31) ─────────────────────────
#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <optional>
#include <memory>
#include <functional>
#include <any>
#include <stdexcept>
#include <cstdint>
#include <cstring>
#include <sstream>
#include <cmath>
#include <cctype>
#include <algorithm>
#include <chrono>
#include <fstream>
#include <filesystem>
#include "json.hpp"
namespace _ume_rt {

// ── Primitive type aliases ───────────────────────────────────
using Int    = int32_t;
using Long   = int64_t;
using Short  = int16_t;
using Byte   = uint8_t;
using Float  = float;
using Double = double;
using Bool   = bool;
using Char   = char;

// ── String class with Ume methods ───────────────────────────
struct UmeString : std::string {
    using std::string::string;
    UmeString() = default;
    UmeString(const std::string& s) : std::string(s) {}
    UmeString(std::string&& s) : std::string(std::move(s)) {}

    // Allow s->method() on value types (mirrors Ume's dot-access syntax)
    UmeString* operator->()             { return this; }
    const UmeString* operator->() const { return this; }

    Int length() const { return (Int)std::string::size(); }
    bool isEmpty() const { return std::string::empty(); }

    UmeString toUpperCase() const {
        UmeString r = *this;
        for (auto& c : r) c = (char)std::toupper((unsigned char)c);
        return r;
    }
    UmeString toLowerCase() const {
        UmeString r = *this;
        for (auto& c : r) c = (char)std::tolower((unsigned char)c);
        return r;
    }
    bool startsWith(const UmeString& pre) const {
        return std::string::rfind(pre, 0) == 0;
    }
    bool endsWith(const UmeString& suf) const {
        return std::string::size() >= suf.size() &&
               std::string::compare(std::string::size() - suf.size(), suf.size(), suf) == 0;
    }
    bool contains(const UmeString& sub) const {
        return std::string::find(sub) != std::string::npos;
    }
    Int indexOf(const UmeString& sub) const {
        auto p = std::string::find(sub);
        return p == std::string::npos ? -1 : (Int)p;
    }
    UmeString substring(Int start, Int end) const {
        return UmeString(std::string::substr((size_t)start, (size_t)(end - start)));
    }
    UmeString replace(const UmeString& from, const UmeString& to) const {
        UmeString r = *this;
        size_t p = 0;
        while ((p = r.std::string::find(from, p)) != std::string::npos) {
            r.std::string::replace(p, from.size(), to);
            p += to.size();
        }
        return r;
    }
    UmeString trim() const {
        size_t i = 0, j = std::string::size();
        while (i < j && std::isspace((unsigned char)std::string::operator[](i))) ++i;
        while (j > i && std::isspace((unsigned char)std::string::operator[](j-1))) --j;
        return UmeString(std::string::substr(i, j - i));
    }
    std::vector<UmeString> split(const UmeString& delim) const {
        std::vector<UmeString> res;
        size_t p = 0, f;
        while ((f = std::string::find(delim, p)) != std::string::npos) {
            res.push_back(UmeString(std::string::substr(p, f - p)));
            p = f + delim.size();
        }
        res.push_back(UmeString(std::string::substr(p)));
        return res;
    }
    UmeString charAt(Int i) const { return UmeString(1, std::string::operator[]((size_t)i)); }
    Int toInt() const { return (Int)std::stoi(*this); }
    Double toDouble() const { return std::stod(*this); }
    UmeString toStr() const { return *this; }

    // ── operator+ as MEMBER functions ──────────────────────────
    // Member functions are found via class member lookup FIRST and have
    // priority over std::string's free-function operator+ (which would
    // require a derived-to-base conversion). This eliminates the
    // C2666 "overloaded functions have similar conversions" ambiguity
    // on MSVC.
    UmeString operator+(const UmeString& b) const {
        return UmeString(static_cast<const std::string&>(*this) + static_cast<const std::string&>(b));
    }
    // std::string overload — exact match, no conversion needed (UmeString IS-A std::string).
    // This resolves `UmeString + std::to_string(x)` which was ambiguous on MSVC.
    UmeString operator+(const std::string& b) const {
        return UmeString(static_cast<const std::string&>(*this) + b);
    }
    UmeString operator+(const char* b) const {
        return UmeString(static_cast<const std::string&>(*this) + b);
    }
    UmeString operator+(char b) const {
        return UmeString(static_cast<const std::string&>(*this) + b);
    }
    // Template for all other arithmetic types (int, long, float, double, bool)
    template<typename T, typename = std::enable_if_t<std::is_arithmetic_v<T> && !std::is_same_v<std::decay_t<T>, char>>>
    UmeString operator+(T b) const {
        std::ostringstream o; o << b;
        return UmeString(static_cast<const std::string&>(*this) + o.str());
    }
};

using String = UmeString;

// Free-function operator+ only for LHS that are NOT UmeString (const char*, primitives)
inline UmeString operator+(const char* a, const UmeString& b) {
    return UmeString(std::string(a) + static_cast<const std::string&>(b));
}
template<typename T, typename = std::enable_if_t<std::is_arithmetic_v<T> && !std::is_same_v<std::decay_t<T>, char>>>
inline UmeString operator+(T a, const UmeString& b) {
    std::ostringstream o; o << a;
    return UmeString(o.str() + static_cast<const std::string&>(b));
}

// ── Custom Any type ─────────────────────────────────────────
struct Any {
    std::any value_;
    Any() = default;
    Any(std::nullptr_t) {}
    template<typename T, typename = std::enable_if_t<!std::is_same_v<std::decay_t<T>, Any>>>
    Any(T&& v) : value_(std::forward<T>(v)) {}

    String toString() const {
        if (!value_.has_value()) return String("null");
        if (auto* p = std::any_cast<int32_t>(&value_))  return String(std::to_string(*p));
        if (auto* p = std::any_cast<int64_t>(&value_))  return String(std::to_string(*p));
        if (auto* p = std::any_cast<int>(&value_))      return String(std::to_string(*p));
        if (auto* p = std::any_cast<double>(&value_))   { std::ostringstream o; o<<*p; return String(o.str()); }
        if (auto* p = std::any_cast<float>(&value_))    { std::ostringstream o; o<<*p; return String(o.str()); }
        if (auto* p = std::any_cast<bool>(&value_))     return String(*p ? "true" : "false");
        if (auto* p = std::any_cast<char>(&value_))     return String(1, *p);
        if (auto* p = std::any_cast<String>(&value_))   return *p;
        if (auto* p = std::any_cast<std::string>(&value_)) return String(*p);
        return String("<any>");
    }

    template<typename T>
    bool operator==(const T& o) const {
        if (auto* p = std::any_cast<T>(&value_)) return *p == o;
        if constexpr (std::is_integral_v<T> && !std::is_same_v<T,bool>) {
            if (auto* p = std::any_cast<int32_t>(&value_)) return (T)*p == o;
            if (auto* p = std::any_cast<int64_t>(&value_)) return (T)*p == o;
            if (auto* p = std::any_cast<int>(&value_))     return (T)*p == o;
        }
        if constexpr (std::is_floating_point_v<T>) {
            if (auto* p = std::any_cast<double>(&value_)) return (T)*p == o;
            if (auto* p = std::any_cast<float>(&value_))  return (T)*p == o;
        }
        return false;
    }
    bool operator==(const Any& o) const { return toString() == o.toString(); }
    bool operator==(std::nullptr_t) const { return !value_.has_value(); }
    template<typename T>
    friend bool operator==(const T& a, const Any& b) { return b == a; }

    bool operator!=(const Any& o)    const { return !(*this == o); }
    bool operator!=(std::nullptr_t)  const { return value_.has_value(); }
    template<typename T>
    bool operator!=(const T& o) const { return !(*this == o); }
};
inline std::ostream& operator<<(std::ostream& os, const Any& a) { return os << a.toString(); }

// operator+ for UmeString + Any and Any + UmeString (convert Any to string)
// These are free functions because Any is defined after UmeString.
// We explicitly cast toString() result to const std::string& to avoid ambiguity
// between UmeString's member operator+ and std::string's free-function operator+.
inline UmeString operator+(const UmeString& a, const Any& b) {
    return UmeString(static_cast<const std::string&>(a) + static_cast<const std::string&>(b.toString()));
}
inline UmeString operator+(const Any& a, const UmeString& b) {
    return UmeString(static_cast<const std::string&>(a.toString()) + static_cast<const std::string&>(b));
}

// ── Null helper – compares with both optional<T> and shared_ptr<T> ─
struct _NullVal {
    template<typename T>
    operator std::shared_ptr<T>() const { return nullptr; }
    template<typename T>
    operator std::optional<T>() const { return std::nullopt; }
    template<typename T>
    operator std::vector<T>() const { return std::vector<T>(); }
    template<typename T>
    bool operator==(const std::optional<T>& o) const { return !o.has_value(); }
    template<typename T>
    friend bool operator==(const std::optional<T>& o, _NullVal) { return !o.has_value(); }
    template<typename T>
    bool operator!=(const std::optional<T>& o) const { return o.has_value(); }
    template<typename T>
    friend bool operator!=(const std::optional<T>& o, _NullVal) { return o.has_value(); }
    template<typename T>
    bool operator==(const std::shared_ptr<T>& p) const { return p == nullptr; }
    template<typename T>
    friend bool operator==(const std::shared_ptr<T>& p, _NullVal) { return p == nullptr; }
    template<typename T>
    bool operator!=(const std::shared_ptr<T>& p) const { return p != nullptr; }
    template<typename T>
    friend bool operator!=(const std::shared_ptr<T>& p, _NullVal) { return p != nullptr; }
    template<typename T>
    bool operator==(const std::vector<T>& v) const { return v.empty(); }
    template<typename T>
    friend bool operator==(const std::vector<T>& v, _NullVal) { return v.empty(); }
    template<typename T>
    bool operator!=(const std::vector<T>& v) const { return !v.empty(); }
    template<typename T>
    friend bool operator!=(const std::vector<T>& v, _NullVal) { return !v.empty(); }
    bool operator==(const String& s) const { return false; }
    friend bool operator==(const String& s, _NullVal) { return false; }
    bool operator!=(const String& s) const { return true; }
    friend bool operator!=(const String& s, _NullVal) { return true; }
    // Raw pointer comparisons (for unsafe alloc<T>(N) results)
    template<typename T>
    bool operator==(T* p) const { return p == nullptr; }
    template<typename T>
    friend bool operator==(T* p, _NullVal) { return p == nullptr; }
    template<typename T>
    bool operator!=(T* p) const { return p != nullptr; }
    template<typename T>
    friend bool operator!=(T* p, _NullVal) { return p != nullptr; }
    // Any comparisons
    bool operator==(const Any& a) const { return !a.value_.has_value(); }
    friend bool operator==(const Any& a, _NullVal) { return !a.value_.has_value(); }
    bool operator!=(const Any& a) const { return a.value_.has_value(); }
    friend bool operator!=(const Any& a, _NullVal) { return a.value_.has_value(); }
    bool operator==(std::nullptr_t) const { return true; }
    bool operator!=(std::nullptr_t) const { return false; }
};
static constexpr _NullVal _ume_null{};

// ── This helper – enables returning/passing raw `this` as shared_ptr ─
template<typename T>
struct _ume_this_helper {
    T* ptr;
    _ume_this_helper(T* p) : ptr(p) {}

    template<typename Target>
    operator std::shared_ptr<Target>() const {
        if constexpr (std::is_base_of_v<std::enable_shared_from_this<T>, T>) {
            try {
                if constexpr (std::is_same_v<T, Target>) {
                    return ptr->shared_from_this();
                } else {
                    return std::static_pointer_cast<Target>(ptr->shared_from_this());
                }
            } catch (...) {
                return std::shared_ptr<Target>(static_cast<Target*>(ptr), [](Target*){});
            }
        } else {
            return std::shared_ptr<Target>(static_cast<Target*>(ptr), [](Target*){});
        }
    }

    T* operator->() const { return ptr; }
    T& operator*() const { return *ptr; }

    template<typename U>
    bool operator==(const std::shared_ptr<U>& o) const { return ptr == o.get(); }
    template<typename U>
    bool operator!=(const std::shared_ptr<U>& o) const { return ptr != o.get(); }
    bool operator==(_NullVal) const { return ptr == nullptr; }
    bool operator!=(_NullVal) const { return ptr != nullptr; }
    bool operator==(T* o) const { return ptr == o; }
    bool operator!=(T* o) const { return ptr != o; }
};

template<typename T>
inline _ume_this_helper<T> _ume_this(T* p) { return _ume_this_helper<T>(p); }

// ── For-each & Index iterator helper ───────────────────────
template<typename T> struct _ume_is_shared_ptr : std::false_type {};
template<typename T> struct _ume_is_shared_ptr<std::shared_ptr<T>> : std::true_type {};

template<typename T>
inline auto& _ume_iter(T& c) {
    using DecayedT = std::decay_t<T>;
    if constexpr (_ume_is_shared_ptr<DecayedT>::value) {
        return *c;
    } else {
        return c;
    }
}

// Overload for const references — strips the const to allow mutation 
// (Ume does not have const-correctness, so arrays passed to functions should be mutable)
template<typename T>
inline auto& _ume_iter(const T& c) {
    using DecayedT = std::decay_t<T>;
    if constexpr (_ume_is_shared_ptr<DecayedT>::value) {
        return *const_cast<DecayedT&>(c);
    } else {
        return const_cast<DecayedT&>(c);
    }
}

// ── Length helper ───────────────────────────────────────────
template<typename T> inline Int _ume_length(const std::vector<T>& v)          { return (Int)v.size(); }
inline Int _ume_length(const String& s)                                        { return (Int)s.size(); }
template<typename T> inline Int _ume_length(const std::shared_ptr<T>& p)      { return _ume_length(*p); }

// ── Lvalue reference helper ──────────────────────────────────
template<typename T> inline T& _ume_lref(T& v) { return v; }
template<typename T> inline const T& _ume_lref(const T& v) { return v; }
template<typename T> inline T _ume_lref(T&& v) { return std::move(v); }

// ── Vector construction helper ──────────────────────────────
// Used by `new Type([...])` to pass an array literal as a std::vector argument.
// MSVC cannot parse `std::vector<T>({...})` (constructor call with braced-init-list
// in parens — that's a GCC extension). But it CAN parse a function call with a
// braced-init-list argument. So we use `_ume_vec<T>({...})` instead.
template<typename T>
inline std::vector<T> _ume_make_vec(size_t n) { return std::vector<T>(n); }
template<typename T>
inline std::vector<T> _ume_make_vec(size_t n, const T& val) { return std::vector<T>(n, val); }

template<typename T>
struct _ume_vec {
    std::vector<T> data;
    _ume_vec(std::initializer_list<T> il) : data(il) {}
    _ume_vec(size_t size) : data(size) {}
    operator std::vector<T>&() { return data; }
    operator const std::vector<T>&() const { return data; }
    operator std::vector<T>() && { return std::move(data); }
};

// ── Exceptions ───────────────────────────────────────────────
struct Exception : std::exception {
    String message_;
    Exception() = default;
    explicit Exception(const String& msg) : message_(msg) {}
    const char* what() const noexcept override { return message_.c_str(); }
    virtual String getMessage() const { return message_; }
};
struct DivisionByZeroException : Exception {
    explicit DivisionByZeroException(const String& m="Division by zero") : Exception(m) {}
};
struct NullPointerException : Exception {
    explicit NullPointerException(const String& m="Null pointer dereference") : Exception(m) {}
};
struct IndexOutOfBoundsException : Exception {
    explicit IndexOutOfBoundsException(const String& m) : Exception(m) {}
};

// ── Console ──────────────────────────────────────────────────
struct Console {
    static void println(const String& s) { std::cout << s << '\n'; }
    static void println(Int v)    { std::cout << v << '\n'; }
    static void println(Long v)   { std::cout << v << '\n'; }
    static void println(Double v) { std::cout << v << '\n'; }
    static void println(Float v)  { std::cout << v << '\n'; }
    static void println(Bool v)   { std::cout << (v ? "true" : "false") << '\n'; }
    static void println(Char v)   { std::cout << v << '\n'; }
    static void println(const Any& a) { std::cout << a.toString() << '\n'; }
    static void println()         { std::cout << '\n'; }
    static void print(const String& s) { std::cout << s; }
    static void print(const Any& a)    { std::cout << a.toString(); }
    static String readLine() { String l; std::getline(std::cin, l); return l; }
    static String readLine(const String& prompt) { std::cout << prompt; return readLine(); }
};

// ── List<T> ──────────────────────────────────────────────────
template<typename T>
struct List {
    std::vector<T> _data;
    void add(const T& v) { _data.push_back(v); }
    T& get(Int i) {
        if (i < 0 || i >= (Int)_data.size())
            throw IndexOutOfBoundsException(String("Index ") + String(std::to_string(i)) + " out of bounds");
        return _data[i];
    }
    Int size() const { return (Int)_data.size(); }
    bool contains(const T& v) const { for (auto& x : _data) if (x==v) return true; return false; }
    void remove(Int i) { _data.erase(_data.begin()+i); }
    void clear() { _data.clear(); }
    bool isEmpty() const { return _data.empty(); }
    typename std::vector<T>::iterator begin() { return _data.begin(); }
    typename std::vector<T>::iterator end()   { return _data.end(); }
    void forEach(std::function<void(T)> fn) { for (auto& x : _data) fn(x); }
    std::shared_ptr<List<T>> filter(std::function<bool(T)> pred) {
        auto r = std::make_shared<List<T>>();
        for (auto& x : _data) if (pred(x)) r->add(x);
        return r;
    }
    template<typename R>
    std::shared_ptr<List<R>> map(std::function<R(T)> fn) {
        auto r = std::make_shared<List<R>>();
        for (auto& x : _data) r->add(fn(x));
        return r;
    }
    T& operator[](Int i) { return get(i); }
    const T& operator[](Int i) const { return _data[i]; }
    std::vector<T> toArray() const { return _data; }
    void addAll(const std::vector<T>& v) { _data.insert(_data.end(), v.begin(), v.end()); }
    void addAll(const List<T>& l) { _data.insert(_data.end(), l._data.begin(), l._data.end()); }
    void addAll(const std::shared_ptr<List<T>>& l) { if (l) _data.insert(_data.end(), l->_data.begin(), l->_data.end()); }
    template<typename R>
    R reduce(R init, std::function<R(R,T)> fn) {
        R acc = init; for (auto& x : _data) acc = fn(acc,x); return acc;
    }
};

// ── Map<K,V> ─────────────────────────────────────────────────
template<typename K, typename V>
struct Map {
    std::unordered_map<K,V> _data;
    void put(const K& k, const V& v) { _data[k] = v; }
    V& get(const K& k) { return _data.at(k); }
    bool containsKey(const K& k) const { return _data.count(k) > 0; }
    void remove(const K& k) { _data.erase(k); }
    Int size() const { return (Int)_data.size(); }
    bool isEmpty() const { return _data.empty(); }
};

// ── Set<T> ───────────────────────────────────────────────────
template<typename T>
struct Set {
    std::unordered_set<T> _data;
    void add(const T& v) { _data.insert(v); }
    bool contains(const T& v) const { return _data.count(v)>0; }
    void remove(const T& v) { _data.erase(v); }
    Int size() const { return (Int)_data.size(); }
    bool isEmpty() const { return _data.empty(); }
};

// ── Stack<T> ─────────────────────────────────────────────────
template<typename T>
struct Stack {
    std::vector<T> _data;
    void push(const T& v) { _data.push_back(v); }
    T pop() {
        if (_data.empty()) throw IndexOutOfBoundsException(String("Stack underflow"));
        T v = _data.back(); _data.pop_back(); return v;
    }
    T& peek() {
        if (_data.empty()) throw IndexOutOfBoundsException(String("Stack empty"));
        return _data.back();
    }
    bool isEmpty() const { return _data.empty(); }
    Int size() const { return (Int)_data.size(); }
    void clear() { _data.clear(); }
};

// ── Queue<T> ─────────────────────────────────────────────────
template<typename T>
struct Queue {
    std::vector<T> _data;
    void enqueue(const T& v) { _data.push_back(v); }
    T dequeue() {
        if (_data.empty()) throw IndexOutOfBoundsException(String("Queue underflow"));
        T v = _data.front(); _data.erase(_data.begin()); return v;
    }
    T& front() {
        if (_data.empty()) throw IndexOutOfBoundsException(String("Queue empty"));
        return _data.front();
    }
    bool isEmpty() const { return _data.empty(); }
    Int size() const { return (Int)_data.size(); }
    void clear() { _data.clear(); }
};

// ── Math ─────────────────────────────────────────────────────
struct Math {
    static constexpr Double PI  = 3.14159265358979323846;
    static constexpr Double E   = 2.71828182845904523536;
    static constexpr Double TAU = 6.28318530717958647692;

    static Double sqrt(Double x)             { return std::sqrt(x); }
    static Double pow(Double b, Double e)    { return std::pow(b, e); }
    static Double abs(Double x)              { return std::abs(x); }
    static Int    abs(Int x)                 { return std::abs(x); }
    static Double floor(Double x)            { return std::floor(x); }
    static Double ceil(Double x)             { return std::ceil(x); }
    static Double round(Double x)            { return std::round(x); }
    static Double trunc(Double x)            { return std::trunc(x); }
    static Double log(Double x)              { return std::log(x); }
    static Double log10(Double x)            { return std::log10(x); }
    static Double sin(Double x)              { return std::sin(x); }
    static Double cos(Double x)              { return std::cos(x); }
    static Double tan(Double x)              { return std::tan(x); }
    static Double atan2(Double y, Double x)  { return std::atan2(y, x); }
    static Double asin(Double x)             { return std::asin(x); }
    static Double acos(Double x)             { return std::acos(x); }
    static Double atan(Double x)             { return std::atan(x); }
    static Double cbrt(Double x)             { return std::cbrt(x); }
    static Double sign(Double x)             { return x > 0 ? 1.0 : (x < 0 ? -1.0 : 0.0); }
    static Double toRadians(Double deg)      { return deg * PI / 180.0; }
    static Double toDegrees(Double rad)      { return rad * 180.0 / PI; }
    static bool   isNaN(Double x)            { return std::isnan(x); }
    static bool   isInfinite(Double x)       { return std::isinf(x); }
    static Double lerp(Double a, Double b, Double t) { return a + t * (b - a); }
    static Double min(Double a, Double b)    { return a < b ? a : b; }
    static Int    min(Int a, Int b)          { return a < b ? a : b; }
    static Double max(Double a, Double b)    { return a > b ? a : b; }
    static Int    max(Int a, Int b)          { return a > b ? a : b; }
    static Double clamp(Double v, Double lo, Double hi) { return v < lo ? lo : (v > hi ? hi : v); }
    static Int    clamp(Int v, Int lo, Int hi)          { return v < lo ? lo : (v > hi ? hi : v); }
    static bool   isNaN(Int)                 { return false; }
    static bool   isNaN(Float x)             { return std::isnan(x); }
};

// ── StringUtils ──────────────────────────────────────────────
struct StringUtils {
    static String toString(Int v)    { return String(std::to_string(v)); }
    static String toString(Long v)   { return String(std::to_string(v)); }
    static String toString(Double v) { std::ostringstream o; o<<v; return String(o.str()); }
    static String toString(Float v)  { std::ostringstream o; o<<v; return String(o.str()); }
    static String toString(Bool v)   { return v ? String("true") : String("false"); }
    static String toString(Char v)   { return String(1,v); }
    static Int    parseInt(const String& s)    { return (Int)std::stoi(s); }
    static Double parseDouble(const String& s) { return std::stod(s); }
    static Bool   parseBool(const String& s)   {
        return s == "true" || s == "1" || s == "yes";
    }
    static bool isInt(const String& s) {
        if (s.empty()) return false;
        size_t i = (s[0]=='-'||s[0]=='+') ? 1 : 0;
        if (i == s.size()) return false;
        for (; i < s.size(); ++i) if (!std::isdigit((unsigned char)s[i])) return false;
        return true;
    }
    static bool isNumber(const String& s) {
        if (s.empty()) return false;
        try { (void)std::stod(s); return true; } catch (...) { return false; }
    }
    static String repeat(const String& s, Int n) {
        String r; for (Int i = 0; i < n; ++i) r += s; return r;
    }
    static String padLeft(const String& s, Int w, Char pad=' ') {
        if ((Int)s.size() >= w) return s;
        return String(w - s.size(), pad) + s;
    }
    static String padRight(const String& s, Int w, Char pad=' ') {
        if ((Int)s.size() >= w) return s;
        return s + String(w - s.size(), pad);
    }
    template<typename T>
    static String join(const std::shared_ptr<List<T>>& lst, const String& sep) {
        String r;
        for (Int i = 0; i < lst->size(); ++i) {
            if (i > 0) r += sep;
            std::ostringstream o; o << lst->get(i); r += o.str();
        }
        return r;
    }
    static String join(const std::shared_ptr<List<String>>& lst, const String& sep) {
        String r;
        for (Int i = 0; i < lst->size(); ++i) {
            if (i > 0) r += sep;
            r += lst->get(i);
        }
        return r;
    }
};

// ── System ───────────────────────────────────────────────────
struct System {
    static String platform() {
#if defined(_WIN32) || defined(_WIN64)
        return String("windows");
#elif defined(__linux__)
        return String("linux");
#elif defined(__APPLE__)
        return String("macos");
#else
        return String("unknown");
#endif
    }
    static String cwd() {
        try { return String(std::filesystem::current_path().string()); } catch (...) { return String("."); }
    }
    static Int currentTimeMillis() {
        auto now = std::chrono::system_clock::now().time_since_epoch();
        return (Int)std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    }
    static String getEnv(const String& key) {
        const char* v = std::getenv(key.c_str());
        return v ? String(v) : String("");
    }
    static void setEnv(const String&, const String&) {}
    static void exit(Int code) { std::exit(code); }
    static Int  nanoTime() { 
        auto now = std::chrono::high_resolution_clock::now().time_since_epoch();
        return (Int)std::chrono::duration_cast<std::chrono::nanoseconds>(now).count();
    }
};

// ── FileSystem ─────────────────────────────────────────────────────
struct FileSystem {
    static bool exists(const String& path) {
        return std::filesystem::exists(std::string(path));
    }
    static String readText(const String& path) {
        std::ifstream f{std::string(path)};
        if (!f) return String("");
        std::ostringstream o; o << f.rdbuf(); return String(o.str());
    }
    static void writeText(const String& path, const String& content) {
        std::ofstream f{std::string(path)}; f << content;
    }
    static void appendText(const String& path, const String& content) {
        std::ofstream f{std::string(path), std::ios::app}; f << content;
    }
    static bool remove(const String& path) {
        return std::filesystem::remove(std::string(path));
    }
};

} // namespace _ume_rt

namespace std {
    template<> struct hash<_ume_rt::UmeString> {
        size_t operator()(const _ume_rt::UmeString& s) const noexcept {
            return std::hash<std::string>{}(s);
        }
    };
}

#include "graphics_compiler_bindings.h"
#include "audio_compiler_bindings.h"
#include "thread_compiler_bindings.h"
using namespace _ume_rt;
// ───────────────────────────────────────────────────────────────
)CPP";

// ─────────────────────────────────────────────────────────────
// Constructor
// ─────────────────────────────────────────────────────────────
static std::string escapeCppKeyword(const std::string& name) {
    static const std::unordered_set<std::string> kCppKeywords = {
        "alignas", "alignof", "and", "and_eq", "asm", "atomic_cancel", "atomic_commit",
        "atomic_noexcept", "auto", "bitand", "bitor", "char", "char8_t", "char16_t",
        "char32_t", "concept", "consteval", "constexpr", "constinit", "const_cast",
        "co_await", "co_return", "co_yield", "decltype", "default", "delete",
        "dynamic_cast", "explicit", "export", "extern", "friend", "inline",
        "mutable", "namespace", "noexcept", "not", "not_eq", "operator", "or",
        "or_eq", "private", "protected", "public", "register", "reinterpret_cast",
        "requires", "signed", "sizeof", "static", "static_assert", "static_cast",
        "struct", "template", "thread_local", "typename", "union", "unsigned",
        "using", "virtual", "volatile", "wchar_t", "xor", "xor_eq"
    };
    if (kCppKeywords.count(name)) return name + "_";
    return name;
}

CodeGenerator::CodeGenerator() = default;

// ─────────────────────────────────────────────────────────────
// Indentation helpers
// ─────────────────────────────────────────────────────────────
void CodeGenerator::indent()  { indent_ += 4; }
void CodeGenerator::dedent()  { if (indent_ >= 4) indent_ -= 4; }
void CodeGenerator::emitIndent() { out_ << std::string(static_cast<size_t>(indent_), ' '); }
void CodeGenerator::emit(const std::string& text) {
    out_ << text;
    if (!text.empty()) {
        atStartOfLine_ = text.back() == '\n';
    }
}
void CodeGenerator::emitLine(const std::string& line) {
    emitIndent();
    out_ << line << '\n';
    atStartOfLine_ = true;
}

// ─────────────────────────────────────────────────────────────
// Type mapping
// ─────────────────────────────────────────────────────────────
std::string CodeGenerator::mapTypeName(const std::string& name) {
    if (name == "int")    return "Int";
    if (name == "long")   return "Long";
    if (name == "short")  return "Short";
    if (name == "byte")   return "Byte";
    if (name == "float")  return "Float";
    if (name == "double") return "Double";
    if (name == "bool")   return "Bool";
    if (name == "char")   return "Char";
    if (name == "string") return "String";
    if (name == "void")   return "void";
    if (name == "any")    return "Any";
    if (name == "var")    return "auto";
    // NOTE: Window, Shader, Mesh, Texture, Key, MouseButton, Time, Random, Matrix4x4, etc.
    // are all stdlib/user classes compiled from .ume source. They are NOT mapped to
    // native C++ types here — they get compiled as regular user-defined classes.
    return name; // user-defined type
}

std::string CodeGenerator::mapType(const TypeAnnotation& ta, bool asParam) {
    if (ta.isFunc) {
        std::string s = "std::function<";
        if (ta.funcParams.empty()) { s += "void()"; }
        else {
            s += mapType(ta.funcParams.back());
            s += "(";
            for (size_t i = 0; i + 1 < ta.funcParams.size(); i++) {
                if (i > 0) s += ",";
                s += mapType(ta.funcParams[i]);
            }
            s += ")";
        }
        s += ">";
        return s;
    }

    std::string base = mapTypeName(ta.name);

    // Generic args
    if (!ta.typeArgs.empty()) {
        base += "<";
        for (size_t i = 0; i < ta.typeArgs.size(); i++) {
            if (i > 0) base += ",";
            base += mapType(ta.typeArgs[i]);
        }
        base += ">";
    }

    // Helper: determine if a base type should be wrapped in std::shared_ptr.
    // User-defined classes become shared_ptr; primitives, enums, structs, and
    // already-managed types (std::shared_ptr<...>, std::vector<...>) are left as-is.
    static const std::vector<std::string> primitiveNames = {
        "Int","Long","Short","Byte","Float","Double","Bool","Char","String","void","auto","Any"
    };
    auto isPrimitive = [&](const std::string& b) -> bool {
        for (auto& p : primitiveNames) if (b == p) return true;
        return false;
    };
    auto isManagedType = [](const std::string& b) -> bool {
        return b.find("std::shared_ptr<") == 0
            || b.find("std::vector<") == 0
            || b.find("std::array<") == 0
            || b.find("std::optional<") == 0;
    };
    auto shouldWrap = [&](const std::string& b) -> bool {
        if (isPrimitive(b)) return false;
        if (enumNames_.count(b) || structNames_.count(b)) return false;
        if (isManagedType(b)) return false;
        if (b == "void" || b == "auto") return false;
        return true;
    };

    if (ta.isArray) {
        // Arrays of class types use shared_ptr elements (reference semantics).
        std::string elem = shouldWrap(base) ? ("std::shared_ptr<" + base + ">") : base;
        if (ta.arraySize > 0) {
            std::string res = "std::array<" + elem + "," + std::to_string(ta.arraySize) + ">";
            return asParam ? ("const " + res + "&") : res;
        }
        int rank = ta.arrayRank > 0 ? ta.arrayRank : 1;
        std::string res = elem;
        for (int i = 0; i < rank; i++) {
            res = "std::vector<" + res + ">";
        }
        return asParam ? ("const " + res + "&") : res;
    }

    // Nullable handling:
    //  - shared_ptr types are already nullable (nullptr)
    //  - value types (primitives, enums, structs) use std::optional
    if (ta.nullable) {
        if (shouldWrap(base)) {
            std::string res = "std::shared_ptr<" + base + ">";
            if (ta.isPointer) for (int i = 0; i < ta.pointerLevel; i++) res += "*";
            return res;
        }
        if (isManagedType(base)) {
            std::string res = base;
            if (ta.isPointer) for (int i = 0; i < ta.pointerLevel; i++) res += "*";
            return res;
        }
        if (base == "void" || base == "auto") return base;
        std::string res = "std::optional<" + base + ">";
        if (ta.isPointer) for (int i = 0; i < ta.pointerLevel; i++) res += "*";
        return res;
    }

    // Non-nullable
    if (shouldWrap(base)) {
        std::string res = "std::shared_ptr<" + base + ">";
        if (ta.isPointer) for (int i = 0; i < ta.pointerLevel; i++) res += "*";
        return res;
    }

    std::string res = base;
    if (ta.isPointer) for (int i = 0; i < ta.pointerLevel; i++) res += "*";
    return res;
}

std::string CodeGenerator::accessStr(AccessModifier am) {
    switch (am) {
    case AccessModifier::Public:    return "public";
    case AccessModifier::Private:   return "private";
    case AccessModifier::Protected: return "protected";
    case AccessModifier::Internal:  return "public"; // treat as public in C++
    default:                        return "public";
    }
}

// ─────────────────────────────────────────────────────────────
// Main entry
// ─────────────────────────────────────────────────────────────
std::string CodeGenerator::generate(const Program& program) {
    return generate(program, "");
}

std::string CodeGenerator::generate(const Program& program, const std::string& sourceFilename) {
    out_.str(""); out_.clear();
    sourceFilename_ = sourceFilename;
    currentSourceLine_ = -1;
    currentSourceFilename_.clear();
    genProgram(program);
    return out_.str();
}

void CodeGenerator::emitLineDirective(const ASTNode& node) {
    if (node.line <= 0 || sourceFilename_.empty()) return;
    std::string filename = node.filename.empty() ? sourceFilename_ : node.filename;
    if (node.line == currentSourceLine_ && filename == currentSourceFilename_) return;
    currentSourceLine_ = node.line;
    currentSourceFilename_ = filename;
    if (!atStartOfLine_) {
        out_ << '\n';
    }
    out_ << "#line " << node.line << " \"" << escapeCppString(filename) << "\"\n";
    atStartOfLine_ = true;
}

void CodeGenerator::genProgram(const Program& prog) {
    out_ << kRuntimeHeader;
    // Collect enum, struct, and class names first so mapType and genMemberAccess can identify types
        enumNames_.clear();
    structNames_.clear();
    classNames_.clear();
    // 1. Collect ALL type names first
    for (auto& node : prog.declarations) {
        if (auto* enm = dynamic_cast<const EnumDecl*>(node.get())) enumNames_.insert(enm->name);
        if (auto* str = dynamic_cast<const StructDecl*>(node.get())) structNames_.insert(str->name);
        if (auto* cls = dynamic_cast<const ClassDecl*>(node.get())) classNames_.insert(cls->name);
        if (auto* ifc = dynamic_cast<const InterfaceDecl*>(node.get())) classNames_.insert(ifc->name);
    }

    static const std::unordered_set<std::string> kBuiltinRuntimeClasses = {
        "Console", "Math", "System", "String", "FileSystem",
        "List", "Map", "Set", "Stack", "Queue",
        "Thread", "Mutex", "ConditionVariable", "AtomicInt", "Task",
        "AudioEngine", "Sound", "Graphics"
    };

    // 2. Forward-declare ALL classes, structs, and interfaces
    for (auto& node : prog.declarations) {
        if (auto* cls = dynamic_cast<const ClassDecl*>(node.get())) {
            if (kBuiltinRuntimeClasses.count(cls->name)) continue;
            if (!cls->typeParams.empty()) {
                emitLine("template<" + genTypeParamList(cls->typeParams) + "> struct " + cls->name + ";");
            } else {
                emitLine("struct " + cls->name + ";");
            }
        } else if (auto* str = dynamic_cast<const StructDecl*>(node.get())) {
            emitLine("struct " + str->name + ";");
        } else if (auto* ifc = dynamic_cast<const InterfaceDecl*>(node.get())) {
            emitLine("struct " + ifc->name + ";");
        }
    }
    emitLine();

    // 3. Forward-declare ALL global functions
    for (auto& node : prog.declarations) {
        if (auto* fn = dynamic_cast<const FuncDecl*>(node.get())) {
            if (fn->name == "main" && fn->access == AccessModifier::Default) continue;
            if (!fn->typeParams.empty()) emitLine("template<" + genTypeParamList(fn->typeParams) + ">");
            emitIndent();
            emit("static " + mapType(fn->returnType) + " " + escapeCppKeyword(fn->name) + "(" + genParamList(fn->params) + ");");
            emitLine();
        }
    }
    emitLine();
    // Reorder declarations (Enums/Structs first, then Classes in topological dependency order)
    std::vector<const ASTNode*> enumsAndStructs;
    std::vector<const ASTNode*> globalVars; // NEW: Globals must come before classes!
    std::vector<const ASTNode*> classes;
    std::vector<const ASTNode*> others;

    for (auto& decl : prog.declarations) {
        if (dynamic_cast<const EnumDecl*>(decl.get()) || dynamic_cast<const StructDecl*>(decl.get())) {
            enumsAndStructs.push_back(decl.get());
        } else if (dynamic_cast<const VarDeclStmt*>(decl.get())) {
            globalVars.push_back(decl.get()); // Put global variables here
        } else if (dynamic_cast<const ClassDecl*>(decl.get())) {
            classes.push_back(decl.get());
        } else {
            others.push_back(decl.get());
        }
    }

    std::vector<const ASTNode*> sortedClasses;
    std::unordered_set<std::string> emittedClassNames;

    std::function<void(const ASTNode*, std::unordered_set<std::string>&)> findTypeReferences =
        [&](const ASTNode* node, std::unordered_set<std::string>& out) {
            if (!node) return;
            if (auto* id = dynamic_cast<const IdentifierExpr*>(node)) {
                if (classNames_.count(id->name)) out.insert(id->name);
            }
            if (auto* ma = dynamic_cast<const MemberAccessExpr*>(node)) {
                if (auto* id = dynamic_cast<const IdentifierExpr*>(ma->object.get())) {
                    if (classNames_.count(id->name)) out.insert(id->name);
                }
                findTypeReferences(ma->object.get(), out);
                return;
            }
            if (auto* b = dynamic_cast<const BinaryExpr*>(node)) { findTypeReferences(b->left.get(), out); findTypeReferences(b->right.get(), out); }
            if (auto* u = dynamic_cast<const UnaryExpr*>(node)) { findTypeReferences(u->operand.get(), out); }
            if (auto* a = dynamic_cast<const AssignExpr*>(node)) { findTypeReferences(a->target.get(), out); findTypeReferences(a->value.get(), out); }
            if (auto* c = dynamic_cast<const CallExpr*>(node)) { findTypeReferences(c->callee.get(), out); for (auto& arg : c->args) findTypeReferences(arg.get(), out); }
            if (auto* idx = dynamic_cast<const IndexExpr*>(node)) { findTypeReferences(idx->object.get(), out); findTypeReferences(idx->index.get(), out); }
            if (auto* n = dynamic_cast<const NewExpr*>(node)) { if (classNames_.count(n->type.name)) out.insert(n->type.name); for (auto& arg : n->args) findTypeReferences(arg.get(), out); }
            if (auto* blk = dynamic_cast<const BlockStmt*>(node)) { for (auto& s : blk->stmts) findTypeReferences(s.get(), out); }
            if (auto* es = dynamic_cast<const ExprStmt*>(node)) { findTypeReferences(es->expr.get(), out); }
            if (auto* ifStmt = dynamic_cast<const IfStmt*>(node)) { findTypeReferences(ifStmt->condition.get(), out); findTypeReferences(ifStmt->thenBranch.get(), out); findTypeReferences(ifStmt->elseBranch.get(), out); }
            if (auto* forStmt = dynamic_cast<const ForStmt*>(node)) { findTypeReferences(forStmt->init.get(), out); findTypeReferences(forStmt->condition.get(), out); findTypeReferences(forStmt->update.get(), out); findTypeReferences(forStmt->body.get(), out); }
            if (auto* ret = dynamic_cast<const ReturnStmt*>(node)) { findTypeReferences(ret->value.get(), out); }
            if (auto* vd = dynamic_cast<const VarDeclStmt*>(node)) { if (classNames_.count(vd->type.name)) out.insert(vd->type.name); findTypeReferences(vd->initializer.get(), out); }
        };

    while (!classes.empty()) {
        bool progress = false;
        for (auto it = classes.begin(); it != classes.end(); ) {
            auto* cls = dynamic_cast<const ClassDecl*>(*it);
            bool depsSatisfied = true;
            std::unordered_set<std::string> deps;
            if (cls->superClass) deps.insert(cls->superClass->name);
            for (auto& f : cls->fields) deps.insert(f->type.name);
            for (auto& m : cls->methods) {
                deps.insert(m->returnType.name);
                for (auto& p : m->params) deps.insert(p.type.name);
                if (m->body) findTypeReferences(m->body.get(), deps);
            }
            for (auto& c : cls->constructors) {
                for (auto& p : c->params) deps.insert(p.type.name);
                if (c->body) findTypeReferences(c->body.get(), deps);
            }
            for (auto& dName : deps) {
                if (classNames_.count(dName) && !emittedClassNames.count(dName) && !kBuiltinRuntimeClasses.count(dName) && dName != cls->name) {
                    depsSatisfied = false;
                    break;
                }
            }
            if (depsSatisfied) {
                sortedClasses.push_back(*it);
                emittedClassNames.insert(cls->name);
                it = classes.erase(it);
                progress = true;
            } else {
                ++it;
            }
        }
        if (!progress) {
            for (auto& c : classes) sortedClasses.push_back(c);
            break;
        }
    }

    std::vector<const ASTNode*> orderedDecls;
    for (auto& d : enumsAndStructs) orderedDecls.push_back(d);
    for (auto& d : globalVars) orderedDecls.push_back(d);   // Emit globals BEFORE classes!
    for (auto& d : sortedClasses) orderedDecls.push_back(d);
    for (auto& d : others) orderedDecls.push_back(d);

    for (auto* node : orderedDecls) {
        if (auto* cls = dynamic_cast<const ClassDecl*>(node)) {
            if (kBuiltinRuntimeClasses.count(cls->name)) continue;
        }
        genNode(*node);
    }

    // Emit C entry point
    // 1. Find a class named "Main" with a static main() method
    bool emittedMain = false;
    for (auto& node : prog.declarations) {
        if (auto* cls = dynamic_cast<const ClassDecl*>(node.get())) {
            if (cls->name == "Main") {
                for (auto& m : cls->methods) {
                    if (m->name == "main" && m->isStatic) {
                        emitLine("\nint main(int argc, char** argv) {");
                        emitLine("    Main::main();");
                        emitLine("    return 0;");
                        emitLine("}");
                        emittedMain = true;
                        break;
                    }
                }
            }
        }
        if (emittedMain) break;
    }
    // 2. Fall back to top-level main() function
    if (!emittedMain) {
        for (auto& node : prog.declarations) {
            if (auto* fn = dynamic_cast<const FuncDecl*>(node.get())) {
                if (fn->name == "main") {
                    // Already emitted by genFuncDecl, but it's not named 'main' in C
                    // Actually it is — nothing extra needed.
                    emittedMain = true;
                    break;
                }
            }
        }
    }
}

void CodeGenerator::genNode(const ASTNode& node) {
    emitLineDirective(node);
    if (auto* cls  = dynamic_cast<const ClassDecl*>(&node))  { genClassDecl(*cls);     return; }
    if (auto* ifc  = dynamic_cast<const InterfaceDecl*>(&node)) { genInterfaceDecl(*ifc); return; }
    if (auto* enm  = dynamic_cast<const EnumDecl*>(&node))   { genEnumDecl(*enm);      return; }
    if (auto* str  = dynamic_cast<const StructDecl*>(&node)) { genStructDecl(*str);    return; }
    if (auto* func = dynamic_cast<const FuncDecl*>(&node))   { genFuncDecl(*func);     return; }
    if (auto* blk  = dynamic_cast<const BlockStmt*>(&node))  { genBlock(*blk);         return; }
    if (auto* var  = dynamic_cast<const VarDeclStmt*>(&node)){ genVarDecl(*var);        return; }
    if (auto* ret  = dynamic_cast<const ReturnStmt*>(&node)) { genReturnStmt(*ret);     return; }
    if (auto* ifs  = dynamic_cast<const IfStmt*>(&node))     { genIfStmt(*ifs);         return; }
    if (auto* whl  = dynamic_cast<const WhileStmt*>(&node))  { genWhileStmt(*whl);      return; }
    if (auto* dw   = dynamic_cast<const DoWhileStmt*>(&node)){ genDoWhileStmt(*dw);     return; }
    if (auto* fr   = dynamic_cast<const ForStmt*>(&node))    { genForStmt(*fr);         return; }
    if (auto* fe   = dynamic_cast<const ForEachStmt*>(&node)){ genForEachStmt(*fe);     return; }
    if (auto* sw   = dynamic_cast<const SwitchStmt*>(&node)) { genSwitchStmt(*sw);      return; }
    if (auto* tc   = dynamic_cast<const TryCatchStmt*>(&node)){ genTryCatch(*tc);       return; }
    if (auto* thr  = dynamic_cast<const ThrowStmt*>(&node))  { genThrowStmt(*thr);      return; }
    if (auto* ub   = dynamic_cast<const UnsafeBlock*>(&node)){ genUnsafeBlock(*ub);     return; }
    if (auto* es   = dynamic_cast<const ExprStmt*>(&node))   { genExprStmt(*es);        return; }
    if (auto* br   = dynamic_cast<const BreakStmt*>(&node))  { emitLine("break;");      return; }
    if (auto* co   = dynamic_cast<const ContinueStmt*>(&node)) { emitLine("continue;"); return; }
    if (auto* inc  = dynamic_cast<const IncludeDirective*>(&node)) {
        emitLine("// #include \"" + inc->path + "\"");  return;
    }
    if (dynamic_cast<const PackageDecl*>(&node)) return; // ignore
    if (dynamic_cast<const ImportDecl*>(&node))  return; // ignore
}

// ─────────────────────────────────────────────────────────────
// Class / Interface / Enum / Struct
// ─────────────────────────────────────────────────────────────
// Helper: find and extract super() call args from a constructor body
// Returns the args string, and sets skipIdx to the index of the super() statement (or -1)
static std::string extractSuperArgs(const BlockStmt* block, int& skipIdx,
                                     const std::function<std::string(const ASTNode&)>& genExprFn) {
    skipIdx = -1;
    if (!block) return "";
    for (int i = 0; i < (int)block->stmts.size(); ++i) {
        if (auto* es = dynamic_cast<const ExprStmt*>(block->stmts[i].get())) {
            if (auto* call = dynamic_cast<const CallExpr*>(es->expr.get())) {
                if (auto* id = dynamic_cast<const IdentifierExpr*>(call->callee.get())) {
                    if (id->name == "super") {
                        skipIdx = i;
                        std::string args;
                        for (size_t j = 0; j < call->args.size(); ++j) {
                            if (j > 0) args += ", ";
                            args += genExprFn(*call->args[j]);
                        }
                        return args;
                    }
                }
            }
        }
    }
    return "";
}

void CodeGenerator::genClassDecl(const ClassDecl& cls) {
    // Skip classes whose C++ implementations are provided by the runtime or bindings headers
    static const std::unordered_set<std::string> kRuntimeProvided = {
        "Console", "Math", "System", "String", "FileSystem",
        "List", "Map", "Set", "Stack", "Queue",
        "AudioEngine", "Sound",
        "Graphics"
        // stdlib classes (Window, Shader, Mesh, Texture, Key, MouseButton, Time,
        // Thread, Mutex, Random, Matrix4x4, etc.) are compiled from .ume source.
    };
    if (kRuntimeProvided.count(cls.name)) return;
    if (!cls.typeParams.empty()) {
        emitLine("template<" + genTypeParamList(cls.typeParams) + ">");
    }
    // Build base list: superClass + all interfaces
    std::string decl = "struct " + cls.name;
    std::vector<std::string> bases;
    if (cls.superClass) bases.push_back("public " + cls.superClass->name);
    else {
        std::string targs = cls.name;
        if (!cls.typeParams.empty()) {
            targs += "<";
            for (size_t i = 0; i < cls.typeParams.size(); ++i) {
                if (i > 0) targs += ", ";
                targs += cls.typeParams[i];
            }
            targs += ">";
        }
        bases.push_back("public std::enable_shared_from_this<" + targs + ">");
    }
    for (auto& ifc : cls.interfaces) bases.push_back("public " + ifc.name);
    if (!bases.empty()) {
        decl += " : ";
        for (size_t i = 0; i < bases.size(); ++i) {
            if (i > 0) decl += ", ";
            decl += bases[i];
        }
    }
    emitLine(decl + " {");
    indent();

    // Fields — emit static keyword when needed; collect statics for out-of-class defs
    std::vector<const FieldDecl*> staticFields;
    for (auto& f : cls.fields) {
        emitIndent();
        if (f->isStatic) {
            if (f->isConst && f->initializer && (f->type.name == "int" || f->type.name == "float" || f->type.name == "bool" || f->type.name == "double" || f->type.name == "long" || f->type.name == "short" || f->type.name == "byte")) {
                emitLine("static constexpr " + mapType(f->type) + " " + f->name + " = " + genExpr(*f->initializer) + ";");
                continue;
            }
            emit("static ");
            staticFields.push_back(f.get());
        }
        std::string line = mapType(f->type) + " " + f->name;
        if (!f->isStatic && f->initializer && !f->type.isArray) line += " = " + genExpr(*f->initializer);
        emitLine(line + ";");
    }

    // Constructors
    if (cls.constructors.empty()) {
        bool hasArrayInit = false;
        for (auto& f : cls.fields) {
            if (!f->isStatic && f->type.isArray && f->initializer) { hasArrayInit = true; break; }
        }
        if (hasArrayInit) {
            emitIndent(); emitLine(cls.name + "() {");
            indent();
            for (auto& f : cls.fields) {
                if (!f->isStatic && f->type.isArray && f->initializer) {
                    emitIndent(); emitLine(f->name + " = " + genExpr(*f->initializer) + ";");
                }
            }
            dedent();
            emitLine("}");
        }
    }

    for (auto& ctor : cls.constructors) {
        emitIndent();
        emit(cls.name + "(");
        emit(genParamList(ctor->params));
        emit(")");
        int superStmtIdx = -1;
        std::string superArgs;
        if (cls.superClass) {
            if (auto* blk = dynamic_cast<const BlockStmt*>(ctor->body.get())) {
                superArgs = extractSuperArgs(blk, superStmtIdx,
                    [this](const ASTNode& n) { return genExpr(n); });
            }
            if (superStmtIdx >= 0)
                emit(" : " + cls.superClass->name + "(" + superArgs + ")");
        }
        emit(" ");
        if (auto* blk = dynamic_cast<const BlockStmt*>(ctor->body.get())) {
            emit("{\n");
            indent();
            for (auto& f : cls.fields) {
                if (!f->isStatic && f->type.isArray && f->initializer) {
                    emitIndent(); emitLine(f->name + " = " + genExpr(*f->initializer) + ";");
                }
            }
            for (int si = 0; si < (int)blk->stmts.size(); ++si) {
                if (si == superStmtIdx) continue; // skip super() statement
                genNode(*blk->stmts[si]);
            }
            dedent();
            emitLine("}");
        } else {
            genNode(*ctor->body);
        }
        emitLine();
    }

    // Destructor
    if (cls.destructor) {
        emitLine("virtual ~" + cls.name + "()");
        genNode(*cls.destructor->body);
        emitLine();
    } else if (!bases.empty()) {
        emitLine("virtual ~" + cls.name + "() = default;");
    }

    // Methods
    for (auto& m : cls.methods) {
        genFuncDecl(*m, cls.name);
    }
    
    // Properties
    for (auto& p : cls.properties) {
        emitIndent();
        std::string ty = mapType(p->type);
        if (p->isStatic) emit("static ");
        emit(ty + " get_" + p->name + "() ");
        if (p->getDecl) {
            if (dynamic_cast<const BlockStmt*>(p->getDecl.get())) {
                genNode(*p->getDecl); emitLine();
            } else {
                emitLine("{ return " + genExpr(*p->getDecl) + "; }");
            }
        } else {
            emitLine("{ return " + p->name + "_val; }");
        }

        if (p->hasSet) {
            emitIndent();
            if (p->isStatic) emit("static ");
            emit("void set_" + p->name + "(" + ty + " value) ");
            if (p->setDecl) {
                if (dynamic_cast<const BlockStmt*>(p->setDecl.get())) {
                    genNode(*p->setDecl); emitLine();
                } else {
                    emitLine("{ " + genExpr(*p->setDecl) + "; }");
                }
            } else {
                emitLine("{ " + p->name + "_val = value; }");
            }
        }
        
        if (!p->getDecl) {
            emitIndent();
            if (p->isStatic) emit("static ");
            emitLine(ty + " " + p->name + "_val;");
        }
    }

    // Operators
    for (auto& op : cls.operators) {
        genOperatorDecl(*op, cls.name);
    }

    // JSON Serialization
    bool isSerializable = false;
    for (const auto& attr : cls.attributes) {
        if (attr.name == "Serializable") {
            isSerializable = true;
            break;
        }
    }

    if (isSerializable) {
        emitLine("_ume_rt::UmeString toJsonString() const {");
        indent();
        emitLine("nlohmann::json _j;");
        for (const auto& f : cls.fields) {
            if (f->isStatic) continue;
            std::string jsonName = f->name;
            for (const auto& attr : f->attributes) {
                if (attr.name == "JsonProperty" && !attr.args.empty()) {
                    if (auto* lit = dynamic_cast<const StringLiteralExpr*>(attr.args[0].get())) {
                        jsonName = lit->value;
                    }
                }
            }
            // For strings, we need to convert _ume_rt::UmeString to std::string for nlohmann json
            if (mapType(f->type) == "String" || mapType(f->type) == "_ume_rt::UmeString") {
                emitLine("_j[\"" + jsonName + "\"] = " + f->name + ".std::string::c_str();");
            } else {
                emitLine("_j[\"" + jsonName + "\"] = " + f->name + ";");
            }
        }
        emitLine("return _ume_rt::UmeString(_j.dump());");
        dedent();
        emitLine("}");
        
        emitLine("static std::shared_ptr<" + cls.name + "> fromJsonString(const _ume_rt::UmeString& _str) {");
        indent();
        emitLine("auto _j = nlohmann::json::parse(_str.std::string::c_str());");
        emitLine("auto _obj = std::make_shared<" + cls.name + ">();");
        for (const auto& f : cls.fields) {
            if (f->isStatic) continue;
            std::string jsonName = f->name;
            for (const auto& attr : f->attributes) {
                if (attr.name == "JsonProperty" && !attr.args.empty()) {
                    if (auto* lit = dynamic_cast<const StringLiteralExpr*>(attr.args[0].get())) {
                        jsonName = lit->value;
                    }
                }
            }
            if (mapType(f->type) == "String" || mapType(f->type) == "_ume_rt::UmeString") {
                emitLine("if (_j.contains(\"" + jsonName + "\")) _obj->" + f->name + " = _ume_rt::UmeString(_j[\"" + jsonName + "\"].get<std::string>());");
            } else {
                emitLine("if (_j.contains(\"" + jsonName + "\")) _obj->" + f->name + " = _j[\"" + jsonName + "\"].get<" + mapType(f->type) + ">();");
            }
        }
        emitLine("return _obj;");
        dedent();
        emitLine("}");
    }

    dedent();
    emitLine("};");

    // Emit out-of-class definitions for static fields
    for (auto* f : staticFields) {
        emitIndent();
        std::string line = mapType(f->type) + " " + cls.name + "::" + f->name;
        if (f->initializer) line += " = " + genExpr(*f->initializer);
        else {
            // Default zero-initialise by type
            std::string t = mapType(f->type);
            if (t == "Int" || t == "Long" || t == "Short" || t == "Byte") line += " = 0";
            else if (t == "Double" || t == "Float") line += " = 0.0";
            else if (t == "Bool") line += " = false";
            else line += " = {}";
        }
        emitLine(line + ";");
    }

    // Emit free-function wrappers for binary operators so that
    // shared_ptr<T> OP shared_ptr<T> invokes the class's member operator.
    // Without this, `a * b` on shared_ptr fails because shared_ptr has no
    // binary operator* (it only has unary operator* for dereferencing).
    for (auto& op : cls.operators) {
        // Only generate for binary operators (those with exactly 1 param)
        if (op->params.size() != 1) continue;
        std::string paramType = mapType(op->params[0].type);
        std::string paramName = op->params[0].name;
        std::string retType = mapType(op->returnType);
        emitLine(retType + " operator" + op->op + "(std::shared_ptr<" + cls.name + "> self, " + paramType + " " + paramName + ") {");
        indent();
        emitLine("return self->operator" + op->op + "(" + paramName + ");");
        dedent();
        emitLine("}");
    }

    emitLine();
}

void CodeGenerator::genInterfaceDecl(const InterfaceDecl& iface) {
    if (!iface.typeParams.empty())
        emitLine("template<" + genTypeParamList(iface.typeParams) + ">");
    emitLine("struct " + iface.name + " {");
    indent();
    for (auto& m : iface.methods) {
        emitIndent();
        if (!m->body) emit("virtual ");
        emit(mapType(m->returnType) + " " + m->name + "(" + genParamList(m->params) + ")");
        if (!m->body) { emit(" = 0"); emitLine(";"); }
        else          { emit(" "); genNode(*m->body); emitLine(); }
    }
    for (auto& p : iface.properties) {
        std::string ty = mapType(p->type);
        emitIndent(); emitLine("virtual " + ty + " get_" + p->name + "() = 0;");
        if (p->hasSet) {
            emitIndent(); emitLine("virtual void set_" + p->name + "(" + ty + " value) = 0;");
        }
    }
    emitLine("virtual ~" + iface.name + "() = default;");
    dedent();
    emitLine("};");
    emitLine();
}

void CodeGenerator::genEnumDecl(const EnumDecl& enm) {
    emitLine("enum class " + enm.name + " {");
    indent();
    for (size_t i = 0; i < enm.values.size(); i++) {
        emitIndent();
        emit(enm.values[i].name);
        if (enm.values[i].value) emit(" = " + genExpr(*enm.values[i].value));
        emit(i + 1 < enm.values.size() ? "," : "");
        emitLine();
    }
    dedent();
    emitLine("};");
    emitLine();
}

void CodeGenerator::genStructDecl(const StructDecl& str) {
    emitLine("struct " + str.name + " {");
    indent();
    for (auto& f : str.fields) {
        std::string line = mapType(f->type) + " " + f->name;
        if (f->initializer) line += " = " + genExpr(*f->initializer);
        emitLine(line + ";");
    }
    for (auto& m : str.methods) genFuncDecl(*m, str.name);
    dedent();
    emitLine("};");
    emitLine();
}

void CodeGenerator::genFuncDecl(const FuncDecl& func, const std::string& ownerClass) {
    // ── Handle Ume's `func void constructor(params)` syntax ──
    // Ume allows constructors to be declared as methods named "constructor".
    // In C++, these must become actual constructors: ClassName(params) { ... }
    if (func.name == "constructor" && !ownerClass.empty()) {
        if (!func.typeParams.empty())
            emitLine("template<" + genTypeParamList(func.typeParams) + ">");
        emitIndent();
        // No return type, no static/virtual for constructors
        emit(ownerClass + "(");
        emit(genParamList(func.params));
        emit(")");
        if (func.isOverride) emit(" override");
        if (func.isAbstract && !func.body) { emitLine(" = 0;"); return; }
        emit(" ");
        if (func.body) genNode(*func.body);
        else emitLine(";");
        emitLine();
        return;
    }

    if (!func.typeParams.empty())
        emitLine("template<" + genTypeParamList(func.typeParams) + ">");
    emitIndent();
    if (func.isStatic)   emit("static ");
    // Emit virtual for all non-static, non-final instance methods inside a class
    // (so overrides work correctly from subclasses)
    bool needsVirtual = !ownerClass.empty() && !func.isStatic && !func.isFinal;
    if (needsVirtual || func.isAbstract) emit("virtual ");
    // C++ requires `int main()` — override the return type for top-level main
    bool isTopMain = (func.name == "main" && ownerClass.empty());
    std::string retType = isTopMain ? "int" : mapType(func.returnType);
    emit(retType + " " + escapeCppKeyword(func.name));
    emit("(" + genParamList(func.params) + ")");
    if (func.isOverride) emit(" override");
    if (func.isAbstract && !func.body) { emitLine(" = 0;"); return; }
    emit(" ");
    bool savedInMain = inMainFunction_;
    inMainFunction_ = isTopMain;
    if (func.body) genNode(*func.body);
    else emitLine(";");
    inMainFunction_ = savedInMain;
    emitLine();
}

void CodeGenerator::genOperatorDecl(const OperatorDecl& op, const std::string& /*className*/) {
    emitIndent();
    emit(mapType(op.returnType) + " operator" + op.op);
    emit("(" + genParamList(op.params) + ") ");
    genNode(*op.body);
    emitLine();
}

// ─────────────────────────────────────────────────────────────
// Statements
// ─────────────────────────────────────────────────────────────
void CodeGenerator::genBlock(const BlockStmt& block) {
    emit("{\n");
    indent();
    for (auto& s : block.stmts) genNode(*s);
    dedent();
    emitLine("}");
}

void CodeGenerator::genVarDecl(const VarDeclStmt& stmt) {
    emitIndent();
    if (stmt.isConst) emit("const ");
    std::string mappedType = mapType(stmt.type);
    emit(mappedType + " " + stmt.name);
    if (stmt.initializer) {
        // Handle null initializers: shared_ptr/optional accept nullptr, but
        // value types (std::vector, primitives) cannot be initialized from _ume_null.
        bool isNullInit = dynamic_cast<const NullLiteralExpr*>(stmt.initializer.get()) != nullptr;
        if (isNullInit) {
            bool canBeNull = mappedType.find("std::shared_ptr<") == 0
                          || mappedType.find("std::optional<") == 0;
            if (canBeNull) {
                emit(" = nullptr");
            }
            // else: skip initializer (default-construct the value type)
        } else {
            emit(" = " + genExpr(*stmt.initializer));
        }
    }
    emitLine(";");
}

void CodeGenerator::genReturnStmt(const ReturnStmt& stmt) {
    if (stmt.value) emitLine("return " + genExpr(*stmt.value) + ";");
    else if (inMainFunction_) emitLine("return 0;"); // void main() → int main()
    else            emitLine("return;");
}

void CodeGenerator::genIfStmt(const IfStmt& stmt) {
    emitIndent();
    emit("if (" + genExpr(*stmt.condition) + ") ");
    genNode(*stmt.thenBranch);
    if (stmt.elseBranch) {
        emitIndent();
        emit("else ");
        genNode(*stmt.elseBranch);
    }
    emitLine();
}

void CodeGenerator::genWhileStmt(const WhileStmt& stmt) {
    emitIndent();
    emit("while (" + genExpr(*stmt.condition) + ") ");
    genNode(*stmt.body);
    emitLine();
}

void CodeGenerator::genDoWhileStmt(const DoWhileStmt& stmt) {
    emitIndent(); emit("do ");
    genNode(*stmt.body);
    emitLine(" while (" + genExpr(*stmt.condition) + ");");
}

void CodeGenerator::genForStmt(const ForStmt& stmt) {
    emitIndent(); emit("for (");
    if (stmt.init) {
        if (auto* vd = dynamic_cast<const VarDeclStmt*>(stmt.init.get())) {
            if (vd->isConst) emit("const ");
            emit(mapType(vd->type) + " " + vd->name);
            if (vd->initializer) emit(" = " + genExpr(*vd->initializer));
        } else if (auto* es = dynamic_cast<const ExprStmt*>(stmt.init.get())) {
            emit(genExpr(*es->expr));
        } else {
            emit(genExpr(*stmt.init));
        }
    }
    emit("; ");
    if (stmt.condition) emit(genExpr(*stmt.condition));
    emit("; ");
    if (stmt.update) {
        if (auto* es = dynamic_cast<const ExprStmt*>(stmt.update.get())) {
            emit(genExpr(*es->expr));
        } else if (auto* blk = dynamic_cast<const BlockStmt*>(stmt.update.get())) {
            for (size_t i = 0; i < blk->stmts.size(); i++) {
                if (i > 0) emit(", ");
                if (auto* s = dynamic_cast<const ExprStmt*>(blk->stmts[i].get())) {
                    emit(genExpr(*s->expr));
                } else {
                    emit(genExpr(*blk->stmts[i]));
                }
            }
        } else {
            emit(genExpr(*stmt.update));
        }
    }
    emit(") ");
    genNode(*stmt.body);
    emitLine();
}

void CodeGenerator::genForEachStmt(const ForEachStmt& stmt) {
    std::string iterExpr = genExpr(*stmt.iterable);
    emitIndent();
    emit("for (auto&& " + stmt.varName + " : _ume_iter(" + iterExpr + ")) ");
    genNode(*stmt.body);
    emitLine();
}

void CodeGenerator::genSwitchStmt(const SwitchStmt& stmt) {
    emitLine("switch (" + genExpr(*stmt.value) + ") {");
    indent();
    for (auto& sc : stmt.cases) {
        if (sc.isDefault) { emitLine("default:"); }
        else { emitLine("case " + genExpr(*sc.value) + ":"); }
        indent();
        for (auto& s : sc.stmts) genNode(*s);
        dedent();
    }
    dedent();
    emitLine("}");
}

void CodeGenerator::genTryCatch(const TryCatchStmt& stmt) {
    emitIndent(); emit("try ");
    genNode(*stmt.tryBody);
    for (auto& cc : stmt.catches) {
        emitIndent();
        emit("catch (" + mapType(cc.type) + "& " + cc.varName + ") ");
        genNode(*cc.body);
    }
    if (stmt.finallyBody) {
        // C++ has no finally; approximate with a scope guard comment
        emitLine("// finally:");
        genNode(*stmt.finallyBody);
    }
    emitLine();
}

void CodeGenerator::genThrowStmt(const ThrowStmt& stmt) {
    emitLine("throw " + genExpr(*stmt.value) + ";");
}

void CodeGenerator::genUnsafeBlock(const UnsafeBlock& block) {
    emitLine("{ // unsafe");
    indent();
    genNode(*block.body);
    dedent();
    emitLine("}");
}

void CodeGenerator::genExprStmt(const ExprStmt& stmt) {
    emitLine(genExpr(*stmt.expr) + ";");
}

// ─────────────────────────────────────────────────────────────
// Expressions
// ─────────────────────────────────────────────────────────────
std::string CodeGenerator::genExpr(const ASTNode& node) {
    if (auto* n = dynamic_cast<const IntLiteralExpr*>(&node))    return std::to_string(n->value);
    if (auto* n = dynamic_cast<const FloatLiteralExpr*>(&node)) {
        std::ostringstream oss; oss << n->value;
        std::string s = oss.str();
        // Ensure it looks like a floating-point literal (has '.', 'e', or is inf/nan)
        // Without this, 0.0 renders as "0" (int), causing narrowing conversion errors
        // in braced-init-lists like std::vector<Float>({0, 1, ...}).
        if (s.find('.') == std::string::npos && s.find('e') == std::string::npos &&
            s.find('i') == std::string::npos && s.find('n') == std::string::npos &&
            s.find('I') == std::string::npos && s.find('N') == std::string::npos) {
            s += ".0";
        }
        return n->isFloat ? s + "f" : s;
    }
    if (auto* n = dynamic_cast<const StringLiteralExpr*>(&node)) return "String(\"" + escapeCppString(n->value) + "\")";
    if (auto* n = dynamic_cast<const CharLiteralExpr*>(&node))   return std::string("'") + n->value + "'";
    if (auto* n = dynamic_cast<const BoolLiteralExpr*>(&node))   return n->value ? "true" : "false";
    if (dynamic_cast<const NullLiteralExpr*>(&node))             return "_ume_null";
    if (auto* n = dynamic_cast<const IdentifierExpr*>(&node)) {
        if (n->name == "this") return "_ume_rt::_ume_this(this)";
        if (n->name == "super") return "__super";
        return escapeCppKeyword(n->name);
    }
    if (auto* n = dynamic_cast<const BinaryExpr*>(&node))            return genBinary(*n);
    if (auto* n = dynamic_cast<const UnaryExpr*>(&node))             return genUnary(*n);
    if (auto* n = dynamic_cast<const AssignExpr*>(&node))            return genAssign(*n);
    if (auto* n = dynamic_cast<const CallExpr*>(&node))              return genCall(*n);
    if (auto* n = dynamic_cast<const MemberAccessExpr*>(&node))      return genMemberAccess(*n);
    if (auto* n = dynamic_cast<const IndexExpr*>(&node))             return genIndex(*n);
    if (auto* n = dynamic_cast<const NewExpr*>(&node))               return genNew(*n);
    if (auto* n = dynamic_cast<const CastExpr*>(&node))              return genCast(*n);
    if (auto* n = dynamic_cast<const TernaryExpr*>(&node))           return genTernary(*n);
    if (auto* n = dynamic_cast<const LambdaExpr*>(&node))            return genLambda(*n);
    if (auto* n = dynamic_cast<const ArrayLiteralExpr*>(&node))      return genArrayLiteral(*n);
    if (auto* n = dynamic_cast<const StructInitExpr*>(&node))        return genStructInit(*n);
    if (auto* n = dynamic_cast<const NullCoalesceExpr*>(&node))      return genNullCoalesce(*n);
    if (auto* n = dynamic_cast<const NullAssertExpr*>(&node))        return genNullAssert(*n);
    if (auto* n = dynamic_cast<const InterpolatedStringExpr*>(&node)) return genInterpolatedString(*n);
    if (auto* n = dynamic_cast<const AllocExpr*>(&node))             return genAlloc(*n);
    if (auto* n = dynamic_cast<const DerefExpr*>(&node))             return genDeref(*n);
    if (auto* n = dynamic_cast<const FreeExpr*>(&node))              return "(void)(" + genExpr(*n->pointer) + ")";
    if (auto* n = dynamic_cast<const AddressOfExpr*>(&node))         return "&(" + genExpr(*n->value) + ")";
    return "/* unknown expr */";
}

std::string CodeGenerator::genBinary(const BinaryExpr& expr) {
    return "(" + genExpr(*expr.left) + " " + expr.op + " " + genExpr(*expr.right) + ")";
}

std::string CodeGenerator::genUnary(const UnaryExpr& expr) {
    if (expr.prefix) return "(" + expr.op + genExpr(*expr.operand) + ")";
    return "(" + genExpr(*expr.operand) + expr.op + ")";
}

std::string CodeGenerator::genAssign(const AssignExpr& expr) {
    if (auto* ma = dynamic_cast<const MemberAccessExpr*>(expr.target.get())) {
        if (ma->isProperty) {
            std::string obj = genExpr(*ma->object);
            bool isStatic = false;
            static const std::unordered_set<std::string> staticBuiltins = { "Console", "Math", "System", "String", "FileSystem" };
            if (staticBuiltins.count(obj)) isStatic = true;
            else if (!obj.empty() && std::isupper((unsigned char)obj[0])) {
                if (obj.find('(') == std::string::npos && obj.find('[') == std::string::npos) isStatic = true;
            }
            std::string op = isStatic ? "::" : "->";
            std::string right = genExpr(*expr.value);
            if (expr.op == "=") {
                return obj + op + "set_" + ma->member + "(" + right + ")";
            } else {
                std::string getCall = obj + op + "get_" + ma->member + "()";
                std::string binOp = expr.op.substr(0, expr.op.size() - 1);
                return obj + op + "set_" + ma->member + "(" + getCall + " " + binOp + " " + right + ")";
            }
        }
    }
    return genExpr(*expr.target) + " " + expr.op + " " + genExpr(*expr.value);
}

std::string CodeGenerator::genCall(const CallExpr& expr) {
    std::string callee = genExpr(*expr.callee);
    // Remap super() call
    if (callee == "__super") callee = "";
    std::string args;
    for (size_t i = 0; i < expr.args.size(); i++) {
        if (i > 0) args += ", ";
        std::string argStr = genExpr(*expr.args[i]);
        args += argStr;
    }
    if (callee.empty()) return args; // super(args) inline

    // ── Intercept member-access calls that need special handling ──────
    if (auto* ma = dynamic_cast<const MemberAccessExpr*>(expr.callee.get())) {
        std::string obj    = genExpr(*ma->object);
        std::string method = ma->member;

        // length() → _ume_length(obj) — works for vector, string, and shared_ptr<List>
        if (method == "length" && expr.args.empty())
            return "_ume_length(" + obj + ")";

        // NOTE: Mesh.Create, Mesh.Quad, Shader.Create, etc. are stdlib class static
        // methods — they get compiled normally as Window::Create(...), Mesh::Quad(), etc.
        // No special-casing needed. The stdlib classes call Graphics::createXxx() internally.

        // Variadic for-each over an initializer_list parameter (the param itself is iterable)
        // No special action needed here — just let fall through

        // String-only methods that UmeString provides via operator->()
        // These work automatically because UmeString::operator->() returns this*.
        // Nothing to do — the generated obj->method(args) form is correct.
    }

    return callee + "(" + args + ")";
}

std::string CodeGenerator::genMemberAccess(const MemberAccessExpr& expr) {
    std::string obj = genExpr(*expr.object);

    // Built-in static classes use '::'; all others use '->'
    // NOTE: Graphics is the only runtime struct. Window, Shader, Mesh, Texture,
    // Key, MouseButton, Time, Thread, Mutex, etc. are all stdlib classes that
    // get compiled from .ume source — they use '::' for static access and '->'
    // for instance access, determined by the uppercase-first-letter heuristic below.
    static const std::unordered_set<std::string> staticBuiltins = {
        "Console", "Math", "System", "String", "FileSystem",
        "Graphics"
    };
    bool isStatic = staticBuiltins.count(obj) > 0 || classNames_.count(obj) > 0 || enumNames_.count(obj) > 0;
    // __super (MSVC keyword for base class) uses :: for member access
    if (obj == "__super") isStatic = true;
    std::string op = isStatic ? "::" : "->";
    if (expr.isProperty) return obj + op + "get_" + expr.member + "()";
    return obj + op + expr.member;
}

std::string CodeGenerator::genIndex(const IndexExpr& expr) {
    return "(_ume_rt::_ume_iter(" + genExpr(*expr.object) + ")[" + genExpr(*expr.index) + "])";
}

std::string CodeGenerator::genNew(const NewExpr& expr) {
    if (expr.type.isArray) {
        std::string elemType = mapTypeName(expr.type.name);
        static const std::vector<std::string> primitiveNames = {
            "Int","Long","Short","Byte","Float","Double","Bool","Char","String","void","auto","Any"
        };
        bool isPrim = false;
        for (auto& p : primitiveNames) if (elemType == p) { isPrim = true; break; }
        bool alreadyManaged = elemType.find("std::shared_ptr<") == 0
                           || elemType.find("std::vector<") == 0;
        bool isValue = enumNames_.count(elemType) || structNames_.count(elemType);
        if (!isPrim && !alreadyManaged && !isValue && elemType != "void" && elemType != "auto") {
            elemType = "std::shared_ptr<" + elemType + ">";
        }
        int rank = expr.type.arrayRank > 0 ? expr.type.arrayRank : (!expr.args.empty() ? (int)expr.args.size() : 1);
        if (rank == 1) {
            std::string sizeArg = (!expr.args.empty()) ? genExpr(*expr.args[0]) : "0";
            return "_ume_rt::_ume_make_vec<" + elemType + ">((size_t)(" + sizeArg + "))";
        } else {
            std::function<std::string(int)> buildNestedVec = [&](int dimIndex) -> std::string {
                std::string subType = elemType;
                for (int i = 0; i < rank - dimIndex - 1; i++) subType = "std::vector<" + subType + ">";
                std::string sizeArg = (dimIndex < (int)expr.args.size()) ? genExpr(*expr.args[dimIndex]) : "0";
                if (dimIndex == rank - 1) {
                    return "_ume_rt::_ume_make_vec<" + elemType + ">((size_t)(" + sizeArg + "))";
                }
                return "_ume_rt::_ume_make_vec<" + subType + ">((size_t)(" + sizeArg + "), " + buildNestedVec(dimIndex + 1) + ")";
            };
            return buildNestedVec(0);
        }
    }
    std::string typeName = expr.type.name;
    std::string args;
    for (size_t i = 0; i < expr.args.size(); i++) {
        if (i > 0) args += ", ";
        args += genExpr(*expr.args[i]);
    }
    if (structNames_.count(typeName)) {
        return typeName + "{" + args + "}";
    }
    if (!expr.type.typeArgs.empty()) {
        typeName += "<";
        for (size_t i = 0; i < expr.type.typeArgs.size(); i++) {
            if (i > 0) typeName += ",";
            typeName += mapType(expr.type.typeArgs[i]);
        }
        typeName += ">";
    }
    // Graphics native types are wrapped by stdlib classes (Window, Shader, Mesh, Texture)
    // which call Graphics::createXxx() internally. Thread and Mutex are also stdlib classes.
    // So no factory mapping is needed here — `new Window(...)` creates a user-class instance
    // via std::make_shared<Window>(...) which calls the constructor that calls Graphics::createXxx().
    static const std::unordered_map<std::string, std::string> runtimeFactories = {
    };
    auto it = runtimeFactories.find(typeName);
    if (it != runtimeFactories.end()) {
        return it->second + "(" + args + ")";
    }
    // Check if any argument is an array literal (braced-init-list). std::make_shared
    // cannot deduce template arguments from braced-init-lists, and MSVC can't parse
    // `new Type({...})` when the constructor takes std::vector. So we wrap each array
    // literal arg in an explicit `std::vector<elemType>{...}` construction.
    bool hasArrayArg = false;
    for (auto& arg : expr.args) {
        if (dynamic_cast<const ArrayLiteralExpr*>(arg.get())) { hasArrayArg = true; break; }
    }
    if (hasArrayArg) {
        // MSVC cannot parse braced-init-lists as function arguments when passed to std::make_shared.
        // The workaround: use the _ume_vec<T>({...}) helper function.
        std::string typedArgs;
        for (size_t i = 0; i < expr.args.size(); i++) {
            if (i > 0) typedArgs += ", ";
            if (auto* arr = dynamic_cast<const ArrayLiteralExpr*>(expr.args[i].get())) {
                // Infer element type from first element
                std::string elemType = "Float"; // default
                if (!arr->elements.empty()) {
                    auto& e = arr->elements[0];
                    if (dynamic_cast<const FloatLiteralExpr*>(e.get())) elemType = "Float";
                    else if (dynamic_cast<const IntLiteralExpr*>(e.get())) elemType = "Int";
                    else if (dynamic_cast<const StringLiteralExpr*>(e.get())) elemType = "String";
                    else if (dynamic_cast<const BoolLiteralExpr*>(e.get())) elemType = "Bool";
                    else if (auto* cast = dynamic_cast<const CastExpr*>(e.get())) {
                        // Use the cast's target type
                        std::string ct = mapType(cast->targetType);
                        if (!ct.empty()) elemType = ct;
                    }
                }
                // Build the braced element list
                std::string elems;
                for (size_t j = 0; j < arr->elements.size(); j++) {
                    if (j > 0) elems += ", ";
                    elems += genExpr(*arr->elements[j]);
                }
                typedArgs += "_ume_vec<" + elemType + ">({" + elems + "})";
            } else {
                typedArgs += genExpr(*expr.args[i]);
            }
        }
        return "std::make_shared<" + typeName + ">(" + typedArgs + ")";
    }
    return "std::make_shared<" + typeName + ">(" + args + ")";
}

std::string CodeGenerator::genCast(const CastExpr& expr) {
    return "static_cast<" + mapType(expr.targetType) + ">(" + genExpr(*expr.value) + ")";
}

std::string CodeGenerator::genTernary(const TernaryExpr& expr) {
    return "(" + genExpr(*expr.condition) + " ? " + genExpr(*expr.thenExpr) + " : " + genExpr(*expr.elseExpr) + ")";
}

std::string CodeGenerator::genLambda(const LambdaExpr& expr) {
    std::string s = "[&](";
    for (size_t i = 0; i < expr.params.size(); i++) {
        if (i > 0) s += ", ";
        s += mapType(expr.params[i].type) + " " + expr.params[i].name;
    }
    s += ") ";
    if (auto* blk = dynamic_cast<const BlockStmt*>(expr.body.get())) {
        // Use a temporary generator for the body to avoid polluting out_
        CodeGenerator inner;
        inner.indent_ = 0;
        inner.enumNames_ = enumNames_;
        inner.genBlock(*blk);
        s += inner.out_.str();
    } else if (auto* ret = dynamic_cast<const ReturnStmt*>(expr.body.get())) {
        // Arrow lambda: body is a wrapped ReturnStmt
        s += "{ return " + (ret->value ? genExpr(*ret->value) : std::string("")) + "; }";
    } else {
        // Plain expression body
        s += "{ return " + genExpr(*expr.body) + "; }";
    }
    return s;
}

std::string CodeGenerator::genArrayLiteral(const ArrayLiteralExpr& expr) {
    std::string s = "{";
    for (size_t i = 0; i < expr.elements.size(); i++) {
        if (i > 0) s += ", ";
        s += genExpr(*expr.elements[i]);
    }
    s += "}";
    return s;
}

std::string CodeGenerator::genStructInit(const StructInitExpr& expr) {
    std::string s = "{";
    for (size_t i = 0; i < expr.fields.size(); i++) {
        if (i > 0) s += ", ";
        s += "." + expr.fields[i].first + " = " + genExpr(*expr.fields[i].second);
    }
    s += "}";
    return s;
}

std::string CodeGenerator::genNullCoalesce(const NullCoalesceExpr& expr) {
    std::string l = genExpr(*expr.left);
    return "(" + l + " ? *" + l + " : " + genExpr(*expr.right) + ")";
}

std::string CodeGenerator::genNullAssert(const NullAssertExpr& expr) {
    std::string e = genExpr(*expr.expr);
    return "([&](){ auto _v = " + e + "; if (!_v) throw NullPointerException(); return *_v; }())";
}

std::string CodeGenerator::genInterpolatedString(const InterpolatedStringExpr& expr) {
    if (expr.parts.empty()) return "String(\"\")";
    std::string result;
    bool first = true;
    for (auto& part : expr.parts) {
        if (!first) result += " + ";
        first = false;
        if (!part.isExpr) {
            result += "String(\"" + escapeCppString(part.text) + "\")";
        } else {
            // Convert expr to string using ostringstream; lambda returns std::string
            result += "[&](){ std::ostringstream _os; _os << (" + genExpr(*part.expr) +
                      "); return _os.str(); }()";
        }
    }
    // Wrap entire concatenation in String(...) so the result type is always UmeString,
    // not std::string (which would be ambiguous between println(String) and println(Any))
    return "String(" + result + ")";
}

std::string CodeGenerator::genAlloc(const AllocExpr& expr) {
    TypeAnnotation elemTa = expr.type;
    elemTa.isArray = false;
    std::string elemType = mapType(elemTa);

    if (auto* arr = dynamic_cast<const ArrayLiteralExpr*>(expr.count.get())) {
        int rank = (int)arr->elements.size();
        std::function<std::string(int)> buildNestedVec = [&](int dimIndex) -> std::string {
            std::string subType = elemType;
            for (int i = 0; i < rank - dimIndex - 1; i++) subType = "std::vector<" + subType + ">";
            std::string sizeArg = genExpr(*arr->elements[dimIndex]);
            if (dimIndex == rank - 1) {
                return "_ume_rt::_ume_make_vec<" + elemType + ">((size_t)(" + sizeArg + "))";
            }
            return "_ume_rt::_ume_make_vec<" + subType + ">((size_t)(" + sizeArg + "), " + buildNestedVec(dimIndex + 1) + ")";
        };
        return buildNestedVec(0);
    }

    return "_ume_rt::_ume_make_vec<" + elemType + ">((size_t)(" + genExpr(*expr.count) + "))";
}

std::string CodeGenerator::genDeref(const DerefExpr& expr) {
    return "(*" + genExpr(*expr.pointer) + ")";
}

// ─────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────
std::string CodeGenerator::genParamList(const std::vector<Parameter>& params) {
    std::string s;
    for (size_t i = 0; i < params.size(); i++) {
        if (i > 0) s += ", ";
        auto& p = params[i];
        if (p.variadic) {
            // Variadic: use std::initializer_list so callers can pass comma-separated values
            s += "std::initializer_list<" + mapTypeName(p.type.name) + "> " + p.name;
        } else {
            s += mapType(p.type, true) + " " + p.name;
        }
        if (p.defaultValue) s += " = " + genExpr(*p.defaultValue);
    }
    return s;
}

std::string CodeGenerator::genTypeParamList(const std::vector<std::string>& tp) {
    std::string s;
    for (size_t i = 0; i < tp.size(); i++) {
        if (i > 0) s += ", ";
        s += "typename " + tp[i];
    }
    return s;
}

std::string CodeGenerator::escapeCppString(const std::string& s) {
    std::string out;
    for (char c : s) {
        switch (c) {
        case '"':  out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n";  break;
        case '\t': out += "\\t";  break;
        case '\r': out += "\\r";  break;
        default:   out += c;
        }
    }
    return out;
}

} // namespace Ume
