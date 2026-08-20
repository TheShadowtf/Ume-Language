# Ume Language: Comprehensive Feature Roadmap

This document outlines the planned, long-term expansions for the Ume language. The goal is to bring Ume's productivity and feature set up to the standard of modern languages like C#, while retaining its native C++ performance and simplicity.

---

## 1. Advanced Language Features (C# Inspired)

### 1.1. Properties & Indexers
Instead of writing explicit `getHealth()` and `setHealth()` methods, Ume will support C#-style properties and indexers.
```csharp
public class Player {
    // Auto-implemented property
    public int health { get; set; }
    
    // Computed property
    public bool isAlive => health > 0;
    
    // Indexers
    public string this[int index] {
        get { return inventory[index]; }
        set { inventory[index] = value; }
    }
}
```

### 1.2. Attributes & Metadata (Crucial for FFI)
Attributes allow developers to attach metadata to classes, methods, and fields, which can be read at compile-time (for transpiler hints) or runtime (for reflection).
```csharp
[Serializable, Obsolete("Use new API")]
[DllImport("user32.dll", entryPoint="MessageBox")]
public class NativeAPI {
    // ...
}
```

### 1.3. Events & Delegates
First-class support for event-driven programming. Delegates act as type-safe function pointers.
```csharp
public delegate void KeyEventHandler(Key key);

public class Window {
    public event KeyEventHandler OnKeyPressed;
    
    func void trigger(Key key) {
        OnKeyPressed?.invoke(key);
    }
}
```

### 1.4. Pattern Matching
Modern C#-style pattern matching for cleaner switch statements and conditionals.
```csharp
string type = animal switch {
    Dog d when d.age > 5 => "Old Dog",
    Cat _                  => "Cat",
    _                      => "Unknown"
};
```

### 1.5. Record Types & Tuples
Records provide built-in immutability and value-equality, perfect for data-transfer objects.
```csharp
public record Point(int x, int y);

// Tuples and Deconstruction
(int x, int y) = getCoordinates();
```

### 1.6. Extension Methods
Add new functionality to existing types (like `string` or `int[]`) without needing to inherit from them.
```csharp
public static func bool isEmpty(this string s) {
    return s.length() == 0;
}
```

### 1.7. LINQ (Language Integrated Query)
Fluid, declarative data querying.
```csharp
var adults = users.where(u => u.age >= 18)
                  .select(u => u.name)
                  .toList();
```

---

## 2. Standard Library Expansions

### 2.1. Audio Library (`audio.ume`)
- **Capabilities:** Playback of `.wav`/`.ogg`/`.mp3` files, volume control, pitch shifting, 3D spatial audio.
- **Backend:** Native integration with `miniaudio.h` or `OpenAL`.

### 2.2. Networking & HTTP (`net.ume`)
- **Capabilities:** `TcpListener`, `TcpClient`, `UdpClient`. 
- **Higher Level:** `HttpClient` for REST APIs (GET, POST, parsing JSON responses).

### 2.3. Concurrency & Async (`threading.ume`)
- **Capabilities:** `async` and `await` keywords mapped to C++ futures/promises.
- **Primitives:** `Thread`, `Mutex`, `Semaphore`, `Task`.

### 2.4. Serialization (`json.ume`)
- Built-in JSON serialization powered by the upcoming Attributes system (`[JsonProperty]`).

---

## 3. Tooling & Ecosystem (From README.md)

As noted in the project's README, the following tooling features are essential for Phase 4 and Phase 5 of the language's lifecycle.

### 3.1. Package Manager (`ume add <pkg>`)
- **Description:** A centralized package manager (like `npm`, `cargo`, or `nuget`).
- **Functionality:** Will automatically download community libraries, resolve dependencies, and link their `.ume` files and `.dll`/`.so` binaries into your local project's `build` directory.

### 3.2. Language Server Protocol (LSP)
- **Description:** A background language server to provide IDE integration.
- **Functionality:** Will hook into VSCode and Visual Studio to provide real-time IntelliSense (autocomplete), go-to-definition, hover documentation, and inline syntax error checking while writing `.ume` files.
