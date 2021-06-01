/**
 *    @file
 *          Provides implementations of the CHIP System Layer platform
 *          time/clock functions that are suitable for use on the ACS platform.
 */

#include <platform/internal/CHIPDeviceLayerInternal.h>

#include <ace/osal_time.h>
#include <ace/ace_status.h>

namespace chip {
namespace System {
namespace Platform {
namespace Layer {

uint64_t GetClock_Monotonic()
{
    aceTime_timespec_t ts;

    aceTime_clockTime(ACETIME_CLOCK_MONOTONIC, &ts);
    return (static_cast<uint64_t>(ts.tv_sec) * UINT64_C(1000000)) + ((static_cast<uint64_t>(ts.tv_nsec)) / 1000);
}

uint64_t GetClock_MonotonicMS()
{
    aceTime_timespec_t ts;

    aceTime_clockTime(ACETIME_CLOCK_MONOTONIC, &ts);
    return (static_cast<uint64_t>(ts.tv_sec) * UINT64_C(1000)) + ((static_cast<uint64_t>(ts.tv_nsec)) / 1000000);
}

uint64_t GetClock_MonotonicHiRes()
{
    aceTime_timespec_t ts;

    aceTime_clockTime(ACETIME_CLOCK_MONOTONIC, &ts);
    return (static_cast<uint64_t>(ts.tv_sec) * UINT64_C(1000000)) + ((static_cast<uint64_t>(ts.tv_nsec)) / 1000);
}

System::Error GetClock_RealTime(uint64_t & curTime)
{
    aceTime_timespec_t ts;

    aceTime_clockTime(ACETIME_CLOCK_REALTIME, &ts);

    if (ts.tv_sec < CHIP_SYSTEM_CONFIG_VALID_REAL_TIME_THRESHOLD)
    {
        return CHIP_SYSTEM_ERROR_REAL_TIME_NOT_SYNCED;
    }
    if (ts.tv_nsec < 0)
    {
        return CHIP_SYSTEM_ERROR_REAL_TIME_NOT_SYNCED;
    }

    curTime = (static_cast<uint64_t>(ts.tv_sec) * UINT64_C(1000000)) + ((static_cast<uint64_t>(ts.tv_nsec)) / 1000);
    return CHIP_SYSTEM_NO_ERROR;
}

System::Error GetClock_RealTimeMS(uint64_t & curTime)
{
    aceTime_timespec_t ts;

    aceTime_clockTime(ACETIME_CLOCK_REALTIME, &ts);

    if (ts.tv_sec < CHIP_SYSTEM_CONFIG_VALID_REAL_TIME_THRESHOLD)
    {
        return CHIP_SYSTEM_ERROR_REAL_TIME_NOT_SYNCED;
    }
    if (ts.tv_nsec < 0)
    {
        return CHIP_SYSTEM_ERROR_REAL_TIME_NOT_SYNCED;
    }

    curTime = (static_cast<uint64_t>(ts.tv_sec) * UINT64_C(1000)) + ((static_cast<uint64_t>(ts.tv_nsec)) / 1000000);
    return CHIP_SYSTEM_NO_ERROR;
}

System::Error SetClock_RealTime(uint64_t newCurTime)
{
    aceTime_timespec_t ts;
    ace_status_t status;

    ts.tv_sec  = static_cast<time_t>(newCurTime / UINT64_C(1000000));
    ts.tv_nsec = static_cast<long>(newCurTime % UINT64_C(1000000000));

    status = aceTime_setClockTime(&ts);
    if (status == ACE_STATUS_OK) {
        return CHIP_SYSTEM_NO_ERROR;
    } else if (status == ACE_STATUS_NO_PERMISSION) {
        return CHIP_SYSTEM_ERROR_ACCESS_DENIED;
    } else {
        return CHIP_SYSTEM_ERROR_NOT_SUPPORTED;
    }
}

} // namespace Layer
} // namespace Platform
} // namespace System
} // namespace chip
