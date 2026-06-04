using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Timefall;

namespace Sandbox
{
    internal class MenuController : Entity
    {
        public override void OnUpdate(float ts)
        {
            if (Input.IsKeyPressedThisFrame(KeyCode.Enter))
                SceneManager.LoadScene("Game");
        }
    }
}
