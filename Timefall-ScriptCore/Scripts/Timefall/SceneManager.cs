namespace Timefall
{
    public static class SceneManager
    {
        // Switches the active scene to the named scene asset (file stem, e.g. "Game") at the next
        // safe frame boundary. Safe to call from a script's OnUpdate.
        public static void LoadScene(string name) => NativeCalls.SceneManager_LoadScene(name);
    }
}
