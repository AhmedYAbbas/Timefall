namespace Timefall
{
    public class Input
    {
        public static bool IsKeyDown(KeyCode keycode)
        {
            return NativeCalls.Input_IsKeyDown(keycode);
        }
    }
}
