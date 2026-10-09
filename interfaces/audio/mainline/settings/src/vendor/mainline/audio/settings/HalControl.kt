/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package vendor.mainline.audio.settings

import android.os.RemoteException
import android.os.ServiceManager
import android.util.Log
import kotlin.concurrent.thread
import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import kotlinx.coroutines.withTimeoutOrNull
import vendor.mainline.audio.IControl
import vendor.mainline.audio.PassthroughProbe
import vendor.mainline.audio.Status

/**
 * The HAL's vendor.mainline.audio.IControl/default. The HAL may restart at any time, so the binder
 * is looked up per call rather than cached, and every call runs on [Dispatchers.IO].
 */
object HalControl {
    private const val TAG = "MainlineAudioSettings"
    private val NAME = "${IControl.DESCRIPTOR}/default"

    /** How long a call waits for a HAL that is (re)starting. */
    private const val CONNECT_TIMEOUT_MS = 2_000L

    /** Whether the device declares the interface, i.e. runs the mainline audio HAL. */
    val isDeclared: Boolean
        get() = ServiceManager.isDeclared(NAME)

    /** Runs [block] on the service; null when it is not declared, does not come up, or fails. */
    suspend fun <T> call(block: (IControl) -> T): T? {
        val control = connect() ?: return null
        return withContext(Dispatchers.IO) {
            try {
                block(control)
            } catch (e: RemoteException) {
                Log.w(TAG, "IControl call failed", e)
                null
            } catch (e: SecurityException) {
                Log.w(TAG, "IControl call refused", e)
                null
            }
        }
    }

    suspend fun status(): Status? = call { it.status }

    /** What [probeHdmi] found. */
    sealed interface ProbeOutcome {
        data class Done(val results: List<PassthroughProbe>) : ProbeOutcome

        /** A stream holds the device; try again once nothing plays to it. */
        data object Busy : ProbeOutcome

        /** No HAL, or the head has no passthrough controls. */
        data object Unavailable : ProbeOutcome
    }

    /** Asks the HAL which IEC 61937 configurations the HDMI head [key] accepts. */
    suspend fun probeHdmi(key: String): ProbeOutcome =
        call {
            try {
                ProbeOutcome.Done(it.probeHdmi(key).toList())
            } catch (e: IllegalStateException) {
                ProbeOutcome.Busy
            } catch (e: IllegalArgumentException) {
                ProbeOutcome.Unavailable
            }
        } ?: ProbeOutcome.Unavailable

    /** Forgets the HDMI heads' high bit rate refusals; false when the HAL did not answer. */
    suspend fun resetHbr(): Boolean = call { it.resetHbr() } != null

    private suspend fun connect(): IControl? {
        if (!withContext(Dispatchers.IO) { isDeclared }) return null
        // waitForDeclaredService() blocks until the service is up and can not be interrupted, so
        // it runs on a thread of its own that a timeout can leave behind.
        val result = CompletableDeferred<IControl?>()
        thread(isDaemon = true, name = "IControl connect") {
            result.complete(IControl.Stub.asInterface(ServiceManager.waitForDeclaredService(NAME)))
        }
        return withTimeoutOrNull(CONNECT_TIMEOUT_MS) { result.await() }
            ?: run {
                Log.w(TAG, "$NAME did not come up within $CONNECT_TIMEOUT_MS ms")
                null
            }
    }
}
