---
name: timefall-new-render-pass
description: Add a new render pass or post-process effect to the Timefall engine's Renderer3D — bloom, SSAO, outline, fog, depth prepass, tonemap stage, any fullscreen effect, or any pass that draws the scene into its own buffer. Use this whenever a task adds a shader that runs as part of the 3D frame, inserts work into Renderer3D::EndScene, adds a new framebuffer/render target, or adds renderer tunables that need an editor panel and scene serialization ("add a bloom pass", "SSAO", "post-process effect X", "render the scene into a buffer for Y", "expose a renderer setting in the editor"). Also triggers on the tell-tale bugs of a mis-wired pass — the effect renders but the 2D overlay, entity-ID mouse picking, or gizmos break afterward (unrestored GL state / draw buffers), the effect works until the viewport is resized (missing lazy framebuffer rebuild), or a setting works in the editor but resets on scene save/load (missing serializer entry). Not for 2D batch rendering (Renderer2D), for changing material/BRDF logic inside the existing lit shader, for one-off offline bakes like IBL prefiltering (see the Environment pattern note at the bottom), or for adding entity components (that is timefall-add-component).
---

# Timefall: Add a Render Pass

A pass isn't "done" when it draws. It must slot into the frame at the right point, leave GL state
exactly as it found it, survive viewport resize, and — if it has tunables — plumb them through
**seven places** so they show in the editor, survive play-mode, and round-trip scene save/load.

All engine-side code lives in `Timefall/src/Timefall/Renderer/Renderer3D.cpp` unless noted.

## The frame, and where your pass slots in

`Renderer3D::EndScene` runs the whole frame in this order:

1. **Lights UBO upload**
2. **Shadow depth passes** — sun cascades, then spot layers, then point cube faces
3. **`EnsureHdrFramebuffer()` + `HdrFB->Bind()`** — everything after this renders HDR (RGBA16F)
4. **Lit opaque+mask pass** (`LitShader`, all non-Blend submissions)
5. **Skybox** (LEQUAL depth, front-face cull)
6. **Lit blended pass** (sorted farthest-first, depth writes off)
7. **Resolve** — fullscreen triangle tonemaps HdrFB → the external LDR `TargetFB`

Placement rules:
- **HDR-domain effects** (bloom, fog, SSAO composite) go between steps 6 and 7 — they read/write
  the HDR buffer before tonemapping crushes the range.
- **LDR effects** (FXAA, vignette, color grading beyond the tonemapper) go after step 7.
- **Geometry pre-passes** (depth prepass, normals buffer for SSAO) go before step 4.
- **Additional scene-geometry passes** iterate `s_Data.Submissions` exactly like the shadow passes do.

## State restoration — the invariant that bites

Every pass inherits default GL state (depth test on, func LESS, depth writes on, back-face cull,
`TargetFB` bound with draw buffers `{color, id}`) and **must hand it back that way**, because the
lit pass, the 2D overlay pass, and entity-ID mouse picking all assume it. The existing passes show
the pattern — skybox restores depth func + culling, blended restores depth writes, resolve restores
depth test and rebinds the full target. Symptoms of getting this wrong appear *downstream*: the 2D
renderer vanishes, picking returns wrong entities, gizmos z-fight.

Two engine-specific traps on top of raw GL state:

- **Rebinding a shader invalidates the material cache.** `Shader::SetX` uploads to the
  *currently-bound* program. After your pass binds its own shader, rebind `LitShader` if lit
  drawing follows, and reset `s_Data.CurrentMaterial = nullptr` — otherwise the material-change
  cache skips re-binding textures/UBOs for the next submission. The skybox pass shows this.
- **Draw-buffer scope.** The resolve writes color only via `TargetFB->BindForSingleColorDraw(0)`
  so the shared entity-ID attachment isn't clobbered, then restores with `TargetFB->Bind()`.
  Any fullscreen pass writing to the target must do the same dance.

## Framebuffers and resize

There is no resize event. The pattern is `EnsureHdrFramebuffer()`: each frame, compare your FB's
spec against the target's width/height and lazily recreate on mismatch. New offscreen buffers
follow the same shape. To share the target's depth or entity-ID attachment instead of allocating
your own, alias it via `FramebufferTextureSpecification::ExternalRendererID` (see how HdrFB
aliases both).

Fullscreen passes: bind the empty `s_Data.FullscreenVAO` and `RenderCommand::DrawArrays(vao, 3)` —
the vertex shader generates the triangle from `gl_VertexID`, no vertex buffer exists.

## Shader + Renderer3D wiring

1. **Shader file**: `Timefall-Editor/Assets/shaders/Renderer3D_<Name>.glsl`, single file with
   `#type vertex` / `#type fragment` sections. Loaded in `Renderer3D::Init` with the *relative*
   path `"assets/shaders/Renderer3D_<Name>.glsl"` (resolved against the editor's working dir).
2. **Renderer3DData members**: `Ref<Shader>`, any `Ref<Framebuffer>`/`Ref<UniformBuffer>`, and
   sampler-slot constants. Slots 0–11 and UBO binding points 0–6 are taken — append, never reuse.
3. **`Init()`**: create the shader, bind it, map sampler uniforms to slots with `SetInt` **once**
   (they never change), create UBOs with their binding point.
4. **UBO mirrors are std140**: match the GLSL block exactly — vec3 pads to 16 bytes (pair it with
   a float), add explicit `_Padding` fields, and comment the struct `// std140 mirror of ...`.
   If the shader switches on an enum, the C++ enum order MUST match (see `ToneMapOperator`).
5. **Per-frame inputs**: if the pass needs data submitted from the scene, add a `Submit*` function
   and reset its state in **both** `BeginScene` overloads — there are two (EditorCamera and
   runtime Camera) with duplicated reset blocks, and missing one means stale state in the other
   mode. Upload UBOs even when the feature is inactive, with a "disabled" flag zeroed — otherwise
   the shader samples stale data from the last frame it was active (see the shadow passes).

## Settings plumbing — seven places or it half-works

Skip this section if the pass has no tunables. Otherwise, follow `PostProcessSettings` end to end:

1. **Struct**: `Timefall/src/Timefall/Renderer/<Name>Settings.h` — plain data with defaults.
2. **`Renderer3D::Set<Name>Settings`** (Renderer3D.h + .cpp): store, clamp, and recreate GPU
   resources when a setting that sizes them changed (see `SetShadowSettings`).
3. **Scene member**: `Scene.h` — field + const/non-const `Get<Name>Settings()` accessors.
4. **`Scene::Copy`** (`Scene.cpp`): copy the field, or play-mode silently reverts it to defaults.
5. **Push to renderer**: `Scene.cpp` calls `Renderer3D::Set<Name>Settings` in **both** render
   paths (editor and runtime) right next to the existing SetShadowSettings/SetPostProcessSettings
   calls — grep for those, there are two sites.
6. **Serializer** (`SceneSerializer.cpp`): write the map on save; on load, read each key with a
   default fallback so old scenes still open.
7. **Editor panel**: `Timefall-Editor/src/Panels/<Name>SettingsPanel.{h,cpp}` writing into
   `scene->Get<Name>Settings()` (copy PostProcessSettingsPanel — it's ~45 lines), plus a member
   in `EditorLayer.h` and an `OnImGuiRender(GetActiveScene())` call next to the other panels.

New `.cpp` files mean regenerating the VS solution: `scripts/Win-GenProjects.bat`.

## Offline bakes are a different pattern

One-off render-to-texture at asset load (equirect→cubemap, irradiance convolve, GGX prefilter)
does not go through EndScene — see `Environment.cpp` for that pattern: temporary FBO, loop over
cube faces/mips, done at load time, cached by asset handle.
