/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package vendor.mainline.sensors.settings

import android.os.RemoteException
import android.os.ServiceManager
import android.util.Log
import kotlin.concurrent.thread
import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import kotlinx.coroutines.withTimeoutOrNull
import vendor.mainline.sensors.IVirtualSensors
import vendor.mainline.sensors.VirtualSensor

/**
 * The HAL's vendor.mainline.sensors.IVirtualSensors/default. The HAL may restart at any time, so
 * the binder is looked up per call rather than cached, and every call runs on [Dispatchers.IO].
 */
object HalControl {
    private const val TAG = "MainlineSensorsSettings"
    private val NAME = "${IVirtualSensors.DESCRIPTOR}/default"

    /** How long a call waits for a HAL that is (re)starting. */
    private const val CONNECT_TIMEOUT_MS = 2_000L

    /** Whether the device declares the interface, i.e. bundles the virtual backend. */
    val isDeclared: Boolean
        get() = ServiceManager.isDeclared(NAME)

    /** Runs [block] on the service; null when it is not declared, does not come up, or fails. */
    suspend fun <T> call(block: (IVirtualSensors) -> T): T? {
        val service = connect() ?: return null
        return withContext(Dispatchers.IO) {
            try {
                block(service)
            } catch (e: RemoteException) {
                Log.w(TAG, "IVirtualSensors call failed", e)
                null
            } catch (e: SecurityException) {
                Log.w(TAG, "IVirtualSensors call refused", e)
                null
            } catch (e: IllegalArgumentException) {
                Log.w(TAG, "IVirtualSensors call rejected", e)
                null
            } catch (e: IllegalStateException) {
                Log.w(TAG, "IVirtualSensors not ready", e)
                null
            }
        }
    }

    suspend fun sensors(): List<VirtualSensor>? = call { it.sensors.toList() }

    private suspend fun connect(): IVirtualSensors? {
        if (!withContext(Dispatchers.IO) { isDeclared }) return null
        // waitForDeclaredService() blocks until the service is up and can not be interrupted, so
        // it runs on a thread of its own that a timeout can leave behind.
        val result = CompletableDeferred<IVirtualSensors?>()
        thread(isDaemon = true, name = "IVirtualSensors connect") {
            result.complete(
                IVirtualSensors.Stub.asInterface(ServiceManager.waitForDeclaredService(NAME))
            )
        }
        return withTimeoutOrNull(CONNECT_TIMEOUT_MS) { result.await() }
            ?: run {
                Log.w(TAG, "$NAME did not come up within $CONNECT_TIMEOUT_MS ms")
                null
            }
    }
}
