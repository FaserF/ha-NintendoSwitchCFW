import json
import sys
import os


def validate_main_json(filepath):
    if not os.path.exists(filepath):
        print(f"ERROR: {filepath} not found!")
        sys.exit(1)

    with open(filepath, "r") as f:
        try:
            data = json.load(f)
        except json.JSONDecodeError as e:
            print(f"ERROR: Invalid JSON in {filepath}. Details: {e}")
            sys.exit(1)

    print(f"Validating {filepath}...")

    # Validate kernel capabilities
    kernel_caps = data.get("kernel_capabilities", [])
    syscalls_dict = {}
    for cap in kernel_caps:
        if cap.get("type") == "syscalls":
            syscalls_dict = cap.get("value", {})
            break

    if not syscalls_dict:
        print("ERROR: No syscalls found in kernel_capabilities!")
        sys.exit(1)

    # Check for crucial modern syscalls (e.g. svcCreateEvent, svcMapPhysicalMemoryUnsafe, etc.)
    # At least svcCreateEvent (0x45) should be mapped for modern networking threading
    crucial_syscalls = [
        "svcCreateEvent",
        "svcCallSecureMonitor",  # 0x7f, used as an upper bound check to ensure they copied the full list
    ]

    missing = []
    for syscall in crucial_syscalls:
        if syscall not in syscalls_dict:
            missing.append(syscall)

    if missing:
        print(f"ERROR: Sysmodule is missing critical modern syscalls: {missing}")
        print(
            "Please ensure the kernel_capabilities syscalls mapping includes all up to 0x7F."
        )
        sys.exit(1)

    # Check for correct optimization patches
    if data.get("disable_device_address_space_merge") is False:
        print(
            "WARNING: 'disable_device_address_space_merge' should usually be true for modern sysmodules."
        )

    print(f"SUCCESS: {filepath} passed validation.")


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python validate_sysmodule.py <path_to_main.json>")
        sys.exit(1)

    validate_main_json(sys.argv[1])
