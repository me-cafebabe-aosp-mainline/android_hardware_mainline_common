/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package vendor.mainline.audio.settings

import android.os.Bundle
import android.view.View
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.lifecycleScope
import androidx.lifecycle.repeatOnLifecycle
import androidx.preference.Preference
import kotlinx.coroutines.launch
import vendor.mainline.audio.settings.common.SettingsFragment

/** The outputs of the HAL, each leading to its speaker setup. */
class SpeakersFragment : SettingsFragment() {
    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        preferenceScreen = preferenceManager.createPreferenceScreen(requireContext())
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        viewLifecycleOwner.lifecycleScope.launch {
            viewLifecycleOwner.repeatOnLifecycle(Lifecycle.State.STARTED) { render() }
        }
    }

    private suspend fun render() {
        val status = HalControl.status()
        val context = requireContext()
        val screen = preferenceManager.createPreferenceScreen(context)
        if (status == null) {
            screen.addFooter(KEY_FOOTER, R.string.hal_unavailable)
            preferenceScreen = screen
            return
        }
        // Endpoints on one path share their processing.
        val outputs = status.endpoints.filter { !it.input && !it.nullDevice }.distinctBy { it.key }
        for (endpoint in outputs) {
            screen.addPreference(
                Preference(context).apply {
                    key = "output/${endpoint.key}"
                    title = endpoint.name
                    summary =
                        context.resources.getQuantityString(
                            R.plurals.speakers_channels,
                            endpoint.maxChannels,
                            endpoint.maxChannels,
                        )
                    isIconSpaceReserved = false
                    fragment = OutputFragment::class.java.name
                    extras.putString(OutputFragment.ARG_KEY, endpoint.key)
                }
            )
        }
        screen.addFooter(KEY_FOOTER, R.string.speakers_footer)
        preferenceScreen = screen
    }

    private companion object {
        const val KEY_FOOTER = "footer"
    }
}
