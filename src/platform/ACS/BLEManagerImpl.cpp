/**
 *    @file
 *          Provides an implementation of the BLEManager singleton object
 *          for ACS platforms.
 */
#include <ble/CHIPBleServiceData.h>
#include <platform/internal/CHIPDeviceLayerInternal.h>
#include <platform/internal/BLEManager.h>

#if CHIP_DEVICE_CONFIG_ENABLE_CHIPOBLE

#include <ace/ace_status.h>
#include <ace/aceBT_ble_api.h>
#include <ace/aceBT_defines.h>
#include <ace/aceBT_session_api.h>
#include <ace/ace_modules.h>
#include <ace/osal_alloc.h>
#include <ace/osal_semaphore.h>
#include <ace/bluetooth_beacon_api.h>
#include <ace/bluetooth_ble_api.h>
#include <ace/aceBT_ble_gatt_client_api.h>

using namespace ::chip::Ble;

namespace chip {
namespace DeviceLayer {
namespace Internal {

namespace {
const ChipBleUUID ChipUUID_CHIPoBLEChar_RX = {{0x18, 0xEE, 0x2E, 0xF5,
                                               0x26, 0x3D, 0x45, 0x59,
                                               0x95, 0x9F, 0x4F, 0x9C,
                                               0x42, 0x9F, 0x9D, 0x11}};
const ChipBleUUID ChipUUID_CHIPoBLEChar_TX = {{0x18, 0xEE, 0x2E, 0xF5,
                                               0x26, 0x3D, 0x45, 0x59,
                                               0x95, 0x9F, 0x4F, 0x9C,
                                               0x42, 0x9F, 0x9D, 0x12 }};
} // namespace

#define SYNC_CALL_WAIT_TIME_MS 5000
#define BLE_DISCRIMINATOR_BYTE 8

BLEManagerImpl BLEManagerImpl::sInstance;

/**
 * @brief bluetooth session struct
 */
typedef struct bt_ctx {
    aceBT_sessionHandle session_hdl;
    aceBT_scanInstanceHandle scan_hdl;
} bt_ctx_t;

typedef struct ble_gattc_conn {
    aceBT_bleConnHandle connHandle;
    aceBT_bleGattsService_t* gatt_db;
    uint32_t no_svc;
    bool subscribed;
} ble_gattc_conn_t;

static aceSemaphore_t sync_call_sem;
static bt_ctx_t* session = NULL;
static ble_gattc_conn_t* gattc_conn = NULL;
static aceBT_bdAddr_t address;
static uint16_t bleDiscriminator;

static void onBeaconScanResults(aceBT_scanInstanceHandle scanInstance,
                                aceBT_BeaconScanRecord_t *record) {
    (void) scanInstance;
    ace_status_t status;
    memset(&address, 0, sizeof(aceBT_bdAddr_t));

    if (!record || !session) {
        ChipLogProgress(Ble, "%s: Invalid record/session",  __func__);
        return;
    }
    ChipLogProgress(Ble, "%s: ble CHIP device found: "
        "%02x:%02x:%02x:%02x:%02x:%02x",
        __func__,
        record->addr.address[0], record->addr.address[1],
        record->addr.address[2], record->addr.address[3],
        record->addr.address[4], record->addr.address[5]);
    memcpy(&address, &record->addr, sizeof(aceBT_bdAddr_t));

    if (bleDiscriminator !=
            (((uint16_t)record->scanRecords.data[BLE_DISCRIMINATOR_BYTE+1] << 8) |
                record->scanRecords.data[BLE_DISCRIMINATOR_BYTE])) {
        return;
    }
    ChipLogProgress(Ble, "%s: Discriminator match found. Continuing to pair..", __func__);
    status = aceSem_post(&sync_call_sem);
    if (status != ACE_STATUS_OK) {
       ChipLogError(Ble, "%s: Failed to post semaphore", __func__);
    }
}

static void onBeaconClientRegistered(ace_status_t status) {
    ChipLogProgress(Ble, "%s", __func__);
    status = aceSem_post(&sync_call_sem);
    if (status != ACE_STATUS_OK) {
       ChipLogError(Ble, "%s: Failed to post semaphore", __func__);
    }
}

static aceBT_beaconCallbacks_t beacon_client_callback = {
    sizeof(aceBT_beaconCallbacks_t),
    NULL,   // on_beaconAdvStateChanged
    NULL,   // onBeaconScanStateChanged,
    onBeaconScanResults,
    onBeaconClientRegistered
};

static void onBleRegistered(ace_status_t status)
{
    ChipLogProgress(Ble, "%s", __func__);
    status = aceSem_post(&sync_call_sem);
    if (status != ACE_STATUS_OK) {
       ChipLogError(Ble, "%s: Failed to post semaphore", __func__);
    }
}

static void onBleConnStateChanged(aceBT_bleConnState_t state,
                                  aceBT_gattStatus_t status,
                                  const aceBT_bleConnHandle connHandle,
                                  aceBT_bdAddr_t* addr) {
    if (state == ACEBT_BLE_STATE_CONNECTED) {
        ChipLogProgress(Ble, "%s: Device connected: "
            "%02x:%02x:%02x:%02x:%02x:%02x",
            __func__,
            addr->address[0], addr->address[1],
            addr->address[2], addr->address[3],
            addr->address[4], addr->address[5]);
        BLEManagerImpl::HandleNewConnection(connHandle);
    } else if (state == ACEBT_BLE_STATE_DISCONNECTED) {
        ChipLogProgress(Ble, "%s: Device disconnected: "
        "%02x:%02x:%02x:%02x:%02x:%02x",
        __func__,
        addr->address[0], addr->address[1],
        addr->address[2], addr->address[3],
        addr->address[4], addr->address[5]);
        BLEManagerImpl::HandleConnectionClosed(connHandle);
    }
}

static aceBT_bleCallbacks_t  ble_client_callback =  {
    sizeof(aceBT_bleCallbacks_t),
    {
        sizeof(aceBT_commonCallbacks_t),
        NULL, // On ble adapter state changed
        NULL, // Bond state callback
    },
    onBleRegistered,
    onBleConnStateChanged,
    NULL, // On ble mtu updated
};

static void onGattcServiceRegistered(ace_status_t status)
{
    ChipLogProgress(Ble, "%s", __func__);
}

static void onGattcServiceDiscovered(aceBT_bleConnHandle connHandle,
                            ace_status_t status) {
    ChipLogProgress(Ble, "%s", __func__);
    gattc_conn = new ble_gattc_conn_t;
    if (!gattc_conn) {
        ChipLogError(Ble, "%s: Failed to allocate memory", __func__);
        return;
    }
    gattc_conn->subscribed = false;
    gattc_conn->connHandle = connHandle;

    status = aceSem_post(&sync_call_sem);
    if (status != ACE_STATUS_OK) {
       ChipLogError(Ble, "%s: Failed to post semaphore", __func__);
    }
}

static void onGattcWriteCharacteristics(
        aceBT_bleConnHandle connHandle,
        aceBT_bleGattCharacteristicsValue_t charsValue,
        ace_status_t status) {
    BLEManagerImpl::HandleWriteComplete(connHandle);
}

static void onGattcWriteDescriptor(
        aceBT_bleConnHandle connHandle,
        aceBT_bleGattCharacteristicsValue_t gattCharacteristics,
        aceBT_status_t status) {
    if (!gattc_conn) {
        return;
    }
    if (!gattc_conn->subscribed) {
        gattc_conn->subscribed = true;
        BLEManagerImpl::HandleSubscribeOpComplete(connHandle, true);
    } else {
        gattc_conn->subscribed = false;
        BLEManagerImpl::HandleSubscribeOpComplete(connHandle, false);
    }
}

static void onGattcNotifyCharacteristics(
        aceBT_bleConnHandle connHandle,
        aceBT_bleGattCharacteristicsValue_t charsValue) {
    uint16_t len = charsValue.blobValue.size / sizeof(uint8_t);
    void *data = charsValue.blobValue.data;
    BLEManagerImpl::HandleTXCharChanged(connHandle,
        static_cast<const uint8_t *>(data), len);
}

static void onGattcGetDb(aceBT_bleConnHandle connHandle,
            aceBT_bleGattsService_t* gatt_service,
            uint32_t no_svc) {
    ace_status_t status;
    ChipLogProgress(Ble, "%s", __func__);
    if (!gattc_conn) {
        return;
    }

    if (gattc_conn->connHandle == connHandle) {
        aceBT_bleCloneGattService(&gattc_conn->gatt_db, gatt_service, (int)no_svc);
        gattc_conn->no_svc = no_svc;
    }

    status = aceSem_post(&sync_call_sem);
    if (status != ACE_STATUS_OK) {
       ChipLogError(Ble, "%s: Failed to post semaphore", __func__);
    }
}

static aceBT_bleGattClientCallbacks_t gatt_client_callback = {
    sizeof(aceBT_bleGattClientCallbacks_t),
    onGattcServiceRegistered,
    onGattcServiceDiscovered,
    NULL, // onGattcReadCharacteristics
    onGattcWriteCharacteristics,
    onGattcNotifyCharacteristics,
    onGattcWriteDescriptor,
    NULL, // onGattcReadDescriptor
    onGattcGetDb,
    NULL, // onGattcExecuteWrite
};

// Helper Functions

static ace_status_t btSessionCreate(void) {
    if (!session) {
        ChipLogError(Ble, "%s: Invalid session reference", __func__);
        return ACE_STATUS_GENERAL_ERROR;
    }
    ace_status_t status = aceBT_openSession(ACEBT_SESSION_TYPE_BLE, NULL,
                                            &session->session_hdl);
    if (status != ACE_STATUS_OK) {
        ChipLogError(Ble, "%s: Unable to open BLE session, err = %u",
        __func__, status);
        return ACE_STATUS_NOT_SUPPORTED;
    }
    status = aceBT_bleRegister(session->session_hdl, &ble_client_callback);
    if (status != ACE_STATUS_OK) {
        ChipLogError(Ble, "%s: Unable to open BLE session, err = %u",
        __func__, status);
        return ACE_STATUS_NOT_SUPPORTED;
    }
    // Wait max of 5s to get registered callback
    ChipLogProgress(Ble, "%s: Waiting 5s for onBleRegistered cb", __func__);
    status = aceSem_waitTimeout(&sync_call_sem, SYNC_CALL_WAIT_TIME_MS);
    if (ACE_STATUS_OK != status) {
        ChipLogError(Ble, "CHIP: %s: Failed to get onBleRegistered cb, err = %d",
        __func__, status);
        return ACE_STATUS_GENERAL_ERROR;
    }

    status = aceBT_RegisterBeaconClient(session->session_hdl, &beacon_client_callback);
    if (status != ACE_STATUS_OK) {
        ChipLogError(Ble, "%s: Unable to register ble beacon client, err = %u",
        __func__, status);
        return ACE_STATUS_NOT_SUPPORTED;
    }
    // Wait max of 5s to get registered callback
    ChipLogProgress(Ble, "%s: Waiting 5s for onBeaconClientRegistered cb", __func__);
    status = aceSem_waitTimeout(&sync_call_sem, SYNC_CALL_WAIT_TIME_MS);
    if (ACE_STATUS_OK != status) {
        ChipLogError(Ble, "%s: Failed to get onBeaconClientRegistered cb, err: %d",
        __func__, status);
        return ACE_STATUS_GENERAL_ERROR;
    }

    status = aceBt_bleRegisterGattClient(session->session_hdl,
                            &gatt_client_callback,
                            ACE_BT_BLE_APPID_GENERIC);
    if (status != ACE_STATUS_OK) {
        ChipLogError(Ble, "%s: Unable to register bt GATT client, err = %u",
        __func__, status);
        return ACE_STATUS_GENERAL_ERROR;
    }

    return ACE_STATUS_OK;
}

static void RegisterBTClient(void)
{
    // Create one session at the start
    session = (bt_ctx_t*)aceAlloc_alloc(ACE_MODULE_GROUP,
                            ACE_ALLOC_BUFFER_GENERIC,
                            sizeof(bt_ctx_t));
    if (!session) {
        ChipLogError(Ble, "%s: Failed to create BT context", __func__);
        return;
    }
    if (btSessionCreate() != ACE_STATUS_OK) {
        ChipLogError(Ble, "%s: Failed to initialize BT session", __func__);
        return;
    }
}

static ace_status_t StartBeaconScan(void) {
    aceBT_BeaconScanSettings_t settings;

    memset(&settings, 0, sizeof(aceBT_BeaconScanSettings_t));
    settings.delay = 0;
    settings.resultType = ACEBT_BEACON_SCAN_RESULT_TYPE_FULL;
    settings.scanMode =  ACEBT_BEACON_SCAN_MODE_LOW_LATENCY;
    settings.callbackType = ACEBT_BEACON_SCAN_CALLBACK_TYPE_ALL_MATCHES;
    settings.matchNum = ACEBT_BEACON_SCAN_MATCH_MAX;
    settings.matchMode = ACEBT_BEACON_SCAN_MATCH_MODE_AGGRESIVE;

    aceBT_uuid_t service_uuid = {{0xff, 0xf6}, ACEBT_UUID_TYPE_16};
    aceBT_uuid_t service_uuid_mask = {{0xff, 0xff}, ACEBT_UUID_TYPE_16};
    aceBT_BeaconScanFilter_t filter;
    aceBT_BeaconScanFilterList_t filters;
    memset(&filter, 0, sizeof(aceBT_BeaconScanFilter_t));
    filter.filterType = ACEBT_BEACON_SCAN_FILTER_SERVICE_DATA_UUID;
    filter.filterValue.uuid.uuid = service_uuid;
    filter.filterValue.uuid.uuidMask = service_uuid_mask;
    memset(&filters, 0, sizeof(aceBT_BeaconScanFilterList_t));
    filters.numberOfFilters = 1;
    filters.filters = &filter;

    ChipLogProgress(Ble, "%s: Starting beacon scan", __func__);
    return aceBT_startBeaconScan(session->session_hdl,
                                 BEACON_CLIENT_TYPE_UNKNOWN,
                                 settings, &filters, 1,
                                 &session->scan_hdl);
}

static void StopBeaconScan(void) {
    ace_status_t status;
    status = aceBT_stopBeaconScan(session->scan_hdl);
    if (ACE_STATUS_OK != status) {
        ChipLogError(Ble, "%s: aceBT_stopBeaconScan failed, status = %d",
        __func__, status);
    }
    ChipLogProgress(Ble, "%s: Scanning stopped", __func__);
}

static void GetGattcService(BLE_CONNECTION_OBJECT conId) {
    aceBT_bleDiscoverAllServices(session->session_hdl,
                (aceBT_bleConnHandle)conId);

    // Wait max of 5s to get discover callback
    ChipLogProgress(Ble, "%s: Waiting 5s for callback for onGattcServiceDiscovered cb",
    __func__);
    ace_status_t status = aceSem_waitTimeout(&sync_call_sem, 2*SYNC_CALL_WAIT_TIME_MS);
    if (ACE_STATUS_OK != status) {
        ChipLogError(Ble, "%s: Failed to get onGattcServiceDiscovered cb, err: %d",
                __func__, status);
        return;
    }

    aceBT_bleGetService((aceBT_bleConnHandle)conId);
    // Wait max of 5s to get gatt db callback
    ChipLogProgress(Ble, "%s: Waiting 5s for callback for onGattcGetDb cb",
    __func__);
    status = aceSem_waitTimeout(&sync_call_sem, SYNC_CALL_WAIT_TIME_MS);
    if (ACE_STATUS_OK != status) {
        ChipLogError(Ble, "%s: Failed to get onGattcGetDb cb, err: %d",
                __func__, status);
        return;
    }
}

void BLEManagerImpl::ConnectToDevice() {
    StopBeaconScan();
    ChipLogProgress(Ble, "%s: Connecting to CHIP device: "
        "%02x:%02x:%02x:%02x:%02x:%02x",
        __func__,
        address.address[0], address.address[1],
        address.address[2], address.address[3],
        address.address[4], address.address[5]);

    aceBt_bleConnect(session->session_hdl, &address,
                    ACE_BT_BLE_CONN_PARAM_MAX,
                    ACEBT_BLE_GATT_CLIENT_ROLE, false,
                    ACE_BT_BLE_CONN_PRIO_MEDIUM);
}

void BLEManagerImpl::ConnectToDevice(intptr_t arg)
{
    sInstance.ConnectToDevice();
}

void BLEManagerImpl::InitiateScan(BleScanState scanType)
{
    RegisterBTClient();
    if (!session) {
        mBLEScanConfig.mBleScanState = BleScanState::kNotScanning;
        BleConnectionDelegate::OnConnectionError (mBLEScanConfig.mAppState, CHIP_ERROR_INTERNAL);
        ChipLogError(Ble, "Failed to create a BLE session for scanning");
        return;
    }
    mBLEScanConfig.mBleScanState = scanType;

    if (ACE_STATUS_OK != StartBeaconScan()) {
        mBLEScanConfig.mBleScanState = BleScanState::kNotScanning;
        ChipLogError(Ble, "Failed to start a BLE scan");
        BleConnectionDelegate::OnConnectionError (mBLEScanConfig.mAppState, CHIP_ERROR_INTERNAL);
        return;
    }

    // Wait max of 10s to get beacon scan result callback
    ChipLogProgress(Ble, "%s: Waiting 10s for BeaconScanResults cb", __func__);
    if (ACE_STATUS_OK != aceSem_waitTimeout(&sync_call_sem, 2*SYNC_CALL_WAIT_TIME_MS)) {
        ChipLogError(Ble, " %s: Failed to get onBeaconScanResults cb", __func__);
        return;
    }
    mBLEScanConfig.mBleScanState = BleScanState::kConnecting;
    PlatformMgr().ScheduleWork(ConnectToDevice, 0);
}

void BLEManagerImpl::InitiateScan(intptr_t arg)
{
    sInstance.InitiateScan(static_cast<BleScanState>(arg));
}

void BLEManagerImpl::CleanScanConfig()
{
    mBLEScanConfig.mBleScanState = BleScanState::kNotScanning;
}

// BLE Platform Abstractions

CHIP_ERROR BLEManagerImpl::_Init()
{
    CHIP_ERROR err;

    ChipLogProgress(Ble, "%s", __func__);
    err = BleLayer::Init(this, this, this, &SystemLayer);
    SuccessOrExit(err);

    mServiceMode = ConnectivityManager::kCHIPoBLEServiceMode_Enabled;
    mAppState = nullptr;

exit:
    return err;
}

CHIP_ERROR BLEManagerImpl::_SetCHIPoBLEServiceMode(CHIPoBLEServiceMode val)
{
    CHIP_ERROR err = CHIP_NO_ERROR;

    ChipLogProgress(Ble, "%s: %d", __func__, val);
    VerifyOrExit(val != ConnectivityManager::kCHIPoBLEServiceMode_NotSupported,
                    err = CHIP_ERROR_INVALID_ARGUMENT);
    VerifyOrExit(mServiceMode == ConnectivityManager::kCHIPoBLEServiceMode_NotSupported,
                    err = CHIP_ERROR_UNSUPPORTED_CHIP_FEATURE);

    if (val != mServiceMode)
    {
        mServiceMode = val;
    }
exit:
    return err;
}

CHIP_ERROR BLEManagerImpl::_SetAdvertisingEnabled(bool val)
{
    ChipLogProgress(Ble, "%s not supported", __func__);
    return BLE_ERROR_NOT_IMPLEMENTED;
}

CHIP_ERROR BLEManagerImpl::_SetAdvertisingMode(BLEAdvertisingMode mode)
{
    ChipLogProgress(Ble, "%s not supported", __func__);
    return BLE_ERROR_NOT_IMPLEMENTED;
}

CHIP_ERROR BLEManagerImpl::_GetDeviceName(char * buf, size_t bufSize)
{
    ChipLogProgress(Ble, "%s not supported", __func__);
    return BLE_ERROR_NOT_IMPLEMENTED;
}

CHIP_ERROR BLEManagerImpl::_SetDeviceName(const char * deviceName)
{
    ChipLogProgress(Ble, "%s not supported", __func__);
    return BLE_ERROR_NOT_IMPLEMENTED;
}

uint16_t BLEManagerImpl::_NumConnections()
{
    ChipLogProgress(Ble, "%s not supported", __func__);
    return 0;
}

CHIP_ERROR BLEManagerImpl::ConfigureBle(uint32_t aAdapterId, bool aIsCentral)
{
    return CHIP_NO_ERROR;
}

void BLEManagerImpl::_OnPlatformEvent(const ChipDeviceEvent * event)
{
    switch (event->Type)
    {
    case DeviceEventType::kCHIPoBLESubscribe:
        HandleSubscribeReceived(event->CHIPoBLESubscribe.ConId,
                &CHIP_BLE_SVC_ID, &ChipUUID_CHIPoBLEChar_TX);
        {
            ChipDeviceEvent connectionEvent;
            connectionEvent.Type = DeviceEventType::kCHIPoBLEConnectionEstablished;
            PlatformMgr().PostEvent(&connectionEvent);
        }
        break;
    case DeviceEventType::kCHIPoBLEUnsubscribe:
        HandleUnsubscribeReceived(event->CHIPoBLEUnsubscribe.ConId,
                &CHIP_BLE_SVC_ID, &ChipUUID_CHIPoBLEChar_TX);
        break;

    case DeviceEventType::kCHIPoBLEWriteReceived:
        HandleWriteReceived(event->CHIPoBLEWriteReceived.ConId,
                &CHIP_BLE_SVC_ID, &ChipUUID_CHIPoBLEChar_RX,
                PacketBufferHandle::Adopt(event->CHIPoBLEWriteReceived.Data));
        break;

    case DeviceEventType::kCHIPoBLEIndicateConfirm:
        HandleIndicationConfirmation(event->CHIPoBLEIndicateConfirm.ConId,
                &CHIP_BLE_SVC_ID, &ChipUUID_CHIPoBLEChar_TX);
        break;

    case DeviceEventType::kCHIPoBLEConnectionError:
        HandleConnectionError(event->CHIPoBLEConnectionError.ConId,
                event->CHIPoBLEConnectionError.Reason);
        break;
    case DeviceEventType::kPlatformLinuxBLECentralConnected:
        if (mBLEScanConfig.mBleScanState == BleScanState::kConnecting)
        {
            // Discover Gattc services for this connection
            GetGattcService(event->Platform.BLECentralConnected.mConnection);
            BleConnectionDelegate::OnConnectionComplete(mBLEScanConfig.mAppState,
                    event->Platform.BLECentralConnected.mConnection);
            CleanScanConfig();
        }
        break;
    case DeviceEventType::kPlatformLinuxBLECentralConnectFailed:
        if (mBLEScanConfig.mBleScanState == BleScanState::kConnecting)
        {
            BleConnectionDelegate::OnConnectionError(mBLEScanConfig.mAppState,
                    event->Platform.BLECentralConnectFailed.mError);
            CleanScanConfig();
        }
        break;
    case DeviceEventType::kPlatformLinuxBLEWriteComplete:
        HandleWriteConfirmation(event->Platform.BLEWriteComplete.mConnection,
                    &CHIP_BLE_SVC_ID, &ChipUUID_CHIPoBLEChar_RX);
        break;
    case DeviceEventType::kPlatformLinuxBLESubscribeOpComplete:
        if (event->Platform.BLESubscribeOpComplete.mIsSubscribed)
            HandleSubscribeComplete(event->Platform.BLESubscribeOpComplete.mConnection,
                    &CHIP_BLE_SVC_ID, &ChipUUID_CHIPoBLEChar_TX);
        else
            HandleUnsubscribeComplete(event->Platform.BLESubscribeOpComplete.mConnection,
                    &CHIP_BLE_SVC_ID, &ChipUUID_CHIPoBLEChar_TX);
        break;
    case DeviceEventType::kPlatformLinuxBLEIndicationReceived:
        HandleIndicationReceived(event->Platform.BLEIndicationReceived.mConnection,
                    &CHIP_BLE_SVC_ID, &ChipUUID_CHIPoBLEChar_TX,
                    PacketBufferHandle::Adopt(event->Platform.BLEIndicationReceived.mData));
        break;
    default:
        ChipLogProgress(Ble, "%s not handled", __func__);
        break;
    }
}

void BLEManagerImpl::NewConnection(BleLayer * bleLayer,
                                   void * appState,
                                   const uint16_t connDiscriminator)
{
    ChipLogProgress(Ble, "%s: Initiate connection to a CHIP BLE peripheral", __func__);
    bleDiscriminator = connDiscriminator;
    mBLEScanConfig.mAppState = appState;

    // Scan initiation performed async, to ensure that the BLE subsystem is initialized.
    PlatformMgr().ScheduleWork(InitiateScan,
        static_cast<intptr_t>(BleScanState::kScanForDiscriminator));
}

BLE_ERROR BLEManagerImpl::CancelConnection()
{
    ChipLogProgress(Ble, "%s", __func__);
    return BLE_ERROR_NOT_IMPLEMENTED;
}

bool BLEManagerImpl::SubscribeCharacteristic(BLE_CONNECTION_OBJECT conId,
                            const ChipBleUUID * svcId,
                            const ChipBleUUID * charId)
{
    bool result = false;

    if (!Ble::UUIDsMatch(svcId, &CHIP_BLE_SVC_ID)) {
        ChipLogError(DeviceLayer, "SubscribeCharacteristic() called with invalid service ID");
        return result;
    } else if (!Ble::UUIDsMatch(charId, &ChipUUID_CHIPoBLEChar_TX)) {
        ChipLogError(DeviceLayer, "SubscribeCharacteristic() called with invalid characteristic ID");
        return result;
    }
    aceBT_gattCharRec_t *char_rec = NULL, *char_rec_needed = NULL;
    aceBT_bleGattsService_t* chip_server = NULL;

    if (!gattc_conn) {
        return result;
    }
    if (gattc_conn->connHandle == (aceBT_bleConnHandle)conId) {
        chip_server = &gattc_conn->gatt_db[2];
    }
    if (!chip_server) {
        ChipLogError(Ble, "%s: Gatt DB not found", __func__);
        return result;
    }

    int count = 0;
    // Need to check UUID match here instead of hardcoding
    STAILQ_FOREACH(char_rec, &chip_server->charsList, link) {
        count++;
        if (count == 2) {
            char_rec_needed = char_rec;
            break;
        }
    }

    if (aceBT_bleSetNotification(session->session_hdl, (aceBT_bleConnHandle)conId,
                            char_rec_needed->value, true) == ACE_STATUS_OK) {
        result = true;
    }
    return result;
}

bool BLEManagerImpl::UnsubscribeCharacteristic(BLE_CONNECTION_OBJECT conId,
                const ChipBleUUID * svcId, const ChipBleUUID * charId)
{
    bool result = false;

    if (!Ble::UUIDsMatch(svcId, &CHIP_BLE_SVC_ID)) {
        ChipLogError(DeviceLayer, "SubscribeCharacteristic() called with invalid service ID");
        return result;
    } else if (!Ble::UUIDsMatch(charId, &ChipUUID_CHIPoBLEChar_TX)) {
        ChipLogError(DeviceLayer, "SubscribeCharacteristic() called with invalid characteristic ID");
        return result;
    }
    aceBT_gattCharRec_t *char_rec = NULL, *char_rec_needed = NULL;
    aceBT_bleGattsService_t* chip_server = NULL;

    if (!gattc_conn) {
        return result;
    }
    if (gattc_conn->connHandle == (aceBT_bleConnHandle)conId) {
        chip_server = &gattc_conn->gatt_db[2];
    }
    if (!chip_server) {
        ChipLogError(Ble, "%s: Gatt DB not found", __func__);
        return result;
    }

    int count = 0;
    // Need to check UUID match here instead of hardcoding
    STAILQ_FOREACH(char_rec, &chip_server->charsList, link) {
        count++;
        if (count == 2) {
            char_rec_needed = char_rec;
            break;
        }
    }

    if (aceBT_bleSetNotification(session->session_hdl, (aceBT_bleConnHandle)conId,
                            char_rec_needed->value, false) == ACE_STATUS_OK) {
        result = true;
    }
    return result;
}

bool BLEManagerImpl::CloseConnection(BLE_CONNECTION_OBJECT conId)
{
    ChipLogProgress(Ble, "%s", __func__);
    if (gattc_conn->connHandle == (aceBT_bleConnHandle)conId) {
        delete gattc_conn;
        return true;
    }
    return false;
}

uint16_t BLEManagerImpl::GetMTU(BLE_CONNECTION_OBJECT conId) const
{
    int mtu = 0;
    aceBT_getMtu(session->session_hdl, (aceBT_bleConnHandle)conId, &mtu);
    ChipLogProgress(Ble, "%s: MTU = %d", __func__, mtu);
    return (uint16_t)mtu;
}

bool BLEManagerImpl::SendIndication(BLE_CONNECTION_OBJECT conId,
                                    const ChipBleUUID * svcId,
                                    const Ble::ChipBleUUID * charId,
                                    chip::System::PacketBufferHandle pBuf)
{
    ChipLogProgress(Ble, "%s not supported", __func__);
    return false;
}

bool BLEManagerImpl::SendWriteRequest(BLE_CONNECTION_OBJECT conId,
                                      const Ble::ChipBleUUID * svcId,
                                      const Ble::ChipBleUUID * charId,
                                      chip::System::PacketBufferHandle pBuf)
{
    bool result = false;

    if (!Ble::UUIDsMatch(svcId, &CHIP_BLE_SVC_ID)) {
        ChipLogError(DeviceLayer, "SendWriteRequest() called with invalid service ID");
        return result;
    } else if (!Ble::UUIDsMatch(charId, &ChipUUID_CHIPoBLEChar_RX)) {
        ChipLogError(DeviceLayer, "SendWriteRequest() called with invalid characteristic ID");
        return result;
    }

    uint16_t len = pBuf->DataLength();
    uint8_t *data = new uint8_t[len];
    pBuf->Read(data, len);

    // Need to check UUID match here instead of hardcoding

    aceBT_gattCharRec_t *char_rec = NULL, *char_rec_needed = NULL;
    aceBT_bleGattsService_t* chip_server = NULL;

    if (!gattc_conn) {
        return result;
    }
    if (gattc_conn->connHandle == (aceBT_bleConnHandle)conId) {
        chip_server = &gattc_conn->gatt_db[2];
    }
    if (!chip_server) {
        ChipLogError(Ble, "%s: Gatt DB not found", __func__);
        delete[] data;
        return result;
    }

    int count = 0;
    STAILQ_FOREACH(char_rec, &chip_server->charsList, link) {
        count++;
        if (count == 1) {
            char_rec_needed = char_rec;
            break;
        }
    }

    char_rec_needed->value.blobValue.size = len * sizeof(uint8_t);
    char_rec_needed->value.blobValue.data = data;
    char_rec_needed->value.blobValue.offset = 0;
    if (aceBT_bleWriteCharacteristics(session->session_hdl, (aceBT_bleConnHandle)conId,
        &char_rec_needed->value, ACEBT_BLE_WRITE_TYPE_RESP_REQUIRED) == ACE_STATUS_OK) {
        result = true;
    }
    delete[] data;
    return result;
}

bool BLEManagerImpl::SendReadRequest(BLE_CONNECTION_OBJECT conId,
                                     const Ble::ChipBleUUID * svcId,
                                     const Ble::ChipBleUUID * charId,
                                     chip::System::PacketBufferHandle pBuf)
{
    ChipLogProgress(Ble, "%s not supported", __func__);
    return false;
}

bool BLEManagerImpl::SendReadResponse(BLE_CONNECTION_OBJECT conId,
                                      BLE_READ_REQUEST_CONTEXT requestContext,
                                      const Ble::ChipBleUUID * svcId,
                                      const Ble::ChipBleUUID * charId)
{
    ChipLogProgress(Ble, "%s not supported", __func__);
    return false;
}

void BLEManagerImpl::NotifyChipConnectionClosed(BLE_CONNECTION_OBJECT conId)
{
    ChipLogProgress(Ble, "%s not supported", __func__);
}

// These are called from the ACS BT callback thread

void BLEManagerImpl::HandleNewConnection(BLE_CONNECTION_OBJECT conId)
{
    ChipDeviceEvent event;
    event.Type = DeviceEventType::kPlatformLinuxBLECentralConnected;
    event.Platform.BLECentralConnected.mConnection = conId;
    PlatformMgr().PostEvent(&event);
}

void BLEManagerImpl::HandleWriteComplete(BLE_CONNECTION_OBJECT conId)
{
    ChipDeviceEvent event;
    event.Type = DeviceEventType::kPlatformLinuxBLEWriteComplete;
    event.Platform.BLEWriteComplete.mConnection = conId;
    PlatformMgr().PostEvent(&event);
}

void BLEManagerImpl::HandleSubscribeOpComplete(BLE_CONNECTION_OBJECT conId, bool subscribed)
{
    ChipDeviceEvent event;
    event.Type = DeviceEventType::kPlatformLinuxBLESubscribeOpComplete;
    event.Platform.BLEWriteComplete.mConnection = conId;
    event.Platform.BLESubscribeOpComplete.mIsSubscribed = subscribed;
    PlatformMgr().PostEvent(&event);
}

void BLEManagerImpl::HandleTXCharChanged(BLE_CONNECTION_OBJECT conId,
        const uint8_t * value, size_t len)
{
    CHIP_ERROR err = CHIP_NO_ERROR;
    System::PacketBufferHandle buf = System::PacketBufferHandle::NewWithData(value, len);

    VerifyOrExit(!buf.IsNull(), err = CHIP_ERROR_NO_MEMORY);

    ChipDeviceEvent event;
    event.Type = DeviceEventType::kPlatformLinuxBLEIndicationReceived;
    event.Platform.BLEIndicationReceived.mConnection = conId;
    event.Platform.BLEIndicationReceived.mData = std::move(buf).UnsafeRelease();
    PlatformMgr().PostEvent(&event);

exit:
    if (err != CHIP_NO_ERROR)
        ChipLogError(Ble, "%s failed: %s", __func__, ErrorStr(err));
}

void BLEManagerImpl::HandleConnectionClosed(BLE_CONNECTION_OBJECT conId)
{
    // Nothing to do
}

} // namespace Internal
} // namespace DeviceLayer
} // namespace chip

#endif // CHIP_DEVICE_CONFIG_ENABLE_CHIPOBLE
