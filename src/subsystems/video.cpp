#include "fortify/boot.h"
#include "fortify/log.h"
#include "fortify/subsystems.h"

bool video_init(void) {
    struct limine_framebuffer_response *fb = boot_framebuffer_response();
    if (fb == 0 || fb->framebuffer_count == 0 || fb->framebuffers == 0 || fb->framebuffers[0] == 0) {
        log_error("video: framebuffer unavailable.");
        return false;
    }

    log_info("video: framebuffer attached.");
    return true;
}

bool renderer_init(void) {
    log_info("renderer: software 3D renderer stub ready.");
    return true;
}

bool compositor_init(void) {
    log_info("compositor: composition pipeline stub ready.");
    return true;
}
