/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <mutex>
#include <string>
#include <vector>

#include <android-base/thread_annotations.h>

#include "alsa/AlsaCard.h"
#include "control/StateStore.h"

namespace aidl::android::hardware::audio::core::mainline::control {

// Mixer control values the user set through IControl. They are stored by card
// id, control name and index (numids and card indexes change between boots)
// and applied again when the HAL starts and whenever UCM enables a device,
// since UCM sequences set the controls they know about. Thread safe.
class MixerSettings {
  public:
    explicit MixerSettings(std::vector<alsa::CardInfo> cards);

    // Applies the stored values of every card.
    void ApplyAll();
    // Applies the stored values of `card`.
    void Apply(int card);

    // Writes `values` to the control `numid` of `card` and remembers them.
    // Returns 0 or a negative errno.
    int Set(int card, unsigned int numid, const std::vector<long>& values);
    // Forgets the stored values of `card`; the current values stay.
    void Forget(int card);
    // Whether a value of the control is stored.
    bool IsStored(int card, const std::string& name, unsigned int index) const;

  private:
    // Card id of `card`, empty for a card the HAL does not use.
    std::string CardId(int card) const;
    void ApplyLocked(int card, const std::string& card_id) REQUIRES(lock_);

    const std::vector<alsa::CardInfo> cards_;
    const StateStore store_;
    mutable std::mutex lock_;
    StateStore::Entries entries_ GUARDED_BY(lock_);
};

}  // namespace aidl::android::hardware::audio::core::mainline::control
