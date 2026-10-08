/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "MainlineAudio_AlsaControls"

#include "alsa/AlsaControls.h"

#include <algorithm>
#include <memory>
#include <sstream>

#include <alsa/asoundlib.h>
#include <android-base/logging.h>

#include "alsa/AlsaError.h"

namespace aidl::android::hardware::audio::core::mainline::alsa {

namespace {

template <typename T, void (*Free)(T*)>
struct Freer {
    void operator()(T* p) const {
        if (p != nullptr) Free(p);
    }
};
using ElemIdPtr =
        std::unique_ptr<snd_ctl_elem_id_t, Freer<snd_ctl_elem_id_t, snd_ctl_elem_id_free>>;
using ElemInfoPtr =
        std::unique_ptr<snd_ctl_elem_info_t, Freer<snd_ctl_elem_info_t, snd_ctl_elem_info_free>>;
using ElemValuePtr =
        std::unique_ptr<snd_ctl_elem_value_t, Freer<snd_ctl_elem_value_t, snd_ctl_elem_value_free>>;
using ElemListPtr =
        std::unique_ptr<snd_ctl_elem_list_t, Freer<snd_ctl_elem_list_t, snd_ctl_elem_list_free>>;

ElemIdPtr AllocId() {
    snd_ctl_elem_id_t* p = nullptr;
    snd_ctl_elem_id_malloc(&p);
    return ElemIdPtr(p);
}

ElemInfoPtr AllocInfo() {
    snd_ctl_elem_info_t* p = nullptr;
    snd_ctl_elem_info_malloc(&p);
    return ElemInfoPtr(p);
}

ElemValuePtr AllocValue() {
    snd_ctl_elem_value_t* p = nullptr;
    snd_ctl_elem_value_malloc(&p);
    return ElemValuePtr(p);
}

CtlHandle OpenCtl(int card) {
    snd_ctl_t* ctl = nullptr;
    const std::string name = "hw:" + std::to_string(card);
    if (const int err = snd_ctl_open(&ctl, name.c_str(), 0); err < 0) {
        LOG(WARNING) << "snd_ctl_open(" << name << "): " << ErrorString(err);
        return nullptr;
    }
    return CtlHandle(ctl);
}

// The control behind `id`, nullopt when it is not one ListControls() offers.
std::optional<ControlInfo> ReadControl(snd_ctl_t* ctl, snd_ctl_elem_id_t* id) {
    ElemInfoPtr info = AllocInfo();
    ElemValuePtr value = AllocValue();
    if (info == nullptr || value == nullptr) return std::nullopt;
    snd_ctl_elem_info_set_id(info.get(), id);
    if (snd_ctl_elem_info(ctl, info.get()) < 0) return std::nullopt;
    if (snd_ctl_elem_info_get_interface(info.get()) != SND_CTL_ELEM_IFACE_MIXER ||
        !snd_ctl_elem_info_is_readable(info.get()) || !snd_ctl_elem_info_is_writable(info.get()) ||
        snd_ctl_elem_info_is_inactive(info.get())) {
        return std::nullopt;
    }

    ControlInfo control;
    control.numid = snd_ctl_elem_info_get_numid(info.get());
    control.name = snd_ctl_elem_info_get_name(info.get());
    control.index = snd_ctl_elem_info_get_index(info.get());
    control.channels = snd_ctl_elem_info_get_count(info.get());
    switch (snd_ctl_elem_info_get_type(info.get())) {
        case SND_CTL_ELEM_TYPE_BOOLEAN:
            control.type = ControlInfo::Type::kBoolean;
            break;
        case SND_CTL_ELEM_TYPE_INTEGER:
            control.type = ControlInfo::Type::kInteger;
            control.min = snd_ctl_elem_info_get_min(info.get());
            control.max = snd_ctl_elem_info_get_max(info.get());
            control.step = snd_ctl_elem_info_get_step(info.get());
            break;
        case SND_CTL_ELEM_TYPE_ENUMERATED: {
            control.type = ControlInfo::Type::kEnumerated;
            const unsigned int items = snd_ctl_elem_info_get_items(info.get());
            for (unsigned int i = 0; i < items; ++i) {
                snd_ctl_elem_info_set_item(info.get(), i);
                if (snd_ctl_elem_info(ctl, info.get()) < 0) return std::nullopt;
                control.items.push_back(snd_ctl_elem_info_get_item_name(info.get()));
            }
            control.min = 0;
            control.max = items > 0 ? static_cast<long>(items) - 1 : 0;
            break;
        }
        default:
            return std::nullopt;
    }
    if (control.channels == 0) return std::nullopt;

    snd_ctl_elem_value_set_id(value.get(), id);
    if (const int err = snd_ctl_elem_read(ctl, value.get()); err < 0) {
        LOG(DEBUG) << "reading \"" << control.name << "\": " << ErrorString(err);
        return std::nullopt;
    }
    for (unsigned int ch = 0; ch < control.channels; ++ch) {
        switch (control.type) {
            case ControlInfo::Type::kBoolean:
                control.values.push_back(snd_ctl_elem_value_get_boolean(value.get(), ch));
                break;
            case ControlInfo::Type::kInteger:
                control.values.push_back(snd_ctl_elem_value_get_integer(value.get(), ch));
                break;
            case ControlInfo::Type::kEnumerated:
                control.values.push_back(snd_ctl_elem_value_get_enumerated(value.get(), ch));
                break;
        }
    }
    if (control.type == ControlInfo::Type::kInteger &&
        snd_ctl_elem_info_is_tlv_readable(info.get())) {
        long min_db = 0;
        long max_db = 0;
        if (snd_ctl_get_dB_range(ctl, id, &min_db, &max_db) == 0) {
            control.db_range = std::make_pair(min_db, max_db);
        }
    }
    return control;
}

}  // namespace

std::string ControlInfo::ToString() const {
    std::ostringstream os;
    os << "#" << numid << " \"" << name << "\"," << index << " values=";
    for (size_t i = 0; i < values.size(); ++i) os << (i == 0 ? "" : ",") << values[i];
    os << " range=" << min << ".." << max;
    return os.str();
}

std::vector<ControlInfo> ListControls(int card) {
    std::vector<ControlInfo> controls;
    CtlHandle ctl = OpenCtl(card);
    if (ctl == nullptr) return controls;
    snd_ctl_elem_list_t* raw_list = nullptr;
    if (snd_ctl_elem_list_malloc(&raw_list) < 0) return controls;
    ElemListPtr list(raw_list);
    if (snd_ctl_elem_list(ctl.get(), list.get()) < 0) return controls;
    const unsigned int count = snd_ctl_elem_list_get_count(list.get());
    if (snd_ctl_elem_list_alloc_space(list.get(), count) < 0) return controls;
    if (snd_ctl_elem_list(ctl.get(), list.get()) >= 0) {
        ElemIdPtr id = AllocId();
        const unsigned int used = snd_ctl_elem_list_get_used(list.get());
        for (unsigned int i = 0; id != nullptr && i < used; ++i) {
            snd_ctl_elem_list_get_id(list.get(), i, id.get());
            if (auto control = ReadControl(ctl.get(), id.get()); control.has_value()) {
                controls.push_back(std::move(*control));
            }
        }
    }
    snd_ctl_elem_list_free_space(list.get());
    return controls;
}

std::optional<ControlInfo> FindControl(int card, const std::string& name, unsigned int index) {
    for (ControlInfo& control : ListControls(card)) {
        if (control.name == name && control.index == index) return std::move(control);
    }
    return std::nullopt;
}

int WriteControl(int card, unsigned int numid, const std::vector<long>& values) {
    if (values.empty()) return -EINVAL;
    CtlHandle ctl = OpenCtl(card);
    if (ctl == nullptr) return -ENODEV;
    ElemIdPtr id = AllocId();
    ElemValuePtr value = AllocValue();
    if (id == nullptr || value == nullptr) return -ENOMEM;
    snd_ctl_elem_id_set_numid(id.get(), numid);
    const std::optional<ControlInfo> control = ReadControl(ctl.get(), id.get());
    if (!control.has_value()) return -EINVAL;
    if (values.size() != 1 && values.size() != control->channels) return -EINVAL;

    snd_ctl_elem_value_set_numid(value.get(), numid);
    for (unsigned int ch = 0; ch < control->channels; ++ch) {
        const long v =
                std::clamp(values.size() == 1 ? values[0] : values[ch], control->min, control->max);
        switch (control->type) {
            case ControlInfo::Type::kBoolean:
                snd_ctl_elem_value_set_boolean(value.get(), ch, v);
                break;
            case ControlInfo::Type::kInteger:
                snd_ctl_elem_value_set_integer(value.get(), ch, v);
                break;
            case ControlInfo::Type::kEnumerated:
                snd_ctl_elem_value_set_enumerated(value.get(), ch, static_cast<unsigned int>(v));
                break;
        }
    }
    if (const int err = snd_ctl_elem_write(ctl.get(), value.get()); err < 0) {
        LOG(WARNING) << "writing \"" << control->name << "\" on card " << card << ": "
                     << ErrorString(err);
        return err;
    }
    return 0;
}

}  // namespace aidl::android::hardware::audio::core::mainline::alsa
