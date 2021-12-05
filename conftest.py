import pytest
import time
import os
import sys
import udsoncan
from udsoncan.client import Client
from udsoncan.connections import PythonIsoTpConnection
from udsoncan.services import *

import isotp
from can.interfaces.virtual import VirtualBus
from can import Message
from ctypes import *
import threading

def pytest_ignore_collect(path, config):
    if path.isfile() and path.islink():
        return True

def security_algo(level, seed, params):
    print(f"level: {level}, seed: {seed}, params: {params}")
    return bytes([0])

UDSONCAN_CLIENT_CONFIG = {
        'exception_on_negative_response'	: True,	
        'exception_on_invalid_response'		: True,
        'exception_on_unexpected_response'	: True,
        'security_algo'				: security_algo,
        'security_algo_params'		: None,
        'tolerate_zero_padding' 	: True,
        'ignore_all_zero_dtc' 		: True,
        'dtc_snapshot_did_size' 	: 2,		# Not specified in standard. 2 bytes matches other services format.
        'server_address_format'		: None,		# 8,16,24,32,40
        'server_memorysize_format'	: None,		# 8,16,24,32,40

        # These follow a fixed layout defined in `test_iso14229_harness.c`
        'data_identifiers' 		:  {
            0x0000: "B",
            0x0001: "b",
            0x0002: "H",
            0x0003: "h",
            0x0004: "I",
            0x0005: "i",
            0x0006: "Q",
            0x0007: "q",
            0x0008: "20B",
        },
        'input_output' 			: {},
        'request_timeout'		: 5,
        'p2_timeout'			: 1.5, 
        'p2_star_timeout'		: 5,
        'standard_version'              : 2013,  # 2006, 2013, 2020
        'use_server_timing'             : False
}


@pytest.fixture
def log(request):
    """ log CAN traffic to a pcap file so we can make use of Wireshark's UDS dissector. """

    # This fixture is only useful for debugging failed tests, so don't bother adding scapy 
    # as a dependency. Just pass if it's not installed.
    try:
        from scapy.layers.can import CAN
        from scapy.utils import wrpcap
    except ImportError:
        yield []
        return

    packets = []
    should_exit = threading.Event()

    def read_task():
        bus = VirtualBus(channel=1)
        while not should_exit.is_set():
            msg = bus.recv(timeout=0.01)
            if msg:
                pkt = CAN(
                        identifier=msg.arbitration_id,
                        data=msg.data,
                        length=msg.dlc)
                if msg.is_extended_id:
                    pkt.flags = 0b100
                pkt.length = len(pkt.data)
                pkt.time = msg.timestamp
                packets.append(pkt)

    thread = threading.Thread(target=read_task)
    thread.start()
    yield thread
    should_exit.set()
    wrpcap(f"{request.node.name}.pcap", packets)
    thread.join()

@pytest.fixture
def client():
    client = Client(
        conn=PythonIsoTpConnection(
            isotp_layer=isotp.CanStack(
                bus=VirtualBus(channel=1),
                address=isotp.Address(rxid=0x7A8, txid=0x7A0),
                params = {
                    "tx_padding": 0xAA,
                    "tx_data_min_length": 8,
                    "blocksize": 0,
                    "squash_stmin_requirement": True
                }
            )
        ),
        config=UDSONCAN_CLIENT_CONFIG,
    )
    client.open()
    yield client
    client.close()

class Iso14229TestHarness:
    """ Python wrapper for test_iso14229_harness.so """

    def __init__(self):
        lib = CDLL("./test_iso14229_harness.so")

        # Setup function types
        SendCANCallbackFUNCTYPE = CFUNCTYPE(c_uint32, c_uint32, POINTER(c_uint8), c_uint8)
        lib.harnessSetSendCANCallback.argtypes = [SendCANCallbackFUNCTYPE]
        lib.harnessRecvCAN.argtypes = [c_uint32, POINTER(c_uint8), c_uint8]
        lib.harnessPoll.argtypes = [c_uint32]
        lib.harnessInit.restype = c_int
        # lib.harnessConfigure.argtypes = [c_uint8, c_uint32]

        # This callback function must be attached to self to avoid being garbage collected
        # when the fixture setup function exits
        self.sccb = SendCANCallbackFUNCTYPE(self.sendCAN)

        lib.harnessSetSendCANCallback(self.sccb)

        self.bus = VirtualBus(channel=1)
        self.lib = lib
        self.should_exit = threading.Event()
        self.recv_thread = threading.Thread(target=self.recv_task)
        self.poll_thread = threading.Thread(target=self.poll_task)

    def sendCAN(self, arb_id, data, size):
        msg = Message(
            timestamp=time.time(),
            channel=1,
            arbitration_id=arb_id, 
            is_extended_id=False, 
            data=[data[i] for i in range(size)])
        self.bus.send(msg)
        return 0

    def recv_task(self):
        static_data = (c_uint8 * 8)()
        while not self.should_exit.is_set():
            msg = self.bus.recv(timeout=0.01)
            if msg:
                for i, x in enumerate(msg.data):
                    static_data[i] = x
                self.lib.harnessRecvCAN(msg.arbitration_id, cast(static_data, POINTER(c_uint8)), len(msg.data))

    def poll_task(self):
        start_time = time.time()
        while not self.should_exit.is_set():
            time_now_ms = int((time.time() - start_time) * 1000)
            self.lib.harnessPoll(time_now_ms)
            time.sleep(0.01)

    def __enter__(self):
        self.recv_thread.start()
        self.poll_thread.start()

    def __exit__(self, a, b, c):
        self.should_exit.set()
        self.recv_thread.join()
        self.poll_thread.join()

    def assertCFuncCalled(self, fn_name: str, timeout_ms=2000):
        while True:
            if c_uint32.in_dll(self.lib, "g_" + fn_name + "CallCount").value > 0:
                return
            if c_uint32.in_dll(self.lib, "g_mock_ms").value > 2000:
                raise pytest.TimeoutException()
            time.sleep(0.01)


@pytest.fixture
def iso14229(request):
    harness = Iso14229TestHarness()
    assert 0 == harness.lib.harnessInit()
    with harness:
        yield harness