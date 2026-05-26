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
        public enum BodyType { Static = 0, Dynamic = 1, Kinematic = 2 }

        public Vector2 LinearVelocity
        {
            get
            {
                NativeCalls.Rigidbody2DComponent_GetLinearVelocity(Entity.ID, out Vector2 linearVelocity);
                return linearVelocity;
            }
        }

        public BodyType Type
        {
            get => NativeCalls.Rigidbody2DComponent_GetBodyType(Entity.ID);
            set => NativeCalls.Rigidbody2DComponent_SetBodyType(Entity.ID, value);
        }

        public void ApplyLinearImpulse(Vector2 impulse, Vector2 worldPosition, bool wake = true)
        {
            NativeCalls.Rigidbody2DComponent_ApplyLinearImpulse(Entity.ID, ref impulse, ref worldPosition, wake);
        }

        public void ApplyLinearImpulse(Vector2 impulse, bool wake = true)
        {
            NativeCalls.Rigidbody2DComponent_ApplyLinearImpulseToCenter(Entity.ID, ref impulse, wake);
        }
    }

    public class TextComponent : Component
    {

        public string Text
        {
            get => NativeCalls.TextComponent_GetText(Entity.ID);
            set => NativeCalls.TextComponent_SetText(Entity.ID, value);
        }

        public Vector4 Color
        {
            get
            {
                NativeCalls.TextComponent_GetColor(Entity.ID, out Vector4 color);
                return color;
            }

            set
            {
                NativeCalls.TextComponent_SetColor(Entity.ID, ref value);
            }
        }

        public float Kerning
        {
            get => NativeCalls.TextComponent_GetKerning(Entity.ID);
            set => NativeCalls.TextComponent_SetKerning(Entity.ID, value);
        }

        public float LineSpacing
        {
            get => NativeCalls.TextComponent_GetLineSpacing(Entity.ID);
            set => NativeCalls.TextComponent_SetLineSpacing(Entity.ID, value);
        }

    }
}
