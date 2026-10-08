/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package vendor.mainline.audio.settings

import vendor.mainline.audio.settings.common.SettingsActivity

/** The page Settings › Sound › Audio › Audio hardware opens. */
class AudioSettingsActivity : SettingsActivity() {
    override val rootTitleRes = R.string.settings_title

    override fun createRootFragment() = AudioSettingsFragment()
}
