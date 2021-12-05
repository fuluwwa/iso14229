#include "iso14229.h"
#include "isotp/isotp.h"
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

#ifndef MIN
#define MIN(a, b) ((a) > (b) ? (b) : (a))
#endif

#define ARRAY_SZ(X) (sizeof((X)) / sizeof((X)[0]))

#define RESPONSE_ID_OF(sid) (sid + 0x40)

static bool suppressPosRspMsgIndicationBitIsSet(uint8_t subfunction) { return subfunction & 0x80; }

static inline void iso14229DownloadHandlerInit(Iso14229DownloadHandler *handler);

// ========================================================================
//                              Private Functions
// ========================================================================

/**
 * @brief Convenience function to send negative response
 *
 * @param self
 * @param response_code
 */
static inline void iso14229SendNegativeResponse(Iso14229Instance *self,
                                                const Iso14229ServiceRequest *req,
                                                uint8_t response_code) {
    TportSend *tport = &self->tport_send;
    Iso14229NegativeResponse *resp = &tport->buf.negResponse;

    resp->negResponseSid = 0x7F;
    resp->requestSid = req->sid;
    resp->responseCode = response_code;

    tport->buf_len_used = sizeof(Iso14229NegativeResponse);
    tport->pending = true;
}

static inline void iso14229SendResponse(Iso14229Instance *self, const Iso14229ServiceRequest *req,
                                        const uint16_t len) {
    TportSend *tport = &self->tport_send;
    tport->buf.posResponse.serviceId = RESPONSE_ID_OF(req->sid);
    const int total_len = offsetof(Iso14229PositiveResponse, type) + len;
    if (total_len > ISO14229_TPORT_SEND_BUFSIZE) {
        ISO14229USERDEBUG("TportSend too small for response");
        return;
    }
    tport->buf_len_used = total_len;
    tport->pending = true;
}

// Convenience method to retrieve from enum
#define GET_RESPONSE_VIEW(self, fieldname) (&self->tport_send.buf.posResponse.type.fieldname)

/**
 * @brief 0x10 DiagnosticSessionControl
 *
 * @param self
 * @param data
 * @param size
 */
void iso14229DiagnosticSessionControl(Iso14229Instance *self, const Iso14229ServiceRequest *req) {
    DiagnosticSessionControlResponse *response = GET_RESPONSE_VIEW(self, diagnosticSessionControl);

    uint8_t diagSessionType = 0;
    if (req->size < 1) {
        iso14229SendNegativeResponse(self, req, kIncorrectMessageLengthOrInvalidFormat);
        return;
    }

    diagSessionType = req->buf[0] & 0x4F;

    // TODO: add user-defined diag modes
    switch (diagSessionType) {
    case kDiagModeDefault:
    case kDiagModeProgramming:
    case kDiagModeExtendedDiagnostic:
        break;
    default:
        iso14229SendNegativeResponse(self, req, kServiceNotSupported);
        return;
    }

    self->diag_mode = diagSessionType;

    if (suppressPosRspMsgIndicationBitIsSet(req->buf[0])) {
        return;
    }

    response->diagSessionType = diagSessionType;

    // ISO14229-1-2013: Table 29
    // resolution: 1ms
    response->P2 = Iso14229htons(self->cfg->p2_ms);
    // resolution: 10ms
    response->P2star = Iso14229htons(self->cfg->p2_star_ms / 10);

    iso14229SendResponse(self, req, sizeof(DiagnosticSessionControlResponse));
}

/**
 * @brief 0x11 ECUReset
 *
 * @param self
 * @param data
 * @param size
 */
void iso14229ECUReset(Iso14229Instance *self, const Iso14229ServiceRequest *req) {
    ECUResetResponse *response = GET_RESPONSE_VIEW(self, ecuReset);
    uint8_t resetType = 0;
    if (req->size < 1) {
        iso14229SendNegativeResponse(self, req, kIncorrectMessageLengthOrInvalidFormat);
        return;
    }

    resetType = req->buf[0] & 0x3F;

    if (kHardReset == resetType) {
        if (self->ecu_reset_requested) {
            ;
        } else {
            self->ecu_reset_100ms_timer = iso14229UserGetms() + 100;
        }
        self->ecu_reset_requested = true;
    }

    response->resetType = resetType;
    response->powerDownTime = 0;

    iso14229SendResponse(self, req, sizeof(ECUResetResponse));
}

typedef struct {
    uint16_t dataIdentifier;
} ReadDataByIdentifierRequest;

/**
 * @brief 0x22 ReadDataByIdentifier
 *
 * @param self
 * @param data
 * @param size
 */
void iso14229ReadDataByIdentifier(Iso14229Instance *self, const Iso14229ServiceRequest *req) {
    ReadDataByIdentifierResponse *response = GET_RESPONSE_VIEW(self, readDataByIdentifier);
    uint8_t numDIDs = req->size / sizeof(ReadDataByIdentifierRequest);
    uint8_t *data_location = NULL;
    uint16_t dataRecordSize = 0;
    uint16_t responseLength = 0;
    uint16_t dataId = 0;
    enum Iso14229ResponseCodeEnum rdbi_response;

    if (NULL == self->cfg->userRDBIHandler) {
        return iso14229SendNegativeResponse(self, req, kServiceNotSupported);
    }

    if (req->size % sizeof(ReadDataByIdentifierRequest) != 0) {
        return iso14229SendNegativeResponse(self, req, kIncorrectMessageLengthOrInvalidFormat);
    }

    if (0 == numDIDs) {
        return iso14229SendNegativeResponse(self, req, kIncorrectMessageLengthOrInvalidFormat);
    }

    for (int i = 0; i < numDIDs; i++) {
        // TODO: make this safe
        dataId = Iso14229ntohs(*((uint16_t *)req->buf + i));

        rdbi_response = self->cfg->userRDBIHandler(dataId, &data_location, &dataRecordSize);
        if (kPositiveResponse == rdbi_response) {
            // TODO: make this safe: ensure that the offset doesn't exceed the
            // response buffer
            uint8_t *offset = ((uint8_t *)response) + responseLength;

            *(uint16_t *)(offset) = Iso14229htons(dataId);
            memcpy(offset + sizeof(uint16_t), data_location, dataRecordSize);

            responseLength += sizeof(uint16_t) + dataRecordSize;
        } else {
            iso14229SendNegativeResponse(self, req, rdbi_response);
            return;
        }
    }

    iso14229SendResponse(self, req, sizeof(ReadDataByIdentifierResponse) + responseLength);
}

typedef struct {
    uint8_t controlType;
    uint8_t communicationType;
    uint16_t nodeIdentificationNumber;
} CommunicationControlRequest;

/**
 * @brief 0x28 CommunicationControl
 *
 * @param self
 * @param data
 * @param size
 */
void iso14229CommunicationControl(Iso14229Instance *self, const Iso14229ServiceRequest *req) {
    CommunicationControlResponse *response = GET_RESPONSE_VIEW(self, communicationControl);
    CommunicationControlRequest *request = (CommunicationControlRequest *)req->buf;

    if (req->size < sizeof(CommunicationControlRequest)) {
        iso14229SendNegativeResponse(self, req, kIncorrectMessageLengthOrInvalidFormat);
        return;
    }
    if (request->communicationType > kDisableRxAndTx) {
        iso14229SendNegativeResponse(self, req, kIncorrectMessageLengthOrInvalidFormat);
        return;
    }
    response->controlType = request->communicationType;
    iso14229SendResponse(self, req, sizeof(CommunicationControlResponse));
}

typedef struct {
    uint16_t dataIdentifier;
    uint8_t dataRecord[];
} WriteDataByIdentifierRequest;

/**
 * @brief 0x2E WriteDataByIdentifier
 *
 * @param self
 * @param data
 * @param size
 */
void iso14229WriteDataByIdentifier(Iso14229Instance *self, const Iso14229ServiceRequest *req) {
    WriteDataByIdentifierResponse *response = GET_RESPONSE_VIEW(self, writeDataByIdentifier);
    WriteDataByIdentifierRequest *request = (WriteDataByIdentifierRequest *)req->buf;

    uint16_t dataLen = 0;
    uint16_t dataId = 0;
    enum Iso14229ResponseCodeEnum wdbi_response;

    /* Must contain at least one byte */
    if (req->size < sizeof(uint16_t) + 1) {
        iso14229SendNegativeResponse(self, req, kIncorrectMessageLengthOrInvalidFormat);
        return;
    }

    dataId = Iso14229ntohs(request->dataIdentifier);
    dataLen = req->size - offsetof(WriteDataByIdentifierRequest, dataRecord);

    response->dataId = Iso14229htons(dataId);

    if (NULL != self->cfg->userWDBIHandler) {
        wdbi_response = self->cfg->userWDBIHandler(dataId, request->dataRecord, dataLen);
        if (kPositiveResponse != wdbi_response) {
            iso14229SendNegativeResponse(self, req, wdbi_response);
            return;
        }
    } else {
        iso14229SendNegativeResponse(self, req, kServiceNotSupported);
        return;
    }

    iso14229SendResponse(self, req, sizeof(WriteDataByIdentifierResponse));
}

enum RoutineControlTypeEnum {
    kStartRoutine = 1,
    kStopRoutine = 2,
    kRequestRoutineResults = 3,
};

typedef struct {
    uint8_t routineControlType;
    uint16_t routineIdentifier;
    uint8_t routineControlOptionRecord[];
} __attribute__((packed)) RoutineControlRequest;

/**
 * @brief 0x31 RoutineControl
 *
 * @param self
 * @param data
 * @param size
 */
void iso14229RoutineControl(Iso14229Instance *self, const Iso14229ServiceRequest *req) {
    RoutineControlResponse *response = GET_RESPONSE_VIEW(self, routineControl);
    RoutineControlRequest *request = (RoutineControlRequest *)req->buf;
    enum Iso14229ResponseCodeEnum responseCode = kPositiveResponse;
    uint16_t routineIdentifier = Iso14229ntohs(request->routineIdentifier);

    if (req->size < sizeof(RoutineControlRequest)) {
        return iso14229SendNegativeResponse(self, req, kIncorrectMessageLengthOrInvalidFormat);
    }

    const Iso14229Routine *routine = NULL;
    for (uint16_t i = 0; i < self->nRegisteredRoutines; i++) {
        if (self->routines[i]->routineIdentifier == routineIdentifier) {
            routine = self->routines[i];
        }
    }

    // The subfunction corresponding to this routineIdentifier
    if (routine == NULL) {
        iso14229SendNegativeResponse(self, req, kSubFunctionNotSupported);
        return;
    }

    // The actual statusRecord length written by the routine
    uint16_t statusRecordLength = 0;

    Iso14229RoutineControlArgs args = {
        .optionRecord = request->routineControlOptionRecord,
        .optionRecordLength = offsetof(RoutineControlRequest, routineControlOptionRecord),
        .statusRecord = response->routineStatusRecord,
        .statusRecordBufferSize =
            ISO14229_TPORT_SEND_BUFSIZE - offsetof(RoutineControlResponse, routineStatusRecord),
        .statusRecordLength = &statusRecordLength,
    };

    switch (request->routineControlType) {
    case kStartRoutine:
        if (NULL != routine->startRoutine) {
            responseCode = routine->startRoutine(routine->userCtx, &args);
        } else {
            iso14229SendNegativeResponse(self, req, kSubFunctionNotSupported);
            return;
        }
        break;
    case kStopRoutine:
        if (NULL != routine->stopRoutine) {
            responseCode = routine->stopRoutine(routine->userCtx, &args);
        } else {
            iso14229SendNegativeResponse(self, req, kSubFunctionNotSupported);
            return;
        }
        break;
    case kRequestRoutineResults:
        if (NULL != routine->requestRoutineResults) {
            responseCode = routine->requestRoutineResults(routine->userCtx, &args);
        } else {
            iso14229SendNegativeResponse(self, req, kSubFunctionNotSupported);
            return;
        }
        break;
    default:
        iso14229SendNegativeResponse(self, req, kIncorrectMessageLengthOrInvalidFormat);
        return;
    }

    if (statusRecordLength > args.statusRecordBufferSize) {
        responseCode = kGeneralProgrammingFailure;
    }

    if (kPositiveResponse != responseCode) {
        iso14229SendNegativeResponse(self, req, responseCode);
    }

    response->routineControlType = request->routineControlType;
    response->routineIdentifier = Iso14229htons(routineIdentifier);
    response->routineInfo = 0;

    iso14229SendResponse(self, req, sizeof(RoutineControlResponse) + statusRecordLength);
}

typedef struct {
    uint8_t dataFormatIdentifier;
    uint8_t addressAndLengthFormatIdentifier;
    uint32_t memoryAddress;
    uint32_t memorySize;
} __attribute__((packed)) RequestDownloadRequest;

/**
 * @brief 0x34 RequestDownload
 *
 * @param self
 * @param data
 * @param size
 */
void iso14229RequestDownload(Iso14229Instance *self, const Iso14229ServiceRequest *const req) {
    RequestDownloadResponse *response = GET_RESPONSE_VIEW(self, requestDownload);
    RequestDownloadRequest *request = (RequestDownloadRequest *)req->buf;

    const Iso14229DownloadHandler *handler = NULL;
    enum Iso14229ResponseCodeEnum err;
    uint16_t maxNumberOfBlockLength = 0;
    void *memoryAddress = NULL;
    uint32_t memorySize = 0;

    if (req->size < sizeof(RequestDownloadRequest)) {
        return iso14229SendNegativeResponse(self, req, kIncorrectMessageLengthOrInvalidFormat);
    }

    uint8_t memorySizeLength = (request->addressAndLengthFormatIdentifier & 0xF0) >> 4;
    uint8_t memoryAddressLength = request->addressAndLengthFormatIdentifier & 0x0F;

    // ASSUMPTION: This server implementation only supports 32 bit memory
    // addressing
    if (memorySizeLength != sizeof(uint32_t) || memoryAddressLength != sizeof(uint32_t)) {
        return iso14229SendNegativeResponse(self, req, kRequestOutOfRange);
    }

    memoryAddress = (void *)((size_t)Iso14229ntohl(request->memoryAddress));
    memorySize = Iso14229ntohl(request->memorySize);

    // TODO: not yet implemented multiple Upload/Download handlers
    // This will need some documented heuristic for determining the correct
    // handler, probably a map of {memoryAddress: handler}
    if (self->nRegisteredDownloadHandlers < 1) {
        return iso14229SendNegativeResponse(self, req, kUploadDownloadNotAccepted);
    }
    handler = self->downloadHandlers[0];

    err = handler->cfg->onRequest(handler->cfg->userCtx, request->dataFormatIdentifier,
                                  memoryAddress, memorySize, &maxNumberOfBlockLength);

    if (err != kPositiveResponse) {
        return iso14229SendNegativeResponse(self, req, err);
    }

    if (0 == maxNumberOfBlockLength) {
        ISO14229USERDEBUG("WARNING: maxNumberOfBlockLength not set");
        return iso14229SendNegativeResponse(self, req, kGeneralProgrammingFailure);
    }

    // ISO-14229-1:2013 Table 401:
    // ASSUMPTION: use fixed size of maxNumberOfBlockLength in RequestDownload
    // response: 2 bytes
    response->lengthFormatIdentifier = 0x20;

// ISO-15764-2-2004 section 5.3.3
#define ISOTP_MTU 4095UL

/* ISO-14229-1:2013 Table 396: maxNumberOfBlockLength
This parameter is used by the requestDownload positive response message to
inform the client how many data bytes (maxNumberOfBlockLength) to include in
each TransferData request message from the client. This length reflects the
complete message length, including the service identifier and the
data-parameters present in the TransferData request message.
*/
#define MAX_TRANSFER_DATA_PAYLOAD_LEN (ISOTP_MTU)

    response->maxNumberOfBlockLength =
        Iso14229htons(MIN(maxNumberOfBlockLength, MAX_TRANSFER_DATA_PAYLOAD_LEN));
    iso14229SendResponse(self, req, sizeof(RequestDownloadResponse));
}

typedef struct {
    uint8_t blockSequenceCounter;
    uint8_t data[];
} __attribute__((packed)) TransferDataRequest;

static inline int blockSequenceNumberIsBad(const uint8_t block_counter,
                                           const Iso14229DownloadHandler *handler) {
    if (block_counter != handler->blockSequenceCounter) {
        return 1;
    }
    return 0;
}

/**
 * @brief 0x36 TransferData
 *
 * @param self
 * @param data
 * @param size
 */
void iso14229TransferData(Iso14229Instance *self, const Iso14229ServiceRequest *req) {
    TransferDataResponse *response = GET_RESPONSE_VIEW(self, transferData);
    TransferDataRequest *request = (TransferDataRequest *)req->buf;
    Iso14229DownloadHandler *handler = NULL;
    enum Iso14229ResponseCodeEnum err;

    uint16_t request_data_len = req->size - offsetof(TransferDataRequest, data);

    if (req->size < sizeof(TransferDataRequest)) {
        err = kIncorrectMessageLengthOrInvalidFormat;
        goto fail;
    }

    if (self->nRegisteredDownloadHandlers < 1) {
        err = kUploadDownloadNotAccepted;
        goto fail;
    }

    handler = self->downloadHandlers[0];

    if (blockSequenceNumberIsBad(request->blockSequenceCounter, handler)) {
        err = kRequestSequenceError;
        goto fail;
    } else {
        handler->blockSequenceCounter++;
    }

    err = handler->cfg->onTransfer(handler->cfg->userCtx, request->data, request_data_len);
    if (err != kPositiveResponse) {
        goto fail;
    }

    response->blockSequenceCounter = request->blockSequenceCounter;

    return iso14229SendResponse(self, req, sizeof(TransferDataResponse));

// There's been an error. Reinitialize the handler to clear out its state
fail:
    iso14229DownloadHandlerInit(handler);
    return iso14229SendNegativeResponse(self, req, err);
}

/**
 * @brief 0x37 RequestTransferExit
 *
 * @param self
 * @param data
 * @param size
 */
void iso14229RequestTransferExit(Iso14229Instance *self, const Iso14229ServiceRequest *req) {
    Iso14229DownloadHandler *handler = NULL;
    enum Iso14229ResponseCodeEnum err;

    if (self->nRegisteredDownloadHandlers < 1) {
        return iso14229SendNegativeResponse(self, req, kUploadDownloadNotAccepted);
    }
    handler = self->downloadHandlers[0];

    err = handler->cfg->onExit(handler->cfg->userCtx);

    if (err != kPositiveResponse) {
        return iso14229SendNegativeResponse(self, req, err);
    }

    iso14229DownloadHandlerInit(handler);

    iso14229SendResponse(self, req, sizeof(RequestTransferExitResponse));
}

typedef struct {
    uint8_t zeroSubFunction;
} TesterPresentRequest;

/**
 * @brief 0x3E TesterPresent
 *
 * @param self
 * @param data
 * @param size
 */
void iso14229TesterPresent(Iso14229Instance *self, const Iso14229ServiceRequest *req) {
    TesterPresentResponse *response = GET_RESPONSE_VIEW(self, testerPresent);
    TesterPresentRequest *request = (TesterPresentRequest *)req->buf;

    if (req->size < 1) {
        iso14229SendNegativeResponse(self, req, kIncorrectMessageLengthOrInvalidFormat);
        return;
    }

    self->s3_session_timeout_timer = iso14229UserGetms() + self->cfg->s3_ms;
    response->zeroSubFunction = request->zeroSubFunction & 0x3F;
    iso14229SendResponse(self, req, sizeof(TesterPresentResponse));
}

uint32_t isotp_user_get_ms() { return iso14229UserGetms(); }

void isotp_user_debug(const char *message, ...) {
    va_list ap;
    va_start(ap, message);
    vprintf(message, ap);
    va_end(ap);
}

int isotp_user_send_can(const uint32_t arbitration_id, const uint8_t *data, const uint8_t size) {
    return iso14229UserSendCAN(arbitration_id, data, size);
}

/**
 * @brief Call the service matching the SID in buf, else reply that the service
 * is unsupported
 *
 * @param self
 * @param buf   incoming data from ISO-TP layer
 * @param size  size of buf
 */
void iso14229CallRequestedService(Iso14229Instance *self, const uint8_t *buf, const uint16_t size) {
    Iso14229ServiceRequest req = {0};

    // The buffer size must be at least 1, the size of the SID byte
    if (NULL == buf || size < 1) {
        return;
    }
    req.sid = buf[0];
    req.buf = buf + 1;

    req.size = size - 1;

    void (*service)() = self->services[req.sid];
    if (NULL != service) {
        service(self, &req);
    } else {
        iso14229SendNegativeResponse(self, &req, kServiceNotSupported);
    }
}

// ========================================================================
//                             Public Functions
// ========================================================================

int iso14229UserInit(Iso14229Instance *self, const Iso14229ServerConfig *const cfg) {
    if (NULL == self || NULL == cfg) {
        return -1;
    }

    memset(self, 0, sizeof(Iso14229Instance));
    self->cfg = cfg;

    // Initialize p2_timer to an already past time, otherwise the server's
    // response to incoming messages will be delayed.
    self->p2_timer = iso14229UserGetms() - self->cfg->p2_ms;

    // Set the session timeout for s3 milliseconds from now.
    self->s3_session_timeout_timer = iso14229UserGetms() + self->cfg->s3_ms;

    self->tport_send.pending = false;
    self->tport_send.buf_len_used = 0;

    if (NULL != cfg->middleware) {
        if (NULL == cfg->middleware->initFunc || NULL == cfg->middleware->pollFunc ||
            NULL == cfg->middleware->self) {
            return -1;
        }
        Iso14229UserMiddleware *mw = cfg->middleware;
        if (0 != mw->initFunc(mw->self, mw->cfg, self)) {
            return -1;
        }
    }

    return 0;
}

/**
 * @brief ISO14229-1-2013 Figure 4
 *
 * @param self
 * @return int
 */
int iso14229StateMachine(Iso14229Instance *self) {
    if (self->ecu_reset_requested &&
        (Iso14229TimeAfter(iso14229UserGetms(), self->ecu_reset_100ms_timer))) {
        self->cfg->userHardReset();
        self->ecu_reset_requested = false;
    }
    return 0;
}

/**
 * @brief Retreive incoming data from the ISO-TP links and process it
 *
 * @param self
 */
void iso14229IsoTpReceive(Iso14229Instance *self) {
    const Iso14229ServerConfig *cfg = self->cfg;
    uint16_t out_size = 0;

    /* Note: passing (NULL, 0) to isotp_receive avoids a redundant copy. */
    if (ISOTP_RET_OK == isotp_receive(cfg->phys_link, NULL, 0, &out_size)) {
        iso14229CallRequestedService(self, cfg->phys_link->receive_buffer,
                                     cfg->phys_link->receive_size);
        self->p2_timer = iso14229UserGetms() + self->cfg->p2_ms;
        return;
    }

    if (ISOTP_RET_OK == isotp_receive(cfg->func_link, NULL, 0, &out_size)) {
        iso14229CallRequestedService(self, cfg->func_link->receive_buffer,
                                     cfg->func_link->receive_size);
        self->p2_timer = iso14229UserGetms() + self->cfg->p2_ms;
        return;
    }
}

void iso14229UserPoll(Iso14229Instance *self) {
    const Iso14229ServerConfig *cfg = self->cfg;

    // Poll the ISO-TP links first to prepare available incoming data, if any.
    isotp_poll(cfg->phys_link);
    isotp_poll(cfg->func_link);

    iso14229StateMachine(self);

    // Run middleware if installed
    if (NULL != cfg->middleware) {
        cfg->middleware->pollFunc(cfg->middleware->self, self);
    }

    if (true == self->tport_send.pending) {
        /* Only send if the server P2 time has elapsed. Otherwise, return
         * immediately */
        if (Iso14229TimeAfter(iso14229UserGetms(), self->p2_timer)) {
            isotp_send(cfg->phys_link, self->tport_send.buf.raw, self->tport_send.buf_len_used);
            /* Poll the ISO-TP links again to immediately send outgoing data */
            isotp_poll(cfg->phys_link);
            isotp_poll(cfg->func_link);

            self->p2_timer = iso14229UserGetms() + self->cfg->p2_ms;
            self->tport_send.pending = false;
            self->tport_send.buf_len_used = 0;
        }
        return;
    } else {
        iso14229IsoTpReceive(self);
    }
}

void iso14229UserReceiveCAN(Iso14229Instance *self, const uint32_t arbitration_id,
                            const uint8_t *data, const uint8_t size) {
    if (arbitration_id == self->cfg->phys_recv_id) {
        isotp_on_can_message(self->cfg->phys_link, (uint8_t *)data, size);
    } else if (arbitration_id == self->cfg->func_recv_id) {
        isotp_on_can_message(self->cfg->func_link, (uint8_t *)data, size);
    } else {
        return;
    }
}

int iso14229UserRegisterRoutine(Iso14229Instance *self, const Iso14229Routine *routine) {
    if ((self->nRegisteredRoutines >= ISO14229_USER_DEFINED_MAX_ROUTINES) || (routine == NULL) ||
        (routine->startRoutine == NULL)) {
        return -1;
    }

    self->routines[self->nRegisteredRoutines] = routine;
    self->nRegisteredRoutines++;
    return 0;
}

static inline void iso14229DownloadHandlerInit(Iso14229DownloadHandler *handler) {
    handler->isActive = false;
    handler->blockSequenceCounter = 1;
}

int iso14229UserRegisterDownloadHandler(Iso14229Instance *self, Iso14229DownloadHandler *handler,
                                        Iso14229DownloadHandlerConfig *cfg) {
    if ((self->nRegisteredDownloadHandlers >= ISO14229_USER_DEFINED_MAX_DOWNLOAD_HANDLERS) ||
        handler == NULL || cfg->onRequest == NULL || cfg->onTransfer == NULL ||
        cfg->onExit == NULL) {
        return -1;
    }

    handler->cfg = cfg;
    iso14229DownloadHandlerInit(handler);

    self->downloadHandlers[self->nRegisteredDownloadHandlers] = handler;
    self->nRegisteredDownloadHandlers++;
    return 0;
}

typedef struct {
    enum Iso14229DiagnosticServiceIdEnum sid;
    void *funcptr;
} ServiceMap;

static const ServiceMap serviceMap[] = {
    {.sid = kSID_DIAGNOSTIC_SESSION_CONTROL, .funcptr = iso14229DiagnosticSessionControl},
    {.sid = kSID_ECU_RESET, .funcptr = iso14229ECUReset},
    {.sid = kSID_READ_DATA_BY_IDENTIFIER, .funcptr = iso14229ReadDataByIdentifier},
    {.sid = kSID_COMMUNICATION_CONTROL, .funcptr = iso14229CommunicationControl},
    {.sid = kSID_WRITE_DATA_BY_IDENTIFIER, .funcptr = iso14229WriteDataByIdentifier},
    {.sid = kSID_ROUTINE_CONTROL, .funcptr = iso14229RoutineControl},
    {.sid = kSID_REQUEST_DOWNLOAD, .funcptr = iso14229RequestDownload},
    {.sid = kSID_TRANSFER_DATA, .funcptr = iso14229TransferData},
    {.sid = kSID_REQUEST_TRANSFER_EXIT, .funcptr = iso14229RequestTransferExit},
    {.sid = kSID_TESTER_PRESENT, .funcptr = iso14229TesterPresent},
};

int iso14229UserEnableService(Iso14229Instance *self, enum Iso14229DiagnosticServiceIdEnum sid) {
    for (int i = 0; i < ARRAY_SZ(serviceMap); i++) {
        if (serviceMap[i].sid == sid) {
            if (self->services[sid] == NULL) {
                self->services[sid] = serviceMap[i].funcptr;
                return 0;
            } else {
                return -2;
            }
        }
    }
    return -1;
}
