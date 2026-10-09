/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <string>
#include <vector>

#include "passthrough/HdmiControl.h"
#include "routing/Endpoint.h"

namespace aidl::android::hardware::audio::core::mainline::passthrough {

// Whether a head accepted one IEC 61937 PCM configuration.
struct ProbeResult {
    unsigned int channels = 0;
    unsigned int rate = 0;
    bool hbr = false;
    bool accepted = false;
    // "channel status" or a PcmOpenError step; empty when accepted.
    std::string failed_step;
    int error = 0;
};

// Opens the PCM device of `head` with each IEC 61937 configuration (2
// channels at 48 and 192 kHz, 8 channels at 192 kHz), with the matching
// non-audio channel status set as for a stream, and closes it again without
// playing anything. The caller must own the device (PcmArbiter). Records
// nothing: no HBR refusal, no attempt.
std::vector<ProbeResult> ProbeHead(HdmiControl& control, const routing::Endpoint& head);

}  // namespace aidl::android::hardware::audio::core::mainline::passthrough
