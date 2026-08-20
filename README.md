<div align="center">

<img src="docs/assets/img/logo.png" alt="Ume Logo" width="120"/>

# Ume Language

**Fast. Expressive. Native.**

A modern, statically-typed, C#-inspired programming language that transpiles directly to optimized C++17.  
Write expressive, high-productivity code — run at native machine speed.

[![License: MIT](https://img.shields.io/badge/License-MIT-teal.svg)](LICENSE)
[![Target: C++17 / C++20](https://img.shields.io/badge/target-C%2B%2B17%20%2F%20C%2B%2B20-blue.svg)](https://en.cppreference.com/w/cpp/17)
[![Platform: Windows](https://img.shields.io/badge/platform-Windows%20x64-blueviolet.svg)]()
[![Documentation: Wiki](https://img.shields.io/badge/docs-Wiki-green.svg)](https://theshadowtf.github.io/Ume-Language/)

</div>

---

## ✨ Key Highlights

* **No Garbage Collector Pause** — Deterministic object lifetime via modern C++ smart pointers and destructors.
* **Dual Execution Model** — Interpret instantly with `-run` for rapid development; transpile and compile with `-compile` / `build` for maximum native release speed.
* **Full Object-Oriented Programming** — Classes, single inheritance (`extends`), interfaces (`implements`), abstract methods, operator overloading, and generic types (`<T>`).
* **C#-Style Properties & Indexers** — Explicit `get`/`set` accessors, fat-arrow computed properties (`=>`), and indexers (`this[i]`).
* **Attribute & Reflection System** — `[Serializable]` and `[JsonProperty("...")]` drive automated JSON serialization at runtime.
* **Built-in Concurrency & Synchronization** — `Thread`, `Mutex`, `ConditionVariable`, `AtomicInt`, and asynchronous `Task<T>`.
* **Rich Standard Library** — Collections (`List`, `Map`, `Set`, `Stack`, `Queue`), FileSystem, Networking (`HttpClient`), Audio (`miniaudio`), and 2D/3D Graphics (`OpenGL`/`GLFW`).
* **Automated Asset Bundling Pipeline** — Seamlessly copy textures, shaders, audio, and configuration files into build outputs via `ume.toml`.

---

## TLDR
Is this project good? No.   
Is the code good? No.   
Is the code messy? Yes.  
Do I understand what I have done? No, not really.   
Is this language useable? Not even a bit.  
Is this language usefull? Not even a bit.  
Do I regret making this project? My heart say yes, but my brain say no. And the monsters under my bed say to shut up.   
Maybe have fun using it? I guess have fun!  

The minecraft demo it is made with AI (forgive me but I really didn't want to also make minecraft rn, maybe after one year) and u can run it using ume build (ume run for some reason does not work, I get an error but ANYWAYS)

## 🚀 Installation & Quick Start

### 1. Prerequisites

To build and compile Ume programs, you only need a standard C++ toolchain and CMake on Windows:

| Prerequisite | Recommended Version | What it's used for |
| :--- | :--- | :--- |
| **Visual Studio** | 2022+ (Community is fine) | MSVC C++20 Compiler (`cl.exe`) & Windows SDK |
| **CMake** | 3.20+ | Build system configuration |
| **Git** | Any | Repository cloning |

> 💡 **Quick Setup for New Machines (PowerShell / CMD):**  
> If you don't have Visual Studio C++ or CMake installed yet, you can install both in one command via Windows Package Manager:
> ```cmd
> winget install Kitware.CMake
> winget install Microsoft.VisualStudio.2022.Community --override "--add Microsoft.VisualStudio.Workload.NativeDesktop --passive"
> ```

---

### 2. ⚡ One-Click Build & Setup (`build.bat`)

Clone the repository and run **`build.bat`**:

```bat
git clone https://github.com/your-org/ume-language.git
cd ume-language
.\build.bat
```

**What `build.bat` does automatically:**
1. ✅ Automatically discovers `cmake` (in PATH or standard install paths).
2. ✅ Auto-detects Visual Studio C++ toolchains using `vswhere` and initializes MSVC environment variables without requiring a Developer Command Prompt.
3. ✅ Configures CMake and compiles the compiler (`ume.exe`), tree-walk interpreter runtime, and OpenGL/GLFW graphics subsystem in **Release mode**.
4. ✅ Deploys all final binaries (`bin\release\ume.exe`, `glad.dll`, `glfw3.dll`), static libraries (`ume_compiler_lib.lib`, `ume_interpreter_lib.lib`, `ume_graphics_lib.lib`), and headers to `bin\release\`.

*(Optional)* Run `.\add_to_path.bat` to add `bin\release` to your User PATH so you can invoke `ume` from anywhere.

---

### 3. Hello, World!

Create a file named `hello.ume`:

```ume
func void main() {
    Console.println("Hello, World!");
}
```

Run it directly or compile to a native `.exe`:

```bat
bin\release\ume.exe -run hello.ume

bin\release\ume.exe -compile hello.ume -o hello
.\hello.exe
```

---

## 📖 Language Tour

### 1. Variables, Types & String Interpolation

```ume
// Type inference with var
var name = "Alice";
var age  = 25;
var pi   = 3.14159;

// Explicit typing
string city = "London";
int counter = 0;
bool active = true;

// Nullable types and null-coalescing (??)
string? nickname = null;
string displayName = nickname ?? "Anonymous";

// String interpolation ($"...")
string greeting = $"Hello, {name}! You are {age} years old.";
Console.println(greeting);
```

### 2. Functions & Default Parameters

```ume
func int add(int a, int b) {
    return a + b;
}

func void logMessage(string msg, bool newLine = true) {
    if (newLine) {
        Console.println(msg);
    } else {
        Console.print(msg);
    }
}

logMessage("Result: ", false);
Console.println(add(10, 20));
```

### 3. Classes, Inheritance & Super

```ume
public class Animal {
    public string name;
    public int age;

    public Animal(string name, int age) {
        this.name = name;
        this.age  = age;
    }

    public func string describe() {
        return name + " (age " + age + ")";
    }
}

public class Dog extends Animal {
    public string breed;

    public Dog(string name, int age, string breed) {
        super(name, age);
        this.breed = breed;
    }

    public override func string describe() {
        return super.describe() + " [" + breed + "]";
    }
}

var dog = new Dog("Buddy", 3, "Golden Retriever");
Console.println(dog.describe()); // Buddy (age 3) [Golden Retriever]
```

### 4. Properties & Custom Indexers

```ume
public class Player {
    private int _hp = 100;

    // get / set property with validation
    public int Health {
        get { return _hp; }
        set {
            if (value < 0) value = 0;
            _hp = value;
        }
    }

    // Computed fat-arrow property (read-only)
    public bool IsAlive => _hp > 0;
}

public class WordList {
    private List<string> words = new List<string>();

    public func void add(string w) { words.add(w); }

    // Indexer access: wl[0]
    public string this[int index] {
        get { return words.get(index); }
        set { words.set(index, value); }
    }
}
```

### 5. Lambdas & Functional Collections

```ume
import std.collections.*;

var nums = new List<int>();
nums.add(1); nums.add(2); nums.add(3); nums.add(4); nums.add(5);

// Map, Filter, Reduce with arrow functions
var doubled = nums.map(n -> n * 2);
var evens   = nums.filter(n -> n % 2 == 0);
var sum     = nums.reduce(0, (acc, n) -> acc + n);

Console.println("Doubled: " + doubled.toString()); // [2, 4, 6, 8, 10]
Console.println("Evens:   " + evens.toString());   // [2, 4]
Console.println("Sum:     " + sum);                // 15

// Key-Value Maps & Sets
Map<string, int> scores = new Map<string, int>();
scores.put("Alice", 98);
scores.put("Bob", 85);
Console.println("Alice score: " + scores.get("Alice"));
```

### 6. Exception Handling

```ume
public class ValidationError extends Exception {
    public string field;

    public ValidationError(string field, string msg) {
        super(msg);
        this.field = field;
    }
}

try {
    throw new ValidationError("email", "Invalid email address format");
} catch (ValidationError ve) {
    Console.println("Validation failed on field '" + ve.field + "': " + ve.getMessage());
} catch (Exception ex) {
    Console.println("Generic error: " + ex.getMessage());
} finally {
    Console.println("Cleanup executed deterministically.");
}
```

### 7. Operator Overloading

```ume
public class Vec2 {
    public double x;
    public double y;

    public Vec2(double x, double y) {
        this.x = x;
        this.y = y;
    }

    public operator + (Vec2 other) -> Vec2 {
        return new Vec2(x + other.x, y + other.y);
    }

    public operator == (Vec2 other) -> bool {
        return x == other.x && y == other.y;
    }

    public func string toString() {
        return "(" + x + ", " + y + ")";
    }
}

var v1 = new Vec2(10.0, 20.0);
var v2 = new Vec2(5.0, 15.0);
var v3 = v1 + v2; // (15, 35)
```

### 8. Attributes & JSON Serialization

```ume
import std.json.*;

[Serializable]
public class Product {
    [JsonProperty("product_id")]
    public int id;

    [JsonProperty("display_name")]
    public string name;

    [JsonProperty("price_usd")]
    public double price;
}

var p = new Product();
p.id = 101;
p.name = "Game Controller";
p.price = 59.99;

// Serialize to JSON
string json = p.toJsonString();
Console.println(json); // {"product_id":101,"display_name":"Game Controller","price_usd":59.99}
```

### 9. Concurrency & Atomics

```ume
import std.thread.*;

var atomicCounter = new AtomicInt(0);
var mtx = new Mutex();

var worker = () -> {
    int i = 0;
    while (i < 1000) {
        atomicCounter.getAndIncrement();
        i++;
    }
};

Thread t1 = new Thread(worker);
Thread t2 = new Thread(worker);
t1.start();
t2.start();
t1.join();
t2.join();

Console.println("Counter: " + atomicCounter.get()); // 2000
```

---

## 📦 Projects & `ume.toml` Manifest

Ume provides full project management and automated asset bundling via `ume.toml`:

```toml
# ume.toml
name    = "MyGame"
version = "1.0.0"

# Main entry point (relative to project root)
entry   = "src/main.ume"

# Compiler backend: "cpp" (default) or "llvm"
backend = "cpp"

# Asset bundling pipeline (automatically copied into build/ upon 'ume build')
# Supports folders, files, and custom source -> destination mappings
assets  = "textures, shaders, audio, resources/data -> data, config/game.json -> game.json"
```

### Managing Projects via CLI

```bat
ume new MyGame
ume build
ume run
```

---

## ⚡ Performance & 🛡️ Security Benchmarks

Ume includes an automated benchmark and security validation suite in `benchmarks/` comparing **Ume Language** with **Native C++** (MSVC 19.51 C++20 `/O2`).

### 📊 Performance Baseline

| Benchmark Workload | Domain / Focus Area | Ume (Interpreter) | Native C++ (`/O2`) | Speedup Ratio | Correctness |
| :--- | :--- | :---: | :---: | :---: | :---: |
| **`fib_recursive`** | Call Stack Overhead ($N=28$) | 11,152.00 ms | 1.070 ms | **10,422.4x** | `MATCH [OK]` |
| **`fib_iterative`** | Loop Arithmetic ($10^6$ ops) | 3,992.00 ms | 2.857 ms | **1,397.3x** | `MATCH [OK]` |
| **`primes_trial_division`** | Integer Modulo & Branches | 891.00 ms | 0.456 ms | **1,953.9x** | `MATCH [OK]` |
| **`primes_sieve_eratosthenes`**| Sieve Array Traversal | 532.00 ms | 0.098 ms | **5,428.6x** | `MATCH [OK]` |
| **`mandelbrot_float_math`** | Floating-Point Math ($150 \times 150$) | 3,243.00 ms | 1.457 ms | **2,225.8x** | `MATCH [OK]` |
| **`oop_instantiation_and_dispatch`**| Virtual OOP & Polymorphism | 1,418.00 ms | 1.990 ms | **712.6x** | `MATCH [OK]` |
| **`string_concat_and_manipulation`**| String & Heap Operations | 206.00 ms | 1.523 ms | **135.3x** | `MATCH [OK]` |
| **`collections_list_map_queue`** | Dynamic List, Map, Queue | 92.00 ms | 0.738 ms | **124.7x** | `MATCH [OK]` |
| **`concurrency_atomics_and_mutex`** | Atomics & Mutex Locks | 338.00 ms | 0.926 ms | **365.0x** | `MATCH [OK]` |

> **Execution Modes:**
> - In **Interpreter Mode (`ume -run`)**, Ume runs as an AST tree-walking interpreter (fast dev turnaround, Python/Ruby tier execution speed).
> - In **Compiled Mode (`ume -compile` / `ume build`)**, Ume transpiles directly to C++17/20, achieving **true native C++ machine speed**.

### 🛡️ Security, Safety & Memory Audit

| Security Dimension | Test Scenario | Ume Result | C++ Result | Safety Verification |
| :--- | :--- | :---: | :---: | :---: |
| **Memory Bounds Safety** | Negative indices (`list.get(-1)`), index overflow, out-of-bounds `set` | **PASSED** (4/4) | **PASSED** (4/4) | Traps boundary violations with `Index out of bounds` exception; prevents memory corruption. |
| **Null Reference Safety** | Null method calls, null coalescing (`??`), null equality checks | **PASSED** (4/4) | **PASSED** (4/4) | Caught as clean runtime exception; prevents OS `0xC0000005` access violations. |
| **Exception Stack Unwinding** | 20-frame deep recursive unwinding with custom exception & `finally` | **PASSED** (3/3) | **PASSED** (3/3) | All 21 `finally` blocks executed deterministically without resource leaks. |
| **Arithmetic Safety** | Integer division by zero, modulo by zero, integer wrapping | **PASSED** (3/3) | **PASSED** (3/3) | Division by zero caught as exception; prevents SIGFPE aborts. |
| **Memory Leak Stress** | 50,000 rapid object allocations/deallocations in a tight loop | **PASSED** | **PASSED** | Deterministic heap cleanup with zero unhandled leaks. |

To run the automated benchmark runner:
```bat
.\run_benchmarks.bat
```

---

## 🛠️ CLI Reference

```
ume <command> [options]

Commands:
  -run    <file.ume> [-- args]    Interpret and run a .ume file
  -compile <file.ume> [-o <out>]  Compile a .ume file to native binary
  new     <ProjectName>           Scaffold a new Ume project
  build   [<projectDir>]          Build project and bundle assets
  run     [<projectDir>]          Run the project in directory
  repl                            Start the interactive REPL
  version                         Print version information
  help                            Show help message
```

---

## 📚 Documentation & Guides

Comprehensive documentation is available online at **[https://theshadowtf.github.io/Ume-Language/](https://theshadowtf.github.io/Ume-Language/)**:

| Topic / Area | Documentation Guide | Description |
| :--- | :--- | :--- |
| **Getting Started** | [Getting Started](https://theshadowtf.github.io/Ume-Language/wiki/getting-started.html) | Installation, compiler build, Hello World, CLI reference |
| **Projects & `ume.toml`** | [Projects & ume.toml](https://theshadowtf.github.io/Ume-Language/wiki/project-config.html) | `ume.toml` specification, build targets, asset copy pipeline (`assets = "..."`) |
| **Modules & Imports** | [Modules & Imports](https://theshadowtf.github.io/Ume-Language/wiki/modules.html) | `package`, `namespace`, selective imports (`import pkg.Symbol`), `#include` |
| **Primitive Types** | [Primitive Types](https://theshadowtf.github.io/Ume-Language/wiki/types.html) | `int`, `double`, `bool`, `char`, `string`, `any`, nullable `T?` |
| **Variables & Scope** | [Variables & Scope](https://theshadowtf.github.io/Ume-Language/wiki/variables.html) | `var`, `const`, explicit types, scoping rules |
| **Strings & Text** | [Strings & Interpolation](https://theshadowtf.github.io/Ume-Language/wiki/strings.html) | `$"Hello {name}"`, escape sequences, string manipulation |
| **Expressions** | [Expressions & Operators](https://theshadowtf.github.io/Ume-Language/wiki/expressions.html) | Arithmetic, comparison, logical, ternary conditional operator |
| **Functions** | [Functions](https://theshadowtf.github.io/Ume-Language/wiki/functions.html) | Parameters, return types, default values, overloading |
| **Control Flow** | [Control Flow](https://theshadowtf.github.io/Ume-Language/wiki/control-flow.html) | `if`/`else`, `while`, `do-while`, `foreach`, `switch`/`case` |
| **Classes & OOP** | [Classes & OOP](https://theshadowtf.github.io/Ume-Language/wiki/classes.html) | Constructors, inheritance (`extends`), `super`, `this`, `abstract`, `static` |
| **Interfaces & Structs** | [Interfaces & Abstract](https://theshadowtf.github.io/Ume-Language/wiki/interfaces.html) | `interface`, `implements`, abstract classes and methods |
| **Enums & Structs** | [Enums & Structs](https://theshadowtf.github.io/Ume-Language/wiki/enums.html) | Value-type `struct`, auto and explicit `enum` definitions |
| **Generics** | [Generics](https://theshadowtf.github.io/Ume-Language/wiki/generics.html) | Generic classes (`Box<T>`), multi-parameter generics (`Pair<K,V>`) |
| **Properties & Indexers**| [Properties & Indexers](https://theshadowtf.github.io/Ume-Language/wiki/properties.html) | `get`/`set` properties, fat-arrow `=>`, custom indexers `this[i]` |
| **Attributes & JSON** | [Attributes & JSON](https://theshadowtf.github.io/Ume-Language/wiki/attributes.html) | `[Serializable]`, `[JsonProperty]`, `.toJsonString()` |
| **Exceptions** | [Exceptions & Unwinding](https://theshadowtf.github.io/Ume-Language/wiki/exceptions.html) | `try`, `catch`, `finally`, `throw`, custom exceptions |
| **Lambdas & Closures** | [Lambdas & Closures](https://theshadowtf.github.io/Ume-Language/wiki/lambdas.html) | Arrow functions `(a, b) -> { }`, closure environment capture |
| **Operator Overload** | [Operator Overloading](https://theshadowtf.github.io/Ume-Language/wiki/operators.html) | Custom operator implementations (`+`, `-`, `==`, `[]`) |
| **Unsafe Blocks** | [Unsafe Memory](https://theshadowtf.github.io/Ume-Language/wiki/unsafe.html) | `unsafe { }`, raw pointers `T*`, `alloc<T>()`, `free()` |
| **Collections** | [Standard Collections](https://theshadowtf.github.io/Ume-Language/wiki/stdlib-collections.html) | `List<T>`, `Map<K,V>`, `Set<T>`, `Stack`, `Queue`, `.filter()`, `.map()` |
| **I/O & FileSystem** | [I/O & FileSystem](https://theshadowtf.github.io/Ume-Language/wiki/stdlib-io.html) | `Console`, `File.write()`, `File.readAll()`, `FileSystem` helpers |
| **Concurrency** | [Concurrency & Threading](https://theshadowtf.github.io/Ume-Language/wiki/stdlib-thread.html) | `Thread`, `Mutex`, `ConditionVariable`, `AtomicInt`, `Task<T>` |
| **Networking** | [Networking & HTTP](https://theshadowtf.github.io/Ume-Language/wiki/stdlib-net.html) | `HttpClient`, `HttpResponse`, URL encoding and query parsing |
| **Audio Engine** | [Audio Engine](https://theshadowtf.github.io/Ume-Language/wiki/stdlib-audio.html) | `AudioEngine`, `Sound` (playback, volume, looping) |
| **Graphics & Window** | [Graphics & Windowing](https://theshadowtf.github.io/Ume-Language/wiki/stdlib-graphics.html) | OpenGL window, shaders, meshes, colors, input handling |
| **Vectors & Math** | [Vectors & Matrices](https://theshadowtf.github.io/Ume-Language/wiki/stdlib-vector.html) | `Vector2`, `Vector3`, `Matrix4`, linear algebra operations |

## 📄 License

Ume Language is released under the [MIT License](LICENSE).
