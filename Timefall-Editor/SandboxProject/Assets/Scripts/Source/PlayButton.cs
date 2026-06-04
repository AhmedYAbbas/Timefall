using Timefall;
using Timefall.UI;

namespace Sandbox
{
    // Concrete menu button: loads the Game scene when clicked.
    // Attach to an entity (with a SpriteRendererComponent quad) via ScriptComponent
    // module "Sandbox.PlayButton".
    public class PlayButton : Button
    {
        protected override void OnClick() => SceneManager.LoadScene("Game");
    }
}
