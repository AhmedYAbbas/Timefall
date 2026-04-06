using System.Reflection;
using System.Runtime.InteropServices;
using System.Runtime.Loader;

namespace Timefall
{
    public delegate void BuildEntityRegistryDelegate(IntPtr assemblyName);
    public delegate void GetFieldValueDelegate(IntPtr instanceHandle, IntPtr fieldName, IntPtr outValue);
    public delegate void SetFieldValueDelegate(IntPtr instanceHandle, IntPtr fieldName, IntPtr value);

    public class TypeRegistry
    {
        // Directories the resolver will probe when the runtime can't find a dependency by name.
        // Populated each time we load a new assembly, so future loads benefit too.
        private static readonly HashSet<string> s_ProbingPaths = new();
        private static bool s_ResolverRegistered = false;

        public static void BuildEntityRegistry(IntPtr assemblyName)
        {
            string resolvedPath = ResolveAssemblyPath(assemblyName);
            if (string.IsNullOrEmpty(resolvedPath))
            {
                Console.WriteLine("BuildRegistry: resolved path is null or empty");
                return;
            }

            var asm = LoadOrGetAssembly(resolvedPath);
            Console.WriteLine($"Loaded: {asm.FullName}");
            Console.WriteLine($"Loaded assembly path: {asm.Location}");

            foreach (var type in asm.GetTypes())
            {
                if (type.IsAssignableTo(typeof(Entity)) && type != typeof(Entity))
                {
                    Console.WriteLine($"Registering Entity Type: {type.FullName} from Assembly: {asm.GetName().Name}");
                    FieldInfo[] fields = type.GetFields(BindingFlags.Instance | BindingFlags.Public);
                    Console.WriteLine($"  Found {fields.Length} fields:");
                    foreach (FieldInfo field in fields)
                    {
                        Console.WriteLine($"  Field: {field.Name} of Type: {field.FieldType.FullName}");
                    }
                    string[] fieldNames = fields.Select(f => f.Name).ToArray();
                    string[] fieldTypeNames = fields.Select(f => f.FieldType.FullName ?? string.Empty).ToArray();
                    NativeCalls.Native_RegisterEntityTypes(type.FullName, asm.GetName().Name, fieldNames, fieldTypeNames, fields.Length);
                }
            }
        }

        public static void GetFieldValue(IntPtr instanceHandle, IntPtr fieldNamePtr, IntPtr outValue)
        {
            GCHandle handle = GCHandle.FromIntPtr(instanceHandle);
            object? instance = handle.Target;
            if (instance == null)
                return;

            string? fieldName = Marshal.PtrToStringUni(fieldNamePtr);
            if (fieldName == null)
                return;

            FieldInfo? field = instance.GetType().GetField(fieldName, BindingFlags.Instance | BindingFlags.Public);
            if (field == null)
                return;

            object? value = field.GetValue(instance);
            if (value == null)
                return;

            Marshal.StructureToPtr(value, outValue, false);
        }
        
        public static void SetFieldValue(IntPtr instanceHandle, IntPtr fieldNamePtr, IntPtr value)
        {
            GCHandle handle = GCHandle.FromIntPtr(instanceHandle);
            object? instance = handle.Target;
            if (instance == null)
                return;

            string? fieldName = Marshal.PtrToStringUni(fieldNamePtr);
            if (fieldName == null)
                return;

            FieldInfo? field = instance.GetType().GetField(fieldName, BindingFlags.Instance | BindingFlags.Public);
            if (field == null)
                return;

            object? boxedValue = Marshal.PtrToStructure(value, field.FieldType);
            if (boxedValue == null)
                return;

            field.SetValue(instance, boxedValue);
        }

        private static string ResolveAssemblyPath(IntPtr assemblyName)
        {
            string? assemblyPath = Marshal.PtrToStringUTF8(assemblyName);

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

        /// <summary>
        /// Returns an already-loaded assembly if one matches by name,
        /// otherwise loads it from disk into the Default ALC.
        /// </summary>
        private static Assembly LoadOrGetAssembly(string path)
        {
            var assemblyName = AssemblyName.GetAssemblyName(path);

            // If an assembly with this name is already loaded (e.g. by hostfxr), reuse it
            foreach (var asm in AppDomain.CurrentDomain.GetAssemblies())
            {
                if (AssemblyName.ReferenceMatchesDefinition(asm.GetName(), assemblyName))
                    return asm;
            }

            // Add the directory to probing paths so the resolver can find sibling DLLs
            string assemblyDir = Path.GetDirectoryName(Path.GetFullPath(path))!;
            s_ProbingPaths.Add(assemblyDir);

            EnsureResolverRegistered();

            return AssemblyLoadContext.Default.LoadFromAssemblyPath(path);
        }

        /// <summary>
        /// Registers a one-time handler on the Default ALC's Resolving event.
        ///
        /// This event fires when the runtime cannot find a dependency through its
        /// normal resolution (deps.json, framework dirs, probing paths). Our handler:
        ///   1. Checks if the assembly is already loaded (avoids duplicate type identity)
        ///   2. Probes registered directories for the DLL on disk
        /// </summary>
        private static void EnsureResolverRegistered()
        {
            if (s_ResolverRegistered)
                return;

            s_ResolverRegistered = true;

            AssemblyLoadContext.Default.Resolving += (context, name) =>
            {
                // 1. If the assembly is already in memory, return it.
                //    This is critical: loading a second copy from disk would create
                //    duplicate types that are incompatible with the originals.
                foreach (var loaded in AppDomain.CurrentDomain.GetAssemblies())
                {
                    if (AssemblyName.ReferenceMatchesDefinition(loaded.GetName(), name))
                        return loaded;
                }

                // 2. Probe the directories of previously loaded assemblies
                foreach (var dir in s_ProbingPaths)
                {
                    string candidate = Path.Combine(dir, name.Name + ".dll");
                    if (File.Exists(candidate))
                        return context.LoadFromAssemblyPath(candidate);
                }

                return null;
            };
        }
    }
}
