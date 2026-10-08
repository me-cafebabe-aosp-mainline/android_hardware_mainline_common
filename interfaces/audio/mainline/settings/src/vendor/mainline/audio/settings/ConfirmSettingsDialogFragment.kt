/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package vendor.mainline.audio.settings

import android.app.Dialog
import android.os.Bundle
import androidx.appcompat.app.AlertDialog
import androidx.fragment.app.DialogFragment
import androidx.fragment.app.viewModels
import androidx.lifecycle.lifecycleScope
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import kotlinx.coroutines.launch

/**
 * Asks to keep the settings the HAL restarted with. Not cancelable: only Keep, Revert and the HAL's
 * own timeout end the trial.
 */
class ConfirmSettingsDialogFragment : DialogFragment() {
    private val viewModel: HalSettingsViewModel by
        viewModels(ownerProducer = { requireParentFragment() })

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        isCancelable = false
        val dialog =
            MaterialAlertDialogBuilder(requireContext())
                .setTitle(R.string.confirm_title)
                .setMessage(message(SECONDS_UNKNOWN))
                .setPositiveButton(R.string.confirm_keep) { _, _ -> viewModel.keep() }
                .setNegativeButton(R.string.confirm_revert) { _, _ -> viewModel.revert() }
                .create()
        lifecycleScope.launch {
            viewModel.ui.collect { ui ->
                val trial = (ui as? HalSettingsViewModel.Ui.Loaded)?.state?.trial ?: return@collect
                (this@ConfirmSettingsDialogFragment.dialog as? AlertDialog)?.setMessage(
                    message(trial.secondsLeft)
                )
            }
        }
        return dialog
    }

    private fun message(secondsLeft: Int) =
        if (secondsLeft == SECONDS_UNKNOWN) {
            getString(R.string.confirm_message_plain)
        } else {
            getString(R.string.confirm_message, secondsLeft)
        }

    companion object {
        const val TAG = "confirm_settings"
        private const val SECONDS_UNKNOWN = -1
    }
}
