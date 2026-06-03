using Timefall;

namespace Sandbox
{
    public class Player : Entity
    {
        private TransformComponent m_Transform;
        private Rigidbody2DComponent m_Rigidbody;

        public float Speed = 40f;
        public float Time;
        public override void OnCreate()
        {
            Console.WriteLine($"Sandbox.Player OnCreate called! - {ID}");

            m_Transform = GetComponent<TransformComponent>();
            m_Rigidbody = GetComponent<Rigidbody2DComponent>();

            var t = AddComponent<BlockTag>();
            t.Kind = 7;
            
            Console.WriteLine($"Has block tag: {HasComponent<BlockTag>()}");
            RemoveComponent<BlockTag>();
            Console.WriteLine($"Has block tag after removal: {HasComponent<BlockTag>()}");
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

            Entity cameraEntity = FindEntityByName("Camera");
            if (cameraEntity != null)
            {
                Camera camera = cameraEntity.As<Camera>();
                if (camera == null)
                    return;

                if (Input.IsKeyDown(KeyCode.Q))
                    camera.DistanceFromPlayer += Speed * 2.0f * ts;
                else if (Input.IsKeyDown(KeyCode.E))
                    camera.DistanceFromPlayer -= Speed * 2.0f * ts;
            }

            velocity *= Speed * ts;

            m_Rigidbody.ApplyLinearImpulse(velocity.XY, true);

            //Vector3 translation = m_Transform.Translation;
            //translation += velocity * ts;
            //m_Transform.Translation = translation;
        }
    }
}
