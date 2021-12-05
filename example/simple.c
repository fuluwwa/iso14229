#include "simple.h"
#include "../iso14229.h"
#include <stdint.h>

#define UDS_PHYS_RECV_ID 0x7A0
#define UDS_FUNC_RECV_ID 0x7A1
#define UDS_SEND_ID 0x7A8

#define ISOTP_BUFSIZE 256

static uint8_t isotpPhysRecvBuf[ISOTP_BUFSIZE];
static uint8_t isotpPhysSendBuf[ISOTP_BUFSIZE];
static uint8_t isotpFuncRecvBuf[ISOTP_BUFSIZE];
static uint8_t isotpFuncSendBuf[ISOTP_BUFSIZE];

static IsoTpLink isotpPhysLink;
static IsoTpLink isotpFuncLink;
static Iso14229Instance uds;

void hardReset() { printf("server hardReset! %u\n", iso14229UserGetms()); }

const Iso14229ServerConfig cfg = {
    .phys_recv_id = UDS_PHYS_RECV_ID,
    .func_recv_id = UDS_FUNC_RECV_ID,
    .send_id = UDS_SEND_ID,
    .phys_link = &isotpPhysLink,
    .func_link = &isotpFuncLink,
    .userRDBIHandler = NULL,
    .userWDBIHandler = NULL,
    .userHardReset = hardReset,
    .p2_ms = 50,
    .p2_star_ms = 2000,
    .s3_ms = 5000,
};

Iso14229Instance srv;

void simpleServerInit() {
    /* initialize the ISO-TP links */
    isotp_init_link(&isotpPhysLink, UDS_SEND_ID, isotpPhysSendBuf, ISOTP_BUFSIZE, isotpPhysRecvBuf,
                    ISOTP_BUFSIZE);
    isotp_init_link(&isotpFuncLink, UDS_SEND_ID, isotpFuncSendBuf, ISOTP_BUFSIZE, isotpFuncRecvBuf,
                    ISOTP_BUFSIZE);

    iso14229UserInit(&srv, &cfg);
    iso14229UserEnableService(&srv, kSID_ECU_RESET);
}

void simpleServerPeriodicTask() {
    uint32_t arb_id;
    uint8_t data[8];
    uint8_t size;

    iso14229UserPoll(&srv);
    if (0 == hostCANRxPoll(&arb_id, data, &size)) {
        iso14229UserReceiveCAN(&srv, arb_id, data, size);
    }
}
