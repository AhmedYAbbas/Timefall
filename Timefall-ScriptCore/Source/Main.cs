using System.Runtime.InteropServices;

namespace MyNamespace
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

    public class TestClass
    {
        public int m_Value;

        public TestClass()
        {
            Console.WriteLine("Managed TestClass Parameterless Constructor");
        }

        public TestClass(int value)
        {
            m_Value = value;
            Console.WriteLine($"Managed TestClass Parameterized Constructor: value is {m_Value}");
        }

        public static IntPtr CreateInstance()
        {
            TestClass instance = new TestClass();
            GCHandle handle = GCHandle.Alloc(instance);
            return GCHandle.ToIntPtr(handle);
        }

        public static IntPtr CreateIntInstance(int value)
        {
            TestClass instance = new TestClass(value);
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
            Console.WriteLine($"Managed PrintString: {msg}");
        }
    }
}
