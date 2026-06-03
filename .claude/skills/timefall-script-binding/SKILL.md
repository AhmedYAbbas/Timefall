---
name: timefall-script-binding
description: Expose an existing C++ engine capability to C# scripting in the Timefall engine across its P/Invoke boundary (ScriptGlue.cpp → NativeCalls.cs → Entity.cs/Components.cs). Use this whenever a script needs to call a native engine function that isn't available yet — reading or writing a component field from C#, an entity operation (parent, destroy, query-by-component), or input/scene/physics/audio access. Trigger even without the word "binding" — "expose X to C#", "let scripts read/set Y", "add a script API for Z", "call this from a script", "scripts can't access W yet", or an EntryPointNotFoundException on a new native call all mean this skill. Covers the marshalling gotchas (glm↔Vector, UUID↔ulong, count-then-fill arrays). Not for introducing a brand-new native component type (that is timefall-add-component), for writing gameplay scripts with the API that already exists, or for general or OS-level P/Invoke to non-engine DLLs.
---

# Timefall C++ → C# Script Binding

Every new script capability crosses the host boundary in **three mechanical edits**. Miss any edit and
you get either a compile error or a runtime `EntryPointNotFoundException`. This skill captures the
pattern and the non-obvious traps.

## The boundary, in one sentence

C# uses **name-based P/Invoke** (`[LibraryImport("Timefall")]`) against functions the engine exports
with `extern "C" __declspec(dllexport)`. `extern "C"` is mandatory — without it the symbol name is
C++-mangled and P/Invoke can't find it.

## The three edit sites

### 1. C++ export — `Timefall/src/Timefall/Scripting/ScriptGlue.cpp`

Add the function **inside the existing `extern "C" { ... }` block**, each prefixed with
`__declspec(dllexport)`. The canonical body resolves the entity from the active scene:

```cpp
__declspec(dllexport)
void TransformComponent_GetWorldTranslation(UUID entityID, glm::vec3* outTranslation)
{
    Scene* scene = ScriptEngine::GetSceneContext();
    TF_CORE_ASSERT(scene, "Scene context is null");
    Entity entity = scene->GetEntityByUUID(entityID);
    TF_CORE_ASSERT(entity, "Entity is null");

    *outTranslation = entity.GetWorldTranslation();
}
```

Naming convention: `Type_Operation` (`TransformComponent_SetRotation`, `Entity_GetParent`,
`Rigidbody2DComponent_ApplyLinearImpulse`). Un-prefixed = local/default; prefix world-space ones
explicitly (`..._GetWorldTranslation`). Plain function bindings need **no registration** — only the
generic component has/add machinery uses the `s_Entity*ComponentFuncs` maps (see
`timefall-add-component`).

### 2. C# import — `Timefall-ScriptCore/Scripts/Timefall/NativeCalls.cs`

Add a matching `partial` to the `NativeCalls` partial class. The native name and the managed name
**must match exactly** (or use `EntryPoint = "..."`).

```csharp
[LibraryImport("Timefall")]
internal static partial void TransformComponent_GetWorldTranslation(ulong entityID, out Vector3 translation);
```

### 3. Friendly wrapper — `Timefall-ScriptCore/Scripts/Scene/Entity.cs` (or `Components.cs`)

Surface a clean property/method that scripts actually call:

```csharp
public Vector3 WorldTranslation
{
    get { NativeCalls.TransformComponent_GetWorldTranslation(ID, out Vector3 v); return v; }
    set { NativeCalls.TransformComponent_SetWorldTranslation(ID, ref value); }
}
```

## Marshalling cheat-sheet

| C++ parameter | C# parameter | Notes |
|---|---|---|
| `glm::vec3*` (out) | `out Vector3` | `Vector3` is 3 floats — layout matches `glm::vec3`. Same for `vec2`/`vec4`. |
| `glm::vec3*` (in) | `ref Vector3` | Use `ref` when the native side reads it. |
| `UUID` | `ulong` | `UUID` is implicitly convertible to/from `uint64_t`. Return `0` for "none". |
| `const char*` | `string` | Add `StringMarshalling = StringMarshalling.Utf8` (or `Utf16`) on the attribute. |
| `bool` return/param | `[return: MarshalAs(UnmanagedType.U1)] bool` / `[MarshalAs(UnmanagedType.U1)] bool` | C++ `bool` is 1 byte. |
| returning a string | `IntPtr` + `Marshal.PtrToStringUTF8(...)` | Return a `const char*`/heap ptr, unwrap in a C# helper. See `TextComponent_GetText`. |

### Returning a variable-length array — count-then-fill

You cannot return a `std::vector` across P/Invoke. Use **two calls**: one for the count, one to fill a
caller-allocated buffer. This avoids any cross-boundary ownership/lifetime ambiguity.

```cpp
__declspec(dllexport)
int Entity_GetChildrenCount(UUID entityID) { /* return children.size() (0 if no component) */ }

__declspec(dllexport)
void Entity_GetChildren(UUID entityID, uint64_t* outChildren, int count)
{
    // ... fetch entity + component ...
    const auto& children = entity.GetComponent<RelationshipComponent>().Children;
    int n = count < (int)children.size() ? count : (int)children.size();
    for (int i = 0; i < n; ++i) outChildren[i] = children[i]; // UUID -> uint64_t
}
```

```csharp
[LibraryImport("Timefall")] internal static partial int  Entity_GetChildrenCount(ulong entityID);
[LibraryImport("Timefall")] internal static partial void Entity_GetChildren(ulong entityID, [Out] ulong[] outChildren, int count);

public Entity[] Children
{
    get
    {
        int count = NativeCalls.Entity_GetChildrenCount(ID);
        if (count == 0) return Array.Empty<Entity>();
        ulong[] ids = new ulong[count];
        NativeCalls.Entity_GetChildren(ID, ids, count);
        var result = new Entity[count];
        for (int i = 0; i < count; i++) result[i] = new Entity(ids[i]);
        return result;
    }
}
```
`[Out] ulong[]` is blittable, so the source generator pins it and the native side writes directly into
the managed buffer — no copy-back marshalling needed.

## Troubleshooting

A binding that **compiles but throws `EntryPointNotFoundException`** at runtime almost always means the
function isn't `extern "C"` (so its symbol got C++-mangled and P/Invoke can't resolve it by name), or
the managed name doesn't exactly match the native export name.

## When renaming a C# script-facing member

Renaming an `Entity`/`Component` property (e.g. `Translation` → `LocalTranslation`) breaks **every
script that uses it**, including example scripts in the project's `Assets/Scripts/Source/`. Grep the
whole `*.cs` set for the old name and update consumers, or the Sandbox assembly won't compile.

## Quick checklist
- [ ] `ScriptGlue.cpp`: `__declspec(dllexport)` fn inside `extern "C"`, scene→`GetEntityByUUID`→touch.
- [ ] `NativeCalls.cs`: matching `[LibraryImport("Timefall")] partial`, exact name, correct marshalling.
- [ ] `Entity.cs`/`Components.cs`: friendly wrapper.
- [ ] Array return? Use count-then-fill.
- [ ] Renamed a member? Update all `*.cs` consumers.
