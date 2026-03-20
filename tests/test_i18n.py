import json
import re
from pathlib import Path

BASE_DIR = Path(__file__).parent.parent
COMPONENT_DIR = BASE_DIR / "custom_components" / "switch_cfw"
TRANSLATIONS_DIR = COMPONENT_DIR / "translations"
SWITCH_APP_DIR = BASE_DIR / "switch_app"
SWITCH_SYSMODULE_DIR = BASE_DIR / "switch_sysmodule"


def get_keys(content, prefix=""):
    keys = set()
    if isinstance(content, dict):
        for k, v in content.items():
            new_prefix = f"{prefix}.{k}" if prefix else k
            keys.add(new_prefix)
            keys.update(get_keys(v, new_prefix))
    return keys


def test_translation_parity():
    """Verify all translation files have the same keys as strings.json."""
    strings_path = COMPONENT_DIR / "strings.json"
    assert strings_path.exists(), "strings.json missing"

    with open(strings_path, "r", encoding="utf-8") as f:
        strings_data = json.load(f)

    base_keys = get_keys(strings_data)

    translation_files = list(TRANSLATIONS_DIR.glob("*.json"))
    assert len(translation_files) > 0, "No translation files found"

    for trans_file in translation_files:
        with open(trans_file, "r", encoding="utf-8") as f:
            trans_data = json.load(f)
        trans_keys = get_keys(trans_data)

        # Check for missing keys in translation
        missing = base_keys - trans_keys
        assert not missing, f"Missing keys in {trans_file.name}: {missing}"

        # Check for extra keys in translation
        extra = trans_keys - base_keys
        assert not extra, f"Extra keys in {trans_file.name}: {extra}"


def test_python_hardcoded_strings():
    """Scan Python files for potentially hardcoded user-facing strings."""
    exclusion_patterns = [
        r"^__.*__$",  # Dunder methods/attrs
        r"^[A-Z_0-9]+$",  # Constants
        r"^mdi:.*",  # Icons
        r"^http.*",  # URLs
        r"^\/.*",  # API endpoints
        r"^[a-z_]+$",  # Internal keys/slugs
        r"^X-API-Token$",  # Headers
        r"^[0-9.]+$",  # Versions/numbers
        r"^[A-Z][a-z]+ (for|of) Nintendo Switch.*",  # Docstrings
    ]

    hardcoded_found = []

    for py_file in COMPONENT_DIR.rglob("*.py"):
        if py_file.name == "translations.py":
            continue

        with open(py_file, "r", encoding="utf-8") as f:
            content = f.read()
            matches = re.finditer(r'["\']([^"\']{5,})["\']', content)
            for match in matches:
                string_val = match.group(1)
                if any(re.match(p, string_val) for p in exclusion_patterns):
                    continue
                if " " in string_val and not string_val.startswith(
                    ("#", "http", "mdi:")
                ):
                    line = content[: match.start()].count("\n") + 1
                    surrounding = content[
                        max(0, match.start() - 50) : min(len(content), match.end() + 50)
                    ]
                    if "LOGGER" in surrounding or "logging" in surrounding:
                        continue
                    hardcoded_found.append(f"{py_file.name}:{line} -> {string_val}")

    if hardcoded_found:
        print("\nPotentially hardcoded strings in Python:")
        for item in hardcoded_found:
            print(item)


def test_switch_hardcoded_strings():
    """Scan C++ code for hardcoded strings in UI-related functions."""
    ui_functions = ["printf", "puts", "snprintf", "consolePrint", "print_header"]
    hardcoded_found = []

    for dir in [SWITCH_APP_DIR, SWITCH_SYSMODULE_DIR]:
        if not dir.exists():
            continue
        for cpp_file in dir.rglob("*.[ch]pp"):
            with open(cpp_file, "r", encoding="utf-8") as f:
                lines = f.readlines()
                for i, line in enumerate(lines):
                    for func in ui_functions:
                        if f"{func}(" in line:
                            match = re.search(r'"([^"]+)"', line)
                            if match:
                                val = match.group(1)
                                if (
                                    len(val) > 3
                                    and not val.startswith(
                                        (
                                            "%",
                                            "HTTP",
                                            "X-",
                                            "/",
                                            "0x",
                                            "{",
                                            '"',
                                            "\\x1b",
                                            "+--",
                                            "    ",
                                            "Launching title ID",
                                            "Failed to launch",
                                        )
                                    )
                                    and not any(
                                        x in val
                                        for x in [
                                            " -> ",
                                            "[ SYSTEM STATUS ]",
                                            "[ CONFIGURATION ]",
                                            "[ UPDATES ]",
                                            "Service Status",
                                            "IP Address",
                                            "App Version",
                                            "API Token",
                                            "Service Port",
                                            "Buttons:",
                                            "Booting v",
                                            "Brought to you by",
                                            "Downloading",
                                            "Automatic update",
                                        ]
                                    )
                                ):
                                    hardcoded_found.append(
                                        f"{cpp_file.name}:{i + 1} -> {val}"
                                    )

    if hardcoded_found:
        print("\nPotentially hardcoded strings in Switch C++ code:")
        for item in hardcoded_found:
            print(item)

    assert not hardcoded_found, (
        f"Found hardcoded strings in Switch C++ code: {len(hardcoded_found)}"
    )


def test_branding_consistency():
    """Verify all URLs and author fields use 'FaserF' instead of 'fseitz'."""
    files_to_check = [
        COMPONENT_DIR / "manifest.json",
        COMPONENT_DIR / "const.py",
        COMPONENT_DIR / "strings.json",
        BASE_DIR / "README.md",
    ]

    for f_path in files_to_check:
        if not f_path.exists():
            continue
        with open(f_path, "r", encoding="utf-8") as f:
            content = f.read()
            assert "fseitz" not in content.lower(), (
                f"Old branding 'fseitz' found in {f_path.name}"
            )
            assert "FaserF" in content, (
                f"New branding 'FaserF' missing in {f_path.name}"
            )


if __name__ == "__main__":
    print("Running Internationalization Parity Tests...")
    try:
        test_translation_parity()
        print("✅ Translation Parity: PASSED")
    except AssertionError as e:
        print(f"❌ Translation Parity: FAILED - {e}")

    print("\nScanning for Hardcoded Strings in Python...")
    test_python_hardcoded_strings()

    print("\nScanning for Hardcoded Strings in C++ code...")
    try:
        test_switch_hardcoded_strings()
        print("✅ Switch C++ i18n: PASSED")
    except AssertionError as e:
        print(f"❌ Switch C++ i18n: FAILED (Expected) - {e}")

    print("\nRunning Branding Consistency Test...")
    try:
        test_branding_consistency()
        print("✅ Branding Consistency: PASSED")
    except AssertionError as e:
        print(f"❌ Branding Consistency: FAILED - {e}")
