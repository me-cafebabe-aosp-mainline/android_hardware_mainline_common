/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package vendor.mainline.audio.settings.common

import android.os.Bundle
import android.view.View
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.updatePadding
import androidx.preference.Preference
import androidx.preference.PreferenceFragmentCompat
import androidx.preference.PreferenceGroup

/** Base of every page: keeps the list clear of the navigation bar and display cutouts. */
abstract class SettingsFragment : PreferenceFragmentCompat() {
    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        val list = listView
        list.clipToPadding = false
        ViewCompat.setOnApplyWindowInsetsListener(list) { v, insets ->
            val bars =
                insets.getInsets(
                    WindowInsetsCompat.Type.systemBars() or WindowInsetsCompat.Type.displayCutout()
                )
            v.updatePadding(left = bars.left, right = bars.right, bottom = bars.bottom)
            insets
        }
    }

    /** Explanatory text at the end of a page. */
    protected fun PreferenceGroup.addFooter(key: String, summaryRes: Int) {
        addPreference(
            Preference(context).apply {
                this.key = key
                setSummary(summaryRes)
                isSelectable = false
                isIconSpaceReserved = false
            }
        )
    }
}
