/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package vendor.mainline.audio.routing

import vendor.mainline.audio.settings.common.SettingsActivity

/** The page Settings › Sound › Audio › Default audio devices opens. */
class RoutingActivity : SettingsActivity() {
    override val rootTitleRes = R.string.routing_title

    override fun createRootFragment() = RoutingFragment()
}
