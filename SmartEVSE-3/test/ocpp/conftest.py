"""
pytest fixtures for OCPP 1.6J compatibility testing.

Provides:
- mock_csms: A running MockCSMS instance on a random available port
- csms_url: The WebSocket URL for the mock CSMS
- charge_point: A connected ChargePointSimulator ready to send messages
- booted_charge_point: A connected + boot-accepted charge point
"""

import asyncio
import logging
import os
import sys

import pytest
import pytest_asyncio

# Ensure the test/ocpp/ directory is on sys.path so tests/ can import mock_csms etc.
_ocpp_test_dir = os.path.dirname(os.path.abspath(__file__))
if _ocpp_test_dir not in sys.path:
    sys.path.insert(0, _ocpp_test_dir)

from mock_csms import MockCSMS
from message_replay import ChargePointSimulator

# Configure logging for test debugging
logging.basicConfig(level=logging.DEBUG, format="%(asctime)s %(name)s %(levelname)s %(message)s")


@pytest_asyncio.fixture
async def mock_csms():
    """Start a mock CSMS on a random port. Stops after the test."""
    csms = MockCSMS()
    await csms.start()
    yield csms
    await csms.stop()


@pytest.fixture
def csms_url(mock_csms):
    """Return the WebSocket URL for the mock CSMS."""
    return mock_csms.url


@pytest_asyncio.fixture
async def charge_point(mock_csms):
    """
    Create and connect a ChargePointSimulator to the mock CSMS.
    Disconnects after the test.
    """
    cp = ChargePointSimulator(mock_csms.url, charge_point_id="SMARTEVSE-TEST-001")
    await cp.connect()
    await mock_csms.wait_for_connection(timeout=5.0)
    yield cp
    await cp.disconnect()


@pytest_asyncio.fixture
async def booted_charge_point(mock_csms, charge_point):
    """
    A charge point that has already completed BootNotification.
    Returns (charge_point, mock_csms) tuple for convenience.
    """
    boot_resp = await charge_point.send_boot_notification()
    assert boot_resp.msg_type == 3  # CALL_RESULT
    assert boot_resp.payload.get("status") == "Accepted"
    yield charge_point, mock_csms
