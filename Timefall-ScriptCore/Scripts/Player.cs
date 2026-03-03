using Timefall;

namespace Sandbox
{
    public class Player : Entity
    {
        public override void OnCreate()
        {
            Console.WriteLine($"Sandbox.Player OnCreate called! - {ID}");
        }

        public override void OnUpdate(float ts)
        {
            Console.WriteLine($"Sandbox.Player OnUpdate called with timestep: {ts}");

            float speed = 1.0f;
            Vector3 velocity = Vector3.Zero;

            if (Input.IsKeyDown(KeyCode.W))
                velocity.Y += 1.0f;
            else if (Input.IsKeyDown(KeyCode.S))
                velocity.Y -= 1.0f;

            if (Input.IsKeyDown(KeyCode.A))
                velocity.X -= 1.0f;
            else if (Input.IsKeyDown(KeyCode.D))
                velocity.X += 1.0f;

            velocity *= speed;

            Vector3 translation = Translation;
            translation += velocity * ts;
            Translation = translation;
        }
    }
}
