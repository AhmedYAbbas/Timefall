using System.Reflection;
using System.Runtime.InteropServices;
using System.Runtime.InteropServices.JavaScript;
using System.Runtime.InteropServices.Marshalling;

namespace Timefall
{
    public delegate void PrintIntDelegate(int value);
    public delegate void PrintIntsDelegate(int value1, int value2);
    public delegate void PrintStringDelegate(IntPtr message);
    public delegate IntPtr CreateInstanceDelegate();
    public delegate IntPtr CreateIntInstanceDelegate(int value);
    public delegate void DestroyInstanceDelegate(IntPtr instance);

    public delegate void OnCreateDelegate();
    public delegate void OnUpdateDelegate(float ts);

    // Delegates for invoking instance methods via handle
    public delegate void InvokeOnCreateDelegate(IntPtr instance);
    public delegate void InvokeOnUpdateDelegate(IntPtr instance, float ts);

    // Delegates for creating instances of derived types by name
    public delegate IntPtr CreateTypedInstanceDelegate(IntPtr typeName);
    public delegate IntPtr CreateTypedInstanceWithIDDelegate(IntPtr typeName, ulong entityID);

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
    public struct NativeArgs
    {
        public IntPtr msg;
        public int value;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct Vector3
    {
        public float X, Y, Z;

        public static Vector3 Zero = new Vector3(0.0f);

        public Vector3(float scalar)
        {
            X = scalar;
            Y = scalar;
            Z = scalar;
        }

        public Vector3(float x, float y, float z)
        {
            X = x;
            Y = y;
            Z = z;
        }

        public static Vector3 operator +(Vector3 a, Vector3 b)
        {
            return new Vector3(a.X + b.X, a.Y + b.Y, a.Z + b.Z);
        }

        public static Vector3 operator*(Vector3 vector, float scalar)
        {
            return new Vector3(vector.X * scalar, vector.Y * scalar, vector.Z * scalar);
        }
    }

    public static partial class InternalCalls
    {
        [LibraryImport("Timefall", StringMarshalling = StringMarshalling.Utf8)]
        internal static partial void NativeLog(string str, int parameter);

        [LibraryImport("Timefall")]
        internal static partial void NativeLog_Vector(ref Vector3 parameter, out Vector3 result);

        [LibraryImport("Timefall")]
        internal static partial float NativeLog_VectorDot(ref Vector3 parameter);

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

    public class Entity
    {
        public ulong ID { get; private set; }

        public Vector3 Translation
        {
            get
            {
                InternalCalls.Entity_GetTranslation(ID, out Vector3 translation);
                return translation;
            }
            set
            {
                InternalCalls.Entity_SetTranslation(ID, ref value);
            }
        }

        protected Entity() : this(0) { }
        internal Entity(ulong id) => ID = id;

        // Virtual lifecycle methods - override in derived classes
        public virtual void OnCreate() { }
        public virtual void OnUpdate(float ts) { }

        // Static invokers that take an instance handle and call virtual methods
        public static void InvokeOnCreate(IntPtr instancePtr)
        {
            GCHandle handle = GCHandle.FromIntPtr(instancePtr);
            Entity entity = (Entity)handle.Target!;
            entity.OnCreate();
        }

        public static void InvokeOnUpdate(IntPtr instancePtr, float ts)
        {
            GCHandle handle = GCHandle.FromIntPtr(instancePtr);
            Entity entity = (Entity)handle.Target!;
            entity.OnUpdate(ts);
        }

        // Factory method that creates an instance of a derived type by fully qualified name (Parameterless constructor only)
        public static IntPtr CreateTypedInstance(IntPtr typeNamePtr)
        {
            string? typeName = Marshal.PtrToStringAuto(typeNamePtr);
            if (string.IsNullOrEmpty(typeName))
            {
                Console.WriteLine("CreateTypedInstance: typeName is null or empty");
                return IntPtr.Zero;
            }

            // Find the type in loaded assemblies
            Type? type = null;
            foreach (var asm in AppDomain.CurrentDomain.GetAssemblies())
            {
                type = asm.GetType(typeName);
                if (type != null)
                    break;
            }

            if (type == null)
            {
                Console.WriteLine($"CreateTypedInstance: Type '{typeName}' not found");
                return IntPtr.Zero;
            }

            // Create instance using parameterless constructor
            Entity? instance = Activator.CreateInstance(type) as Entity;
            if (instance == null)
            {
                Console.WriteLine($"CreateTypedInstance: Failed to create instance of '{typeName}'");
                return IntPtr.Zero;
            }

            GCHandle handle = GCHandle.Alloc(instance);
            return GCHandle.ToIntPtr(handle);
        }

        // Factory method that creates an instance of a derived type by fully qualified name
        // Entity ID (ulong) from C++
        public static IntPtr CreateTypedInstanceWithID(IntPtr typeNamePtr, ulong entityID)
        {
            string? typeName = Marshal.PtrToStringAuto(typeNamePtr);
            if (string.IsNullOrEmpty(typeName))
            {
                Console.WriteLine("CreateTypedInstanceWithID: typeName is null or empty");
                return IntPtr.Zero;
            }

            // Find the type in loaded assemblies
            Type? type = null;
            foreach (var asm in AppDomain.CurrentDomain.GetAssemblies())
            {
                type = asm.GetType(typeName);
                if (type != null)
                    break;
            }

            if (type == null)
            {
                Console.WriteLine($"CreateTypedInstanceWithID: Type '{typeName}' not found");
                return IntPtr.Zero;
            }

            Entity? instance = Activator.CreateInstance(type, nonPublic: true) as Entity;
            if (instance != null)
                instance.ID = entityID;

            if (instance == null)
            {
                Console.WriteLine($"CreateTypedInstanceWithID: Failed to create instance of '{typeName}'");
                return IntPtr.Zero;
            }

            GCHandle handle = GCHandle.Alloc(instance);
            return GCHandle.ToIntPtr(handle);
        }

        public static void DestroyInstance(IntPtr instancePtr)
        {
            GCHandle handle = GCHandle.FromIntPtr(instancePtr);
            handle.Free();
            Console.WriteLine("Managed Entity Instance Destroyed");
        }
    }
}