#!/usr/bin/env python3

import pytest
import time
import os
import sys
import udsoncan
from udsoncan.client import Client
from udsoncan.connections import PythonIsoTpConnection
from udsoncan.services import *
from ctypes import *


def test_ecu_reset(client, iso14229):
    client.ecu_reset(ECUReset.ResetType.hardReset)
    iso14229.assertCFuncCalled("mockSystemReset")


# @pytest.mark.parametrize("srvcfg", [
#     pytest.param(("boot", {}))]
# )
# def test_erase_flash_routine(client, iso14229):
#     # client.change_session(DiagnosticSessionControl.Session.extendedDiagnosticSession)
#     client.routine_control(
#         routine_id=0xff00,
#         control_type=RoutineControl.ControlType.startRoutine,
#         data=bytes([0, 0, 0xf0, 0, 0, 0x1, 0x60, 0x14])
#     )
#     iso14229.assertCFuncCalled("mockEraseProgramFlash")


# @pytest.mark.parametrize("srvcfg", [
#     pytest.param(("boot", {"app_download.buffer_size": 8196 }), id="sim_huada_FLASH"),
#     pytest.param(("boot", {"app_download.buffer_size": 2048 }), id="sim_nxp_s32k_FLASH"),
# ])
# def test_transfer_data(client, iso14229):
#     mock_upload = bytes([i & 0xff for i in range(20000)])
    
#     response = client.request_download(
#         udsoncan.MemoryLocation(0xf000, len(mock_upload),address_format=32, memorysize_format=32)
#     )

#     block_length = response.service_data.max_length - 4

#     chunked_data = [mock_upload[i:i+block_length] for i in range(0, len(mock_upload), block_length)]

#     for i, chunk in enumerate(chunked_data):
#         client.transfer_data((i+1) & 0xff, chunk)

#     mock_flash = (c_uint8 * len(mock_upload)).in_dll(iso14229.lib, "mock_flash")

#     for i, n in enumerate(mock_upload):
#         if (mock_upload[i] != mock_flash[i]):
#             with open('transfer.bin', 'wb') as transfer:
#                 transfer.write(mock_upload)
#             with open('memory.bin', 'wb') as memory:
#                 memory.write(mock_flash)
#             assert mock_upload[i] == mock_flash[i] , f"failed at byte {i}"

def test_rdbi_single_did(log, client, iso14229):
    vals = client.read_data_by_identifier(didlist=[0x0003]).service_data.values
    print(vals)
    assert vals[0x0003] == (3,)

def test_rdbi_multiple_did(log, client, iso14229):
    vals = client.read_data_by_identifier(didlist=[
        0x0000,
        0x0001,
        0x0002,
        0x0003,
        0x0004,
        0x0005,
        0x0006,
        0x0007,
        0x0008,
    ]).service_data.values

    assert vals[0x0000] == (0,)
    assert vals[0x0001] == (1,)
    assert vals[0x0002] == (2,)
    assert vals[0x0003] == (3,)
    assert vals[0x0004] == (4,)
    assert vals[0x0005] == (5,)
    assert vals[0x0006] == (6,)
    assert vals[0x0007] == (7,)
    assert vals[0x0008] == (1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20)


if __name__ == "__main__":
    sys.exit(pytest.main([__file__]))