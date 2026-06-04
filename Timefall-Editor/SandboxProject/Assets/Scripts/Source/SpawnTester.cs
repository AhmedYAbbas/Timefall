using System;
using System.Collections.Generic;
using Timefall;

namespace Sandbox
{
    // Phase 0 plumbing proof: spawn / move / recolor / destroy entities from C# at runtime.
    // Attach this script to an entity in a scene, enter Play mode, then:
    //   Space      -> spawn a randomly-colored quad
    //   Backspace  -> destroy all spawned quads
    //   P          -> parent all spawned quads under a new "Piece" entity (Phase 2 hierarchy test)
    //   B          -> spawn BlockTag-tagged entities + query them (Phase 4 query test)
    //   Left-click -> spawn a quad at the cursor's world position (Phase 5 mouse->world test)
    // Spawned quads drift upward each frame to prove translation works; once parented, the parent
    // rotates and the quads should orbit it (proving SetParent world-preservation + world transforms).
    public class SpawnTester : Entity
    {
        private readonly List<Entity> m_Spawned = new();
        private readonly Random m_Random = new();

        private Entity? m_Parent;

        public float MoveSpeed = 2.0f;

        public override void OnCreate()
        {
            Console.WriteLine($"SpawnTester.OnCreate - {ID}. Space = spawn, Backspace = clear.");
        }

        public override void OnUpdate(float ts)
        {
            // Native per-frame edges (Phase 5): one action per tap, no manual held-state tracking.
            if (Input.IsKeyPressedThisFrame(KeyCode.Space))
                SpawnQuad();

            if (Input.IsKeyPressedThisFrame(KeyCode.Backspace))
                ClearAll();

            if (Input.IsKeyPressedThisFrame(KeyCode.P))
                ParentAll();

            if (Input.IsKeyPressedThisFrame(KeyCode.B))
                BlockEntity();

            // Left-click spawns a quad at the cursor's world position — proves mouse -> world unprojection.
            if (Input.IsMouseButtonPressedThisFrame(MouseButton.Left))
                SpawnQuadAt(Input.GetMouseWorldPosition());

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

        // Phase 4 query test: tag a few entities with BlockTag, then enumerate them by type.
        private void BlockEntity()
        {
            for (int i = 0; i < 3; i++)
            {
                Entity e = Entity.Create($"Block{i}");
                e.AddComponent<BlockTag>();
            }
            Entity.Create("NotABlock");

            Entity[] blocks = GetEntitiesWith<BlockTag>();
            Entity[] sprites = GetEntitiesWith<SpriteRendererComponent>();
            Console.WriteLine($"GetEntitiesWith<BlockTag> = {blocks.Length} (expect 3, +3 each press)");
            Console.WriteLine($"GetEntitiesWith<SpriteRendererComponent> = {sprites.Length}");
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

        // Spawns a yellow quad at a world position (used for the mouse->world click test).
        private void SpawnQuadAt(Vector2 worldPosition)
        {
            Entity quad = Entity.Create($"ClickedQuad_{m_Spawned.Count}");
            quad.LocalTranslation = new Vector3(worldPosition.X, worldPosition.Y, 0.0f);

            SpriteRendererComponent sprite = quad.AddComponent<SpriteRendererComponent>();
            sprite.Color = new Vector4(1.0f, 1.0f, 0.0f, 1.0f);

            m_Spawned.Add(quad);
            Console.WriteLine($"Clicked world ({worldPosition.X:F2}, {worldPosition.Y:F2}) -> quad {quad.ID}");
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
