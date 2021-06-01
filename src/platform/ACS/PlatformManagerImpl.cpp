/**
 *    @file
 *          Provides an implementation of the PlatformManager object
 *          for Linux platforms.
 */

#include <platform/internal/CHIPDeviceLayerInternal.h>

#include <platform/PlatformManager.h>
#include <platform/internal/GenericPlatformManagerImpl_POSIX.cpp>

namespace chip {
namespace DeviceLayer {

PlatformManagerImpl PlatformManagerImpl::sInstance;


CHIP_ERROR PlatformManagerImpl::_InitChipStack()
{
    CHIP_ERROR err;

    err = Internal::GenericPlatformManagerImpl_POSIX<PlatformManagerImpl>::_InitChipStack();
    SuccessOrExit(err);

exit:
    return err;
}

} // namespace DeviceLayer
} // namespace chip
