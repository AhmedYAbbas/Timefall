using System;
using System.Collections.Generic;
using Timefall;

namespace Sandbox
{
    // Phase 0 plumbing proof: spawn / move / recolor / destroy entities from C# at runtime.
    // Attach this script to an entity in a scene, enter Play mode, then:
    //   Space     -> spawn a randomly-colored quad
    //   Backspace -> destroy all spawned quads
    // Spawned quads drift upward each frame to prove translation works.
    public class SpawnTester : Entity
    {
        private readonly List<Entity> m_Spawned = new();
        private readonly Random m_Random = new();

        // Held-state edge tracking (interim until native input edges land in Phase 5).
        private bool m_SpawnHeld;
        private bool m_ClearHeld;

        public float MoveSpeed = 2.0f;

        public override void OnCreate()
        {
            Console.WriteLine($"SpawnTester.OnCreate - {ID}. Space = spawn, Backspace = clear.");
        }

        public override void OnUpdate(float ts)
        {
            bool spawnDown = Input.IsKeyDown(KeyCode.Space);
            if (spawnDown && !m_SpawnHeld)
                SpawnQuad();
            m_SpawnHeld = spawnDown;

            bool clearDown = Input.IsKeyDown(KeyCode.Backspace);
            if (clearDown && !m_ClearHeld)
                ClearAll();
            m_ClearHeld = clearDown;

            // Move every spawned quad upward to prove Translation get/set round-trips.
            foreach (Entity e in m_Spawned)
            {
                Vector3 t = e.Translation;
                t.Y += MoveSpeed * ts;
                e.Translation = t;
            }
        }

        private void SpawnQuad()
        {
            Entity quad = Entity.Create($"SpawnedQuad_{m_Spawned.Count}");
            quad.Translation = new Vector3(-4.0f + m_Spawned.Count * 1.2f, -3.0f, 0.0f);

            SpriteRendererComponent sprite = quad.AddComponent<SpriteRendererComponent>();
            sprite.Color = new Vector4(
                (float)m_Random.NextDouble(),
                (float)m_Random.NextDouble(),
                (float)m_Random.NextDouble(),
                1.0f);

            m_Spawned.Add(quad);
            Console.WriteLine($"Spawned {quad.ID} (total {m_Spawned.Count})");
        }

        private void ClearAll()
        {
            foreach (Entity e in m_Spawned)
                e.Destroy();

            Console.WriteLine($"Destroyed {m_Spawned.Count} entities");
            m_Spawned.Clear();
        }
    }
}
