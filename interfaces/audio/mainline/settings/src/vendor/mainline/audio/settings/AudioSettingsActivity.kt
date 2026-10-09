/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package vendor.mainline.audio.settings

import vendor.mainline.audio.settings.common.SettingsActivity

/**
 * The page Settings › Sound › Audio › Audio hardware opens, or a single hardware page when the
 * intent names one ([HardwarePage.EXTRA_PAGE]).
 */
class AudioSettingsActivity : SettingsActivity() {
    private val page by lazy { HardwarePage.fromIntent(intent) }

    override val rootTitleRes
        get() = page?.titleRes ?: R.string.settings_title

    override fun createRootFragment() =
        page?.let {
            supportFragmentManager.fragmentFactory.instantiate(classLoader, it.fragment.name)
        } ?: AudioSettingsFragment()
}
