using System.Runtime.InteropServices;

namespace Timefall
{
    public static partial class NativeCalls
    {
        [LibraryImport("Timefall", StringMarshalling = StringMarshalling.Utf16)]
        internal static partial void Native_RegisterEntityTypes(string? typeName, string? assemblyName, string[] fieldNames, string[] fieldTypeNames, int fieldCount);

        [LibraryImport("Timefall", StringMarshalling = StringMarshalling.Utf16)]
        internal static partial void Native_RegisterComponentTypes(string? typeName, string? assemblyName, string[] fieldNames, string[] fieldTypeNames, int fieldCount);

        [LibraryImport("Timefall")]
        internal static partial IntPtr GetScriptInstance(ulong entityID);

        [LibraryImport("Timefall", StringMarshalling = StringMarshalling.Utf8)]
        [return: MarshalAs(UnmanagedType.U1)]
        internal static partial bool Entity_HasComponent(ulong entityID, string? componentTypeFullName);
        [LibraryImport("Timefall", StringMarshalling = StringMarshalling.Utf8)]
        internal static partial ulong Entity_FindEntityByName(string name);

        [LibraryImport("Timefall", StringMarshalling = StringMarshalling.Utf8)]
        internal static partial ulong Scene_CreateEntity(string? name);
        [LibraryImport("Timefall")]
        internal static partial void Entity_Destroy(ulong entityID);
        [LibraryImport("Timefall", StringMarshalling = StringMarshalling.Utf8)]
        internal static partial void Entity_AddComponent(ulong entityID, string? componentTypeFullName);

        [LibraryImport("Timefall", StringMarshalling = StringMarshalling.Utf8)]
        internal static partial void Entity_RemoveComponent(ulong entityID, string? componentTypeFullName);

        [LibraryImport("Timefall", StringMarshalling = StringMarshalling.Utf16)]
        internal static partial void ManagedComponent_GetField(ulong entityID, string typeName, string fieldName, IntPtr outValue, int size);
        [LibraryImport("Timefall", StringMarshalling = StringMarshalling.Utf16)]
        internal static partial void ManagedComponent_SetField(ulong entityID, string typeName, string fieldName, IntPtr value, int size);

        [LibraryImport("Timefall", StringMarshalling = StringMarshalling.Utf8)]
        internal static partial int Entity_GetEntitiesWithComponentCount(string componentTypeFullName);
        [LibraryImport("Timefall", StringMarshalling = StringMarshalling.Utf8)]
        internal static partial void Entity_GetEntitiesWithComponent(string componentTypeFullName, [Out] ulong[] outEntities, int count);

        [LibraryImport("Timefall")]
        internal static partial void SpriteRendererComponent_GetColor(ulong entityID, out Vector4 color);
        [LibraryImport("Timefall")]
        internal static partial void SpriteRendererComponent_SetColor(ulong entityID, ref Vector4 color);

        [LibraryImport("Timefall")]
        internal static partial void TransformComponent_GetTranslation(ulong entityID, out Vector3 translation);
        [LibraryImport("Timefall")]
        internal static partial void TransformComponent_SetTranslation(ulong entityID, ref Vector3 translation);
        [LibraryImport("Timefall")]
        internal static partial void TransformComponent_GetRotation(ulong entityID, out Vector3 rotation);
        [LibraryImport("Timefall")]
        internal static partial void TransformComponent_SetRotation(ulong entityID, ref Vector3 rotation);
        [LibraryImport("Timefall")]
        internal static partial void TransformComponent_GetScale(ulong entityID, out Vector3 scale);
        [LibraryImport("Timefall")]
        internal static partial void TransformComponent_SetScale(ulong entityID, ref Vector3 scale);

        [LibraryImport("Timefall")]
        internal static partial void TransformComponent_GetWorldTranslation(ulong entityID, out Vector3 translation);
        [LibraryImport("Timefall")]
        internal static partial void TransformComponent_SetWorldTranslation(ulong entityID, ref Vector3 translation);
        [LibraryImport("Timefall")]
        internal static partial void TransformComponent_GetWorldRotation(ulong entityID, out Vector3 rotation);
        [LibraryImport("Timefall")]
        internal static partial void TransformComponent_SetWorldRotation(ulong entityID, ref Vector3 rotation);
        [LibraryImport("Timefall")]
        internal static partial void TransformComponent_GetWorldScale(ulong entityID, out Vector3 scale);

        [LibraryImport("Timefall")]
        internal static partial void Entity_SetParent(ulong childID, ulong parentID);
        [LibraryImport("Timefall")]
        internal static partial ulong Entity_GetParent(ulong entityID);
        [LibraryImport("Timefall")]
        internal static partial int Entity_GetChildrenCount(ulong entityID);
        [LibraryImport("Timefall")]
        internal static partial void Entity_GetChildren(ulong entityID, [Out] ulong[] outChildren, int count);

        [LibraryImport("Timefall")]
        internal static partial void Rigidbody2DComponent_ApplyLinearImpulse(ulong entityID, ref Vector2 impulse, ref Vector2 point, [MarshalAs(UnmanagedType.U1)] bool wake);
        [LibraryImport("Timefall")]
        internal static partial void Rigidbody2DComponent_ApplyLinearImpulseToCenter(ulong entityID, ref Vector2 impulse, [MarshalAs(UnmanagedType.U1)] bool wake);
        [LibraryImport("Timefall")]
        internal static partial void Rigidbody2DComponent_GetLinearVelocity(ulong entityID, out Vector2 velocity);
        [LibraryImport("Timefall")]
        internal static partial Rigidbody2DComponent.BodyType Rigidbody2DComponent_GetBodyType(ulong entityID);
        [LibraryImport("Timefall")]
        internal static partial void Rigidbody2DComponent_SetBodyType(ulong entityID, Rigidbody2DComponent.BodyType type);

        [LibraryImport("Timefall")]
        [return: MarshalAs(UnmanagedType.U1)]
        internal static partial bool Input_IsKeyDown(KeyCode keycode);
        [LibraryImport("Timefall")]
        [return: MarshalAs(UnmanagedType.U1)]
        internal static partial bool Input_IsKeyPressedThisFrame(KeyCode keycode);
        [LibraryImport("Timefall")]
        [return: MarshalAs(UnmanagedType.U1)]
        internal static partial bool Input_IsKeyReleasedThisFrame(KeyCode keycode);

        [LibraryImport("Timefall")]
        [return: MarshalAs(UnmanagedType.U1)]
        internal static partial bool Input_IsMouseButtonDown(MouseButton button);
        [LibraryImport("Timefall")]
        [return: MarshalAs(UnmanagedType.U1)]
        internal static partial bool Input_IsMouseButtonPressedThisFrame(MouseButton button);
        [LibraryImport("Timefall")]
        [return: MarshalAs(UnmanagedType.U1)]
        internal static partial bool Input_IsMouseButtonReleasedThisFrame(MouseButton button);

        [LibraryImport("Timefall")]
        internal static partial void Input_GetMousePosition(out Vector2 position);
        [LibraryImport("Timefall")]
        internal static partial void Input_GetMouseWorldPosition(out Vector2 position);

        [LibraryImport("Timefall", EntryPoint = "TextComponent_GetText")]
        internal static partial IntPtr TextComponent_GetText_Native(ulong entityID);
        [LibraryImport("Timefall", StringMarshalling = StringMarshalling.Utf8)]
        internal static partial void TextComponent_SetText(ulong entityID, string? text);
        [LibraryImport("Timefall")]
        internal static partial void TextComponent_GetColor(ulong entityID, out Vector4 color);
        [LibraryImport("Timefall")]
        internal static partial void TextComponent_SetColor(ulong entityID, ref Vector4 color);
        [LibraryImport("Timefall")]
        internal static partial float TextComponent_GetKerning(ulong entityID);
        [LibraryImport("Timefall")]
        internal static partial void TextComponent_SetKerning(ulong entityID, float kerning);
        [LibraryImport("Timefall")]
        internal static partial float TextComponent_GetLineSpacing(ulong entityID);
        [LibraryImport("Timefall")]
        internal static partial void TextComponent_SetLineSpacing(ulong entityID, float lineSpacing);

        internal static string? TextComponent_GetText(ulong entityID) => Marshal.PtrToStringUTF8(TextComponent_GetText_Native(entityID));
    }
}
