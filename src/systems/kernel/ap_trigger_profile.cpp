#include "fortify/systems/kernel/ap_trigger_profile.h"

#include "fortify/systems/kernel/kernel_string.h"
#include "fortify/systems/console/console.h"
#include "fortify/systems/serial/serial.h"
#include "fortify/systems/subsystems/subsystems.h"

namespace {
void write_profile_details(void (*write_fn)(const char *), uint64_t profile) {
    switch (profile) {
        case kApTriggerProfileDefault:
            write_fn(" (default: ports 9/2, fs 7/2, net 4/2, input 1/1, usb 2/1, audio 2/1, webcam 2/1)\n");
            break;
        case kApTriggerProfileBalanced:
            write_fn(" (balanced: ports 7/2, fs 5/2, net 2/2, input 1/1, usb 1/1, audio 1/1, webcam 1/1)\n");
            break;
        case kApTriggerProfileAggressive:
            write_fn(" (aggressive: all subsystems 1/1)\n");
            break;
        default:
            write_fn(" (unknown)\n");
            break;
    }
}
} // namespace

const char *ap_trigger_profile_name(uint64_t profile) {
    if (profile == kApTriggerProfileDefault) {
        return "default";
    }
    if (profile == kApTriggerProfileBalanced) {
        return "balanced";
    }
    if (profile == kApTriggerProfileAggressive) {
        return "aggressive";
    }
    return "unknown";
}

bool apply_ap_trigger_profile(uint64_t profile) {
    switch (profile) {
        case kApTriggerProfileDefault:
            return port_manager_set_ap_maintenance_trigger_config(9, 2) &&
                   filesystem_set_ap_maintenance_trigger_config(7, 2) &&
                   networking_set_ap_cleanup_trigger_config(4, 2) &&
                   input_set_ap_processing_trigger_config(1, 1) &&
                   usb_set_ap_processing_trigger_config(2, 1) &&
                   audio_set_ap_buffer_trigger_config(2, 1) &&
                   webcam_set_ap_processing_trigger_config(2, 1);
        case kApTriggerProfileBalanced:
            return port_manager_set_ap_maintenance_trigger_config(7, 2) &&
                   filesystem_set_ap_maintenance_trigger_config(5, 2) &&
                   networking_set_ap_cleanup_trigger_config(2, 2) &&
                   input_set_ap_processing_trigger_config(1, 1) &&
                   usb_set_ap_processing_trigger_config(1, 1) &&
                   audio_set_ap_buffer_trigger_config(1, 1) &&
                   webcam_set_ap_processing_trigger_config(1, 1);
        case kApTriggerProfileAggressive:
            return port_manager_set_ap_maintenance_trigger_config(1, 1) &&
                   filesystem_set_ap_maintenance_trigger_config(1, 1) &&
                   networking_set_ap_cleanup_trigger_config(1, 1) &&
                   input_set_ap_processing_trigger_config(1, 1) &&
                   usb_set_ap_processing_trigger_config(1, 1) &&
                   audio_set_ap_buffer_trigger_config(1, 1) &&
                   webcam_set_ap_processing_trigger_config(1, 1);
        default:
            return false;
    }
}

void log_ap_trigger_profile(uint64_t profile) {
    char profile_hex[19];
    kernel_format_hex64(profile_hex, profile);
    serial_write("[INFO] smp: AP trigger profile=");
    serial_write(profile_hex);
    write_profile_details(serial_write, profile);
    console_write("[INFO] smp: AP trigger profile=");
    console_write(profile_hex);
    write_profile_details(console_write, profile);
}
