/**
 *    @file
 *          Platform-specific implementatiuon of KVS for ACS.
 */

#include <platform/KeyValueStoreManager.h>

namespace chip {
namespace DeviceLayer {
namespace PersistedStorage {

KeyValueStoreManagerImpl KeyValueStoreManagerImpl::sInstance;

CHIP_ERROR KeyValueStoreManagerImpl::_Get(const char * key, void * value, size_t value_size, size_t * read_bytes_size,
                                          size_t offset_bytes)
{
    return CHIP_NO_ERROR;
}

CHIP_ERROR KeyValueStoreManagerImpl::_Put(const char * key, const void * value, size_t value_size)
{
    return CHIP_NO_ERROR;
}

CHIP_ERROR KeyValueStoreManagerImpl::_Delete(const char * key)
{
    return CHIP_NO_ERROR;
}

} // namespace PersistedStorage
} // namespace DeviceLayer
} // namespace chip
