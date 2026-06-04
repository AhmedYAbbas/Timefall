namespace Timefall.UI
{
    // Minimal world-space clickable button. Put a subclass on an entity that has a
    // SpriteRendererComponent quad; an optional child entity holds the TextComponent label.
    // Override OnClick() in the subclass.
    //
    // Limitation: the hit-test is an axis-aligned bounding box, so it assumes an UNROTATED
    // quad. Rotation/skew are ignored (fine for menus; real screen-space UI is Phase 9).
    public abstract class Button : Entity
    {
        // Feedback multipliers applied to the authored sprite color.
        public float HoverBrightness = 1.2f;
        public float PressedBrightness = 0.8f;

        private SpriteRendererComponent? m_Sprite;
        private Vector4 m_BaseColor;
        private bool m_Armed; // mouse pressed down while inside; click fires on release-inside

        public override void OnCreate()
        {
            m_Sprite = GetComponent<SpriteRendererComponent>();
            if (m_Sprite != null)
                m_BaseColor = m_Sprite.Color;
        }

        public override void OnUpdate(float ts)
        {
            Vector2 mouse = Input.GetMouseWorldPosition();
            bool inside = Contains(mouse);

            // Arm on press-inside.
            if (inside && Input.IsMouseButtonPressedThisFrame(MouseButton.Left))
                m_Armed = true;

            // Fire only on a clean release-inside; release-outside cancels.
            if (Input.IsMouseButtonReleasedThisFrame(MouseButton.Left))
            {
                if (m_Armed && inside)
                    OnClick();
                m_Armed = false;
            }

            // Safety: any mouse-up state with the button not held disarms.
            if (!Input.IsMouseButtonDown(MouseButton.Left))
                m_Armed = false;

            UpdateVisual(inside);
        }

        // Override in concrete buttons to define the action.
        protected virtual void OnClick() { }

        // Point-in-AABB using the quad's world center and world size.
        private bool Contains(Vector2 p)
        {
            Vector3 c = WorldTranslation;
            Vector3 s = WorldScale;
            float hx = MathF.Abs(s.X) * 0.5f;
            float hy = MathF.Abs(s.Y) * 0.5f;
            return p.X >= c.X - hx && p.X <= c.X + hx
                && p.Y >= c.Y - hy && p.Y <= c.Y + hy;
        }

        private void UpdateVisual(bool inside)
        {
            if (m_Sprite == null)
                return;

            float k = (m_Armed && inside) ? PressedBrightness : (inside ? HoverBrightness : 1.0f);
            m_Sprite.Color = new Vector4(m_BaseColor.X * k, m_BaseColor.Y * k, m_BaseColor.Z * k, m_BaseColor.W);
        }
    }
}
