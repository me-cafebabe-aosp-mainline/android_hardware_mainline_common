/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package vendor.mainline.audio.settings

import android.os.Bundle
import androidx.activity.enableEdgeToEdge
import androidx.appcompat.app.AppCompatActivity
import androidx.fragment.app.commit
import androidx.preference.Preference
import androidx.preference.PreferenceFragmentCompat
import com.google.android.material.appbar.MaterialToolbar

/**
 * The page Settings › Sound › Audio devices opens. Sub-pages are fragments on the back stack,
 * opened through a preference's `fragment`.
 */
class AudioSettingsActivity :
    AppCompatActivity(), PreferenceFragmentCompat.OnPreferenceStartFragmentCallback {
    override fun onCreate(savedInstanceState: Bundle?) {
        enableEdgeToEdge()
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_settings)
        setSupportActionBar(findViewById<MaterialToolbar>(R.id.toolbar))
        supportActionBar?.setDisplayHomeAsUpEnabled(true)
        supportFragmentManager.addOnBackStackChangedListener(::updateTitle)
        if (savedInstanceState == null) {
            supportFragmentManager.commit { replace(R.id.content, AudioSettingsFragment()) }
        }
        updateTitle()
    }

    override fun onSupportNavigateUp(): Boolean {
        onBackPressedDispatcher.onBackPressed()
        return true
    }

    override fun onPreferenceStartFragment(
        caller: PreferenceFragmentCompat,
        pref: Preference,
    ): Boolean {
        val className = pref.fragment ?: return false
        val fragment =
            supportFragmentManager.fragmentFactory.instantiate(classLoader, className).apply {
                arguments = Bundle(pref.extras).apply { putCharSequence(ARG_TITLE, pref.title) }
            }
        supportFragmentManager.commit {
            replace(R.id.content, fragment)
            addToBackStack(null)
        }
        return true
    }

    private fun updateTitle() {
        val count = supportFragmentManager.backStackEntryCount
        val current = supportFragmentManager.findFragmentById(R.id.content)
        title =
            current?.arguments?.getCharSequence(ARG_TITLE).takeIf { count > 0 }
                ?: getString(R.string.settings_title)
    }

    companion object {
        /** Title of a sub-page, from the preference that opened it. */
        const val ARG_TITLE = "title"
    }
}
