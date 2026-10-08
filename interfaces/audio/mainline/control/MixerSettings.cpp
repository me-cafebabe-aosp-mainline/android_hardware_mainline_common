/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "MainlineAudio_Control"

#include "control/MixerSettings.h"

#include <map>
#include <optional>
#include <utility>

#include <android-base/logging.h>
#include <android-base/parseint.h>
#include <android-base/strings.h>

#include "alsa/AlsaControls.h"
#include "alsa/AlsaError.h"

namespace aidl::android::hardware::audio::core::mainline::control {

namespace {

// "<card id>|<index>|<name>": card ids are identifiers and indexes numbers, so
// the first two separators are unambiguous whatever the name contains.
std::string Key(const std::string& card_id, const std::string& name, unsigned int index) {
    return card_id + "|" + std::to_string(index) + "|" + name;
}

struct ParsedKey {
    std::string card_id;
    unsigned int index = 0;
    std::string name;
};

std::optional<ParsedKey> ParseKey(const std::string& key) {
    const size_t first = key.find('|');
    if (first == std::string::npos) return std::nullopt;
    const size_t second = key.find('|', first + 1);
    if (second == std::string::npos) return std::nullopt;
    ParsedKey parsed{.card_id = key.substr(0, first), .name = key.substr(second + 1)};
    if (!::android::base::ParseUint(key.substr(first + 1, second - first - 1), &parsed.index)) {
        return std::nullopt;
    }
    return parsed;
}

std::string FormatValues(const std::vector<long>& values) {
    std::vector<std::string> parts;
    for (const long v : values) parts.push_back(std::to_string(v));
    return ::android::base::Join(parts, ",");
}

std::optional<std::vector<long>> ParseValues(const std::string& text) {
    std::vector<long> values;
    for (const std::string& part : ::android::base::Split(text, ",")) {
        long v = 0;
        if (!::android::base::ParseInt(part, &v)) return std::nullopt;
        values.push_back(v);
    }
    return values;
}

}  // namespace

MixerSettings::MixerSettings(std::vector<alsa::CardInfo> cards)
    : cards_(std::move(cards)), store_("mixer"), entries_(store_.Load()) {}

std::string MixerSettings::CardId(int card) const {
    for (const alsa::CardInfo& info : cards_) {
        if (info.index == card) return info.id;
    }
    return {};
}

void MixerSettings::ApplyAll() {
    std::lock_guard guard(lock_);
    for (const alsa::CardInfo& card : cards_) ApplyLocked(card.index, card.id);
}

void MixerSettings::Apply(int card) {
    const std::string card_id = CardId(card);
    if (card_id.empty()) return;
    std::lock_guard guard(lock_);
    ApplyLocked(card, card_id);
}

void MixerSettings::ApplyLocked(int card, const std::string& card_id) {
    std::map<std::pair<std::string, unsigned int>, std::vector<long>> wanted;
    for (const auto& [key, value] : entries_) {
        const auto parsed = ParseKey(key);
        if (!parsed.has_value() || parsed->card_id != card_id) continue;
        if (auto values = ParseValues(value); values.has_value()) {
            wanted[{parsed->name, parsed->index}] = std::move(*values);
        }
    }
    if (wanted.empty()) return;
    for (const alsa::ControlInfo& control : alsa::ListControls(card)) {
        const auto it = wanted.find({control.name, control.index});
        if (it == wanted.end() || it->second == control.values) continue;
        if (const int err = alsa::WriteControl(card, control.numid, it->second); err < 0) {
            LOG(WARNING) << "card " << card_id << ": restoring " << control.ToString()
                         << " failed: " << alsa::ErrorString(err);
        } else {
            LOG(DEBUG) << "card " << card_id << ": restored \"" << control.name << "\"";
        }
    }
}

int MixerSettings::Set(int card, unsigned int numid, const std::vector<long>& values) {
    const std::string card_id = CardId(card);
    if (card_id.empty()) return -ENODEV;
    std::lock_guard guard(lock_);
    if (const int err = alsa::WriteControl(card, numid, values); err < 0) return err;
    // Store what the driver took, after clamping and per channel expansion.
    for (const alsa::ControlInfo& control : alsa::ListControls(card)) {
        if (control.numid != numid) continue;
        if (!StateStore::IsStorable(control.name)) break;
        entries_[Key(card_id, control.name, control.index)] = FormatValues(control.values);
        store_.Save(entries_);
        LOG(INFO) << "card " << card_id << ": set " << control.ToString();
        break;
    }
    return 0;
}

void MixerSettings::Forget(int card) {
    const std::string card_id = CardId(card);
    if (card_id.empty()) return;
    std::lock_guard guard(lock_);
    std::erase_if(entries_, [&card_id](const auto& entry) {
        const auto parsed = ParseKey(entry.first);
        return parsed.has_value() && parsed->card_id == card_id;
    });
    store_.Save(entries_);
    LOG(INFO) << "card " << card_id << ": forgot the stored mixer values";
}

bool MixerSettings::IsStored(int card, const std::string& name, unsigned int index) const {
    const std::string card_id = CardId(card);
    if (card_id.empty()) return false;
    std::lock_guard guard(lock_);
    return entries_.count(Key(card_id, name, index)) != 0;
}

}  // namespace aidl::android::hardware::audio::core::mainline::control
