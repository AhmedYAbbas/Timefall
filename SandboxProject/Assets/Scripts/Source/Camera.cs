using Timefall;

namespace Sandbox
{
    public class Camera : Entity
    {
        public override void OnUpdate(float ts)
        {
            float speed = 1f;
            Vector3 velocity = Vector3.Zero;

            if (Input.IsKeyDown(KeyCode.Up))
                velocity.Y += 1f;
            else if (Input.IsKeyDown(KeyCode.Down))
                velocity.Y -= 1f;

            if (Input.IsKeyDown(KeyCode.Left))
                velocity.X -= 1f;
            else if (Input.IsKeyDown(KeyCode.Right))
                velocity.X += 1f;

            velocity *= speed;

            Vector3 translation = Translation;
            translation += velocity * ts;
            Translation = translation;
        }
    }
}
