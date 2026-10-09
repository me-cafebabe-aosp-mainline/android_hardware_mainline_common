/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "MainlineAudio_PcmArbiter"

#include "routing/PcmArbiter.h"

#include <iterator>
#include <sstream>

#include <android-base/logging.h>

namespace aidl::android::hardware::audio::core::mainline::routing {

PcmArbiter::Result PcmArbiter::Acquire(const std::shared_ptr<Client>& client,
                                       const alsa::PcmIdentity& pcm, int32_t* yield_timeout_ms) {
    std::lock_guard guard(lock_);
    Entry& entry = entries_[pcm];
    if (entry.owner == client) return Result::kGranted;
    const auto refuse = [&entry, &client] {
        const bool for_direct =
                (entry.owner != nullptr && entry.owner->priority() == Priority::kDirect) ||
                (entry.claimant != nullptr && entry.claimant->priority() == Priority::kDirect);
        if (client->priority() == Priority::kMixed && for_direct) {
            entry.silenced[client.get()] = client->name();
        }
        return Result::kBusy;
    };

    if (entry.owner == nullptr) {
        // A device given up for a claimant stays reserved for it.
        if (entry.claimant != nullptr && entry.claimant != client) return refuse();
        entry.owner = client;
        entry.claimant = nullptr;
        entry.silenced.erase(client.get());
        LOG(INFO) << __func__ << ": " << pcm.ToString() << " owned by " << client->name();
        return Result::kGranted;
    }

    if (client->priority() > entry.owner->priority() &&
        (entry.claimant == nullptr || entry.claimant == client)) {
        if (entry.claimant == nullptr) {
            LOG(INFO) << __func__ << ": " << client->name() << " asks " << entry.owner->name()
                      << " to yield " << pcm.ToString();
            entry.claimant = client;
        }
        entry.owner->yield_requested_.store(true);
        *yield_timeout_ms = 2 * entry.owner->burst_ms_ + kYieldMarginMs;
        return Result::kWaitForYield;
    }
    return refuse();
}

void PcmArbiter::ReleaseLocked(const Client& client,
                               std::map<alsa::PcmIdentity, Entry>::iterator it) {
    Entry& entry = it->second;
    if (entry.owner.get() == &client) {
        entry.owner = nullptr;
        LOG(INFO) << "Release: " << it->first.ToString() << " released by " << client.name()
                  << (entry.claimant != nullptr ? " for " + entry.claimant->name() : "");
    }
    if (entry.claimant.get() == &client) {
        entry.claimant = nullptr;
        LOG(INFO) << "Release: " << client.name() << " withdrew its claim on "
                  << it->first.ToString();
    }
    entry.silenced.erase(&client);
    if (entry.owner == nullptr && entry.claimant == nullptr) entries_.erase(it);
}

void PcmArbiter::Release(const Client& client, const alsa::PcmIdentity& pcm) {
    std::lock_guard guard(lock_);
    if (const auto it = entries_.find(pcm); it != entries_.end()) ReleaseLocked(client, it);
}

void PcmArbiter::ReleaseAll(const Client& client) {
    std::lock_guard guard(lock_);
    for (auto it = entries_.begin(); it != entries_.end();) {
        const auto next = std::next(it);
        ReleaseLocked(client, it);
        it = next;
    }
}

std::vector<alsa::PcmIdentity> PcmArbiter::PendingYields(const Client& client) const {
    std::lock_guard guard(lock_);
    std::vector<alsa::PcmIdentity> result;
    for (const auto& [pcm, entry] : entries_) {
        if (entry.owner.get() == &client && entry.claimant != nullptr) result.push_back(pcm);
    }
    return result;
}

std::string PcmArbiter::OwnerOf(const alsa::PcmIdentity& pcm) const {
    std::lock_guard guard(lock_);
    const auto it = entries_.find(pcm);
    if (it == entries_.end() || it->second.owner == nullptr) return {};
    return it->second.owner->name();
}

std::vector<std::string> PcmArbiter::SilencedOn(const alsa::PcmIdentity& pcm) const {
    std::lock_guard guard(lock_);
    std::vector<std::string> names;
    if (const auto it = entries_.find(pcm); it != entries_.end()) {
        for (const auto& [client, name] : it->second.silenced) names.push_back(name);
    }
    return names;
}

std::string PcmArbiter::Dump() const {
    std::lock_guard guard(lock_);
    std::ostringstream os;
    os << "PcmArbiter: " << entries_.size() << " device(s) in use\n";
    for (const auto& [pcm, entry] : entries_) {
        os << "  " << pcm.ToString()
           << ": owner=" << (entry.owner != nullptr ? entry.owner->name() : "none");
        if (entry.claimant != nullptr) os << " claimed by " << entry.claimant->name();
        for (const auto& [client, name] : entry.silenced) os << ", silencing " << name;
        os << "\n";
    }
    return os.str();
}

}  // namespace aidl::android::hardware::audio::core::mainline::routing
