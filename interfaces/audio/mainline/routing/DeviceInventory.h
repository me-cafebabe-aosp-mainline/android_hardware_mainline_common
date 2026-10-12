/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <aidl/android/media/audio/common/AudioDevice.h>

#include "Properties.h"
#include "alsa/AlsaCard.h"
#include "alsa/HdaFrontPanel.h"
#include "routing/Endpoint.h"
#include "ucm/UcmManager.h"

namespace aidl::android::hardware::audio::core::mainline::routing {

// Discovers the sound cards of the system once at start-up and turns them
// into a list of endpoints. The inventory is immutable afterwards (apart from
// the port ids assigned by the configuration builder) and shared between the
// module and its streams.
class DeviceInventory {
  public:
    static std::shared_ptr<DeviceInventory> Discover(const Properties& properties);

    const std::vector<alsa::CardInfo>& cards() const { return cards_; }
    int primary_card() const { return primary_card_; }
    const std::vector<Endpoint>& endpoints() const { return endpoints_; }
    std::vector<Endpoint>& mutable_endpoints() { return endpoints_; }

    // Finds the endpoint behind a device port. `device` must match type,
    // connection and address exactly.
    const Endpoint* FindByDevice(
            const ::aidl::android::media::audio::common::AudioDevice& device) const;
    const Endpoint* FindByPortId(int32_t port_id) const;

    // Chooses the highest-priority plugged HDMI head behind the one template.
    // Unknown jack states retain the normal priority order as a fallback.
    const Endpoint* SelectHdmiEndpoint(const Endpoint& template_endpoint) const;

    // Synthesizes an endpoint for a USB device that the framework connected
    // through connectExternalDevice(). The address carries the ALSA card and
    // device numbers.
    std::optional<Endpoint> MakeUsbEndpoint(
            const ::aidl::android::media::audio::common::AudioDevice& device, bool is_input) const;

    // UCM manager of a card, nullptr when the card has no profile.
    ucm::UcmManager* UcmForCard(int card) const;

    // True when at least one output endpoint accepts six or more channels.
    bool HasMultichannelOutput() const;

    std::string Dump() const;

  private:
    DeviceInventory() = default;

    void SelectCards(const Properties& properties);
    void ConfigureFrontPanels(const Properties& properties);
    // Whether the card's front panel jacks were marked as not detectable.
    bool HasAc97FrontPanel(int card) const;
    // Turns wired templates of an AC97 front panel into bus ports.
    void DemoteUndetectableJacks(const alsa::CardInfo& card, const ucm::UcmManager& ucm,
                                 size_t first_endpoint);
    void CollectCandidates(const Properties& properties);
    void CollectFromUcm(const alsa::CardInfo& card, ucm::UcmManager& ucm);
    void CollectFromPcmDevices(const alsa::CardInfo& card);
    void ProbeCapabilities();
    void FilterCapabilities(const Properties& properties);
    void AssignRoles();
    void AddNullEndpointsIfNeeded(bool null_mic);
    void FinalizeEndpoints();

    // The settings discovery ran with, also for USB cards connected later.
    Properties properties_;
    std::vector<alsa::CardInfo> cards_;
    // By card index, for cards with `ac97_front_panel`.
    std::map<int, alsa::FrontPanelResult> front_panels_;
    int primary_card_ = -1;
    std::map<int, std::unique_ptr<ucm::UcmManager>> ucm_managers_;
    std::vector<Endpoint> endpoints_;
};

}  // namespace aidl::android::hardware::audio::core::mainline::routing
