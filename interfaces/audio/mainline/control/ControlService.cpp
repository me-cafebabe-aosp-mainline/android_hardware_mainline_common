/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "MainlineAudio_Control"

#include "control/ControlService.h"

#include <algorithm>
#include <utility>

#include <android-base/file.h>
#include <android-base/logging.h>

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
using ::aidl::vendor::mainline::audio::SinkFormat;
using ::aidl::vendor::mainline::audio::Status;

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
                               std::shared_ptr<MixerSettings> mixer)
    : module_(std::move(module)), mixer_(std::move(mixer)) {}

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

}  // namespace aidl::android::hardware::audio::core::mainline::control
