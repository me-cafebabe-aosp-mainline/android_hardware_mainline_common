/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "MainlineAudio_Properties"

#include "Properties.h"

#include <algorithm>
#include <regex>
#include <set>
#include <sstream>

#include <android-base/logging.h>
#include <android-base/parsebool.h>
#include <android-base/parseint.h>
#include <android-base/properties.h>
#include <android-base/strings.h>

namespace aidl::android::hardware::audio::core::mainline {

namespace {

using Overrides = std::map<std::string, std::string>;
using Spec = Properties::Spec;

// Reads settings: a stored override, else the Android property.
class Source {
  public:
    explicit Source(const Overrides& overrides) : overrides_(overrides) {}

    std::string String(const std::string& key, const std::string& fallback) const {
        if (const auto it = overrides_.find(key); it != overrides_.end()) return it->second;
        return ::android::base::GetProperty(Properties::kPrefix + key, fallback);
    }

    bool Bool(const std::string& key, bool fallback) const {
        switch (::android::base::ParseBool(String(key, ""))) {
            case ::android::base::ParseBoolResult::kTrue:
                return true;
            case ::android::base::ParseBoolResult::kFalse:
                return false;
            case ::android::base::ParseBoolResult::kError:
                break;
        }
        return fallback;
    }

    // Unset when neither an override nor the property gives a boolean.
    std::optional<bool> OptionalBool(const std::string& key) const {
        switch (::android::base::ParseBool(String(key, ""))) {
            case ::android::base::ParseBoolResult::kTrue:
                return true;
            case ::android::base::ParseBoolResult::kFalse:
                return false;
            case ::android::base::ParseBoolResult::kError:
                break;
        }
        return std::nullopt;
    }

    int Int(const std::string& key, int fallback) const {
        int value = fallback;
        if (!::android::base::ParseInt(::android::base::Trim(String(key, "")), &value)) {
            return fallback;
        }
        return value;
    }

  private:
    const Overrides& overrides_;
};

std::vector<std::string> SplitList(const std::string& value) {
    std::vector<std::string> result;
    for (const auto& item : ::android::base::Split(value, ",")) {
        std::string trimmed = ::android::base::Trim(item);
        if (!trimmed.empty()) result.push_back(std::move(trimmed));
    }
    return result;
}

std::set<int> ParseIntSet(const std::string& value) {
    std::set<int> result;
    for (const auto& item : SplitList(value)) {
        int v;
        if (::android::base::ParseInt(item, &v)) result.insert(v);
    }
    return result;
}

const char* BoolText(bool value) {
    return value ? "true" : "false";
}

std::string JoinInts(const std::set<int>& values) {
    std::vector<std::string> parts;
    for (const int v : values) parts.push_back(std::to_string(v));
    return ::android::base::Join(parts, ",");
}

// Global settings. Keep in sync with Load(), Values() and README.md.
const std::vector<Spec>& GlobalSpecs() {
    static const std::vector<Spec> specs = {
            {"cards", Spec::Type::kList},
            {"wait_for_cards_ms", Spec::Type::kInt, 0, 60000},
            {"primary_card", Spec::Type::kString},
            {"include_usb_cards", Spec::Type::kBool},
            {"null_mic", Spec::Type::kBool},
            {"ucm.enabled", Spec::Type::kBool},
            {"ucm.verb", Spec::Type::kString},
            {"mixer.init", Spec::Type::kBool},
            {"mixer.playback_percent", Spec::Type::kInt, 0, 100},
            {"mixer.capture_percent", Spec::Type::kInt, 0, 100},
            {"mixer.toggle", Spec::Type::kBool},
            {"latency_ms", Spec::Type::kInt, 5, 500},
            {"fast_latency_ms", Spec::Type::kInt, 0, 500},
            {"multichannel", Spec::Type::kBool},
            {"hdmi.passthrough", Spec::Type::kBool},
            {"hdmi.passthrough_formats", Spec::Type::kList},
            {"log.verbose", Spec::Type::kBool},
    };
    return specs;
}

// Per-card settings, "card.<selector>.<suffix>".
std::vector<Spec> CardSpecs(const std::string& selector) {
    const std::string prefix = "card." + selector + ".";
    return {
            {prefix + "rates", Spec::Type::kIntList, 8000, 768000},
            {prefix + "bits", Spec::Type::kIntList, 8, 32},
            {prefix + "hbr", Spec::Type::kBool},
            {prefix + "ac97_front_panel", Spec::Type::kBool},
            {prefix + "ucm_hda", Spec::Type::kBool},
    };
}

}  // namespace

Properties Properties::Load(std::map<std::string, std::string> overrides) {
    Properties props;
    props.overrides = std::move(overrides);
    const Source source(props.overrides);
    props.cards = SplitList(source.String("cards", ""));
    props.wait_for_cards_ms =
            std::clamp(source.Int("wait_for_cards_ms", props.wait_for_cards_ms), 0, 60000);
    props.primary_card = ::android::base::Trim(source.String("primary_card", ""));
    props.include_usb_cards = source.Bool("include_usb_cards", props.include_usb_cards);
    props.null_mic = source.Bool("null_mic", props.null_mic);
    props.ucm_enabled = source.Bool("ucm.enabled", props.ucm_enabled);
    props.ucm_verb = source.String("ucm.verb", props.ucm_verb);
    props.mixer_init = source.Bool("mixer.init", props.mixer_init);
    props.mixer_playback_percent =
            std::clamp(source.Int("mixer.playback_percent", props.mixer_playback_percent), 0, 100);
    props.mixer_capture_percent =
            std::clamp(source.Int("mixer.capture_percent", props.mixer_capture_percent), 0, 100);
    props.mixer_toggle = source.Bool("mixer.toggle", props.mixer_toggle);
    props.latency_ms = std::clamp(source.Int("latency_ms", props.latency_ms), 5, 500);
    if (const int fast = source.Int("fast_latency_ms", props.fast_latency_ms); fast > 0) {
        props.fast_latency_ms = std::min(fast, 500);
    }
    props.multichannel = source.Bool("multichannel", props.multichannel);
    props.hdmi_passthrough = source.Bool("hdmi.passthrough", props.hdmi_passthrough);
    props.hdmi_passthrough_formats = SplitList(source.String("hdmi.passthrough_formats", ""));
    props.verbose_logging = source.Bool("log.verbose", props.verbose_logging);
    return props;
}

std::string Properties::ToString() const {
    std::ostringstream os;
    os << "cards=[" << ::android::base::Join(cards, ",") << "]"
       << " wait_for_cards_ms=" << wait_for_cards_ms << " primary_card=\"" << primary_card << "\""
       << " include_usb_cards=" << include_usb_cards << " null_mic=" << null_mic
       << " ucm.enabled=" << ucm_enabled << " ucm.verb=\"" << ucm_verb << "\""
       << " mixer.init=" << mixer_init << " mixer.playback_percent=" << mixer_playback_percent
       << " mixer.capture_percent=" << mixer_capture_percent << " mixer.toggle=" << mixer_toggle
       << " latency_ms=" << latency_ms
       << " fast_latency_ms=" << fast_latency_ms << " multichannel=" << multichannel
       << " hdmi.passthrough=" << hdmi_passthrough << " hdmi.passthrough_formats=["
       << ::android::base::Join(hdmi_passthrough_formats, ",")
       << "] log.verbose=" << verbose_logging;
    if (!overrides.empty()) {
        os << " overrides={";
        bool first = true;
        for (const auto& [key, value] : overrides) {
            os << (first ? "" : ", ") << key << "=\"" << value << "\"";
            first = false;
        }
        os << "}";
    }
    return os.str();
}

Properties::CardProperties Properties::ForCard(const std::string& card_id, int card_index,
                                               const std::string& card_name) const {
    const Source source(overrides);
    CardProperties merged;
    std::string name_with_underscores = card_name;
    std::replace(name_with_underscores.begin(), name_with_underscores.end(), ' ', '_');

    for (const std::string& selector :
         {card_id, std::to_string(card_index), name_with_underscores}) {
        const std::string prefix = "card." + selector + ".";
        auto rates = ParseIntSet(source.String(prefix + "rates", ""));
        merged.rates.insert(rates.begin(), rates.end());
        auto bits = ParseIntSet(source.String(prefix + "bits", ""));
        merged.bits.insert(bits.begin(), bits.end());
        if (!merged.hbr.has_value()) merged.hbr = source.OptionalBool(prefix + "hbr");
        merged.ac97_front_panel |= source.Bool(prefix + "ac97_front_panel", false);
        merged.ucm_hda |= source.Bool(prefix + "ucm_hda", false);
    }
    return merged;
}

std::vector<Properties::Spec> Properties::Specs(const std::vector<std::string>& card_ids) {
    std::vector<Spec> specs = GlobalSpecs();
    for (const std::string& id : card_ids) {
        const std::vector<Spec> card = CardSpecs(id);
        specs.insert(specs.end(), card.begin(), card.end());
    }
    return specs;
}

std::optional<Properties::Spec> Properties::SpecOf(const std::string& key) {
    for (const Spec& spec : GlobalSpecs()) {
        if (spec.key == key) return spec;
    }
    // Selectors are card ids, indexes or names with underscores for spaces.
    static const std::regex kCardKey(
            R"(card\.([A-Za-z0-9_\-]+)\.(rates|bits|hbr|ac97_front_panel|ucm_hda))");
    std::smatch match;
    if (!std::regex_match(key, match, kCardKey)) return std::nullopt;
    for (const Spec& spec : CardSpecs(match[1].str())) {
        if (spec.key == key) return spec;
    }
    return std::nullopt;
}

bool Properties::IsValid(const Spec& spec, const std::string& value) {
    if (value.find_first_of("\t\r\n") != std::string::npos) return false;
    switch (spec.type) {
        case Spec::Type::kBool:
            return ::android::base::ParseBool(value) != ::android::base::ParseBoolResult::kError;
        case Spec::Type::kInt: {
            int v = 0;
            return ::android::base::ParseInt(::android::base::Trim(value), &v, spec.min, spec.max);
        }
        case Spec::Type::kString:
        case Spec::Type::kList:
            return true;
        case Spec::Type::kIntList:
            for (const std::string& item : SplitList(value)) {
                int v = 0;
                if (!::android::base::ParseInt(item, &v, spec.min, spec.max)) return false;
            }
            return true;
    }
    return false;
}

std::map<std::string, std::string> Properties::Values(
        const std::vector<std::string>& card_ids) const {
    std::map<std::string, std::string> values = {
            {"cards", ::android::base::Join(cards, ",")},
            {"wait_for_cards_ms", std::to_string(wait_for_cards_ms)},
            {"primary_card", primary_card},
            {"include_usb_cards", BoolText(include_usb_cards)},
            {"null_mic", BoolText(null_mic)},
            {"ucm.enabled", BoolText(ucm_enabled)},
            {"ucm.verb", ucm_verb},
            {"mixer.init", BoolText(mixer_init)},
            {"mixer.playback_percent", std::to_string(mixer_playback_percent)},
            {"mixer.capture_percent", std::to_string(mixer_capture_percent)},
            {"mixer.toggle", BoolText(mixer_toggle)},
            {"latency_ms", std::to_string(latency_ms)},
            {"fast_latency_ms", std::to_string(fast_latency_ms)},
            {"multichannel", BoolText(multichannel)},
            {"hdmi.passthrough", BoolText(hdmi_passthrough)},
            {"hdmi.passthrough_formats", ::android::base::Join(hdmi_passthrough_formats, ",")},
            {"log.verbose", BoolText(verbose_logging)},
    };
    // Per-card keys report their own selector only, not what the other
    // selectors of the card add (see ForCard()).
    const Source source(overrides);
    for (const std::string& id : card_ids) {
        const std::string prefix = "card." + id + ".";
        values[prefix + "rates"] = JoinInts(ParseIntSet(source.String(prefix + "rates", "")));
        values[prefix + "bits"] = JoinInts(ParseIntSet(source.String(prefix + "bits", "")));
        const auto hbr = source.OptionalBool(prefix + "hbr");
        values[prefix + "hbr"] = hbr.has_value() ? BoolText(*hbr) : "";
        values[prefix + "ac97_front_panel"] =
                BoolText(source.Bool(prefix + "ac97_front_panel", false));
        values[prefix + "ucm_hda"] = BoolText(source.Bool(prefix + "ucm_hda", false));
    }
    return values;
}

}  // namespace aidl::android::hardware::audio::core::mainline
