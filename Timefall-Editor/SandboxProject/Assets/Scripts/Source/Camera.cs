using Timefall;

namespace Sandbox
{
    public class Camera : Entity
    {
        public float DistanceFromPlayer = 20.0f;

        private Entity m_Player;

        public override void OnCreate()
        {
            m_Player = FindEntityByName("Player");
        }

        public override void OnUpdate(float ts)
        {
            if (m_Player != null)
                LocalTranslation = new Vector3(m_Player.LocalTranslation.XY, DistanceFromPlayer);

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
            DistanceFromPlayer += ts;

            Vector3 translation = LocalTranslation;
            translation += velocity * ts;
            LocalTranslation = translation;
        }
    }
}
