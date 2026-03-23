# Meter Modbus Register Documentation

This directory provides links to official Modbus register documentation for each
supported energy meter type. Register addresses in meter profiles are
cross-referenced with these datasheets.

## Supported Meters

| # | Meter | Datasheet / Modbus Protocol | Status |
|---|-------|---------------------------|--------|
| 1 | Sensorbox v2 | SmartEVSE custom protocol (no public datasheet) | Verified from firmware |
| 2 | Phoenix Contact EEM-350-D-MCB | [Modbus Protocol (phoenixcontact.com)](https://www.phoenixcontact.com/en-pc/products/energy-meter-eem-350-d-mcb-2907946) | Verified |
| 3 | Finder 7E.78.8.400.0212 | [Modbus Map (findernet.com)](https://www.findernet.com/en/products/series/7E-series-energy-meters) | Verified |
| 4 | Eastron SDM630 | [SDM630 Modbus Protocol v2 (eastroneurope.com)](https://www.eastroneurope.com/products/view/sdm630modbus) | Verified |
| 5 | InvEastron (SDM630 inverted CT) | Same as Eastron SDM630 | Same register map |
| 6 | ABB B23 212-100 | [B23 Modbus Register List (abb.com)](https://new.abb.com/products/2CMA100166R1000/b23-212-100) | Verified |
| 7 | SolarEdge (SunSpec) | [SunSpec Modbus Map (solaredge.com)](https://www.solaredge.com/sites/default/files/sunspec-implementation-technical-note.pdf) | Verified |
| 8 | WAGO 879-30x0 | [WAGO 879-3000 Manual (wago.com)](https://www.wago.com/global/measurement-technology/energy-meter/p/879-3000) | Verified |
| 10 | Eastron SDM120 | [SDM120 Modbus Protocol (eastroneurope.com)](https://www.eastroneurope.com/products/view/sdm120modbus) | Verified |
| 11 | Finder 7M.38.8.400.0212 | [Finder 7M Modbus Map (findernet.com)](https://www.findernet.com/en/products/series/7M-series-energy-meters) | Verified |
| 12 | Sinotimer DTS6619 | Manufacturer documentation (sinotimer.com) | Register map from community |
| 14 | Schneider iEM3x5x | [iEM3000 Modbus Register Map (se.com)](https://www.se.com/ww/en/product-range/62096-iem3000-series/) | Verified |
| 15 | Chint DTSU666 | [DTSU666 Modbus Manual (chint.com)](https://www.chint.com) | Register map from community |
| 16 | Carlo Gavazzi EM340 | [EM340 Communication Protocol (gavazzi.com)](https://www.gavazziautomation.com/images/PIM/DATASHEET/ENG/EM340_DS_ENG.pdf) | Verified |
| 17 | Orno OR-WE-517 | [WE-517 Modbus Map (orno.pl)](https://orno.pl/en/product/1183/3-phase-multi-tariff-energy-meter-with-rs-485-100a-mid-3-modules-din-th-35mm) | Verified |
| 18 | Orno OR-WE-516 | [WE-516 Modbus Map (orno.pl)](https://orno.pl/en/product/1184/1-phase-multi-tariff-energy-meter-with-rs-485-100a-mid-1-module-din-th-35mm) | Verified |
| 19 | Custom | User-defined registers | N/A |

## Register Address Convention

Modbus register addresses in `EMConfig[]` use **0-based** addressing (protocol
data unit / PDU addresses). Some meter datasheets use 1-based addressing
(e.g., Input Register 30007 = PDU address 0x0006).

For function code 4 (input registers):
- Datasheet register 30001 = PDU address 0x0000
- Datasheet register 30007 = PDU address 0x0006

For function code 3 (holding registers):
- Datasheet register 40001 = PDU address 0x0000

## Known Discrepancies

None found so far. If a test fails because the meter profile register address
does not match the datasheet, file an issue.
