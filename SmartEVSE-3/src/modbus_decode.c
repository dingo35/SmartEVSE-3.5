/*
 * modbus_decode.c - Pure C Modbus frame parser
 *
 * Extracted from modbus.cpp ModbusDecode() for native testability.
 * No platform dependencies.
 */

#include "modbus_decode.h"
#include <string.h>

void modbus_frame_init(modbus_frame_t *frame)
{
    if (!frame) return;
    memset(frame, 0, sizeof(*frame));
    frame->Type = MODBUS_INVALID;
}

void modbus_decode(modbus_frame_t *frame, const uint8_t *buf, uint8_t len)
{
    if (!frame || !buf) return;

    /* Clear old values */
    frame->Address = 0;
    frame->Function = 0;
    frame->Register = 0;
    frame->RegisterCount = 0;
    frame->Value = 0;
    frame->DataLength = 0;
    frame->Type = MODBUS_INVALID;
    frame->Exception = 0;
    frame->Data = NULL;

    /* Modbus exception packets: 3 bytes (address + function|0x80 + exception code) */
    if (len == 3) {
        frame->Type = MODBUS_EXCEPTION;
        frame->Address = buf[0];
        frame->Function = buf[1];
        frame->Exception = buf[2];
        return;
    }

    /* Minimum data packet: 5 bytes (address + function + at least 3 payload bytes) */
    if (len < 5) return;

    frame->Address = buf[0];
    frame->Function = buf[1];

    switch (frame->Function) {
        case 0x03: /* Read holding register */
        case 0x04: /* Read input register */
            if (len == 6) {
                /* Request packet: addr + func + reg(2) + count(2) */
                frame->Type = MODBUS_REQUEST;
                frame->Register = (uint16_t)(buf[2] << 8) | buf[3];
                frame->RegisterCount = (uint16_t)(buf[4] << 8) | buf[5];
            } else {
                /* Response packet: addr + func + bytecount + data... */
                frame->DataLength = buf[2];
                if (frame->DataLength == len - 3) {
                    frame->Type = MODBUS_RESPONSE;
                }
                /* else: invalid packet length, Type stays MODBUS_INVALID */
            }
            break;

        case 0x06: /* Write single register */
            if (len == 6) {
                /* Request and response have same format */
                frame->Type = MODBUS_OK;
                frame->Register = (uint16_t)(buf[2] << 8) | buf[3];
                frame->RegisterCount = 1;
                frame->Value = (uint16_t)(buf[4] << 8) | buf[5];
            }
            /* else: invalid, Type stays MODBUS_INVALID */
            break;

        case 0x10: /* Write multiple registers */
            frame->Register = (uint16_t)(buf[2] << 8) | buf[3];
            frame->RegisterCount = (uint16_t)(buf[4] << 8) | buf[5];
            if (len == 6) {
                /* Response packet: addr + func + reg(2) + count(2) */
                frame->Type = MODBUS_RESPONSE;
            } else if (len >= 7) {
                /* Request packet: addr + func + reg(2) + count(2) + bytecount + data */
                frame->DataLength = buf[6];
                if (frame->DataLength == len - 7) {
                    frame->Type = MODBUS_REQUEST;
                }
                /* else: invalid */
            }
            break;

        default:
            /* Unknown function code */
            break;
    }

    /* Set Data pointer if we have data */
    if (frame->Type != MODBUS_INVALID && frame->DataLength > 0) {
        /* Data is at the end of the buffer, length DataLength bytes */
        frame->Data = (uint8_t *)(buf + (len - frame->DataLength));
    }

    /* Request-Response matching logic */
    switch (frame->Type) {
        case MODBUS_REQUEST:
            frame->RequestAddress = frame->Address;
            frame->RequestFunction = frame->Function;
            frame->RequestRegister = frame->Register;
            break;

        case MODBUS_RESPONSE:
            /* If address and function match the pending request, fill in register */
            if (frame->Address == frame->RequestAddress &&
                frame->Function == frame->RequestFunction) {
                if (frame->Function == 0x03 || frame->Function == 0x04) {
                    frame->Register = frame->RequestRegister;
                }
            }
            frame->RequestAddress = 0;
            frame->RequestFunction = 0;
            frame->RequestRegister = 0;
            break;

        case MODBUS_OK:
            /* FC06: disambiguate request vs response based on pending request */
            if (frame->Address == frame->RequestAddress &&
                frame->Function == frame->RequestFunction &&
                frame->Address != MODBUS_BROADCAST_ADR) {
                frame->Type = MODBUS_RESPONSE;
                frame->RequestAddress = 0;
                frame->RequestFunction = 0;
                frame->RequestRegister = 0;
            } else {
                frame->Type = MODBUS_REQUEST;
                frame->RequestAddress = frame->Address;
                frame->RequestFunction = frame->Function;
                frame->RequestRegister = frame->Register;
            }
            break;

        default:
            break;
    }
}
