/**
 *    @file
 *          Provides the implementation of the Device Layer ConfigurationManager object
 *          for ACS platforms.
 */

#include <platform/internal/CHIPDeviceLayerInternal.h>

#include <platform/ConfigurationManager.h>
#include <platform/ACS/PosixConfig.h>
#include <platform/internal/GenericConfigurationManagerImpl.cpp>

namespace chip {
namespace DeviceLayer {

using namespace ::chip::DeviceLayer::Internal;

/** Singleton instance of the ConfigurationManager implementation object.
 */
ConfigurationManagerImpl ConfigurationManagerImpl::sInstance;

CHIP_ERROR ConfigurationManagerImpl::_Init()
{
    return CHIP_NO_ERROR;
}

CHIP_ERROR ConfigurationManagerImpl::_GetPrimaryWiFiMACAddress(uint8_t * buf)
{
    return CHIP_NO_ERROR;
}

bool ConfigurationManagerImpl::_CanFactoryReset()
{
    return true;
}

void ConfigurationManagerImpl::_InitiateFactoryReset()
{

}

CHIP_ERROR ConfigurationManagerImpl::_ReadPersistedStorageValue(::chip::Platform::PersistedStorage::Key key, uint32_t & value)
{
    return CHIP_NO_ERROR;
}

CHIP_ERROR ConfigurationManagerImpl::_WritePersistedStorageValue(::chip::Platform::PersistedStorage::Key key, uint32_t value)
{
    return CHIP_NO_ERROR;
}

} // namespace DeviceLayer
} // namespace chip
