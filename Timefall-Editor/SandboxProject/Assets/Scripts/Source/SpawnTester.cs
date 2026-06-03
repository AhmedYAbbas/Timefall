using System;
using System.Collections.Generic;
using Timefall;

namespace Sandbox
{
    // Phase 0 plumbing proof: spawn / move / recolor / destroy entities from C# at runtime.
    // Attach this script to an entity in a scene, enter Play mode, then:
    //   Space     -> spawn a randomly-colored quad
    //   Backspace -> destroy all spawned quads
    //   P         -> parent all spawned quads under a new "Piece" entity (Phase 2 hierarchy test)
    // Spawned quads drift upward each frame to prove translation works; once parented, the parent
    // rotates and the quads should orbit it (proving SetParent world-preservation + world transforms).
    public class SpawnTester : Entity
    {
        private readonly List<Entity> m_Spawned = new();
        private readonly Random m_Random = new();

        // Held-state edge tracking (interim until native input edges land in Phase 5).
        private bool m_SpawnHeld;
        private bool m_ClearHeld;
        private bool m_ParentHeld;

        private Entity? m_Parent;

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

            bool parentDown = Input.IsKeyDown(KeyCode.P);
            if (parentDown && !m_ParentHeld)
                ParentAll();
            m_ParentHeld = parentDown;

            if (m_Parent != null)
            {
                // Rotate the parent — children should orbit it, proving SetParent (world-preserved)
                // and world-transform inheritance in the renderer.
                Vector3 r = m_Parent.LocalRotation;
                r.Z += ts;
                m_Parent.LocalRotation = r;
            }
            else
            {
                // Move every spawned quad upward to prove local translation get/set round-trips.
                foreach (Entity e in m_Spawned)
                {
                    Vector3 t = e.LocalTranslation;
                    t.Y += MoveSpeed * ts;
                    e.LocalTranslation = t;
                }
            }
        }

        private void ParentAll()
        {
            if (m_Spawned.Count == 0 || m_Parent != null)
                return;

            m_Parent = Entity.Create("Piece");
            foreach (Entity e in m_Spawned)
                e.Parent = m_Parent;

            Console.WriteLine($"Parented {m_Spawned.Count} quads under {m_Parent.ID}");
        }

        private void SpawnQuad()
        {
            Entity quad = Entity.Create($"SpawnedQuad_{m_Spawned.Count}");
            quad.LocalTranslation = new Vector3(-4.0f + m_Spawned.Count * 1.2f, -3.0f, 0.0f);

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

            if (m_Parent != null)
            {
                m_Parent.Destroy(); // recursive destroy also covers any still-parented children
                m_Parent = null;
            }
        }
    }
}
