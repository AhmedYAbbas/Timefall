---
name: timefall-add-component
description: Add a brand-new native (C++/entt) component type to the Timefall engine and wire it through every system that must know about it. Use this whenever a new built-in component is introduced — "add a Light/Health/AudioSource/Tag component", "the engine needs a component for X", "store Y natively on entities", or any task creating a new `struct …Component` in Components.h that the editor should show and scenes should save. Also triggers on the tell-tale failure of a half-registered component — it doesn't appear in the inspector, isn't serialized, or the build throws an "unresolved external symbol" (the missing OnComponentAdded specialization). Covers the full registration checklist across AllComponents, OnComponentAdded, the serializer, the hierarchy panel, and Scene::Copy. Not for exposing an existing component to C# scripts (that is timefall-script-binding), changing fields or defaults on an existing component, C# script classes that subclass Entity, or components in other engines such as Unity.
---

# Timefall: Add a Native Component

A component isn't "done" when the struct exists — it must be registered in **6 places**, or it will
silently fail to copy, serialize, show in the editor, or (worst) **fail to link**. This skill is the
checklist plus the one trap that costs the most time.

## The component itself — `Timefall/src/Timefall/Scene/Components.h`

Define the struct with `TF_API` and the standard constructors, then **add it to the `AllComponents`
`ComponentGroup`** at the bottom of the file. `AllComponents` is the master tuple that drives generic
copy, duplication, and C# registration — a component missing from it is invisible to those systems.

```cpp
struct TF_API MyComponent
{
    int Value = 0;

    MyComponent() = default;
    MyComponent(const MyComponent&) = default;
};

// ... at the bottom ...
using AllComponents = ComponentGroup<
    TransformComponent,
    MyComponent,            // <-- add here
    SpriteRendererComponent,
    /* ... */>;
```
Keep components as **pure data** (no behavior methods) — that's the established convention here;
matrix/transform logic lives in `Math::` helpers and `Entity`, not on the component.

## The link-error trap — `OnComponentAdded` specialization (`Scene.cpp`)

**This is the one that bites.** `Entity::AddComponent<T>` calls `Scene::OnComponentAdded<T>`. The
primary template is defined **only in `Scene.cpp`** (empty body); every component has an explicit,
`TF_API`-exported specialization. Because `AddComponent<T>` is instantiated in **other translation
units** — notably `ScriptGlue.cpp`'s `RegisterComponent(AllComponents{})`, which instantiates
`AddComponent<T>` for *every* type in `AllComponents` — the linker needs an exported specialization for
your type. Forget it and you get an **unresolved external symbol** at link time, far from the cause.

Add, next to the others in `Scene.cpp`:

```cpp
template<>
TF_API void Scene::OnComponentAdded<MyComponent>(Entity entity, MyComponent& component)
{
}
```
(Put real init logic here only if needed — e.g. `CameraComponent` sets viewport size. Usually empty.)

## Serialization — `Timefall/src/Timefall/Scene/SceneSerializer.cpp`

Two spots, both inside the per-entity loops.

```cpp
// SerializeEntity(...)
if (entity.HasComponent<MyComponent>())
{
    out << YAML::Key << "MyComponent" << YAML::BeginMap;
    auto& c = entity.GetComponent<MyComponent>();
    out << YAML::Key << "Value" << YAML::Value << c.Value;
    out << YAML::EndMap;
}

// Deserialize(...), inside the entity loop
if (auto node = entity["MyComponent"])
{
    auto& c = deserializedEntity.AddComponent<MyComponent>();
    c.Value = node["Value"].as<int>();
}
```
Guarding deserialize with `if (auto node = entity["MyComponent"])` keeps **older scene files
backward-compatible** (missing key → skipped). Note: `TransformComponent` is special — it's always
present, so it uses `GetComponent` (not `AddComponent`) on load.

## Editor inspector — `Timefall-Editor/src/Panels/SceneHierarchyPanel.cpp`

Two spots: render it, and let users add it.

```cpp
// In DrawComponents(...): render + edit the fields.
DrawComponent<MyComponent>("My Component", entity, [](auto& component)
{
    ImGui::DragInt("Value", &component.Value);
});

// In the "AddComponent" popup (DrawComponents), so users can attach it:
DisplayAddComponentEntry<MyComponent>("My Component");
```
`DrawComponent<T>` already handles the collapsing header, the remove-component button, and the
`HasComponent` check — just supply the ImGui widgets for the fields in the lambda.

## Runtime copy — `Scene::Copy` (`Scene.cpp`)

`Scene::Copy` (used by Play/Simulate and scene loading) copies components **explicitly per type** — it
does *not* iterate `AllComponents`. Add your line:

```cpp
CopyComponent<MyComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);
```
Miss this and your component vanishes the moment the user hits Play.

## Duplication — automatic, with a caveat

`Scene::DuplicateEntity` copies via `CopyComponentIfExists(AllComponents{}, ...)`, so once your
component is in `AllComponents` it duplicates for free. **Caveat:** a component holding UUID references
to *other entities* (like `RelationshipComponent`) is duplicated verbatim — those references will point
at the original's relations. Such components need custom handling in the duplicate path (reset + rewire
with fresh UUIDs); plain-data components are fine.

## Optional: expose to C# scripts

If scripts need to read/add/check the component, that's a separate workflow — see the
**`timefall-script-binding`** skill. Note the generic `Entity_HasComponent` / `Entity_AddComponent`
already work for any type in `AllComponents` (registered by managed type name `Timefall.<Name>` via
`RegisterComponent`), provided a matching C# `Component` subclass exists in ScriptCore.

## Checklist
- [ ] `Components.h`: define `struct TF_API MyComponent` **and** add to `AllComponents`.
- [ ] `Scene.cpp`: `OnComponentAdded<MyComponent>` specialization (prevents the link error).
- [ ] `SceneSerializer.cpp`: serialize block **and** guarded deserialize block.
- [ ] `SceneHierarchyPanel.cpp`: `DrawComponent<T>` render + `DisplayAddComponentEntry<T>` in Add menu.
- [ ] `Scene::Copy`: `CopyComponent<MyComponent>(...)`.
- [ ] Holds cross-entity UUIDs? Add custom handling in `DuplicateEntity`.
- [ ] Build engine + editor; verify add/edit in inspector, save/load round-trip, and Play (copy).
