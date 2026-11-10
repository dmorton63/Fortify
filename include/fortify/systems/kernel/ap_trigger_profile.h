#ifndef FORTIFY_KERNEL_AP_TRIGGER_PROFILE_H
#define FORTIFY_KERNEL_AP_TRIGGER_PROFILE_H

#include <stdint.h>

constexpr uint64_t kApTriggerProfileDefault = 0;
constexpr uint64_t kApTriggerProfileBalanced = 1;
constexpr uint64_t kApTriggerProfileAggressive = 2;

const char *ap_trigger_profile_name(uint64_t profile);
bool apply_ap_trigger_profile(uint64_t profile);
void log_ap_trigger_profile(uint64_t profile);

#endif
