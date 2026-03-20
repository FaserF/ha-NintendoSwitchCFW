import re
import json
import os
import sys


def update_files(new_version):
    print(f"Updating all project files to version {new_version}...")

    # Update pyproject.toml
    if os.path.exists("pyproject.toml"):
        with open("pyproject.toml") as f:
            content = f.read()
        content = re.sub(
            r'version\s*=\s*"[^"]+"', f'version = "{new_version}"', content, count=1
        )
        with open("pyproject.toml", "w") as f:
            f.write(content)
        print("- Updated pyproject.toml")

    # Update manifest.json
    manifest_path = "custom_components/switch_cfw/manifest.json"
    if os.path.exists(manifest_path):
        with open(manifest_path) as f:
            manifest = json.load(f)
        manifest["version"] = new_version
        with open(manifest_path, "w") as f:
            json.dump(manifest, f, indent=2)
            f.write("\n")
        print("- Updated manifest.json")

    # Update const.py
    const_path = "custom_components/switch_cfw/const.py"
    if os.path.exists(const_path):
        with open(const_path) as f:
            content = f.read()
        # Update Startup Message Version
        content = re.sub(
            r"Version: \d+\.\d+\.\d+(-beta\.\d+)?", f"Version: {new_version}", content
        )
        # Update VERSION constant if it exists
        content = re.sub(
            r'VERSION\s*=\s*"[^"]+"', f'VERSION = "{new_version}"', content
        )
        with open(const_path, "w") as f:
            f.write(content)
        print("- Updated const.py")

    # Update switch_sysmodule Makefile and/or include/SysmoduleConstants.h if version is there
    # Currently sysmodule uses APP_VERSION define from Makefile
    sys_makefile = "switch_sysmodule/Makefile"
    if os.path.exists(sys_makefile):
        with open(sys_makefile) as f:
            content = f.read()
        content = re.sub(
            r"APP_VERSION\s*?[:?]?=\s*?.*",
            f"APP_VERSION\t:=\t{new_version}",
            content,
            count=1,
        )
        with open(sys_makefile, "w") as f:
            f.write(content)
        print("- Updated switch_sysmodule/Makefile")

    # Update switch_app Makefile
    app_makefile_path = "switch_app/Makefile"
    if os.path.exists(app_makefile_path):
        with open(app_makefile_path) as f:
            content = f.read()
        content = re.sub(
            r"VERSION\s*?[:?]?=\s*?.*", f"VERSION\t?=\t{new_version}", content, count=1
        )
        with open(app_makefile_path, "w") as f:
            f.write(content)
        print("- Updated switch_app/Makefile")


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python scripts/set_version.py <version>")
        sys.exit(1)

    update_files(sys.argv[1])
