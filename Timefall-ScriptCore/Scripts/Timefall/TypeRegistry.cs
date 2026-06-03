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
        private static readonly HashSet<string> s_ProbingPaths = new();
        private static bool s_DefaultResolverRegistered = false;

        // Collectible ALC for app assemblies — replaced on each hot-reload.
        private static AssemblyLoadContext? s_AppLoadContext = null;

        // Fast type lookup by fully-qualified name, rebuilt on every BuildEntityRegistry call.
        private static readonly Dictionary<string, Type> s_RegisteredTypes = new();

        public static Type? FindType(string typeName)
        {
            s_RegisteredTypes.TryGetValue(typeName, out Type? type);
            return type;
        }

        public static void BuildEntityRegistry(IntPtr assemblyNamePtr)
        {
            string resolvedPath = ResolveAssemblyPath(assemblyNamePtr);
            if (string.IsNullOrEmpty(resolvedPath))
            {
                Console.WriteLine("BuildRegistry: resolved path is null or empty. Could not load App Assembly.");
                return;
            }

            // Drop previously registered types and unload old collectible ALC.
            s_RegisteredTypes.Clear();
            if (s_AppLoadContext != null)
            {
                s_AppLoadContext.Unload();
                s_AppLoadContext = null;
                GC.Collect();
                GC.WaitForPendingFinalizers();
            }

            string assemblyDir = Path.GetDirectoryName(Path.GetFullPath(resolvedPath))!;
            s_ProbingPaths.Add(assemblyDir);
            EnsureDefaultResolverRegistered();

            // Load into a new collectible ALC so the next reload can unload this one.
            // Use LoadFromStream so the source file is not locked — the bytes are read into
            // memory and the file handle is released before we return.
            s_AppLoadContext = new AssemblyLoadContext("AppAssembly", isCollectible: true);
            s_AppLoadContext.Resolving += ResolveForAppContext;

            byte[] assemblyBytes = File.ReadAllBytes(resolvedPath);
            using var assemblyStream = new MemoryStream(assemblyBytes);
            var asm = s_AppLoadContext.LoadFromStream(assemblyStream);
            Console.WriteLine($"Loaded: {asm.FullName}");
            Console.WriteLine($"Loaded assembly path: {resolvedPath}");

            foreach (var type in asm.GetTypes())
            {
                if (type.IsAssignableTo(typeof(Entity)) && type != typeof(Entity))
                {
                    Console.WriteLine($"Registering Entity Type: {type.FullName} from Assembly: {asm.GetName().Name}");
                    FieldInfo[] fields = type.GetFields(BindingFlags.Instance | BindingFlags.Public);
                    Console.WriteLine($"  Found {fields.Length} fields:");
                    foreach (FieldInfo field in fields)
                        Console.WriteLine($"  Field: {field.Name} of Type: {field.FieldType.FullName}");

                    s_RegisteredTypes[type.FullName!] = type;

                    string[] fieldNames = fields.Select(f => f.Name).ToArray();
                    string[] fieldTypeNames = fields.Select(f => f.FieldType.FullName ?? string.Empty).ToArray();
                    NativeCalls.Native_RegisterEntityTypes(type.FullName, asm.GetName().Name, fieldNames, fieldTypeNames, fields.Length);
                }
                else if (type.IsAssignableTo(typeof(Component)) && type != typeof(Component) && !type.IsAbstract)
                {
                    Console.WriteLine($"Registering Component Type: {type.FullName} from Assembly: {asm.GetName().Name}");
                    PropertyInfo[] props = type.GetProperties(BindingFlags.Instance | BindingFlags.Public)
                                               .Where(p => p.CanRead && p.CanWrite
                                                        && p.GetIndexParameters().Length == 0
                                                        && p.DeclaringType != typeof(Component))
                                               .ToArray();

                    s_RegisteredTypes[type.FullName!] = type;

                    string[] fieldNames = props.Select(p => p.Name).ToArray();
                    string[] fieldTypeNames = props.Select(p => p.PropertyType.FullName ?? string.Empty).ToArray();
                    NativeCalls.Native_RegisterComponentTypes(type.FullName, asm.GetName().Name, fieldNames, fieldTypeNames, props.Length);
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

        private static Assembly? ResolveForAppContext(AssemblyLoadContext ctx, AssemblyName name)
        {
            // Prefer already-loaded assemblies (e.g. ScriptCore) to keep type identity consistent.
            foreach (var loaded in AppDomain.CurrentDomain.GetAssemblies())
            {
                if (AssemblyName.ReferenceMatchesDefinition(loaded.GetName(), name))
                    return loaded;
            }

            foreach (var dir in s_ProbingPaths)
            {
                string candidate = Path.Combine(dir, name.Name + ".dll");
                if (File.Exists(candidate))
                    return AssemblyLoadContext.Default.LoadFromAssemblyPath(candidate);
            }

            return null;
        }

        private static string ResolveAssemblyPath(IntPtr assemblyName)
        {
            string? assemblyPath = Marshal.PtrToStringUni(assemblyName);

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

        private static void EnsureDefaultResolverRegistered()
        {
            if (s_DefaultResolverRegistered)
                return;

            s_DefaultResolverRegistered = true;

            AssemblyLoadContext.Default.Resolving += (context, name) =>
            {
                foreach (var loaded in AppDomain.CurrentDomain.GetAssemblies())
                {
                    if (AssemblyName.ReferenceMatchesDefinition(loaded.GetName(), name))
                        return loaded;
                }

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
