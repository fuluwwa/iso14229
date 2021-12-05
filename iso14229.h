#ifndef ISO14229_H
#define ISO14229_H

#include <stdbool.h>
#include <stdint.h>
#include "iso14229_config.h"
#include "isotp/isotp.h"

/* returns true if `a` is after `b` */
#define Iso14229TimeAfter(a, b) ((int32_t)((int32_t)(b) - (int32_t)(a)) < 0)

/**
 * @defgroup ISO14229Standardized ISO14229 Mandated Step
 * @defgroup ISO14229Optional ISO14229 Optional/Recommended Step
 * @defgroup ISO14229MfgSpecific ISO14229 Manufacturer Specific Step
 */

// ISO-14229-1:2013 Table 2
#define ISO14229_MAX_DIAGNOSTIC_SERVICES 0x7F

enum Iso14229DiagnosticServiceIdEnum {
    // ISO 14229-1 service requests
    kSID_DIAGNOSTIC_SESSION_CONTROL = 0x10,
    kSID_ECU_RESET = 0x11,
    kSID_READ_DATA_BY_IDENTIFIER = 0x22,
    kSID_COMMUNICATION_CONTROL = 0x28,
    kSID_WRITE_DATA_BY_IDENTIFIER = 0x2E,
    kSID_ROUTINE_CONTROL = 0x31,
    kSID_REQUEST_DOWNLOAD = 0x34,
    kSID_TRANSFER_DATA = 0x36,
    kSID_REQUEST_TRANSFER_EXIT = 0x37,
    kSID_TESTER_PRESENT = 0x3E,
    // ...
};

enum Iso14229DiagnosticModeEnum {
    kDiagModeDefault = 1,
    kDiagModeProgramming,
    kDiagModeExtendedDiagnostic,
};

enum Iso14229ResponseCodeEnum {
    kPositiveResponse = 0,
    kGeneralReject = 0x10,
    kServiceNotSupported = 0x11,
    kSubFunctionNotSupported = 0x12,
    kIncorrectMessageLengthOrInvalidFormat = 0x13,
    kResponseTooLong = 0x14,
    kBusyRepeatRequest = 0x21,
    kConditionsNotCorrect = 0x22,
    kRequestSequenceError = 0x24,
    kNoResponseFromSubnetComponent = 0x25,
    kFailurePreventsExecutionOfRequestedAction = 0x26,
    kRequestOutOfRange = 0x31,
    kSecurityAccessDenied = 0x33,
    kInvalidKey = 0x35,
    kExceedNumberOfAttempts = 0x36,
    kRequiredTimeDelayNotExpired = 0x37,
    kUploadDownloadNotAccepted = 0x70,
    kTransferDataSuspended = 0x71,
    kGeneralProgrammingFailure = 0x72,
    kWrongBlockSequenceCounter = 0x73,
    kRequestCorrectlyReceived_ResponsePending = 0x78,
    kSubFunctionNotSupportedInActiveSession = 0x7E,
    kServiceNotSupportedInActiveSession = 0x7F,
    kRpmTooHigh = 0x81,
    kRpmTooLow = 0x82,
    kEngineIsRunning = 0x83,
    kEngineIsNotRunning = 0x84,
    kEngineRunTimeTooLow = 0x85,
    kTemperatureTooHigh = 0x86,
    kTemperatureTooLow = 0x87,
    kVehicleSpeedTooHigh = 0x88,
    kVehicleSpeedTooLow = 0x89,
    kThrottlePedalTooHigh = 0x8A,
    kThrottlePedalTooLow = 0x8B,
    kTransmissionRangeNotInNeutral = 0x8C,
    kTransmissionRangeNotInGear = 0x8D,
    kISOSAEReserved = 0x8E,
    kBrakeSwitchNotClosed = 0x8F,
    kShifterLeverNotInPark = 0x90,
    kTorqueConverterClutchLocked = 0x91,
    kVoltageTooHigh = 0x92,
    kVoltageTooLow = 0x93,
};

typedef struct {
    uint8_t diagSessionType;
    uint16_t P2;
    uint16_t P2star;
} __attribute__((packed)) DiagnosticSessionControlResponse;

enum Iso14229ECUResetResetType {
    kHardReset = 1,
    kKeyOffOnReset = 2,
    kSoftReset = 3,
    kEnableRapidPowerShutDown = 4,
    kDisableRapidPowerShutDown = 5,
};

typedef struct {
    uint8_t resetType;
    uint8_t powerDownTime;
} __attribute__((packed)) ECUResetResponse;

enum Iso1422CommunicationControlType {
    kEnableRxAndTx = 0,
    kEnableRxAndDisableTx = 1,
    kDisableRxAndEnableTx = 2,
    kDisableRxAndTx = 3,
};

typedef struct {
    uint8_t controlType;
} __attribute__((packed)) CommunicationControlResponse;

typedef struct {
} __attribute__((packed)) ReadDataByIdentifierResponse;

typedef struct {
    uint16_t dataId;
} __attribute__((packed)) WriteDataByIdentifierResponse;

typedef struct {
    uint8_t routineControlType;
    uint16_t routineIdentifier;
    uint8_t routineInfo;
    uint8_t routineStatusRecord[];
} __attribute__((packed)) RoutineControlResponse;

typedef struct {
    uint8_t lengthFormatIdentifier;
    uint16_t maxNumberOfBlockLength;
} __attribute__((packed)) RequestDownloadResponse;

typedef struct {
    uint8_t blockSequenceCounter;
} __attribute__((packed)) TransferDataResponse;

typedef struct {
    // uint8_t transferResponseParameterRecord[]; // error: flexible array
    // member in a struct with no named members
} __attribute__((packed)) RequestTransferExitResponse;

typedef struct {
    uint8_t zeroSubFunction;
} __attribute__((packed)) TesterPresentResponse;

typedef struct {
    const uint8_t *optionRecord;
    const uint16_t optionRecordLength;

    uint8_t *statusRecord;
    const uint16_t statusRecordBufferSize;
    uint16_t *statusRecordLength;
} Iso14229RoutineControlArgs;

typedef enum Iso14229ResponseCodeEnum (*Iso14229RoutineControlUserCallbackType)(
    void *userCtx, Iso14229RoutineControlArgs *args);

typedef struct Iso14229Routine {
    uint16_t routineIdentifier; // Table 378 — Request message definition [0-0xFFFF]
    Iso14229RoutineControlUserCallbackType startRoutine;
    Iso14229RoutineControlUserCallbackType stopRoutine;
    Iso14229RoutineControlUserCallbackType requestRoutineResults;
    void *userCtx; // Pointer to user data
} Iso14229Routine;

typedef struct Iso14229Instance Iso14229Instance;

/*
Service Request context
*/
typedef struct {
    const uint8_t *buf; // service data
    uint16_t size;      // size of service data (not including service ID)
    uint8_t sid;        // the service ID
} Iso14229ServiceRequest;

typedef void (*Iso14229Service)(Iso14229Instance *self, const Iso14229ServiceRequest *req);

typedef struct {
    uint8_t negResponseSid;
    uint8_t requestSid;
    uint8_t responseCode;
} Iso14229NegativeResponse;

union Iso14229AllResponseTypes {
    DiagnosticSessionControlResponse diagnosticSessionControl;
    ECUResetResponse ecuReset;
    CommunicationControlResponse communicationControl;
    ReadDataByIdentifierResponse readDataByIdentifier;
    WriteDataByIdentifierResponse writeDataByIdentifier;
    RoutineControlResponse routineControl;
    RequestDownloadResponse requestDownload;
    TransferDataResponse transferData;
    RequestTransferExitResponse requestTransferExit;
    TesterPresentResponse testerPresent;
};

typedef struct {
    uint8_t serviceId;
    union Iso14229AllResponseTypes type;
} Iso14229PositiveResponse;

typedef struct {
    uint16_t buf_len_used;
    bool pending;
    union {
        uint8_t raw[ISO14229_TPORT_SEND_BUFSIZE];
        Iso14229PositiveResponse posResponse;
        Iso14229NegativeResponse negResponse;
    } buf;
} TportSend;

/**
 * @brief User-Defined handler for 0x34 RequestDownload, 0x36 TransferData, and
 * 0x37 RequestTransferExit
 *
 */
typedef struct {
    /**
     * @brief
     * @param maxNumberOfBlockLength maximum chunk size that the client can
     * accept in bytes
     * @return one of [kPositiveResponse, kRequestOutOfRange]
     */
    enum Iso14229ResponseCodeEnum (*onRequest)(void *userCtx, const uint8_t dataFormatIdentifier,
                                               const void *memoryAddress, const size_t memorySize,
                                               uint16_t *maxNumberOfBlockLength);
    enum Iso14229ResponseCodeEnum (*onTransfer)(void *userCtx, uint8_t *data, uint32_t len);
    enum Iso14229ResponseCodeEnum (*onExit)(void *userCtx);

    void *userCtx;
} Iso14229DownloadHandlerConfig;

typedef struct {
    const Iso14229DownloadHandlerConfig *cfg;

    // ISO14229-1-2013: 14.4.2.3, Table 404: The blockSequenceCounter parameter
    // value starts at 0x01
    uint8_t blockSequenceCounter;

    /**
     * @brief ISO14229-1-2013: Table 407 - 0x36 TransferData Supported negative
     * response codes requires that the server keep track of whether the
     * transfer is active
     */
    bool isActive;
} Iso14229DownloadHandler;

/**
 * @brief UserMiddleware: an interface for extending iso14299
 * @note See appsoftware.h and bootsoftware.h for examples
 */
typedef struct {
    /**
     * @brief the middleware object
     *
     */
    void *self;

    /**
     * @brief a pointer to middleware config
     *
     */
    const void *cfg;

    /**
     * @brief the middleware initialization function
     * @note This is intended to perform additional configuration the iso14229
     * instance such as enabling services, implementing RoutineControl or Upload
     * Download functional unit handlers.
     */
    int (*initFunc)(void *self, const void *cfg, struct Iso14229Instance *iso14229);

    /**
     * @brief the middleware polling function
     * @note this is indended to run middleware-specific time-dependent
     * state-machine behavior which can modify the state of the middleware or
     * the iso14229 instance
     */
    int (*pollFunc)(void *self, struct Iso14229Instance *iso14229);
} Iso14229UserMiddleware;

typedef struct {
    uint16_t phys_recv_id;
    uint16_t func_recv_id;
    uint16_t send_id;
    IsoTpLink *phys_link;
    IsoTpLink *func_link;

    /**
     * @brief user-provided RDBI handler. Permitted responses:
     *  0x00 positiveResponse
     *  0x13 incorrectMessageLengthOrInvalidFormat
     *  0x22 conditionsNotCorrect
     *  0x31 requestOutOfRange
     *  0x33 securityAccessDenied
     */
    enum Iso14229ResponseCodeEnum (*userRDBIHandler)(uint16_t dataId, uint8_t **data_location,
                                                     uint16_t *len);

    /**
     * @brief user-provided WDBI handler. Permitted responses:
     *  0x00 positiveResponse
     *  0x13 incorrectMessageLengthOrInvalidFormat
     *  0x22 conditionsNotCorrect
     *  0x31 requestOutOfRange
     *  0x33 securityAccessDenied
     *  0x72 generalProgrammingFailure
     */
    enum Iso14229ResponseCodeEnum (*userWDBIHandler)(uint16_t dataId, const uint8_t *data,
                                                     uint16_t len);

    /**
     * @brief user-provided function to reset the ECU
     */
    void (*userHardReset)();

    /**
     * @brief Server time constants (milliseconds)
     */
    uint16_t p2_ms;      // Default P2_server_max timing supported by the server for
                         // the activated diagnostic session.
    uint16_t p2_star_ms; // Enhanced (NRC 0x78) P2_server_max supported by the
                         // server for the activated diagnostic session.
    uint16_t s3_ms;      // Session timeout

    Iso14229UserMiddleware *middleware;
} Iso14229ServerConfig;

/**
 * @brief Iso14229 root struct
 *
 */
typedef struct Iso14229Instance {
    const Iso14229ServerConfig *cfg;

    Iso14229Service services[ISO14229_MAX_DIAGNOSTIC_SERVICES];
    const Iso14229Routine *routines[ISO14229_USER_DEFINED_MAX_ROUTINES]; // 0x31 RoutineControl
    uint16_t nRegisteredRoutines;

    Iso14229DownloadHandler *downloadHandlers[ISO14229_USER_DEFINED_MAX_DOWNLOAD_HANDLERS];
    uint16_t nRegisteredDownloadHandlers;

    enum Iso14229DiagnosticModeEnum diag_mode;
    bool ecu_reset_requested;
    uint32_t ecu_reset_100ms_timer;    // for delaying resetting until a response
                                       // has been sent to the client
    uint32_t p2_timer;                 // for rate limiting server responses
    uint32_t s3_session_timeout_timer; // for knowing when the diagnostic
                                       // session has timed out
    TportSend tport_send;
} Iso14229Instance;

void iso14229CallRequestedService(Iso14229Instance *inst, const uint8_t *buf, const uint16_t size);

// ========================================================================
//                              User Functions
// ========================================================================

/**
 * @brief Initialize the Iso14229Instance
 *
 * @param self
 * @param cfg
 * @return int
 */
int iso14229UserInit(Iso14229Instance *self, const Iso14229ServerConfig *cfg);

/**
 * @brief Poll the iso14229 object and its children. Call this function
 * periodically.
 *
 * @param self: pointer to initialized Iso14229Instance
 */
void iso14229UserPoll(Iso14229Instance *inst);

/**
 * @brief Pass receieved CAN frames to the Iso14229Instance
 *
 * @param self: pointer to initialized Iso14229Instance
 * @param arbitration_id
 * @param data
 * @param size
 */
void iso14229UserReceiveCAN(Iso14229Instance *inst, const uint32_t arbitration_id,
                            const uint8_t *data, const uint8_t size);

/**
 * @brief User-implemented get time function
 *
 * @return uint32_t
 */
extern uint32_t iso14229UserGetms();

/**
 * @brief User-implemented CAN send function
 *
 * @param arbitration_id
 * @param data
 * @param size
 * @return uint32_t
 */
extern uint32_t iso14229UserSendCAN(const uint32_t arbitration_id, const uint8_t *data,
                                    const uint8_t size);

/**
 * @brief Enable the requested service. Services are disabled by default
 *
 * @param self
 * @param sid
 * @return int 0: success, -1: service not found, -2:service already installed
 */
int iso14229UserEnableService(Iso14229Instance *self, enum Iso14229DiagnosticServiceIdEnum sid);

/**
 * @brief Register a 0x31 RoutineControl routine
 *
 * @param self
 * @param routine
 * @return int 0: success
 */
int iso14229UserRegisterRoutine(Iso14229Instance *self, const Iso14229Routine *routine);

/**
 * @brief Register a handler for the sequence [0x34 RequestDownload, 0x36
 * TransferData, 0x37 RequestTransferExit]
 *
 * @param self
 * @param handler
 * @param cfg
 * @return int 0: success
 */
int iso14229UserRegisterDownloadHandler(Iso14229Instance *self, Iso14229DownloadHandler *handler,
                                        Iso14229DownloadHandlerConfig *cfg);

// ========================================================================
//                              Helper functions
// ========================================================================

/**
 * @brief host to network short
 *
 * @param hostshort
 * @return uint16_t
 */
static inline uint16_t Iso14229htons(uint16_t hostshort) {
    return ((hostshort & 0xff) << 8) | ((hostshort & 0xff00) >> 8);
}

/**
 * @brief network to host short
 *
 * @param hostshort
 * @return uint16_t
 */
static inline uint16_t Iso14229ntohs(uint16_t networkshort) { return Iso14229htons(networkshort); }

static inline uint32_t Iso14229htonl(uint32_t hostlong) {
    return (((hostlong & 0xff) << 24) | ((hostlong & 0xff00) << 8) | ((hostlong & 0xff0000) >> 8) |
            ((hostlong & 0xff000000) >> 24));
}

static inline uint32_t Iso14229ntohl(uint32_t networklong) { return Iso14229htonl(networklong); }

#endif
