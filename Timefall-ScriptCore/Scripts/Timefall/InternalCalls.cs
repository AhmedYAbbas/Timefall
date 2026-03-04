using System.Runtime.InteropServices;

namespace Timefall
{
    public static partial class InternalCalls
    {
        [LibraryImport("Timefall")]
        internal static partial void Entity_GetTranslation(ulong entityID, out Vector3 translation);

        [LibraryImport("Timefall")]
        internal static partial void Entity_SetTranslation(ulong entityID, ref Vector3 translation);

        // Explicitly specify marshalling for the bool return to resolve SYSLIB1051.
        // Use UnmanagedType.U1 to marshal a 1-byte C-style bool.
        [LibraryImport("Timefall")]
        [return: MarshalAs(UnmanagedType.U1)]
        internal static partial bool Input_IsKeyDown(KeyCode keycode);
    }
}
