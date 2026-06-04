namespace Timefall
{
    public class Input
    {
        public static bool IsKeyDown(KeyCode keycode) => NativeCalls.Input_IsKeyDown(keycode);
        public static bool IsKeyPressedThisFrame(KeyCode keycode) => NativeCalls.Input_IsKeyPressedThisFrame(keycode);
        public static bool IsKeyReleasedThisFrame(KeyCode keycode) => NativeCalls.Input_IsKeyReleasedThisFrame(keycode);

        public static bool IsMouseButtonDown(MouseButton button) => NativeCalls.Input_IsMouseButtonDown(button);
        public static bool IsMouseButtonPressedThisFrame(MouseButton button) => NativeCalls.Input_IsMouseButtonPressedThisFrame(button);
        public static bool IsMouseButtonReleasedThisFrame(MouseButton button) => NativeCalls.Input_IsMouseButtonReleasedThisFrame(button);

        // Viewport-relative pixels (top-left origin).
        public static Vector2 GetMousePosition()
        {
            NativeCalls.Input_GetMousePosition(out Vector2 position);
            return position;
        }

        // World units, unprojected through the active scene's primary camera.
        public static Vector2 GetMouseWorldPosition()
        {
            NativeCalls.Input_GetMouseWorldPosition(out Vector2 position);
            return position;
        }
    }
}
