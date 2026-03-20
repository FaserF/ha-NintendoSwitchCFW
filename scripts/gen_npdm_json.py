import json


def main():
    output_path = "/src/switch_sysmodule/main.json"
    sys_id = "0x4200000000000001"

    # Manual, bare-minimum robust JSON that avoids template bugs
    data = {
        "name": "HomeAssistant",
        "program_id": sys_id,
        "program_id_range_min": sys_id,
        "program_id_range_max": sys_id,
        "version": "0x0001",
        "main_thread_stack_size": "0x00010000",
        "main_thread_priority": 42,
        "default_cpu_id": 3,
        "process_category": 0,
        "is_retail": True,
        "pool_partition": 2,
        "is_64_bit": True,
        "address_space_type": 1,
        "filesystem_access": {"permissions": "0xffffffffffffffff"},
        "service_access": ["*"],
        "service_host": ["*"],
        "kernel_capabilities": [
            {
                "type": "kernel_flags",
                "value": {
                    "highest_thread_priority": 63,
                    "lowest_thread_priority": 24,
                    "lowest_cpu_id": 3,
                    "highest_cpu_id": 3,
                },
            },
            {
                "type": "syscalls",
                "value": {
                    "svcConnectToNamedPort": "0x1f",
                    "svcSendSyncRequest": "0x21",
                    "svcCloseHandle": "0x16",
                    "svcGetInfo": "0x29",
                },
            },
            {"type": "min_kernel_version", "value": "0x0030"},
            {"type": "handle_table_size", "value": 1023},
            {
                "type": "debug_flags",
                "value": {
                    "allow_debug": False,
                    "force_debug": True,
                    "force_debug_prod": False,
                    "prevent_code_reads": False,
                    "prevent_code_writes": False,
                    "enable_aslr": True,
                    "address_space_merge": False,
                    "optimize_memory_allocation": False,
                    "disable_device_address_space_merge": False,
                    "enable_alias_region_extra_size": False,
                },
            },
        ],
    }

    with open(output_path, "w") as f:
        json.dump(data, f, indent=4)
    print(f"Generated CLEAN {output_path}")


if __name__ == "__main__":
    main()
