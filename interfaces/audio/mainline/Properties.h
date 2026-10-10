/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace aidl::android::hardware::audio::core::mainline {

// All settings understood by the HAL. Each one is an Android property with
// the "vendor.audio.mainline." prefix, which a setting stored through the
// control interface overrides (`overrides`, keyed by the name without the
// prefix). Values are read once at start-up; the HAL has to be restarted for
// a change to take effect.
//
// The names are documented in README.md. Keep both in sync, and keep
// Properties::Specs() in sync with Load().
struct Properties {
    // Stored settings in effect, by key without the prefix. They win over the
    // Android properties.
    std::map<std::string, std::string> overrides;

    // Comma separated list of sound cards to use. Each entry is either a card
    // index ("0"), a card id ("PCH") or a card name ("HDA Intel PCH"). An empty
    // list means "every card except USB ones".
    std::vector<std::string> cards;

    // Maximum time in milliseconds to wait for the cards listed in `cards` to
    // appear before proceeding with enumeration. 0 means no waiting (use
    // whatever is present). Only effective when `cards` is non-empty.
    int wait_for_cards_ms = 0;

    // Card providing the built-in "Speaker" and "Built-In Mic" device ports.
    // Same syntax as a single entry of `cards`. Empty means "pick the first
    // card that has an analog playback device".
    std::string primary_card;

    // Also expose USB sound cards that are present at start-up as static
    // cards. Normally USB cards are handled through the connectExternalDevice
    // flow driven by the framework's UsbAlsaManager.
    bool include_usb_cards = false;

    // Expose a silent built-in microphone when no capture path exists. Only
    // useful for bring-up; normally do not advertise nonexistent hardware.
    bool null_mic = false;

    // Use the ALSA Use Case Manager (alsa-ucm-conf) to discover and route
    // devices when a profile exists for the card.
    bool ucm_enabled = true;

    // UCM verb to select. The first available verb is used when the requested
    // one does not exist.
    std::string ucm_verb = "HiFi";

    // For cards without a UCM profile: unmute the mixer and apply default
    // volumes at start-up.
    bool mixer_init = true;
    int mixer_playback_percent = 100;
    int mixer_capture_percent = 80;
    // Turn each switch off and on again while initialising the mixer, for
    // emulated cards whose host audio stream only follows a mixer write.
    bool mixer_toggle = false;

    // Nominal latency reported for PCM streams, also drives the stream buffer
    // size negotiated with the framework.
    int latency_ms = 20;

    // Nominal latency of the primary output when it is to be served by the
    // framework's FastMixer (AudioOutputFlags::FAST, fast tracks). 0 disables
    // it and keeps `latency_ms` for the primary output. The FAST flag is only
    // set when the resulting buffer is smaller than AudioFlinger's normal
    // mixer period, see ConfigurationBuilder.
    int fast_latency_ms = 0;

    // Expose a DIRECT "multichannel output" mix port when a device supports
    // six or more channels.
    bool multichannel = true;

    // Expose the "hdmi passthrough" mix port (compressed audio to the HDMI
    // sink as IEC 61937) when there is an HDMI output.
    bool hdmi_passthrough = true;

    // Encoded formats to offer for passthrough instead of those in the
    // sink's ELD, for sinks or bridges with a missing or wrong ELD. Names as
    // in passthrough::EncodedFormatFromString(). Empty means "use the ELD".
    std::vector<std::string> hdmi_passthrough_formats;

    // Log verbosely (sets the minimum severity to VERBOSE instead of DEBUG).
    bool verbose_logging = false;

    // Per-card properties: sample rates and bit depths to allow for each card.
    // Empty sets mean "use all rates/bits supported by the hardware".
    // Read at enumeration time using card id, index, or name (with spaces
    // replaced by underscores) as the selector.
    struct CardProperties {
        std::set<int> rates;
        std::set<int> bits;
        // Forces high bit rate passthrough formats on / off for the card's
        // HDMI heads, overriding passthrough/Quirks. Unset: the quirk decides.
        std::optional<bool> hbr;
    };

    static Properties Load(std::map<std::string, std::string> overrides = {});
    std::string ToString() const;

    // Reads the per-card settings of a card, merging across all matching
    // selectors (id, index, name with spaces as underscores).
    CardProperties ForCard(const std::string& card_id, int card_index,
                           const std::string& card_name) const;

    // What a setting is, for the control interface to list and validate.
    struct Spec {
        enum class Type {
            kBool,
            kInt,  // Within [min, max]; Load() clamps.
            kString,
            kList,    // Comma separated.
            kIntList  // Comma separated integers within [min, max].
        };
        std::string key;  // Without the prefix, e.g. "latency_ms", "card.PCH.rates".
        Type type = Type::kString;
        int min = 0;
        int max = 0;
    };
    // The global settings, and the per-card ones of `card_ids`.
    static std::vector<Spec> Specs(const std::vector<std::string>& card_ids);
    // The spec of `key`, also for per-card keys of any selector; nullopt for
    // an unknown key.
    static std::optional<Spec> SpecOf(const std::string& key);
    // Whether `value` is valid for `spec`; Load() would accept and not clamp it.
    static bool IsValid(const Spec& spec, const std::string& value);
    // The value each spec of Specs(card_ids) has in these properties, as text
    // (booleans "true" / "false", lists comma separated, unset "").
    std::map<std::string, std::string> Values(const std::vector<std::string>& card_ids) const;

    static constexpr const char* kPrefix = "vendor.audio.mainline.";
};

}  // namespace aidl::android::hardware::audio::core::mainline
