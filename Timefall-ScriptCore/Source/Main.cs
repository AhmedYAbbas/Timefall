using System.Runtime.InteropServices;

namespace MyNamespace
{
    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
    public struct NativeArgs
    {
        public IntPtr msg;
        public int value;
    }

    public class TestClass
    {
        public static int MyStaticMethod(IntPtr args, int sizeBytes)
        {
            var native = Marshal.PtrToStructure<NativeArgs>(args);
            string s = Marshal.PtrToStringAnsi(native.msg);

            Console.WriteLine($"Managed received: {s}, {native.value}");
            return native.value;
        }
    }
}
