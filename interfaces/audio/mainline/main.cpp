/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "MainlineAudio_Main"

#include <cstdlib>
#include <ctime>
#include <string>
#include <vector>

#include <Log.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>
#include <core-impl/ChildInterface.h>
#include <core-impl/Module.h>

#include "MainlineConfig.h"
#include "ModuleMainline.h"
#include "Properties.h"
#include "alsa/AlsaError.h"
#include "control/ControlService.h"

using aidl::android::hardware::audio::core::ChildInterface;
using aidl::android::hardware::audio::core::Module;
using aidl::android::hardware::audio::core::mainline::MainlineConfig;
using aidl::android::hardware::audio::core::mainline::ModuleMainline;
using aidl::android::hardware::audio::core::mainline::Properties;
using aidl::android::hardware::audio::core::mainline::control::ControlService;
using aidl::android::hardware::audio::core::mainline::control::MixerSettings;
using aidl::android::hardware::audio::core::mainline::control::OutputSettingsStore;
using aidl::android::hardware::audio::core::mainline::control::SettingsStore;

namespace {

constexpr int kBinderThreadPoolSize = 16;

// Registers `module` as android.hardware.audio.core.IModule/<instance>.
bool RegisterModule(const std::shared_ptr<Module>& module, const std::string& instance,
                    std::vector<ChildInterface<Module>>* keep_alive) {
    if (module == nullptr) {
        LOG(ERROR) << "no module instance for " << instance;
        return false;
    }
    ChildInterface<Module> child;
    child = module;
    const std::string fqn = std::string(Module::descriptor) + "/" + instance;
    if (const binder_status_t status = AServiceManager_addService(child.getBinder(), fqn.c_str());
        status != STATUS_OK) {
        LOG(ERROR) << "failed to register " << fqn << ": " << status;
        return false;
    }
    LOG(INFO) << "registered " << fqn;
    keep_alive->push_back(std::move(child));
    return true;
}

// Instantiates one of the example HAL's software modules (r_submix,
// bluetooth). These have no hardware dependency.
void RegisterExampleModule(Module::Type type, const std::string& instance,
                           std::vector<ChildInterface<Module>>* keep_alive) {
    // Passing no configuration at all, rather than an empty one: Module only
    // falls back to the built-in configuration of its type when it is null.
    // The single argument createInstance() hands out an empty one instead,
    // which leaves the module without a single port.
    std::shared_ptr<Module> module = Module::createInstance(type, nullptr);
    if (module == nullptr) {
        LOG(ERROR) << "failed to create the " << instance << " module";
        return;
    }
    RegisterModule(module, instance, keep_alive);
}

}  // namespace

int main() {
    // The example HAL's stream implementation uses std::rand() for the
    // internal command cookie.
    std::srand(static_cast<unsigned int>(std::time(nullptr)));

    // Settings stored through IControl override the properties; this also
    // decides whether new ones run on trial.
    auto settings = std::make_shared<SettingsStore>();
    const Properties properties = Properties::Load(settings->active());
    ::android::base::SetMinimumLogSeverity(properties.verbose_logging ? ::android::base::VERBOSE
                                                                      : ::android::base::DEBUG);
    LOG(INFO) << "Mainline audio HAL starting";
    LOG(INFO) << "properties: " << properties.ToString();

    ABinderProcess_setThreadPoolMaxThreadCount(kBinderThreadPoolSize);
    ABinderProcess_startThreadPool();

    aidl::android::hardware::audio::core::mainline::alsa::InstallAlsaErrorHandler();

    // IConfig/default
    auto config = ndk::SharedRefBase::make<MainlineConfig>();
    const std::string config_fqn = std::string(MainlineConfig::descriptor) + "/default";
    if (const binder_status_t status =
                AServiceManager_addService(config->asBinder().get(), config_fqn.c_str());
        status != STATUS_OK) {
        LOG(ERROR) << "failed to register " << config_fqn << ": " << status;
    } else {
        LOG(INFO) << "registered " << config_fqn;
    }

    // IModule/default: the alsa-lib backed module. Discovery happens here, so
    // the sound cards must already exist when the HAL starts.
    std::vector<ChildInterface<Module>> modules;
    const std::shared_ptr<ModuleMainline> module = ModuleMainline::Create(properties);
    // Mixer values the user set in the settings app: once now, after the UCM
    // boot sequences ran, and again whenever UCM enables a device.
    auto mixer = std::make_shared<MixerSettings>(module->inventory().cards());
    mixer->ApplyAll();
    module->SetDeviceEnabledListener([mixer](int card) { mixer->Apply(card); });
    // Output processing stored through IControl, in force before any stream opens.
    auto outputs = std::make_shared<OutputSettingsStore>(module->output_configs());
    RegisterModule(module, "default", &modules);

    // vendor.mainline.audio.IControl/default: the settings app's view of the
    // module. The HAL works without it, so a failure is only logged.
    auto control = ndk::SharedRefBase::make<ControlService>(module, mixer, settings, outputs);
    const std::string control_fqn = std::string(ControlService::descriptor) + "/default";
    if (const binder_status_t status =
                AServiceManager_addService(control->asBinder().get(), control_fqn.c_str());
        status != STATUS_OK) {
        LOG(ERROR) << "failed to register " << control_fqn << ": " << status;
    } else {
        LOG(INFO) << "registered " << control_fqn;
    }

    // Software modules reused from the example HAL.
    RegisterExampleModule(Module::Type::R_SUBMIX, "r_submix", &modules);
#ifdef MAINLINE_AUDIO_WITH_BLUETOOTH
    RegisterExampleModule(Module::Type::BLUETOOTH, "bluetooth", &modules);
#endif

    LOG(INFO) << "Mainline audio HAL ready with " << modules.size() << " module(s)";
    ABinderProcess_joinThreadPool();
    return EXIT_FAILURE;  // Not reached.
}
