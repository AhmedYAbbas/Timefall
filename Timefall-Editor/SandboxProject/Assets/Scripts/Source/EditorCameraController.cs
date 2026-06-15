using Timefall;

namespace Sandbox
{
    // Unity-style editor "flythrough" camera. Attach to the scene's active camera entity.
    // Hold Right Mouse to engage: drag to look, W/A/S/D to move, E/Q up/down, Shift to go faster.
    public class EditorCameraController : Entity
    {
        public float MoveSpeed = 5f;        // units per second
        public float FastMultiplier = 3f;   // Shift boost
        public float LookSensitivity = 0.005f; // radians per pixel of mouse movement

        private float m_Yaw;
        private float m_Pitch;
        private Vector2 m_LastMousePos;

        private const float PitchLimit = 1.5533f; // ~89deg, keeps us off the poles

        public override void OnCreate()
        {
            // Pick up wherever the camera is currently aimed.
            Vector3 rot = LocalRotation;
            m_Pitch = rot.X;
            m_Yaw = rot.Y;
        }

        public override void OnUpdate(float ts)
        {
            // Like Unity's editor camera, nothing happens unless Right Mouse is held.
            if (!Input.IsMouseButtonDown(MouseButton.Right))
                return;

            UpdateLook();
            UpdateMove(ts);
        }

        private void UpdateLook()
        {
            Vector2 mouse = Input.GetMousePosition();

            // On the frame RMB is first pressed, seed the anchor and skip the delta to avoid a jump.
            if (Input.IsMouseButtonPressedThisFrame(MouseButton.Right))
            {
                m_LastMousePos = mouse;
                return;
            }

            float dx = mouse.X - m_LastMousePos.X;
            float dy = mouse.Y - m_LastMousePos.Y;
            m_LastMousePos = mouse;

            m_Yaw -= dx * LookSensitivity;   // drag right -> look right
            m_Pitch -= dy * LookSensitivity; // drag up -> look up (mouse Y grows downward)

            if (m_Pitch > PitchLimit) m_Pitch = PitchLimit;
            else if (m_Pitch < -PitchLimit) m_Pitch = -PitchLimit;

            LocalRotation = new Vector3(m_Pitch, m_Yaw, 0f);
        }

        private void UpdateMove(float ts)
        {
            Vector3 forward = RotateByEuler(m_Pitch, m_Yaw, new Vector3(0f, 0f, -1f));
            Vector3 right = RotateByEuler(m_Pitch, m_Yaw, new Vector3(1f, 0f, 0f));
            Vector3 up = new Vector3(0f, 1f, 0f); // world up

            Vector3 dir = Vector3.Zero;

            if (Input.IsKeyDown(KeyCode.W)) dir += forward;
            else if (Input.IsKeyDown(KeyCode.S)) dir += forward * -1f;

            if (Input.IsKeyDown(KeyCode.D)) dir += right;
            else if (Input.IsKeyDown(KeyCode.A)) dir += right * -1f;

            if (Input.IsKeyDown(KeyCode.E)) dir += up;
            else if (Input.IsKeyDown(KeyCode.Q)) dir += up * -1f;

            if (dir.X == 0f && dir.Y == 0f && dir.Z == 0f)
                return;

            dir = Normalize(dir);

            float speed = MoveSpeed * (Input.IsKeyDown(KeyCode.LeftShift) ? FastMultiplier : 1f);
            LocalTranslation += dir * (speed * ts);
        }

        // Rotate a vector by the engine's Euler convention: glm::quat(vec3(pitch, yaw, roll)).
        // Mirrors glm's quat-from-euler so the move axes match exactly how the camera is oriented.
        private static Vector3 RotateByEuler(float pitch, float yaw, Vector3 v)
        {
            float cx = MathF.Cos(pitch * 0.5f), sx = MathF.Sin(pitch * 0.5f);
            float cy = MathF.Cos(yaw * 0.5f), sy = MathF.Sin(yaw * 0.5f);
            // roll = 0  ->  cz = 1, sz = 0
            float qw = cx * cy;
            float qx = sx * cy;
            float qy = cx * sy;
            float qz = -sx * sy;

            // v' = v + 2*cross(u, cross(u, v) + w*v), with u = (qx, qy, qz)
            Vector3 u = new Vector3(qx, qy, qz);
            Vector3 t = Cross(u, v) * 2f;
            return v + (Cross(u, t) + t * qw);
        }

        private static Vector3 Cross(Vector3 a, Vector3 b)
        {
            return new Vector3(
                a.Y * b.Z - a.Z * b.Y,
                a.Z * b.X - a.X * b.Z,
                a.X * b.Y - a.Y * b.X);
        }

        private static Vector3 Normalize(Vector3 v)
        {
            float len = MathF.Sqrt(v.X * v.X + v.Y * v.Y + v.Z * v.Z);
            if (len <= 0.00001f)
                return Vector3.Zero;
            return v * (1f / len);
        }
    }
}
