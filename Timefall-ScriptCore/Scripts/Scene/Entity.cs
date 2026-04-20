using System.Runtime.InteropServices;

namespace Timefall
{
    public delegate void DestroyInstanceDelegate(IntPtr instance);

    // Delegates for invoking instance methods via handle
    public delegate void InvokeOnCreateDelegate(IntPtr instance);
    public delegate void InvokeOnUpdateDelegate(IntPtr instance, float ts);

    // Delegates for creating instances of derived types by name
    public delegate IntPtr CreateTypedInstanceDelegate(IntPtr typeName);
    public delegate IntPtr CreateTypedInstanceWithIDDelegate(IntPtr typeName, ulong entityID);


    public class Entity
    {
        public ulong ID { get; private set; }

        public Vector3 Translation
        {
            get
            {
                NativeCalls.TransformComponent_GetTranslation(ID, out Vector3 translation);
                return translation;
            }
            set
            {
                NativeCalls.TransformComponent_SetTranslation(ID, ref value);
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

        public bool HasComponent<T>() where T : Component, new()
        {
            return NativeCalls.Entity_HasComponent(ID, typeof(T).FullName);
        }

        public T GetComponent<T>() where T : Component, new()
        {
            if (!HasComponent<T>())
                return null;

            return new T() { Entity = this };
        }

        public Entity FindEntityByName(string name)
        {
            ulong foundID = NativeCalls.Entity_FindEntityByName(name);
            if (foundID == 0)
                return null;

            return new Entity(foundID);
        }

        public T As<T>() where T : Entity, new()
        {
            IntPtr ptr = NativeCalls.GetScriptInstance(ID);
            if (ptr == IntPtr.Zero)
                return null;

            return GCHandle.FromIntPtr(ptr).Target as T;
        }
    }
}