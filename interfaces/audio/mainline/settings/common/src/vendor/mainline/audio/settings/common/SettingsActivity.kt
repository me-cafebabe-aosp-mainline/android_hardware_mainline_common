/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package vendor.mainline.audio.settings.common

import android.content.res.Configuration
import android.os.Bundle
import androidx.activity.enableEdgeToEdge
import androidx.appcompat.app.AppCompatActivity
import androidx.fragment.app.Fragment
import androidx.fragment.app.commit
import androidx.preference.Preference
import androidx.preference.PreferenceFragmentCompat
import com.google.android.material.appbar.MaterialToolbar

/**
 * Base of the activities Settings (or TvSettings) opens: a toolbar over the root page, sub-pages as
 * fragments on the back stack, opened through a preference's `fragment`.
 */
abstract class SettingsActivity :
    AppCompatActivity(), PreferenceFragmentCompat.OnPreferenceStartFragmentCallback {
    /** Title of the root page. */
    protected abstract val rootTitleRes: Int

    protected abstract fun createRootFragment(): Fragment

    override fun onCreate(savedInstanceState: Bundle?) {
        enableEdgeToEdge()
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_settings)
        setSupportActionBar(findViewById<MaterialToolbar>(R.id.toolbar))
        // A remote has a Back key, and a focusable Up button would take the first focus away from
        // the list.
        supportActionBar?.setDisplayHomeAsUpEnabled(!isTelevision())
        supportFragmentManager.addOnBackStackChangedListener(::updateTitle)
        if (savedInstanceState == null) {
            supportFragmentManager.commit { replace(R.id.content, createRootFragment()) }
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

    private fun isTelevision() =
        (resources.configuration.uiMode and Configuration.UI_MODE_TYPE_MASK) ==
            Configuration.UI_MODE_TYPE_TELEVISION

    private fun updateTitle() {
        val count = supportFragmentManager.backStackEntryCount
        val current = supportFragmentManager.findFragmentById(R.id.content)
        title =
            current?.arguments?.getCharSequence(ARG_TITLE).takeIf { count > 0 }
                ?: getString(rootTitleRes)
    }

    companion object {
        /** Title of a sub-page, from the preference that opened it. */
        const val ARG_TITLE = "title"
    }
}
