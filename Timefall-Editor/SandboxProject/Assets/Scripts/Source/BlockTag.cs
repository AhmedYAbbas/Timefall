using Timefall;

namespace Sandbox
{
    // Phase 3 verification fixture: a user-defined component declared in C#.
    // - Stored natively in the entity's ManagedComponentStorage (no GCHandle).
    // - Shown/edited in the editor inspector; serialized to/from the scene YAML.
    // - Each field is a property backed by the Component base's Get<T>/Set<T> helpers,
    //   so the property name is the field name in the native store.
    // An empty tag (no properties) is equally valid: `public class BlockTag : Component { }`.
    public class BlockTag : Component
    {
        public int     Kind  { get => Get<int>();     set => Set(value); }
        public Vector4 Color { get => Get<Vector4>(); set => Set(value); }
    }
}
