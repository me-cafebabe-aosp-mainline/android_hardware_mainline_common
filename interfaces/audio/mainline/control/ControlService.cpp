/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "MainlineAudio_Control"

#include "control/ControlService.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <optional>
#include <utility>

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/strings.h>

#include "alsa/AlsaCard.h"
#include "alsa/AlsaControls.h"
#include "alsa/AlsaError.h"
#include "passthrough/Format.h"
#include "passthrough/SinkCapabilities.h"
#include "ucm/UcmManager.h"

namespace aidl::android::hardware::audio::core::mainline::control {

using ::aidl::android::media::audio::common::AudioDeviceAddress;
using ::aidl::vendor::mainline::audio::CardStatus;
using ::aidl::vendor::mainline::audio::EndpointStatus;
using ::aidl::vendor::mainline::audio::HdmiSinkStatus;
using ::aidl::vendor::mainline::audio::MixerControl;
using ::aidl::vendor::mainline::audio::OutputSettings;
using ::aidl::vendor::mainline::audio::Setting;
using ::aidl::vendor::mainline::audio::SettingChange;
using ::aidl::vendor::mainline::audio::SinkFormat;
using ::aidl::vendor::mainline::audio::Status;
using ::aidl::vendor::mainline::audio::TrialStatus;

namespace {

// "OUT_SPEAKER", "OUT_DEVICE hdmi", ...
std::string DescribeDeviceType(const ::aidl::android::media::audio::common::AudioDevice& device) {
    std::string text = ::aidl::android::media::audio::common::toString(device.type.type);
    if (!device.type.connection.empty()) text += " " + device.type.connection;
    return text;
}

std::string AddressOf(const ::aidl::android::media::audio::common::AudioDevice& device) {
    if (device.address.getTag() != AudioDeviceAddress::Tag::id) return {};
    return device.address.get<AudioDeviceAddress::Tag::id>();
}

int32_t ToAidl(Properties::Spec::Type type) {
    switch (type) {
        case Properties::Spec::Type::kBool:
            return Setting::TYPE_BOOLEAN;
        case Properties::Spec::Type::kInt:
            return Setting::TYPE_INTEGER;
        case Properties::Spec::Type::kString:
            return Setting::TYPE_STRING;
        case Properties::Spec::Type::kList:
            return Setting::TYPE_LIST;
        case Properties::Spec::Type::kIntList:
            return Setting::TYPE_INTEGER_LIST;
    }
    return Setting::TYPE_STRING;
}

// Checks that go beyond the spec's type.
bool IsAcceptable(const std::string& key, const std::string& value) {
    if (key != "hdmi.passthrough_formats") return true;
    for (const std::string& name : ::android::base::Split(value, ",")) {
        const std::string trimmed = ::android::base::Trim(name);
        if (!trimmed.empty() && !passthrough::EncodedFormatFromString(trimmed).has_value()) {
            return false;
        }
    }
    return true;
}

constexpr std::array<std::pair<int32_t, dsp::Layout>, 5> kLayouts = {{
        {OutputSettings::LAYOUT_AUTO, dsp::Layout::kAuto},
        {OutputSettings::LAYOUT_STEREO, dsp::Layout::kStereo},
        {OutputSettings::LAYOUT_QUAD, dsp::Layout::kQuad},
        {OutputSettings::LAYOUT_5_1, dsp::Layout::k5Point1},
        {OutputSettings::LAYOUT_7_1, dsp::Layout::k7Point1},
}};

OutputSettings ToAidl(const dsp::OutputConfig& config) {
    OutputSettings settings{.fill = config.fill,
                            .swapLeftRight = config.swap_left_right,
                            .mono = config.mono,
                            .subwoofer = config.subwoofer,
                            .smallSpeakers = config.small_speakers,
                            .crossoverHz = config.crossover_hz};
    for (const auto& [aidl, layout] : kLayouts) {
        if (layout == config.layout) settings.layout = aidl;
    }
    const size_t channels = dsp::LayoutRoles(config.layout).size();
    settings.levelsDb.assign(config.levels_db.begin(), config.levels_db.begin() + channels);
    settings.delaysMs.assign(config.delays_ms.begin(), config.delays_ms.begin() + channels);
    return settings;
}

std::optional<dsp::OutputConfig> FromAidl(const OutputSettings& settings) {
    dsp::OutputConfig config;
    const auto layout = std::find_if(kLayouts.begin(), kLayouts.end(), [&](const auto& entry) {
        return entry.first == settings.layout;
    });
    if (layout == kLayouts.end()) return std::nullopt;
    config.layout = layout->second;
    config.fill = settings.fill;
    config.swap_left_right = settings.swapLeftRight;
    config.mono = settings.mono;
    config.subwoofer = settings.subwoofer;
    config.small_speakers = settings.smallSpeakers;
    config.crossover_hz = settings.crossoverHz;
    if (settings.levelsDb.size() > dsp::kMaxChannels ||
        settings.delaysMs.size() > dsp::kMaxChannels) {
        return std::nullopt;
    }
    std::copy(settings.levelsDb.begin(), settings.levelsDb.end(), config.levels_db.begin());
    std::copy(settings.delaysMs.begin(), settings.delaysMs.end(), config.delays_ms.begin());
    // Speakers the layout does not have carry nothing.
    for (size_t i = dsp::LayoutRoles(config.layout).size(); i < dsp::kMaxChannels; ++i) {
        config.levels_db[i] = 0;
        config.delays_ms[i] = 0;
    }
    if (!config.IsValid()) return std::nullopt;
    return config;
}

CardStatus MakeCardStatus(const alsa::CardInfo& card, const routing::DeviceInventory& inventory) {
    CardStatus status{.index = card.index,
                      .id = card.id,
                      .driver = card.driver,
                      .name = card.name,
                      .longName = card.long_name,
                      .mixerName = card.mixer_name,
                      .primary = card.index == inventory.primary_card()};
    if (ucm::UcmManager* ucm = inventory.UcmForCard(card.index); ucm != nullptr) {
        status.ucmVerb = ucm->verb();
        for (const ucm::UcmDevice& device : ucm->devices()) {
            status.ucmDevices.push_back(device.name);
            if (ucm->IsDeviceEnabled(device.name)) status.ucmEnabledDevices.push_back(device.name);
        }
    }
    return status;
}

}  // namespace

ControlService::ControlService(std::shared_ptr<ModuleMainline> module,
                               std::shared_ptr<MixerSettings> mixer,
                               std::shared_ptr<SettingsStore> settings,
                               std::shared_ptr<OutputSettingsStore> outputs)
    : module_(std::move(module)),
      mixer_(std::move(mixer)),
      settings_(std::move(settings)),
      outputs_(std::move(outputs)) {}

const routing::Endpoint* ControlService::FindOutput(const std::string& key) const {
    for (const routing::Endpoint& endpoint : module_->inventory().endpoints()) {
        if (!endpoint.is_input && !endpoint.IsNull() && endpoint.Key() == key) return &endpoint;
    }
    return nullptr;
}

bool ControlService::UsesCard(int32_t card) const {
    const auto& cards = module_->inventory().cards();
    return std::any_of(cards.begin(), cards.end(),
                       [card](const alsa::CardInfo& info) { return info.index == card; });
}

ndk::ScopedAStatus ControlService::getStatus(Status* _aidl_return) {
    const routing::DeviceInventory& inventory = module_->inventory();
    Status status;
    for (const alsa::CardInfo& card : inventory.cards()) {
        status.cards.push_back(MakeCardStatus(card, inventory));
    }
    for (const routing::Endpoint& endpoint : inventory.endpoints()) {
        status.endpoints.push_back(MakeEndpointStatus(endpoint));
    }
    *_aidl_return = std::move(status);
    return ndk::ScopedAStatus::ok();
}

EndpointStatus ControlService::MakeEndpointStatus(const routing::Endpoint& endpoint) const {
    EndpointStatus status{.key = endpoint.Key(),
                          .name = endpoint.name,
                          .role = routing::ToString(endpoint.role),
                          .input = endpoint.is_input,
                          .attached = endpoint.IsAttached(),
                          .isDefault = endpoint.is_default,
                          .deviceType = DescribeDeviceType(endpoint.device),
                          .address = AddressOf(endpoint.device),
                          .nullDevice = endpoint.IsNull(),
                          .card = endpoint.card,
                          .cardId = endpoint.card_id,
                          .pcm = endpoint.pcm_name,
                          .ucmDevice = endpoint.ucm_device,
                          .priority = endpoint.priority,
                          .minChannels = static_cast<int32_t>(endpoint.caps.min_channels),
                          .maxChannels = static_cast<int32_t>(endpoint.caps.max_channels),
                          .activeStreams = module_->routing_controller().UseCount(endpoint),
                          .jack = EndpointStatus::JACK_UNKNOWN};
    for (const snd_pcm_format_t format : endpoint.caps.formats) {
        if (const char* name = snd_pcm_format_name(format); name != nullptr) {
            status.formats.push_back(name);
        }
    }
    for (const unsigned int rate : endpoint.caps.rates) {
        status.sampleRates.push_back(static_cast<int32_t>(rate));
    }
    if (endpoint.IsNull()) return status;
    if (endpoint.pcm_identity.IsKnown()) {
        status.pcmOwner = module_->pcm_arbiter().OwnerOf(endpoint.pcm_identity);
    }
    if (!endpoint.jack_control.empty()) {
        if (const auto plugged = alsa::ReadJackState(endpoint.card, endpoint.jack_control);
            plugged.has_value()) {
            status.jack = *plugged ? EndpointStatus::JACK_PLUGGED : EndpointStatus::JACK_UNPLUGGED;
        }
    }
    if (endpoint.is_hdmi_head && !endpoint.is_input) status.hdmi = MakeHdmiSinkStatus(endpoint);
    return status;
}

HdmiSinkStatus ControlService::MakeHdmiSinkStatus(const routing::Endpoint& head) const {
    HdmiSinkStatus status{
            .source = passthrough::ToString(passthrough::SinkCapabilities::Source::kNone)};
    std::unique_ptr<passthrough::HdmiControl> control = module_->MakeHdmiControl(head);
    if (control == nullptr) return status;
    const passthrough::SinkCapabilities sink = module_->ResolveSinkCapabilities(*control);
    status.passthroughPossible = true;
    status.controls = control->Describe();
    status.source = passthrough::ToString(sink.source);
    status.monitorName = sink.monitor_name;
    status.maxPcmChannels = static_cast<int32_t>(sink.max_pcm_channels);
    status.hbr = passthrough::ToString(control->Hbr());
    for (const auto& [format, caps] : sink.formats) {
        SinkFormat entry{.name = passthrough::ToString(format),
                         .maxChannels = static_cast<int32_t>(caps.max_channels)};
        for (const uint32_t rate : caps.rates) {
            entry.sampleRates.push_back(static_cast<int32_t>(rate));
        }
        status.formats.push_back(std::move(entry));
    }
    return status;
}

ndk::ScopedAStatus ControlService::getReport(std::string* _aidl_return) {
    std::string report = module_->DumpState();
    for (const char* path : {"/proc/asound/cards", "/proc/asound/pcm"}) {
        std::string contents;
        if (!::android::base::ReadFileToString(path, &contents)) {
            contents = "(unreadable)\n";
        }
        report += std::string("\n") + path + ":\n" + contents;
    }
    *_aidl_return = std::move(report);
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus ControlService::getMixerControls(int32_t card,
                                                    std::vector<MixerControl>* _aidl_return) {
    if (!UsesCard(card)) return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    std::vector<MixerControl> controls;
    for (const alsa::ControlInfo& info : alsa::ListControls(card)) {
        MixerControl control{.numid = static_cast<int32_t>(info.numid),
                             .name = info.name,
                             .index = static_cast<int32_t>(info.index),
                             .min = info.min,
                             .max = info.max,
                             .step = info.step,
                             .items = info.items,
                             .stored = mixer_->IsStored(card, info.name, info.index)};
        switch (info.type) {
            case alsa::ControlInfo::Type::kBoolean:
                control.type = MixerControl::TYPE_BOOLEAN;
                break;
            case alsa::ControlInfo::Type::kInteger:
                control.type = MixerControl::TYPE_INTEGER;
                break;
            case alsa::ControlInfo::Type::kEnumerated:
                control.type = MixerControl::TYPE_ENUMERATED;
                break;
        }
        control.values.assign(info.values.begin(), info.values.end());
        if (info.db_range.has_value()) {
            control.hasDbRange = true;
            control.minDb = static_cast<int32_t>(info.db_range->first);
            control.maxDb = static_cast<int32_t>(info.db_range->second);
        }
        controls.push_back(std::move(control));
    }
    *_aidl_return = std::move(controls);
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus ControlService::setMixerControl(int32_t card, int32_t numid,
                                                   const std::vector<int64_t>& values) {
    if (!UsesCard(card) || numid <= 0 || values.empty()) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }
    const std::vector<long> alsa_values(values.begin(), values.end());
    if (const int err = mixer_->Set(card, static_cast<unsigned int>(numid), alsa_values); err < 0) {
        LOG(WARNING) << __func__ << ": card " << card << " control " << numid << ": "
                     << alsa::ErrorString(err);
        return err == -EINVAL ? ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT)
                              : ndk::ScopedAStatus::fromServiceSpecificError(-err);
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus ControlService::resetMixerControls(int32_t card) {
    if (!UsesCard(card)) return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    mixer_->Forget(card);
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus ControlService::getSettings(std::vector<Setting>* _aidl_return) {
    std::vector<std::string> card_ids;
    for (const alsa::CardInfo& card : module_->inventory().cards()) card_ids.push_back(card.id);
    const auto running = module_->properties().Values(card_ids);
    const auto defaults = Properties::Load().Values(card_ids);
    const SettingsStore::Settings stored = settings_->committed();
    std::vector<Setting> settings;
    for (const Properties::Spec& spec : Properties::Specs(card_ids)) {
        Setting setting{.key = spec.key,
                        .type = ToAidl(spec.type),
                        .min = spec.min,
                        .max = spec.max,
                        .value = running.at(spec.key),
                        .defaultValue = defaults.at(spec.key)};
        if (const auto it = stored.find(spec.key); it != stored.end()) {
            setting.stored = it->second;
        }
        settings.push_back(std::move(setting));
    }
    *_aidl_return = std::move(settings);
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus ControlService::applySettings(const std::vector<SettingChange>& changes) {
    if (settings_->on_trial()) return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
    SettingsStore::Settings next = settings_->committed();
    for (const SettingChange& change : changes) {
        const auto spec = Properties::SpecOf(change.key);
        if (!spec.has_value()) {
            LOG(WARNING) << __func__ << ": unknown setting \"" << change.key << "\"";
            return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
        }
        if (!change.value.has_value()) {
            next.erase(change.key);
            continue;
        }
        const std::string value = ::android::base::Trim(*change.value);
        if (!Properties::IsValid(*spec, value) || !IsAcceptable(change.key, value)) {
            LOG(WARNING) << __func__ << ": invalid value \"" << value << "\" for " << change.key;
            return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
        }
        next[change.key] = value;
    }
    if (!settings_->StartTrial(std::move(next))) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus ControlService::getTrialStatus(TrialStatus* _aidl_return) {
    *_aidl_return =
            TrialStatus{.active = settings_->on_trial(),
                        .secondsLeft = static_cast<int32_t>(settings_->TrialTimeLeft().count()),
                        .revertedAfterFailure = settings_->reverted_after_failure()};
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus ControlService::keepSettings() {
    return settings_->Keep() ? ndk::ScopedAStatus::ok()
                             : ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
}

ndk::ScopedAStatus ControlService::revertSettings() {
    return settings_->Revert() ? ndk::ScopedAStatus::ok()
                               : ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
}

ndk::ScopedAStatus ControlService::resetSettings() {
    return settings_->Reset() ? ndk::ScopedAStatus::ok()
                              : ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
}

ndk::ScopedAStatus ControlService::getOutputSettings(const std::string& key,
                                                     OutputSettings* _aidl_return) {
    if (FindOutput(key) == nullptr) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }
    *_aidl_return = ToAidl(module_->output_configs()->Get(key));
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus ControlService::setOutputSettings(const std::string& key,
                                                     const OutputSettings& settings) {
    const routing::Endpoint* endpoint = FindOutput(key);
    const auto config = FromAidl(settings);
    if (endpoint == nullptr || !config.has_value() ||
        dsp::LayoutRoles(config->layout).size() > endpoint->caps.max_channels) {
        LOG(WARNING) << __func__ << ": rejected for " << key << ": " << settings.toString();
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }
    outputs_->Set(key, *config);
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus ControlService::resetOutputSettings(const std::string& key) {
    if (FindOutput(key) == nullptr) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }
    outputs_->Reset(key);
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus ControlService::playTestTone(const std::string& key, int32_t channel,
                                                int32_t duration_ms) {
    if (FindOutput(key) == nullptr) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }
    const std::shared_ptr<dsp::OutputConfigs>& configs = module_->output_configs();
    if (channel < 0) {
        configs->StopTone();
        return ndk::ScopedAStatus::ok();
    }
    const size_t speakers = dsp::LayoutRoles(configs->Get(key).layout).size();
    if (static_cast<size_t>(channel) >= speakers) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }
    configs->StartTone(key, static_cast<unsigned int>(channel),
                       std::chrono::milliseconds(std::clamp(duration_ms, 100, 5000)));
    return ndk::ScopedAStatus::ok();
}

}  // namespace aidl::android::hardware::audio::core::mainline::control
