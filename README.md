# iso14229

iso14229 is a UDS server implementation (ISO14229-1:2013) targeting embedded systems

**Stability: Experimental**

## Basic Usage


```c
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

void hardReset() {
    printf("server hardReset! %u\n", iso14229UserGetms());
}

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
    isotp_init_link(&isotpPhysLink, UDS_SEND_ID, isotpPhysSendBuf, ISOTP_BUFSIZE, isotpPhysRecvBuf, ISOTP_BUFSIZE);
    isotp_init_link(&isotpFuncLink, UDS_SEND_ID, isotpFuncSendBuf, ISOTP_BUFSIZE, isotpFuncRecvBuf, ISOTP_BUFSIZE);

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

```

## Example (linux, no additional hardware required)

See [example](/example) for a simple server with socketCAN bindings

```sh
# setup a virtual socketCAN interface
sudo ip link add name can9 type vcan
sudo ip link set can9 up

# build the example server
make example/linux

# run the example server on can9
./example/linux can9
```

```sh
# In another shell, install the required python packages
pip3 install -r requirements.txt

# then run the client
./example/client.py can9
```

```sh
# (Optional) In a third shell, monitor the virtual link
candump can9
```


## Custom Service Handlers

| Service | `iso14229` Function |
| - | - |
| 0x31 RoutineControl | `int iso14229UserRegisterRoutine(Iso14229Instance* self, const Iso14229Routine *routine);` |
| 0x34 RequestDownload, 0x36 TransferData, 0x37 RequestTransferExit | `int iso14229UserRegisterDownloadHandler(Iso14229Instance* self, Iso14229DownloadHandlerConfig *handler);` |

## Application / Boot Software (Middleware)



## Contributing

I'll gladly review and work towards merging contributions.
(including but not limited to:)

- bugfixes
- implementing standard behavior
- implementing optional or manufacturer-specific extensions easy to add
- API improvements

see: [HACKING](./HACKING.md)

# Thank

This project would not be possible without:

- [`isotp`](https://github.com/lishen2/isotp-c) which this project embeds
- [`python-udsoncan`](https://github.com/pylessard/python-udsoncan)
- [`python-can-isotp`](https://github.com/pylessard/python-can-isotp)
- [`python-can`](https://github.com/hardbyte/python-can)

# License

MIT