"""PostToolUse hook: validates edited .glsl files with glslangValidator.

Reads the hook JSON from stdin, splits the engine's `#type <stage>` sections,
validates each stage, and exits 2 with errors on stderr so Claude fixes them.
"""
import json
import os
import subprocess
import sys
import tempfile

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VALIDATOR = os.path.join(REPO_ROOT, "vendor", "glslang", "glslangValidator.exe")

# spec-UB but fine on desktop GL; used deliberately by the 2D batch renderer
BENIGN_ERRORS = ("variable indexing sampler array",)

# engine tokens (OpenGLShader.cpp) -> glslang stage extensions
STAGE_EXT = {
    "vertex": "vert",
    "fragment": "frag",
    "pixel": "frag",
    "geometry": "geom",
    "compute": "comp",
}


def split_stages(text):
    """Yield (stage_token, start_line, source) per #type section.

    Source is padded with blank lines so glslang's reported line numbers
    match the original combined file.
    """
    stages = []
    lines = text.splitlines()
    current = None  # [token, header_line_index]
    body_start = None
    for i, line in enumerate(lines):
        stripped = line.strip()
        if stripped.startswith("#type"):
            if current is not None:
                stages.append((current[0], current[1], lines[body_start:i]))
            token = stripped[len("#type"):].strip()
            current = [token, i + 1]
            body_start = i + 1
        # content before the first #type is ignored, matching the engine parser
    if current is not None:
        stages.append((current[0], current[1], lines[body_start:]))
    return [(tok, hdr, "\n".join([""] * hdr + body) + "\n") for tok, hdr, body in stages]


def main():
    try:
        payload = json.load(sys.stdin)
    except json.JSONDecodeError:
        return 0
    tool_input = payload.get("tool_input") or {}
    path = tool_input.get("file_path") or (payload.get("tool_response") or {}).get("filePath") or ""
    if not path.lower().endswith(".glsl") or not os.path.isfile(path):
        return 0
    if not os.path.isfile(VALIDATOR):
        print(f"validate-shader: {VALIDATOR} not found, skipping", file=sys.stderr)
        return 0

    with open(path, encoding="utf-8", errors="replace") as f:
        text = f.read()

    stages = split_stages(text)
    if stages is None or not stages:
        print(f"{path}: no '#type <stage>' sections found (engine expects them)", file=sys.stderr)
        return 2

    errors = []
    for token, header_line, source in stages:
        ext = STAGE_EXT.get(token)
        if ext is None:
            errors.append(f"{path}:{header_line}: unknown shader stage '#type {token}' "
                          f"(engine accepts: {', '.join(STAGE_EXT)})")
            continue
        with tempfile.NamedTemporaryFile("w", suffix=f".{ext}", delete=False,
                                         encoding="utf-8") as tmp:
            tmp.write(source)
            tmp_path = tmp.name
        try:
            result = subprocess.run([VALIDATOR, tmp_path], capture_output=True, text=True)
            if result.returncode != 0:
                out = (result.stdout + result.stderr).replace(tmp_path, path)
                real = [l for l in out.splitlines()
                        if l.startswith("ERROR:")
                        and "compilation terminated" not in l
                        and "compilation errors." not in l
                        and not any(b in l for b in BENIGN_ERRORS)]
                if real:
                    errors.append(f"--- stage '{token}' (starts line {header_line}) ---\n{out.strip()}")
        finally:
            os.unlink(tmp_path)

    if errors:
        print(f"glslangValidator found errors in {os.path.basename(path)}:\n" + "\n".join(errors),
              file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())
