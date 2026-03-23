"""
Configurable mock Central System (CSMS) for OCPP 1.6J compatibility testing.

Uses the mobilityhouse/ocpp library to handle incoming OCPP messages from a
charge point (real or simulated). The CSMS behavior is fully configurable:
response overrides, error injection, message delays, and connection control.

This module is used by conftest.py fixtures and by individual test modules.
"""

import asyncio
import logging
from datetime import datetime, timezone
from typing import Any

from ocpp.routing import on
from ocpp.v16 import ChargePoint as CpHandler
from ocpp.v16 import call, call_result
from ocpp.v16.enums import (
    Action,
    AuthorizationStatus,
    ChargePointErrorCode,
    ChargePointStatus,
    RegistrationStatus,
)

logger = logging.getLogger(__name__)


class MockChargePointHandler(CpHandler):
    """
    Handles OCPP 1.6J messages from a single charge point connection.

    All @on handlers log received messages and check for configured overrides
    before returning responses. This allows tests to:
    - Inspect what the charge point sent (via received_messages)
    - Control what the CSMS responds (via response_overrides / inject_errors)
    - Simulate slow/dropped responses (via message_delay / drop_messages)
    """

    def __init__(self, charge_point_id: str, connection, parent_csms):
        super().__init__(charge_point_id, connection)
        self._parent = parent_csms
        self.received_messages: list[tuple[str, dict]] = []

    def _get_override(self, action: str) -> dict | None:
        return self._parent.response_overrides.get(action)

    def _get_error(self, action: str) -> Exception | None:
        return self._parent.inject_errors.get(action)

    def _get_delay(self) -> float:
        return self._parent.message_delay

    def _should_drop(self, action: str) -> bool:
        return action in self._parent.drop_messages

    async def _maybe_delay(self):
        delay = self._get_delay()
        if delay > 0:
            await asyncio.sleep(delay)

    @on(Action.boot_notification)
    async def on_boot_notification(self, charge_point_model, charge_point_vendor, **kwargs):
        self.received_messages.append(("BootNotification", {
            "charge_point_model": charge_point_model,
            "charge_point_vendor": charge_point_vendor,
            **kwargs,
        }))
        await self._maybe_delay()

        err = self._get_error("BootNotification")
        if err:
            raise err

        override = self._get_override("BootNotification") or {}
        status = override.get("status", RegistrationStatus.accepted)
        interval = override.get("interval", 300)

        return call_result.BootNotification(
            current_time=datetime.now(timezone.utc).isoformat(),
            interval=interval,
            status=status,
        )

    @on(Action.heartbeat)
    async def on_heartbeat(self, **kwargs):
        self.received_messages.append(("Heartbeat", kwargs))
        await self._maybe_delay()
        return call_result.Heartbeat(
            current_time=datetime.now(timezone.utc).isoformat(),
        )

    @on(Action.authorize)
    async def on_authorize(self, id_tag, **kwargs):
        self.received_messages.append(("Authorize", {"id_tag": id_tag, **kwargs}))
        await self._maybe_delay()

        err = self._get_error("Authorize")
        if err:
            raise err

        override = self._get_override("Authorize") or {}
        status = override.get("status", AuthorizationStatus.accepted)

        return call_result.Authorize(
            id_tag_info={"status": status},
        )

    @on(Action.start_transaction)
    async def on_start_transaction(self, connector_id, id_tag, meter_start, timestamp, **kwargs):
        self.received_messages.append(("StartTransaction", {
            "connector_id": connector_id,
            "id_tag": id_tag,
            "meter_start": meter_start,
            "timestamp": timestamp,
            **kwargs,
        }))
        await self._maybe_delay()

        err = self._get_error("StartTransaction")
        if err:
            raise err

        override = self._get_override("StartTransaction") or {}
        tx_id = override.get("transaction_id", 1)
        status = override.get("status", AuthorizationStatus.accepted)

        return call_result.StartTransaction(
            transaction_id=tx_id,
            id_tag_info={"status": status},
        )

    @on(Action.stop_transaction)
    async def on_stop_transaction(self, meter_stop, timestamp, transaction_id, **kwargs):
        self.received_messages.append(("StopTransaction", {
            "meter_stop": meter_stop,
            "timestamp": timestamp,
            "transaction_id": transaction_id,
            **kwargs,
        }))
        await self._maybe_delay()

        err = self._get_error("StopTransaction")
        if err:
            raise err

        override = self._get_override("StopTransaction") or {}
        status = override.get("status", AuthorizationStatus.accepted)

        return call_result.StopTransaction(
            id_tag_info={"status": status},
        )

    @on(Action.meter_values)
    async def on_meter_values(self, connector_id, meter_value, **kwargs):
        self.received_messages.append(("MeterValues", {
            "connector_id": connector_id,
            "meter_value": meter_value,
            **kwargs,
        }))
        await self._maybe_delay()
        return call_result.MeterValues()

    @on(Action.status_notification)
    async def on_status_notification(self, connector_id, error_code, status, **kwargs):
        self.received_messages.append(("StatusNotification", {
            "connector_id": connector_id,
            "error_code": error_code,
            "status": status,
            **kwargs,
        }))
        await self._maybe_delay()
        return call_result.StatusNotification()

    @on(Action.data_transfer)
    async def on_data_transfer(self, vendor_id, **kwargs):
        self.received_messages.append(("DataTransfer", {
            "vendor_id": vendor_id,
            **kwargs,
        }))
        await self._maybe_delay()
        return call_result.DataTransfer(status="Accepted")

    @on(Action.diagnostics_status_notification)
    async def on_diagnostics_status_notification(self, status, **kwargs):
        self.received_messages.append(("DiagnosticsStatusNotification", {
            "status": status,
            **kwargs,
        }))
        return call_result.DiagnosticsStatusNotification()

    @on(Action.firmware_status_notification)
    async def on_firmware_status_notification(self, status, **kwargs):
        self.received_messages.append(("FirmwareStatusNotification", {
            "status": status,
            **kwargs,
        }))
        return call_result.FirmwareStatusNotification()


class MockCSMS:
    """
    A configurable mock OCPP 1.6J Central System.

    Manages a WebSocket server that accepts charge point connections using the
    ocpp1.6 subprotocol. Each connection gets a MockChargePointHandler that
    logs messages and applies configured response behaviors.

    Usage in tests:
        csms = MockCSMS()
        csms.response_overrides["Authorize"] = {"status": "Blocked"}
        await csms.start()
        # ... connect charge point ...
        assert csms.handler.received_messages[0][0] == "BootNotification"
        await csms.stop()
    """

    def __init__(self):
        self.response_overrides: dict[str, dict] = {}
        self.inject_errors: dict[str, Exception] = {}
        self.message_delay: float = 0
        self.drop_messages: list[str] = []
        self.close_after: int | None = None
        self.handler: MockChargePointHandler | None = None
        self._server = None
        self._host = "127.0.0.1"
        self._port = 0  # OS assigns a free port
        self._connection_event = asyncio.Event()
        self._message_count = 0
        self._close_task = None

    @property
    def port(self) -> int:
        if self._server is None:
            raise RuntimeError("CSMS not started yet")
        return self._server.sockets[0].getsockname()[1]

    @property
    def url(self) -> str:
        return f"ws://{self._host}:{self.port}"

    @property
    def received_messages(self) -> list[tuple[str, dict]]:
        if self.handler is None:
            return []
        return self.handler.received_messages

    async def _on_connect(self, websocket):
        """Handle new charge point WebSocket connection."""
        # Extract charge point ID from path (e.g., /ocpp/CP001 -> CP001)
        # websockets v12+ uses websocket.request.path;
        # websockets v13+ may use websocket.path
        path = ""
        if hasattr(websocket, "request") and websocket.request is not None:
            path = getattr(websocket.request, "path", "")
        elif hasattr(websocket, "path"):
            path = websocket.path or ""
        cp_id = path.strip("/").split("/")[-1] if path else "unknown"
        logger.info("Charge point connected: %s", cp_id)

        self.handler = MockChargePointHandler(cp_id, websocket, self)
        self._connection_event.set()

        try:
            await self.handler.start()
        except Exception as e:
            logger.info("Connection closed: %s", e)

    async def start(self):
        """Start the mock CSMS WebSocket server."""
        import websockets

        # websockets v13+ changed the serve API; v12 uses the legacy API
        try:
            # Try websockets v12+ (legacy) / v13+ compatible approach
            self._server = await websockets.serve(
                self._on_connect,
                self._host,
                self._port,
                subprotocols=["ocpp1.6"],
            )
        except TypeError:
            # Fallback for API differences
            self._server = await websockets.serve(
                self._on_connect,
                host=self._host,
                port=self._port,
                subprotocols=["ocpp1.6"],
            )
        logger.info("Mock CSMS started on %s", self.url)

    async def stop(self):
        """Stop the mock CSMS."""
        if self._server:
            self._server.close()
            await self._server.wait_closed()
            self._server = None
        self.handler = None
        self._connection_event.clear()

    async def wait_for_connection(self, timeout: float = 10.0):
        """Wait for a charge point to connect."""
        await asyncio.wait_for(self._connection_event.wait(), timeout=timeout)

    async def wait_for_message(self, action: str, timeout: float = 10.0) -> dict:
        """Wait until a specific message type is received."""
        deadline = asyncio.get_event_loop().time() + timeout
        while asyncio.get_event_loop().time() < deadline:
            for msg_action, msg_data in self.received_messages:
                if msg_action == action:
                    return msg_data
            await asyncio.sleep(0.1)
        raise TimeoutError(f"Timed out waiting for {action} message")

    async def wait_for_message_count(self, action: str, count: int, timeout: float = 10.0) -> list[dict]:
        """Wait until N messages of a specific type are received."""
        deadline = asyncio.get_event_loop().time() + timeout
        while asyncio.get_event_loop().time() < deadline:
            matches = [d for a, d in self.received_messages if a == action]
            if len(matches) >= count:
                return matches[:count]
            await asyncio.sleep(0.1)
        found = len([d for a, d in self.received_messages if a == action])
        raise TimeoutError(f"Timed out waiting for {count} {action} messages (got {found})")

    async def send_remote_start(self, id_tag: str, connector_id: int = 1):
        """Send RemoteStartTransaction to the connected charge point."""
        if self.handler is None:
            raise RuntimeError("No charge point connected")
        request = call.RemoteStartTransaction(id_tag=id_tag, connector_id=connector_id)
        return await self.handler.call(request)

    async def send_remote_stop(self, transaction_id: int):
        """Send RemoteStopTransaction to the connected charge point."""
        if self.handler is None:
            raise RuntimeError("No charge point connected")
        request = call.RemoteStopTransaction(transaction_id=transaction_id)
        return await self.handler.call(request)

    async def send_set_charging_profile(self, connector_id: int, profile: dict):
        """Send SetChargingProfile to the connected charge point."""
        if self.handler is None:
            raise RuntimeError("No charge point connected")
        request = call.SetChargingProfile(
            connector_id=connector_id,
            cs_charging_profiles=profile,
        )
        return await self.handler.call(request)

    async def send_clear_charging_profile(self, **kwargs):
        """Send ClearChargingProfile to the connected charge point."""
        if self.handler is None:
            raise RuntimeError("No charge point connected")
        # ClearChargingProfile accepts optional: id, connector_id,
        # charging_profile_purpose, stack_level
        request = call.ClearChargingProfile(**kwargs)
        return await self.handler.call(request)

    async def send_get_composite_schedule(self, connector_id: int, duration: int):
        """Send GetCompositeSchedule to the connected charge point."""
        if self.handler is None:
            raise RuntimeError("No charge point connected")
        request = call.GetCompositeSchedule(
            connector_id=connector_id,
            duration=duration,
        )
        return await self.handler.call(request)

    async def send_change_configuration(self, key: str, value: str):
        """Send ChangeConfiguration to the connected charge point."""
        if self.handler is None:
            raise RuntimeError("No charge point connected")
        request = call.ChangeConfiguration(key=key, value=value)
        return await self.handler.call(request)

    async def send_raw_message(self, message: str):
        """Send a raw WebSocket message (for error injection tests)."""
        if self.handler is None:
            raise RuntimeError("No charge point connected")
        await self.handler._connection.send(message)

    async def close_connection(self):
        """Forcibly close the WebSocket connection to the charge point."""
        if self.handler is not None:
            await self.handler._connection.close()
