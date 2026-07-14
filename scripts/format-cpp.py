"""PostToolUse hook: runs clang-format on edited engine C++ files.

Reads the hook JSON from stdin and formats the file in place using the
repo's .clang-format. Only touches first-party sources, never vendor code.
"""
import json
import os
import shutil
import subprocess
import sys

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# first-party source roots (vendor code is excluded on purpose)
SOURCE_ROOTS = (
    os.path.join(REPO_ROOT, "Timefall", "src"),
    os.path.join(REPO_ROOT, "Timefall-Editor", "src"),
    os.path.join(REPO_ROOT, "Sandbox", "src"),
)

EXTENSIONS = (".h", ".hpp", ".cpp", ".inl")

VSWHERE = r"C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe"


def find_clang_format():
    exe = shutil.which("clang-format")
    if exe:
        return exe
    if os.path.isfile(VSWHERE):
        try:
            vs = subprocess.run([VSWHERE, "-latest", "-property", "installationPath"],
                                capture_output=True, text=True, timeout=10).stdout.strip()
            for sub in (r"VC\Tools\Llvm\x64\bin", r"VC\Tools\Llvm\bin"):
                candidate = os.path.join(vs, sub, "clang-format.exe")
                if os.path.isfile(candidate):
                    return candidate
        except (subprocess.SubprocessError, OSError):
            pass
    return None


def main():
    try:
        payload = json.load(sys.stdin)
    except json.JSONDecodeError:
        return 0
    tool_input = payload.get("tool_input") or {}
    path = tool_input.get("file_path") or (payload.get("tool_response") or {}).get("filePath") or ""
    if not path or not path.lower().endswith(EXTENSIONS) or not os.path.isfile(path):
        return 0
    norm = os.path.normcase(os.path.abspath(path))
    if not any(norm.startswith(os.path.normcase(root) + os.sep) for root in SOURCE_ROOTS):
        return 0

    clang_format = find_clang_format()
    if clang_format is None:
        print("format-cpp: clang-format not found (PATH or VS install), skipping", file=sys.stderr)
        return 0

    result = subprocess.run([clang_format, "-i", "--style=file", path],
                            capture_output=True, text=True)
    if result.returncode != 0:
        print(f"format-cpp: clang-format failed on {path}:\n{result.stderr.strip()}",
              file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
