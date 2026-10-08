/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package vendor.mainline.audio.settings.common

import android.content.Context
import androidx.preference.CheckBoxPreference

/**
 * One choice of a group. A click only reports the choice through the click listener; the owner
 * checks the right preference when it renders the state, so a click never unchecks it.
 */
class RadioPreference(context: Context) : CheckBoxPreference(context) {
    init {
        widgetLayoutResource = R.layout.preference_widget_radio
        isPersistent = false
    }

    override fun onClick() {}
}
