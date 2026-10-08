/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package vendor.mainline.audio.settings

import android.os.Bundle
import androidx.preference.Preference
import androidx.preference.PreferenceCategory
import androidx.preference.PreferenceScreen
import vendor.mainline.audio.settings.common.SettingsFragment

/** The entry page: the hardware pages of the mainline audio HAL. */
class AudioSettingsFragment : SettingsFragment() {
    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        val screen = preferenceManager.createPreferenceScreen(requireContext())
        screen.addHardware()
        preferenceScreen = screen
    }

    /** The pages, each backed by IControl. */
    private fun PreferenceScreen.addHardware() {
        val category =
            PreferenceCategory(context).apply {
                key = KEY_HARDWARE
                setTitle(R.string.hardware_title)
            }
        addPreference(category)
        category.addPreference(
            Preference(context).apply {
                key = "$KEY_HARDWARE/diagnostics"
                setTitle(R.string.diagnostics_title)
                setSummary(R.string.diagnostics_summary)
                fragment = DiagnosticsFragment::class.java.name
            }
        )
        category.addPreference(
            Preference(context).apply {
                key = "$KEY_HARDWARE/speakers"
                setTitle(R.string.speakers_title)
                setSummary(R.string.speakers_summary)
                fragment = SpeakersFragment::class.java.name
            }
        )
        category.addPreference(
            Preference(context).apply {
                key = "$KEY_HARDWARE/mixer"
                setTitle(R.string.mixer_title)
                setSummary(R.string.mixer_summary)
                fragment = MixerFragment::class.java.name
            }
        )
        category.addPreference(
            Preference(context).apply {
                key = "$KEY_HARDWARE/settings"
                setTitle(R.string.settings_page_title)
                setSummary(R.string.settings_page_summary)
                fragment = HalSettingsFragment::class.java.name
            }
        )
    }

    private companion object {
        const val KEY_HARDWARE = "hardware"
    }
}
