"""
WebSocket message replay client for OCPP 1.6J compatibility testing (Strategy B).

Instead of compiling MicroOcppSimulator natively, this module replays captured
OCPP message traces over WebSocket. Each trace is a sequence of OCPP messages
that a charge point would send, with expected CSMS responses that trigger
subsequent messages.

Traces can be:
1. Captured from a real SmartEVSE session (JSON files in traces/)
2. Programmatically constructed in test code

The replay client acts as a mock charge point that sends messages to the mock
CSMS and processes responses, simulating the MicroOcpp library's behavior.
"""

import asyncio
import json
import logging
import uuid
from datetime import datetime, timezone
from typing import Any

import websockets

logger = logging.getLogger(__name__)

# OCPP 1.6 message type IDs
CALL = 2
CALL_RESULT = 3
CALL_ERROR = 4

# Valid OCPP 1.6 ChargePointStatus values
VALID_CP_STATUSES = {
    "Available", "Preparing", "Charging", "SuspendedEVSE",
    "SuspendedEV", "Finishing", "Reserved", "Unavailable", "Faulted",
}

# Valid OCPP 1.6 ChargePointErrorCode values
VALID_ERROR_CODES = {
    "ConnectorLockFailure", "EVCommunicationError", "GroundFailure",
    "HighTemperature", "InternalError", "LocalListConflict",
    "NoError", "OtherError", "OverCurrentFailure", "OverVoltage",
    "PowerMeterFailure", "PowerSwitchFailure", "ReaderFailure",
    "ResetFailure", "UnderVoltage", "WeakSignal",
}

# Valid OCPP 1.6 Measurand values
VALID_MEASURANDS = {
    "Current.Export", "Current.Import", "Current.Offered",
    "Energy.Active.Export.Register", "Energy.Active.Import.Register",
    "Energy.Reactive.Export.Register", "Energy.Reactive.Import.Register",
    "Energy.Active.Export.Interval", "Energy.Active.Import.Interval",
    "Energy.Reactive.Export.Interval", "Energy.Reactive.Import.Interval",
    "Frequency", "Power.Active.Export", "Power.Active.Import",
    "Power.Factor", "Power.Offered", "Power.Reactive.Export",
    "Power.Reactive.Import", "RPM", "SoC", "Temperature", "Voltage",
}

# Valid OCPP 1.6 UnitOfMeasure values
VALID_UNITS = {
    "Wh", "kWh", "varh", "kvarh", "W", "kW", "VA", "kVA",
    "var", "kvar", "A", "V", "Celsius", "Fahrenheit", "K",
    "Percent",
}

# Valid OCPP 1.6 Phase values
VALID_PHASES = {"L1", "L2", "L3", "N", "L1-N", "L2-N", "L3-N", "L1-L2", "L2-L3", "L3-L1"}

# Valid OCPP 1.6 StopTransaction reasons
VALID_STOP_REASONS = {
    "DeAuthorized", "EmergencyStop", "EVDisconnected", "HardReset",
    "Local", "Other", "PowerLoss", "Reboot", "Remote", "SoftReset",
    "UnlockCommand",
}

# Measurand to expected unit mapping
MEASURAND_UNITS = {
    "Energy.Active.Import.Register": {"Wh", "kWh"},
    "Energy.Active.Export.Register": {"Wh", "kWh"},
    "Current.Import": {"A"},
    "Current.Export": {"A"},
    "Current.Offered": {"A"},
    "Power.Active.Import": {"W", "kW"},
    "Power.Active.Export": {"W", "kW"},
    "Voltage": {"V"},
    "Temperature": {"Celsius", "Fahrenheit", "K"},
    "SoC": {"Percent"},
    "Frequency": set(),  # Hz not in OCPP 1.6 UoM list
}


class OcppMessage:
    """Represents a single OCPP 1.6J message."""

    def __init__(self, action: str, payload: dict, msg_type: int = CALL):
        self.action = action
        self.payload = payload
        self.msg_type = msg_type
        self.unique_id = str(uuid.uuid4())[:8]

    def to_json(self) -> str:
        if self.msg_type == CALL:
            return json.dumps([CALL, self.unique_id, self.action, self.payload])
        elif self.msg_type == CALL_RESULT:
            return json.dumps([CALL_RESULT, self.unique_id, self.payload])
        elif self.msg_type == CALL_ERROR:
            code = self.payload.get("code", "InternalError")
            desc = self.payload.get("description", "")
            details = self.payload.get("details", {})
            return json.dumps([CALL_ERROR, self.unique_id, code, desc, details])
        raise ValueError(f"Unknown message type: {self.msg_type}")

    @staticmethod
    def from_json(raw: str) -> "OcppMessage":
        data = json.loads(raw)
        msg_type = data[0]
        if msg_type == CALL:
            return OcppMessage(action=data[2], payload=data[3], msg_type=CALL)
        elif msg_type == CALL_RESULT:
            msg = OcppMessage(action="", payload=data[2], msg_type=CALL_RESULT)
            msg.unique_id = data[1]
            return msg
        elif msg_type == CALL_ERROR:
            msg = OcppMessage(
                action="",
                payload={"code": data[2], "description": data[3], "details": data[4] if len(data) > 4 else {}},
                msg_type=CALL_ERROR,
            )
            msg.unique_id = data[1]
            return msg
        raise ValueError(f"Unknown OCPP message type: {msg_type}")


class ChargePointSimulator:
    """
    Simulates a charge point by sending OCPP 1.6J messages over WebSocket.

    Supports both trace replay (from JSON files) and programmatic message
    construction. Handles the Call/CallResult protocol flow.
    """

    def __init__(self, csms_url: str, charge_point_id: str = "SMARTEVSE-TEST-001"):
        self.csms_url = csms_url.rstrip("/")
        self.charge_point_id = charge_point_id
        self._ws = None
        self._pending_calls: dict[str, OcppMessage] = {}
        self.received_responses: list[tuple[str, Any]] = []
        self.received_calls: list[tuple[str, dict]] = []
        self._response_handlers: dict[str, asyncio.Future] = {}
        self._listen_task = None
        self._transaction_id: int | None = None
        self._meter_start: int = 0

    async def connect(self):
        """Connect to the mock CSMS via WebSocket with ocpp1.6 subprotocol."""
        url = f"{self.csms_url}/ocpp/{self.charge_point_id}"
        self._ws = await websockets.connect(
            url,
            subprotocols=["ocpp1.6"],
        )
        # Start background listener for CSMS-initiated messages
        self._listen_task = asyncio.create_task(self._listen())
        logger.info("Connected to CSMS at %s as %s", url, self.charge_point_id)

    async def disconnect(self):
        """Disconnect from the CSMS."""
        if self._listen_task:
            self._listen_task.cancel()
            try:
                await self._listen_task
            except asyncio.CancelledError:
                pass
        if self._ws:
            await self._ws.close()
            self._ws = None

    async def _listen(self):
        """Background task to listen for CSMS-initiated messages."""
        try:
            async for raw in self._ws:
                msg = OcppMessage.from_json(raw)
                if msg.msg_type == CALL_RESULT:
                    # Response to our Call
                    if msg.unique_id in self._response_handlers:
                        self._response_handlers[msg.unique_id].set_result(msg)
                    self.received_responses.append((msg.unique_id, msg.payload))
                elif msg.msg_type == CALL_ERROR:
                    if msg.unique_id in self._response_handlers:
                        self._response_handlers[msg.unique_id].set_result(msg)
                    self.received_responses.append((msg.unique_id, msg.payload))
                elif msg.msg_type == CALL:
                    # CSMS-initiated Call (RemoteStart, SetChargingProfile, etc.)
                    self.received_calls.append((msg.action, msg.payload))
                    await self._handle_csms_call(msg)
        except websockets.exceptions.ConnectionClosed:
            logger.info("WebSocket connection closed")
        except asyncio.CancelledError:
            pass

    async def _handle_csms_call(self, msg: OcppMessage):
        """Handle CSMS-initiated Call messages with default responses."""
        action = msg.action
        response_payload = {}

        if action == "RemoteStartTransaction":
            # Accept if we have a handler, otherwise accept by default
            response_payload = {"status": "Accepted"}
        elif action == "RemoteStopTransaction":
            response_payload = {"status": "Accepted"}
        elif action == "SetChargingProfile":
            response_payload = {"status": "Accepted"}
        elif action == "ClearChargingProfile":
            response_payload = {"status": "Accepted"}
        elif action == "GetCompositeSchedule":
            response_payload = {"status": "Accepted"}
        elif action == "ChangeConfiguration":
            response_payload = {"status": "Accepted"}
        elif action == "GetConfiguration":
            response_payload = {"configurationKey": [], "unknownKey": []}
        else:
            # Unknown action -> CALLERROR NotImplemented
            error = json.dumps([CALL_ERROR, msg.unique_id, "NotImplemented",
                                f"Action {action} not supported", {}])
            await self._ws.send(error)
            return

        response = json.dumps([CALL_RESULT, msg.unique_id, response_payload])
        await self._ws.send(response)

    async def send_call(self, action: str, payload: dict, timeout: float = 10.0) -> OcppMessage:
        """Send an OCPP Call and wait for the response."""
        msg = OcppMessage(action=action, payload=payload)
        self._pending_calls[msg.unique_id] = msg

        future = asyncio.get_event_loop().create_future()
        self._response_handlers[msg.unique_id] = future

        await self._ws.send(msg.to_json())
        logger.debug("Sent %s: %s", action, payload)

        response = await asyncio.wait_for(future, timeout=timeout)
        del self._response_handlers[msg.unique_id]
        return response

    # Convenience methods for common OCPP 1.6 charge point messages

    async def send_boot_notification(
        self,
        vendor: str = "SmartEVSE",
        model: str = "SmartEVSE-3",
        serial_number: str = "SMARTEVSE-TEST-001",
        firmware_version: str = "v3.7.0",
    ) -> OcppMessage:
        """Send BootNotification to the CSMS."""
        payload = {
            "chargePointVendor": vendor,
            "chargePointModel": model,
            "chargePointSerialNumber": serial_number,
            "firmwareVersion": firmware_version,
        }
        return await self.send_call("BootNotification", payload)

    async def send_heartbeat(self) -> OcppMessage:
        """Send Heartbeat to the CSMS."""
        return await self.send_call("Heartbeat", {})

    async def send_authorize(self, id_tag: str) -> OcppMessage:
        """Send Authorize to the CSMS."""
        return await self.send_call("Authorize", {"idTag": id_tag})

    async def send_start_transaction(
        self,
        connector_id: int = 1,
        id_tag: str = "AB12CD34",
        meter_start: int = 0,
    ) -> OcppMessage:
        """Send StartTransaction to the CSMS."""
        self._meter_start = meter_start
        payload = {
            "connectorId": connector_id,
            "idTag": id_tag,
            "meterStart": meter_start,
            "timestamp": datetime.now(timezone.utc).isoformat(),
        }
        response = await self.send_call("StartTransaction", payload)
        if response.msg_type == CALL_RESULT:
            self._transaction_id = response.payload.get("transactionId")
        return response

    async def send_stop_transaction(
        self,
        meter_stop: int = 1000,
        reason: str = "Local",
        transaction_id: int | None = None,
    ) -> OcppMessage:
        """Send StopTransaction to the CSMS."""
        tx_id = transaction_id if transaction_id is not None else self._transaction_id
        payload = {
            "transactionId": tx_id,
            "meterStop": meter_stop,
            "timestamp": datetime.now(timezone.utc).isoformat(),
            "reason": reason,
        }
        response = await self.send_call("StopTransaction", payload)
        self._transaction_id = None
        return response

    async def send_meter_values(
        self,
        connector_id: int = 1,
        transaction_id: int | None = None,
        energy_wh: int = 500,
        current_a: float = 16.0,
        voltage_v: float = 230.0,
        power_w: float = 3680.0,
        phases: int = 1,
    ) -> OcppMessage:
        """Send MeterValues with standard measurands."""
        tx_id = transaction_id if transaction_id is not None else self._transaction_id
        sampled_values = [
            {
                "measurand": "Energy.Active.Import.Register",
                "value": str(energy_wh),
                "unit": "Wh",
            },
            {
                "measurand": "Power.Active.Import",
                "value": str(power_w),
                "unit": "W",
            },
        ]

        # Add per-phase current
        phase_names = ["L1", "L2", "L3"]
        for i in range(min(phases, 3)):
            sampled_values.append({
                "measurand": "Current.Import",
                "value": str(current_a),
                "unit": "A",
                "phase": phase_names[i],
            })

        # Add per-phase voltage
        for i in range(min(phases, 3)):
            sampled_values.append({
                "measurand": "Voltage",
                "value": str(voltage_v),
                "unit": "V",
                "phase": phase_names[i],
            })

        payload = {
            "connectorId": connector_id,
            "meterValue": [
                {
                    "timestamp": datetime.now(timezone.utc).isoformat(),
                    "sampledValue": sampled_values,
                }
            ],
        }
        if tx_id is not None:
            payload["transactionId"] = tx_id

        return await self.send_call("MeterValues", payload)

    async def send_status_notification(
        self,
        connector_id: int = 1,
        status: str = "Available",
        error_code: str = "NoError",
    ) -> OcppMessage:
        """Send StatusNotification to the CSMS."""
        payload = {
            "connectorId": connector_id,
            "errorCode": error_code,
            "status": status,
            "timestamp": datetime.now(timezone.utc).isoformat(),
        }
        return await self.send_call("StatusNotification", payload)

    async def send_data_transfer(self, vendor_id: str, **kwargs) -> OcppMessage:
        """Send DataTransfer to the CSMS."""
        payload = {"vendorId": vendor_id, **kwargs}
        return await self.send_call("DataTransfer", payload)

    async def run_full_session(
        self,
        id_tag: str = "AB12CD34",
        energy_wh: int = 5000,
        meter_intervals: int = 3,
        phases: int = 3,
    ) -> dict:
        """
        Run a complete charge session: boot -> auth -> start -> meter -> stop.

        Returns a summary dict with all message responses for assertion.
        """
        results = {}

        # Boot
        boot_resp = await self.send_boot_notification()
        results["boot"] = boot_resp

        # Status: Available
        await self.send_status_notification(status="Available")

        # Authorize
        auth_resp = await self.send_authorize(id_tag)
        results["authorize"] = auth_resp

        if auth_resp.msg_type == CALL_RESULT and auth_resp.payload.get("idTagInfo", {}).get("status") == "Accepted":
            # Status: Preparing
            await self.send_status_notification(status="Preparing")

            # Start transaction
            start_resp = await self.send_start_transaction(id_tag=id_tag, meter_start=0)
            results["start_transaction"] = start_resp

            if start_resp.msg_type == CALL_RESULT:
                tx_id = start_resp.payload.get("transactionId")

                # Status: Charging
                await self.send_status_notification(status="Charging")

                # Send meter values
                meter_responses = []
                for i in range(meter_intervals):
                    energy = (energy_wh * (i + 1)) // meter_intervals
                    resp = await self.send_meter_values(
                        energy_wh=energy,
                        phases=phases,
                        transaction_id=tx_id,
                    )
                    meter_responses.append(resp)
                results["meter_values"] = meter_responses

                # Status: Finishing
                await self.send_status_notification(status="Finishing")

                # Stop transaction
                stop_resp = await self.send_stop_transaction(
                    meter_stop=energy_wh,
                    reason="Local",
                    transaction_id=tx_id,
                )
                results["stop_transaction"] = stop_resp

            # Status: Available
            await self.send_status_notification(status="Available")

        return results


def load_trace(trace_path: str) -> list[dict]:
    """Load an OCPP message trace from a JSON file."""
    with open(trace_path) as f:
        return json.load(f)
