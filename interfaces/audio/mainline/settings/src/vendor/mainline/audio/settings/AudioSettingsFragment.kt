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
        for (page in HardwarePage.entries) {
            category.addPreference(
                Preference(context).apply {
                    key = "$KEY_HARDWARE/${page.key}"
                    setTitle(page.titleRes)
                    setSummary(page.summaryRes)
                    fragment = page.fragment.name
                }
            )
        }
    }

    private companion object {
        const val KEY_HARDWARE = "hardware"
    }
}
