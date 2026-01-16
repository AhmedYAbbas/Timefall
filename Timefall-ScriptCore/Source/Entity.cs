using System.Runtime.InteropServices;

namespace Timefall
{
    public delegate void PrintIntDelegate(int value);
    public delegate void PrintIntsDelegate(int value1, int value2);
    public delegate void PrintStringDelegate(IntPtr message);
    public delegate IntPtr CreateInstanceDelegate();
    public delegate IntPtr CreateIntInstanceDelegate(int value);
    public delegate void DestroyInstanceDelegate(IntPtr instance);

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

        public Vector3(float x, float y, float z)
        {
            X = x;
            Y = y;
            Z = z;
        }
    }

    public static partial class InternalCalls
    {
        [LibraryImport("Timefall-Native", StringMarshalling = StringMarshalling.Utf8)]
        internal static partial void NativeLog(string str, int parameter);

        [LibraryImport("Timefall-Native")]
        internal static partial void NativeLog_Vector(ref Vector3 parameter, out Vector3 result);

        [LibraryImport("Timefall-Native")]
        internal static partial float NativeLog_VectorDot(ref Vector3 parameter);
    }

    public class Entity
    {
        public int m_Value;

        public Entity()
        {
            Console.WriteLine("Managed TestClass Parameterless Constructor");
        }

        public Entity(int value)
        {
            m_Value = value;
            Console.WriteLine($"Managed TestClass Parameterized Constructor: value is {m_Value}");
        }

        public static IntPtr CreateInstance()
        {
            Entity instance = new Entity();
            GCHandle handle = GCHandle.Alloc(instance);
            return GCHandle.ToIntPtr(handle);
        }

        public static IntPtr CreateIntInstance(int value)
        {
            Entity instance = new Entity(value);
            GCHandle handle = GCHandle.Alloc(instance);
            return GCHandle.ToIntPtr(handle);
        }

        public static void DestroyInstance(IntPtr instancePtr)
        {
            GCHandle handle = GCHandle.FromIntPtr(instancePtr);
            handle.Free();
            Console.WriteLine("Managed TestClass Instance Destroyed");
        }

        public static int DefaultDelegate(IntPtr args, int sizeBytes)
        {
            var native = Marshal.PtrToStructure<NativeArgs>(args);
            string s = Marshal.PtrToStringAnsi(native.msg);

            Console.WriteLine($"Managed DefaultDelegate: {s}, {native.value}");
            return native.value;
        }

        public static void PrintInt(int value)
        {
            Console.WriteLine($"Managed PrintInt: {value}");
        }

        public static void PrintInts(int value1, int value2)
        {
            Console.WriteLine($"Managed PrintInts: {value1} and {value2}");
        }

        public static void PrintString(IntPtr message)
        {
            string msg = Marshal.PtrToStringAnsi(message);
            Console.WriteLine($"Managed PrintString C#: {msg}");

            Log("Soska", 1753);

            Vector3 vec = new Vector3(5.0f, 2.5f, 1.0f);
            Vector3 result = Log(vec);
            Console.WriteLine($"Managed Log Vector3 Result: X={result.X}, Y={result.Y}, Z={result.Z}");
            Console.WriteLine("{0}", InternalCalls.NativeLog_VectorDot(ref vec));
        }

        private static void Log(string text, int parameter)
        {
            InternalCalls.NativeLog(text, parameter);
        }

        private static Vector3 Log(Vector3 parameter)
        {
            InternalCalls.NativeLog_Vector(ref parameter, out Vector3 result);
            return result;
        }
    }
}