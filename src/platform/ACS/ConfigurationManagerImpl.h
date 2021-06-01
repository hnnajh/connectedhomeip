/**
 *    @file
 *          Provides an implementation of the ConfigurationManager object
 *          for Linux platforms.
 */

#pragma once

#include <platform/internal/GenericConfigurationManagerImpl.h>

#include <platform/ACS/PosixConfig.h>

namespace chip {
namespace DeviceLayer {

/**
 * Concrete implementation of the ConfigurationManager singleton object for the ACS platform.
 */
class ConfigurationManagerImpl final : public ConfigurationManager,
                                       public Internal::GenericConfigurationManagerImpl<ConfigurationManagerImpl>,
                                       private Internal::PosixConfig
{
    // Allow the ConfigurationManager interface class to delegate method calls to
    // the implementation methods provided by this class.
    friend class ConfigurationManager;

    // Allow the GenericConfigurationManagerImpl base class to access helper methods and types
    // defined on this class.
#ifndef DOXYGEN_SHOULD_SKIP_THIS
    friend class Internal::GenericConfigurationManagerImpl<ConfigurationManagerImpl>;
#endif

private:
    // ===== Members that implement the ConfigurationManager public interface.

    CHIP_ERROR _Init();
    CHIP_ERROR _GetPrimaryWiFiMACAddress(uint8_t * buf);
    bool _CanFactoryReset();
    void _InitiateFactoryReset();
    CHIP_ERROR _ReadPersistedStorageValue(::chip::Platform::PersistedStorage::Key key, uint32_t & value);
    CHIP_ERROR _WritePersistedStorageValue(::chip::Platform::PersistedStorage::Key key, uint32_t value);

    // NOTE: Other public interface methods are implemented by GenericConfigurationManagerImpl<>.

    // ===== Members for internal use by the following friends.

    friend ConfigurationManager & ConfigurationMgr();
    friend ConfigurationManagerImpl & ConfigurationMgrImpl();

    static ConfigurationManagerImpl sInstance;

    // ===== Private members reserved for use by this class only.
};

/**
 * Returns the public interface of the ConfigurationManager singleton object.
 *
 * chip applications should use this to access features of the ConfigurationManager object
 * that are common to all platforms.
 */
inline ConfigurationManager & ConfigurationMgr()
{
    return ConfigurationManagerImpl::sInstance;
}

/**
 * Returns the platform-specific implementation of the ConfigurationManager singleton object.
 *
 * chip applications can use this to gain access to features of the ConfigurationManager
 * that are specific to the ESP32 platform.
 */
inline ConfigurationManagerImpl & ConfigurationMgrImpl()
{
    return ConfigurationManagerImpl::sInstance;
}

} // namespace DeviceLayer
} // namespace chip
