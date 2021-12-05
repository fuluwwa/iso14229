#include "iso14229.h"
#include <assert.h>
#include <stdint.h>
#include <sys/types.h>

/*******************************************************************************
 * Local types
 ******************************************************************************/
typedef uint32_t (*sendCAN_t)(const uint32_t arbitration_id, const uint8_t *data,
                              const uint8_t size);

/*******************************************************************************
 * Local function prototypes ('static')
 ******************************************************************************/
static enum Iso14229ResponseCodeEnum rdbiHandler(uint16_t dataId, uint8_t **data_location,
                                                 uint16_t *len);

static enum Iso14229ResponseCodeEnum wdbiHandler(uint16_t dataId, const uint8_t *data,
                                                 uint16_t len);

static void mockSystemReset();
static int mockWriteAppProgramFlash(uint8_t *const addr, const uint32_t len,
                                    const uint8_t *const data);
static enum Iso14229ResponseCodeEnum mockEraseAppProgramFlash(void *userCtx,
                                                              Iso14229RoutineControlArgs *args);
static bool mockUserApplicationIsValid();
static void mockUserEnterApplication();

/*******************************************************************************
 * Preprocessor definitions
 ******************************************************************************/

#define UDS_SEND_ID 0x7A8
#define UDS_PHYS_RECV_ID 0x7A0
#define UDS_FUNC_RECV_ID 0x7DF
#define ISOTP_BUFSIZE 8192

/*******************************************************************************
 * Global variable definitions
 ******************************************************************************/

uint32_t g_mockSystemResetCallCount = 0;
uint32_t g_mockEraseProgramFlashCallCount = 0;
bool g_mockUserApplicationIsValid = true;
uint32_t g_mockUserApplicationIsValidCallCount = 0;
uint32_t g_mock_ms = 0; // 时间

/*******************************************************************************
 * Local variable definitions ('static')
 ******************************************************************************/

/* this will be set to the address of a ctypes-wrapped Python function */
static sendCAN_t harnessSendCANCallback = NULL;

static uint8_t isotpPhysRecvBuf[ISOTP_BUFSIZE];
static uint8_t isotpPhysSendBuf[ISOTP_BUFSIZE];
static uint8_t isotpFuncRecvBuf[ISOTP_BUFSIZE];
static uint8_t isotpFuncSendBuf[ISOTP_BUFSIZE];

static IsoTpLink isotpPhysLink;
static IsoTpLink isotpFuncLink;

static Iso14229Instance uds;

static Iso14229ServerConfig uds_srv_cfg = {
    .phys_recv_id = UDS_PHYS_RECV_ID,
    .func_recv_id = UDS_FUNC_RECV_ID,
    .send_id = UDS_SEND_ID,
    .phys_link = &isotpPhysLink,
    .func_link = &isotpFuncLink,
    .userRDBIHandler = rdbiHandler,
    .userWDBIHandler = wdbiHandler,
    .userHardReset = mockSystemReset,
    .p2_ms = 50,
    .p2_star_ms = 2000,
    .s3_ms = 5000,
};

typedef struct {
    uint8_t u8;
    int8_t i8;
    uint16_t u16;
    int16_t i16;
    uint32_t u32;
    int32_t i32;
    uint64_t u64;
    int64_t i64;
    uint8_t u8arr[20];
} RdbiData;

RdbiData rdbiData = {
    .u8 = 0,
    .i8 = 1,
    .u16 = 2,
    .i16 = 3,
    .u32 = 4,
    .i32 = 5,
    .u64 = 6,
    .i64 = 7,
    .u8arr = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20}};

enum Iso14229ResponseCodeEnum rdbiHandler(uint16_t dataId, uint8_t **data_location, uint16_t *len) {

#define SET_TO(field)                                                                              \
    ({                                                                                             \
        *data_location = (uint8_t *)&rdbiData.field;                                               \
        *len = sizeof(rdbiData.field);                                                             \
    })

    switch (dataId) {
    case 0x0000:
        SET_TO(u8);
        break;
    case 0x0001:
        SET_TO(i8);
        break;
    case 0x0002:
        SET_TO(u16);
        break;
    case 0x0003:
        SET_TO(i16);
        break;
    case 0x0004:
        SET_TO(u32);
        break;
    case 0x0005:
        SET_TO(i32);
        break;
    case 0x0006:
        SET_TO(u64);
        break;
    case 0x0007:
        SET_TO(i64);
        break;
    case 0x0008:
        SET_TO(u8arr);
        break;
    default:
        return kRequestOutOfRange;
    }
    return kPositiveResponse;
}

enum Iso14229ResponseCodeEnum wdbiHandler(uint16_t dataId, const uint8_t *data, uint16_t len) {
    if (false) {
        return kRequestOutOfRange;
    } else {
        return kPositiveResponse;
    }
}

void mockSystemReset() { g_mockSystemResetCallCount++; }

int mockWriteAppProgramFlash(uint8_t *const addr, const uint32_t len, const uint8_t *const data) {
    return 0;
}

enum Iso14229ResponseCodeEnum mockEraseAppProgramFlash(void *userCtx,
                                                       Iso14229RoutineControlArgs *args) {
    g_mockEraseProgramFlashCallCount++;
    return kPositiveResponse;
}

static bool mockUserApplicationIsValid() {
    g_mockUserApplicationIsValidCallCount++;
    return true;
}

static void mockUserEnterApplication() {}

/**
 * @brief set the C->Python CAN send callback function
 * @param cb callback function pointer
 */
void harnessSetSendCANCallback(sendCAN_t cb) { harnessSendCANCallback = cb; }

/**
 * @brief Python->C CAN receive function
 * @param arbitration_id
 * @param data
 * @param size
 */
void harnessRecvCAN(const uint32_t arbitration_id, const uint8_t *data, const uint8_t size) {
    iso14229UserReceiveCAN(&uds, arbitration_id, data, size);
}

/**
 * @brief initialize iso14229 and the ISO-TP links
 */
int harnessInit() {
    isotp_init_link(&isotpPhysLink, UDS_SEND_ID, isotpPhysSendBuf, ISOTP_BUFSIZE, isotpPhysRecvBuf,
                    ISOTP_BUFSIZE);
    isotp_init_link(&isotpFuncLink, UDS_SEND_ID, isotpFuncSendBuf, ISOTP_BUFSIZE, isotpFuncRecvBuf,
                    ISOTP_BUFSIZE);

    int retval = iso14229UserInit(&uds, (const Iso14229ServerConfig *)&uds_srv_cfg);
    iso14229UserEnableService(&uds, kSID_ECU_RESET);
    iso14229UserEnableService(&uds, kSID_READ_DATA_BY_IDENTIFIER);
    return retval;
}

/**
 * @brief run the iso14229 main loop
 * @param time_now_ms
 */
void harnessPoll(uint32_t time_now_ms) {
    g_mock_ms = time_now_ms;
    iso14229UserPoll(&uds);
}

/**
 * @brief implementation of iso14229 extern function
 */
uint32_t iso14229UserSendCAN(const uint32_t arbitration_id, const uint8_t *data,
                             const uint8_t size) {
    /* call the Python-configured callback to send CAN data into the Python
     * process */
    return harnessSendCANCallback(arbitration_id, data, size);
}

uint32_t iso14229UserGetms() { return g_mock_ms; }
