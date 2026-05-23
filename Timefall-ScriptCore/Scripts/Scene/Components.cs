namespace Timefall
{
    public abstract class Component
    {
        public Entity Entity { get; internal set; } = null!;
    }

    public class TransformComponent : Component
    {
        public Vector3 Translation
        {
            get
            {
                NativeCalls.TransformComponent_GetTranslation(Entity.ID, out Vector3 translation);
                return translation;
            }
            set
            {
                NativeCalls.TransformComponent_SetTranslation(Entity.ID, ref value);
            }
        }
    }
    
    public class Rigidbody2DComponent : Component
    {
        public void ApplyLinearImpulse(Vector2 impulse, Vector2 worldPosition, bool wake = true)
        {
            NativeCalls.Rigidbody2DComponent_ApplyLinearImpulse(Entity.ID, ref impulse, ref worldPosition, wake);
        }

        public void ApplyLinearImpulse(Vector2 impulse, bool wake = true)
        {
            NativeCalls.Rigidbody2DComponent_ApplyLinearImpulseToCenter(Entity.ID, ref impulse, wake);
        }
    }
}
