using System.Reflection;
using System.Runtime.InteropServices;
using System.Runtime.Loader;

namespace Timefall
{
    public delegate void BuildEntityRegistryDelegate(IntPtr assemblyName);

    public static partial class TypeRegistryInternalCalls
    {
        [LibraryImport("Timefall", StringMarshalling = StringMarshalling.Utf16)]
        internal static partial void Native_RegisterEntityTypes(string? typeName, string? assemblyName);
    }

    public class TypeRegistry
    {
        public static void BuildEntityRegistry(IntPtr assemblyName)
        { 
            string resolvedPath = ResolveAssemblyPath(assemblyName);
            if (string.IsNullOrEmpty(resolvedPath))
            {
                Console.WriteLine("BuildRegistry: resolved path is null or empty");
                return;
            }

            // Load into the default AssemblyLoadContext to avoid duplicate assembly identity problems
            var asm = LoadOrGetAssembly(resolvedPath);
            Console.WriteLine($"Loaded: {asm.FullName}");
            Console.WriteLine($"Loaded assembly path: {asm.Location}");

            foreach (var type in asm.GetTypes())
            {
                if (type.IsAssignableTo(typeof(Entity)) && type != typeof(Entity))
                {
                    Console.WriteLine($"Registering Entity Type: {type.FullName} from Assembly: {asm.GetName().Name}");
                    TypeRegistryInternalCalls.Native_RegisterEntityTypes(type.FullName, asm.GetName().Name);
                }
            }
        }

        private static string ResolveAssemblyPath(IntPtr assemblyName)
        {
            string assemblyPath = Marshal.PtrToStringUTF8(assemblyName);

            if (string.IsNullOrEmpty(assemblyPath))
            {
                Console.WriteLine("ScanAssembly: path is null or empty");
                return string.Empty;
            }

            string resolvedPath = Path.Combine(Environment.CurrentDirectory, assemblyPath);
            if (!File.Exists(resolvedPath))
            {
                Console.WriteLine($"ScanAssembly: file not found: {resolvedPath}");
                return string.Empty;
            }
           
            return resolvedPath;
        }

        private static Assembly LoadOrGetAssembly(string path)
        {
            var loadedAssemblies = AppDomain.CurrentDomain.GetAssemblies();
            var assemblyName = AssemblyName.GetAssemblyName(path);

            foreach (var asm in loadedAssemblies)
            {
                if (AssemblyName.ReferenceMatchesDefinition(asm.GetName(), assemblyName))
                    return asm;
            }

            return AssemblyLoadContext.Default.LoadFromAssemblyPath(path);
        }
    }
}
