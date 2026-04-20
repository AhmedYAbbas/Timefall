using System.Runtime.InteropServices;

namespace Timefall
{
    public static partial class NativeCalls
    {
        [LibraryImport("Timefall", StringMarshalling = StringMarshalling.Utf16)]
        internal static partial void Native_RegisterEntityTypes(string? typeName, string? assemblyName, string[] fieldNames, string[] fieldTypeNames, int fieldCount);

        [LibraryImport("Timefall")]
        internal static partial IntPtr GetScriptInstance(ulong entityID);

        [LibraryImport("Timefall", StringMarshalling = StringMarshalling.Utf8)]
        [return: MarshalAs(UnmanagedType.U1)]
        internal static partial bool Entity_HasComponent(ulong entityID, string? componentTypeFullName);
        [LibraryImport("Timefall", StringMarshalling = StringMarshalling.Utf8)]
        internal static partial ulong Entity_FindEntityByName(string name);

        [LibraryImport("Timefall")]
        internal static partial void TransformComponent_GetTranslation(ulong entityID, out Vector3 translation);

        [LibraryImport("Timefall")]
        internal static partial void TransformComponent_SetTranslation(ulong entityID, ref Vector3 translation);

        [LibraryImport("Timefall")]
        internal static partial void Rigidbody2DComponent_ApplyLinearImpulse(ulong entityID, ref Vector2 impulse, ref Vector2 point, [MarshalAs(UnmanagedType.U1)] bool wake);
        
        [LibraryImport("Timefall")]
        internal static partial void Rigidbody2DComponent_ApplyLinearImpulseToCenter(ulong entityID, ref Vector2 impulse, [MarshalAs(UnmanagedType.U1)] bool wake);

        [LibraryImport("Timefall")]
        [return: MarshalAs(UnmanagedType.U1)]
        internal static partial bool Input_IsKeyDown(KeyCode keycode);
    }
}
