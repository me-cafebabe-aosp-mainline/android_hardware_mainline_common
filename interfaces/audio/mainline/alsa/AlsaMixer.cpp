/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "MainlineAudio_AlsaMixer"

#include "alsa/AlsaMixer.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <initializer_list>
#include <set>
#include <string_view>

#include <alsa/asoundlib.h>
#include <android-base/logging.h>
#include <android-base/strings.h>

#include "alsa/AlsaError.h"

namespace aidl::android::hardware::audio::core::mainline::alsa {

namespace {

std::string ToLower(std::string_view value) {
    std::string lower(value);
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return lower;
}

bool ContainsAny(const std::string& lower, std::initializer_list<const char*> needles) {
    return std::any_of(needles.begin(), needles.end(), [&lower](const char* needle) {
        return lower.find(needle) != std::string::npos;
    });
}

// Controls that must not be touched by a generic initialisation.
bool IsBlacklisted(const std::string& lower_name) {
    return ContainsAny(lower_name, {"beep", "loopback", "boost", "analog loopback", "sidetone",
                                    "auto-mute", "auto gain", "agc", "iec958", "spdif", "jack"});
}

// The playback part of an input is its analog loopback into the outputs (HDA
// "Front Mic Playback", AC97 "Line", "CD"). alsactl init mutes it: unmuted, the
// inputs are heard on the outputs, a mic as feedback.
bool IsInputLoopback(const std::string& lower_name) {
    static const std::set<std::string> kInputs = {"line", "line in", "cd", "aux", "phone", "video"};
    if (kInputs.count(lower_name) > 0) return true;
    for (const std::string& word : ::android::base::Split(lower_name, " ")) {
        if (word == "mic" || word == "microphone") return true;
    }
    return false;
}

void MuteLoopback(snd_mixer_elem_t* elem, const std::string& name) {
    if (!snd_mixer_selem_has_playback_switch(elem)) return;
    if (const int err = snd_mixer_selem_set_playback_switch_all(elem, 0); err < 0) {
        LOG(DEBUG) << "  '" << name << "' loopback off: " << ErrorString(err);
    } else {
        LOG(DEBUG) << "  '" << name << "' loopback off";
    }
}

// Without UCM the kernel's auto-mute switches between speakers and headphones;
// whatever disabled it (a run with UCM, alsactl) would leave both playing.
void EnableAutoMute(snd_mixer_elem_t* elem, const std::string& name) {
    if (!snd_mixer_selem_is_enumerated(elem)) return;
    unsigned int item = 0;
    if (snd_mixer_selem_get_enum_item(elem, SND_MIXER_SCHN_MONO, &item) < 0) return;
    char text[64] = {};
    if (snd_mixer_selem_get_enum_item_name(elem, item, sizeof(text), text) < 0) return;
    if (std::string_view(text) != "Disabled" || snd_mixer_selem_get_enum_items(elem) < 2) return;
    if (const int err = snd_mixer_selem_set_enum_item(elem, SND_MIXER_SCHN_MONO, 1); err < 0) {
        LOG(DEBUG) << "  '" << name << "' enable: " << ErrorString(err);
    } else {
        LOG(DEBUG) << "  '" << name << "' enabled";
    }
}

// Opens the simple mixer of a card. Returns nullptr on failure.
MixerHandle OpenMixer(int card) {
    snd_mixer_t* raw = nullptr;
    int err = snd_mixer_open(&raw, 0);
    if (err < 0) {
        LOG(WARNING) << __func__ << ": snd_mixer_open: " << ErrorString(err);
        return nullptr;
    }
    MixerHandle mixer(raw);
    const std::string name = "hw:" + std::to_string(card);
    if ((err = snd_mixer_attach(raw, name.c_str())) < 0) {
        LOG(WARNING) << __func__ << ": snd_mixer_attach(" << name << "): " << ErrorString(err);
        return nullptr;
    }
    if ((err = snd_mixer_selem_register(raw, nullptr, nullptr)) < 0) {
        LOG(WARNING) << __func__ << ": snd_mixer_selem_register: " << ErrorString(err);
        return nullptr;
    }
    if ((err = snd_mixer_load(raw)) < 0) {
        LOG(WARNING) << __func__ << ": snd_mixer_load(" << name << "): " << ErrorString(err);
        return nullptr;
    }
    return mixer;
}

long PercentToValue(long min, long max, int percent) {
    if (max <= min) return max;
    return min + std::lround((max - min) * (std::clamp(percent, 0, 100) / 100.0));
}

// With `toggle`, the switch is turned off first, so that turning it on is a change the card sees.
void InitPlaybackElement(snd_mixer_elem_t* elem, const std::string& name, int percent,
                         bool toggle) {
    if (snd_mixer_selem_has_playback_switch(elem)) {
        if (toggle) {
            if (const int err = snd_mixer_selem_set_playback_switch_all(elem, 0); err < 0) {
                LOG(DEBUG) << "  '" << name << "' playback switch off: " << ErrorString(err);
            }
        }
        if (const int err = snd_mixer_selem_set_playback_switch_all(elem, 1); err < 0) {
            LOG(DEBUG) << "  '" << name << "' playback switch on: " << ErrorString(err);
        } else {
            LOG(DEBUG) << "  '" << name << "' playback switch on";
        }
    }
    if (snd_mixer_selem_has_playback_volume(elem)) {
        long min = 0, max = 0;
        if (snd_mixer_selem_get_playback_volume_range(elem, &min, &max) == 0) {
            const long value = PercentToValue(min, max, percent);
            if (const int err = snd_mixer_selem_set_playback_volume_all(elem, value); err < 0) {
                LOG(DEBUG) << "  '" << name << "' playback volume " << value << ": "
                           << ErrorString(err);
            } else {
                LOG(DEBUG) << "  '" << name << "' playback volume " << value << " [" << min << ".."
                           << max << "]";
            }
        }
    }
}

void InitCaptureElement(snd_mixer_elem_t* elem, const std::string& name, int percent, bool toggle) {
    if (snd_mixer_selem_has_capture_switch(elem)) {
        if (toggle) {
            if (const int err = snd_mixer_selem_set_capture_switch_all(elem, 0); err < 0) {
                LOG(DEBUG) << "  '" << name << "' capture switch off: " << ErrorString(err);
            }
        }
        if (const int err = snd_mixer_selem_set_capture_switch_all(elem, 1); err < 0) {
            LOG(DEBUG) << "  '" << name << "' capture switch on: " << ErrorString(err);
        } else {
            LOG(DEBUG) << "  '" << name << "' capture switch on";
        }
    }
    if (snd_mixer_selem_has_capture_volume(elem)) {
        long min = 0, max = 0;
        if (snd_mixer_selem_get_capture_volume_range(elem, &min, &max) == 0) {
            const long value = PercentToValue(min, max, percent);
            if (const int err = snd_mixer_selem_set_capture_volume_all(elem, value); err < 0) {
                LOG(DEBUG) << "  '" << name << "' capture volume " << value << ": "
                           << ErrorString(err);
            } else {
                LOG(DEBUG) << "  '" << name << "' capture volume " << value << " [" << min << ".."
                           << max << "]";
            }
        }
    }
}

}  // namespace

bool InitializeMixer(int card, const MixerInitOptions& options) {
    MixerHandle mixer = OpenMixer(card);
    if (mixer == nullptr) return false;

    LOG(INFO) << __func__ << ": card " << card << " playback=" << options.playback_percent
              << "% capture=" << options.capture_percent
              << "% toggle_switches=" << options.toggle_switches;
    for (snd_mixer_elem_t* elem = snd_mixer_first_elem(mixer.get()); elem != nullptr;
         elem = snd_mixer_elem_next(elem)) {
        if (!snd_mixer_selem_is_active(elem)) continue;
        const std::string name = snd_mixer_selem_get_name(elem);
        const std::string lower = ToLower(name);
        if (lower == "auto-mute mode") {
            EnableAutoMute(elem, name);
            continue;
        }
        if (IsBlacklisted(lower)) {
            LOG(DEBUG) << "  '" << name << "' skipped";
            continue;
        }
        if (IsInputLoopback(lower)) {
            MuteLoopback(elem, name);
        } else {
            InitPlaybackElement(elem, name, options.playback_percent, options.toggle_switches);
        }
        InitCaptureElement(elem, name, options.capture_percent, options.toggle_switches);
    }
    return true;
}

void SetPlaybackLevelUnity(int card, const std::vector<std::string>& elements) {
    if (elements.empty()) return;
    MixerHandle mixer = OpenMixer(card);
    if (mixer == nullptr) return;
    snd_mixer_selem_id_t* id = nullptr;
    snd_mixer_selem_id_alloca(&id);
    for (const std::string& name : elements) {
        snd_mixer_selem_id_set_name(id, name.c_str());
        snd_mixer_selem_id_set_index(id, 0);
        snd_mixer_elem_t* elem = snd_mixer_find_selem(mixer.get(), id);
        if (elem == nullptr || !snd_mixer_selem_has_playback_volume(elem)) continue;
        // Without a dB scale nothing says what the maximum is: leave it.
        long min_db = 0, max_db = 0;
        if (snd_mixer_selem_get_playback_dB_range(elem, &min_db, &max_db) < 0 || max_db <= min_db) {
            continue;
        }
        // dB values are in 1/100 dB; round down so that 0 dB is never exceeded.
        const int err = snd_mixer_selem_set_playback_dB_all(elem, std::min(0L, max_db), -1);
        if (err < 0) {
            LOG(WARNING) << __func__ << ": card " << card << " '" << name
                         << "': " << ErrorString(err);
        } else {
            LOG(DEBUG) << __func__ << ": card " << card << " '" << name << "' at unity";
        }
    }
}

}  // namespace aidl::android::hardware::audio::core::mainline::alsa
