using Timefall;

namespace Sandbox
{
    public class Player : Entity
    {
        private TransformComponent m_Transform;
        private Rigidbody2DComponent m_Rigidbody;

        public float Speed;
        public float Time;
        public override void OnCreate()
        {
            Console.WriteLine($"Sandbox.Player OnCreate called! - {ID}");

            m_Transform = GetComponent<TransformComponent>();
            m_Rigidbody = GetComponent<Rigidbody2DComponent>();
        }

        public override void OnUpdate(float ts)
        {
            //Console.WriteLine($"Sandbox.Player OnUpdate called with timestep: {ts}");
            Time += ts;

            Vector3 velocity = Vector3.Zero;

            if (Input.IsKeyDown(KeyCode.W))
                velocity.Y += 1f;
            else if (Input.IsKeyDown(KeyCode.S))
                velocity.Y -= 1f;

            if (Input.IsKeyDown(KeyCode.A))
                velocity.X -= 1f;
            else if (Input.IsKeyDown(KeyCode.D))
                velocity.X += 1f;

            velocity *= Speed * ts;

            m_Rigidbody.ApplyLinearImpulse(velocity.XY, true);

            //Vector3 translation = m_Transform.Translation;
            //translation += velocity * ts;
            //m_Transform.Translation = translation;
        }
    }
}
