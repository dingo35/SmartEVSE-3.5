
# How to configure

## Getting started

1. **Start in Normal mode.** Walk through the settings shown on the LCD. At first, leave MAINS MET unconfigured. Set CONFIG, PWR SHARE, MAX, and the hardware settings (SWITCH, RCMON, RFID, WIFI).
2. **Test Normal mode.** Verify your EV charges at MAX current. If you don't need Smart/Solar charging and have no mains meter, you are done.
3. **Configure your mains meter.** Set MODE to Smart or Solar, then configure MAINS MET. New settings will appear in the menu (MAINS, MIN, and more). Browse through them again.
4. **If using API or MQTT meter feeds**, set up the data feeds before selecting "API" for the meter type. Data is expected within 11 seconds of selecting API. Test scripts are available in the test directory.
5. **For multiple SmartEVSEs**, see [Power Share setup](#multiple-smartevse-controllers-on-one-mains-supply-power-share) below.
6. **For Solar mode**, switch to Solar and configure the solar-specific settings (START, STOP, IMPORT) that appear.

## Quick reference: menu visibility

The LCD menu is dynamic. Which settings appear depends on your current MODE and PWR SHARE role. The table below shows all settings **in the order they appear on screen**, with their visibility conditions.

**Legend:** Always = always visible | Smart/Solar = only in Smart or Solar mode | Solar = only in Solar mode | D/M = only when PWR SHARE is Disabled or Master (not shown on Nodes) | conditional = depends on another setting

| # | Setting | Visible when | D/M only | Range | Default |
|---|---------|-------------|----------|-------|---------|
| 1 | [MODE](#mode) | Always | | Normal / Smart / Solar | Normal |
| 2 | [CONFIG](#config) | Always | | Socket / Fixed | Socket |
| 3 | [LOCK](#lock) | CONFIG = Socket | | Disabled / Solenoid / Motor | Disabled |
| 4 | [PWR SHARE](#pwr-share) | Always | | Disabled / Master / Node 1-7 | Disabled |
| 5 | [MAINS MET](#mains-met) | Smart/Solar | D/M | Disabled / Sensorbox / API / ... | Disabled |
| 6 | [GRID](#grid) | MAINS MET = Sensorbox (with CTs) | D/M | 4Wire / 3Wire | 4Wire |
| 7 | [SB2 WIFI](#sb2-wifi) | MAINS MET = Sensorbox v2 | D/M | Disabled / Enabled / Portal | Disabled |
| 8 | [MAINS ADR](#mains-adr) | MAINS MET = Modbus meter | D/M | 10-247 | 10 |
| 9 | [EV METER](#ev-meter) | Smart/Solar | | Disabled / API / ... | Disabled |
| 10 | [EV ADR](#ev-adr) | EV METER = Modbus meter | | 10-247 | 12 |
| 11 | [Custom meter settings](#custom-meter-settings) | Custom meter selected | D/M | (see below) | |
| 12 | [MAINS](#mains) | MAINS MET configured | D/M | 10-200A | 25A |
| 13 | [MIN](#min) | MAINS MET configured | D/M | 6-16A | 6A |
| 14 | [MAX](#max) | Always | | 6-80A | 16A |
| 15 | [CIRCUIT](#circuit) | Master, or (Disabled + Smart/Solar + EV METER) | | 10-160A | 25A |
| 16 | [START](#start) | Solar | D/M | 0-48A | 4A |
| 17 | [STOP](#stop) | Solar | D/M | Disabled / 1-60 min | 10 min |
| 18 | [IMPORT](#import) | Solar | D/M | 0-48A | 0A |
| 19 | [CONTACT 2](#contact-2) | Smart/Solar | | (see below) | Always On |
| 20 | [SWITCH](#switch) | Always | | (see below) | Disabled |
| 21 | [RCMON](#rcmon) | Always | | Disabled / Enabled | Disabled |
| 22 | [RFID](#rfid) | Always | | (see below) | Disabled |
| 23 | [WIFI](#wifi) | Always | | Disabled / SetupWifi / Enabled | Disabled |
| 24 | [AUTOUPDAT](#autoupdat) | WIFI = Enabled | | Disabled / Enabled | Disabled |
| 25 | [APP PIN](#app-pin) | WIFI = Enabled | | | |
| 26 | [APP SERVR](#app-servr) | WIFI = Enabled | | Disabled / Enabled | Disabled |
| 27 | [MAX TEMP](#max-temp) | Always | | 40-75 C | 65 C |
| 28 | [CAPACITY](#capacity) | MAINS MET configured | D/M | Disabled / 10-600A | Disabled |
| 29 | [CAP STOP](#cap-stop) | CAPACITY configured | D/M | Disabled / 1-60 min | Disabled |
| 30 | [LCD PIN](#lcd-pin) | Always | | 0-9999 | 0 |
| 31 | [PRIORITY](#priority) | PWR SHARE = Master | D/M | Modbus Adr / First Conn / Last Conn | Modbus Adr |
| 32 | [ROTATION](#rotation) | PWR SHARE = Master | D/M | 0, 30-1440 min | 0 (off) |
| 33 | [IDLE TMO](#idle-tmo) | PWR SHARE = Master | D/M | 30-300 s | 60 s |

**"D/M only"** means the setting is hidden when PWR SHARE is set to Node 1-7. Only Disabled and Master see these settings.

### What each role sees

**Standalone (PWR SHARE = Disabled) or Master:**
All settings in the table above, subject to MODE and meter conditions.

**Node (PWR SHARE = Node 1-7):**
Only these settings appear: MODE, CONFIG, LOCK, PWR SHARE, EV METER, EV ADR, MAX, CONTACT 2, SWITCH, RCMON, RFID, WIFI (and sub-items), MAX TEMP, LCD PIN. Additionally:
- SWITCH: the **Grid Relay** option is not selectable on Nodes.
- CONTACT 2: the **Auto** option is not selectable on Nodes.

---

# LCD menu settings reference

Settings are listed below in the order they appear on the LCD screen.

## MODE
By default, you are in normal EVSE mode. You can also choose smart mode or solar mode, but these modes require configuring a [MAINS MET](#mains-met) to function.

- **Normal**: The EV will charge with the current set at [MAX](#max).
- **Smart**: The EV will charge with a dynamic charge current, depending on [MAINS MET](#mains-met) data, [MAINS](#mains), [MAX](#max) and [MIN](#min) settings.
- **Solar**: The EV will charge using solar power.

## CONFIG
Configure SmartEVSE with Type 2 Socket or fixed cable.

- **Socket**: The SmartEVSE is connected to a socket, so it will need to sense the cable used for its maximum capacity.
- **Fixed**: The SmartEVSE is connected to a fixed cable, and MAX will determine your maximum charge current.

## LOCK
> Visible when: CONFIG = Socket

- **Disabled**: No lock is used.
- **Solenoid**: Dostar, DUOSIDA DSIEC-ELB / ELM, or Ratio lock.
- **Motor**: Signal wire reversed, DUOSIDA DSIEC-EL or Phoenix Contact.

## PWR SHARE
Power Share (formerly LOAD BAL). Up to 8 SmartEVSEs can be connected via Modbus, and the available power will be shared.

- **Disabled**: Power sharing is not used (single SmartEVSE).
- **Master**: Set the first SmartEVSE to Master. Only one Master should be set.
- **Node 1-7**: Set the other SmartEVSEs to Node 1-7.

Changing this setting controls which other settings are visible. See [quick reference table](#quick-reference-menu-visibility) and [Power Share setup](#multiple-smartevse-controllers-on-one-mains-supply-power-share).

## MAINS MET
> Visible when: MODE = Smart or Solar, AND PWR SHARE = Disabled or Master

Set the type of MAINS kWh Meter.

- **Disabled**: No MAINS meter connected (only Normal mode possible).
- **Sensorbox**: The Sensorbox sends measurement data to the SmartEVSE.
- **API**: MAINS meter data is fed through the [REST API](REST_API.md) or [MQTT API](#mqtt-api).
- **Phoenix C** / **Finder** / **...** / **Custom**: A Modbus kWh meter is used.
- **HmWzrd P1**: HomeWizard P1 meter (wifi based connection to the smart meter's P1 port).

**Note**:
- Eastron1P is for single-phase Eastron meters.
- Eastron3P is for Eastron three-phase meters.
- InvEastron is for Eastron three-phase meters fed from below (inverted).

Configuring a mains meter unlocks additional settings: GRID, SB2 WIFI, MAINS ADR, MAINS, MIN, CAPACITY, and (in Solar mode) START, STOP, IMPORT.

## GRID
> Visible when: MAINS MET = Sensorbox with CT's, AND PWR SHARE = Disabled or Master

- **4Wire**: Star connection with 3 phase wires and neutral.
- **3Wire**: Delta connection with 3 phase wires without neutral.

## SB2 WIFI
> Visible when: MAINS MET = Sensorbox v2, AND PWR SHARE = Disabled or Master

Connect Sensorbox-2 to WiFi.

- **Disabled**: Sensorbox-2 WiFi disabled.
- **Enabled**: Connect Sensorbox-2 via WiFi.
- **Portal**: WiFi configuration portal.

## MAINS ADR
> Visible when: MAINS MET = Modbus meter (not Disabled, API, Sensorbox, or HmWzrd P1), AND PWR SHARE = Disabled or Master

Set the Modbus address for the mains kWh meter: 10-247.

## EV METER
> Visible when: MODE = Smart or Solar (any PWR SHARE role, including Nodes)

Set the type of EV kWh Meter (measures power and charged energy).

- **Disabled**: No EV meter connected.
- **API**: EV meter data is fed through the REST API or MQTT API.
- **Phoenix C** / **Finder** / **...** / **Custom**: A Modbus kWh meter is used.

**Note**:
- Eastron1P is for single-phase Eastron meters.
- Eastron3P is for Eastron three-phase meters.
- InvEastron is for Eastron three-phase meters fed from below (inverted).

## EV ADR
> Visible when: EV METER = Modbus meter (not Disabled or API)

Set the Modbus address for the EV Meter: 10-247.

## Custom meter settings
> Visible when: MAINS MET or EV METER = Custom, AND PWR SHARE = Disabled or Master

These settings configure a custom Modbus energy meter. All share the same configuration (one custom meter definition is used for both mains and EV if both are set to Custom).

| Setting | Description | Range |
|---------|-------------|-------|
| BYTE ORD | Byte order | 0-3 |
| DATA TYPE | Modbus data type | 0-4 |
| FUNCTION | Modbus function (3=holding, 4=input) | 3-4 |
| VOL REGI | Register for Voltage (V) | 0-65530 |
| VOL DIVI | Divisor for Voltage | 0-7 |
| CUR REGI | Register for Current (A) | 0-65530 |
| CUR DIVI | Divisor for Current | 0-7 |
| POW REGI | Register for Power (W) | 0-65534 |
| POW DIVI | Divisor for Power | 0-7 |
| ENE REGI | Register for Energy (kWh) | 0-65534 |
| ENE DIVI | Divisor for Energy | 0-7 |

## MAINS
> Visible when: MAINS MET configured (not Disabled), AND PWR SHARE = Disabled or Master

Set max mains current: 10-200A per phase. This is the hard limit of your mains connection.

## MIN
> Visible when: MAINS MET configured (not Disabled), AND PWR SHARE = Disabled or Master

Set the minimum charge current the EV will accept: 6-16A per phase.

## MAX
Set the MAX charge current for the EV: 6-80A per phase. Always visible regardless of mode or role.

If [CONFIG](#config) is set to **Fixed**, configure MAX to be lower than or equal to the maximum current that your fixed cable can carry.

## CIRCUIT
> Visible when: PWR SHARE = Master, OR (PWR SHARE = Disabled AND MODE = Smart/Solar AND EV METER configured)

Set the max current of the EVSE circuit: 10-160A per phase. When power sharing, this is the total current that will be split between connected and charging EVs.

## START
> Visible when: MODE = Solar, AND PWR SHARE = Disabled or Master

Set the surplus energy current at which the EV should start solar charging: 0-48A (sum of all phases).

## STOP
> Visible when: MODE = Solar, AND PWR SHARE = Disabled or Master

Stop charging when there is not enough solar power available.

- **Disabled**: Never stop charging.
- **1-60 min**: Stop after this many minutes below the threshold.

## IMPORT
> Visible when: MODE = Solar, AND PWR SHARE = Disabled or Master

Allow additional grid power when solar charging: 0-48A (sum of all phases). Use this when there is not enough solar power but you want to use as much solar power as possible.

**Important Note**: START and IMPORT are summed over all phases, and MIN is per phase!

## CONTACT 2
> Visible when: MODE = Smart or Solar

Use a second contactor (C2) to switch phases L2 and L3.

- **Not present**: The second contactor is not present, and SmartEVSE assumes 3-phase charging.
- **Always Off**: C2 is always off, single-phase charging. WE RECOMMEND THIS SETTING IF YOU ARE SINGLE PHASE CHARGING IN SOLAR MODE, EVEN IF YOU DONT HAVE A second contactor INSTALLED!
- **Always On**: C2 is always on, three-phase charging (default).
- **Solar Off**: C2 is always on except in Solar Mode, where it is always off.
- **Auto**: SmartEVSE starts charging at 3-phase, but in Solar Mode, it will switch off C2 when there is not enough current for 3 phases, continuing on 1 phase; if there is enough current it will switch on C2, continuing on 3 phases. In Smart mode we will charge 3P, since we assume you are switching to Smart mode because not enough sun is available for Solar mode. **Note: Auto is not available on Nodes (PWR SHARE = Node 1-7).**

**Important**: Wire your C2 contactor according to the schematics in the [Hardware installation](installation.md).

## SWITCH
Set the Function of an External Switch (Pin SW or Connector P2).

- **Disabled**: A push button can be used to stop charging.
- **Access B**: A momentary push button is used to enable/disable access to the charging station.
- **Access S**: A toggle switch is used to enable/disable access to the charging station.
- **Sma-Sol B**: A momentary push button is used to switch between Smart and Solar modes.
- **Sma-Sol S**: A toggle switch is used to switch between Smart and Solar modes.
- **Grid Relay**: A relay from your energy provider is connected; when the relay is open, power usage is limited to 4.2kW (Energy Industry Act, par 14a). **Note: Grid Relay is not available on Nodes (PWR SHARE = Node 1-7).**
- **Custom B**: A momentary push button can be used for external integrations.
- **Custom S**: A toggle switch can be used for external integrations.

## RCMON
Residual Current Monitor (RCM14-03) plugged into connector P1.

- **Disabled**: The RCD option is not used.
- **Enabled**: When a fault current is detected, the contactor will be opened.

## RFID
Use an RFID Card Reader to Enable/Disable Access to the EVSE. A maximum of 100 RFID cards can be stored. Only a push button can be used simultaneously with the RFID reader.

- **Disabled**: RFID reader turned off.
- **EnableAll**: Accept all learned cards for enabling/disabling the SmartEVSE.
- **EnableOne**: Only allow a single (learned) card for enabling/disabling the SmartEVSE.
  - In this mode, the lock (if used) will lock the cable in the charging socket, and the same card is used to unlock it.
- **Learn**: Learn a new card and store it in the SmartEVSE. Present a card in front of the reader, and "Card Stored" will be shown.
- **Delete**: Erase a previously learned card. Hold the card in front of the reader, and "Card Deleted" will be shown.
- **DeleteAll**: Erase all cards from the SmartEVSE.
- **Rmt/OCPP**: Authorize remotely over OCPP and bypass the SmartEVSE's local RFID storage.

## WIFI
Enable WiFi connection to your network.

- **Disabled**: WiFi connection is disabled.
- **SetupWifi**
  - v3.9.0 or newer:
  The SmartEVSE presents itself as a WiFi Access Point with SSID "SmartEVSE-config"; the password is displayed at the top line of your LCD display. Connect with your phone to that access point, go to [http://192.168.4.1/](http://192.168.4.1/) and configure your WiFi SSID and password.
  - v3.6.4 until v3.8.x:
    - Connect your smartphone to the WiFi network you want your SmartEVSE connected to.
Note: If you have a multi AP setup, with the same SSID, you need to be connected to the desired AP, as the configuration is based on the BSSID, so it will choose the specific AP your phone is connected to.
    - Download and run the ESPTouch app from your favorite app store [Android](https://play.google.com/store/apps/details?id=com.fyent.esptouch.android&hl=en_US:) (please ignore the strange Author name) or [Apple](https://apps.apple.com/us/app/espressif-esptouch/id1071176700) or  [Github](https://github.com/EspressifApp/EsptouchForAndroid) (for source code).
    - Choose EspTouch V2.
    - Fill in the key (password) of the WiFi network.
    - Fill in **1** in device count for provisioning.
    - On the SmartEVSE LCD screen, select **WiFi**, select **SetupWifi**
    - Press the middle button to start the configuration procedure.
    - Once pressed, the bottom line shows you a 16 character key, first 8 are 01234567. Note that from this point on, you have 120s TO FINISH this procedure!
    - Fill in that key in the ESPTouch app, in the AES Key field
    - Leave Custom Data empty
    - Press **Confirm**, within 30 seconds the app will confirm a MAC address and an IP address.
    - You are connected now. If you want special stuff (static IP address, special DNS address), configure them on your AP/router.

  - v3.6.4 and until v3.8.x: BACKUP PROCEDURE: if you don't get it to work with the ESPTouch app, there is a backup procedure:
    - connect your SmartEVSE with a USB cable to your PC
    - install the USB driver (Windows) or not (Linux) for ESP32 chipset
    - connect your favorite serial terminal to the appropriate port,
    - use the following settings:
      - 115200 bps
      - 8 bits
      - no parity
      - 1 stopbit
    - on the SmartEVSE LCD screen, select "WiFi", select "SetupWifi"
    - press the middle button to start the configuration procedure
    - on your terminal window you should see a request to enter your WiFi access point SSID and password.
    - the controller should now connect to WiFi.
  - v3.6.3 or older:
  The SmartEVSE presents itself as a WiFi Access Point with SSID "smartevse-xxxx". Connect with your phone to that access point, go to [http://192.168.4.1/](http://192.168.4.1/) and configure your WiFi SSID and key (password).
- **Enabled**: Connect to your network via WiFi.

## AUTOUPDAT
> Visible when: WIFI = Enabled

Automatic update of the SmartEVSE firmware.

- **Disabled**: No automatic update.
- **Enabled**: Checks daily for a new stable firmware version and installs it when no EV is connected.
  **Note**: This will not work if your version is not in the format `vx.y.z` (e.g., v3.6.1). Locally compiled versions or RCx versions will not auto-update.

## APP PIN
> Visible when: WIFI = Enabled

Generate a 6-digit pairing PIN for connecting the SmartEVSE mobile app.

- Press the middle button to generate a new PIN.
- The PIN is displayed on the LCD screen and is valid for 2 minutes.
- Enter this PIN in the SmartEVSE mobile app to pair your device.
- The PIN is automatically cleared when you exit the menu or after the timeout expires.

## APP SERVR
> Visible when: WIFI = Enabled

Enable or disable the connection to the SmartEVSE cloud server for mobile app integration.

- **Disabled**: No connection to the SmartEVSE server.
- **Enabled**: Connect to `mqtt.smartevse.nl` for mobile app communication.

When enabled, the top row shows the current connection status:
- **Connected to server**: Successfully connected to the SmartEVSE server.
- **No server connection**: Not connected to the server.

This feature uses a unique per-device key stored in the controller. If you somehow erased the controller completely this data will be lost, and the controller will not be able to connect to the server.

## MAX TEMP
Maximum allowed temperature for your SmartEVSE: 40-75 C (default 65 C).
Charging will stop once the internal temperature reaches this threshold and resume once it drops to 55 C.

## CAPACITY
> Visible when: MAINS MET configured (not Disabled), AND PWR SHARE = Disabled or Master

Maximum allowed mains current summed over all phases: 10-600A. Used for [EU Capacity Rate Limiting](#eu-capacity-rate-limiting).

- **Disabled** (0): Capacity rate limiting not used.
- **10-600A**: Limit the sum of all mains phase currents to this value.

## CAP STOP
> Visible when: CAPACITY is configured (not Disabled)

Timer in minutes. If CAPACITY is exceeded, charging will not immediately stop but will wait until the timer expires.

- **Disabled**: Charging stops immediately when CAPACITY is exceeded.
- **1-60 min**: Wait this many minutes before stopping.

## LCD PIN
Pin code so that you can use the buttons on the LCD menu on the web-interface.
Left button increases the digit by one, Right button goes to next digit, Middle button ends entry.

## PRIORITY
> Visible when: PWR SHARE = Master

Set the priority strategy for load sharing when there is not enough power for all connected EVSEs. See [Priority-Based Power Scheduling](priority-scheduling.md) for full details.

- **Modbus Adr**: Lower Modbus address = higher priority (Master first, then Node 1, 2, ...).
- **First Conn**: The car that plugged in first gets priority.
- **Last Conn**: The most recently plugged-in car gets priority.

## ROTATION
> Visible when: PWR SHARE = Master

How often to rotate which EVSE is actively charging. See [Priority-Based Power Scheduling](priority-scheduling.md) for full details.

- **0**: Disabled — no rotation. The highest-priority EVSE charges until done.
- **30–1440**: Rotate every N minutes.

## IDLE TMO
> Visible when: PWR SHARE = Master

How many seconds to wait before deciding an EVSE is idle and moving to the next one. Also serves as anti-flap protection. See [Priority-Based Power Scheduling](priority-scheduling.md) for full details.

- **30–300**: Seconds (default 60).

---

# Setup guides by role

## Single SmartEVSE (standalone)

1. Set **PWR SHARE** = Disabled
2. Set **CONFIG** (Socket or Fixed) and **LOCK** if using a socket
3. Set **MAX** to the maximum current your cable/installation supports
4. Set **WIFI**, **SWITCH**, **RCMON**, **RFID** as needed for your hardware
5. If you only need Normal mode, you are done
6. For Smart/Solar: set **MODE**, then configure **MAINS MET** and the settings that appear (MAINS, MIN, etc.)
7. For Solar mode: configure **START**, **STOP**, **IMPORT**
8. For single-phase charging in Solar mode: set **CONTACT 2** = Always Off

## Master setup (Power Share)

1. Set **PWR SHARE** = Master
2. Set **MODE** to Smart (or Solar) with a Sensorbox or configured kWh meter
3. Configure **MAINS MET** and **MAINS** (maximum current of the mains connection per phase)
4. Set **CIRCUIT** to the maximum current of the EVSE circuit per phase (shared between EVs)
5. Set **MAX** for the EV connected to this SmartEVSE
6. Set **MIN** to the lowest allowable charging current for all connected EVs
7. Configure remaining hardware settings (WIFI, SWITCH, RCMON, RFID, etc.)
8. Optionally configure **PRIORITY**, **ROTATION**, and **IDLE TMO** for [priority scheduling](priority-scheduling.md) — these control what happens when there isn't enough power for all EVSEs

## Node setup (Power Share)

Nodes have a reduced menu. Most metering and balancing settings are managed by the Master.

1. Set **PWR SHARE** = Node 1-7 (unique number for each Node)
2. Set **MAX** for the EV connected to this SmartEVSE
3. Optionally configure **EV METER** if this Node has its own EV meter
4. Configure hardware settings (CONFIG, LOCK, SWITCH, RCMON, RFID, WIFI, etc.)

**Note**: The following restrictions apply to Nodes:
- SWITCH: the Grid Relay option is not available
- CONTACT 2: the Auto option is not available
- Settings managed by the Master (MAINS MET, MAINS, MIN, CIRCUIT, START, STOP, IMPORT, CAPACITY, custom meter configuration) do not appear in the Node menu

---

# SINGLE PHASE CHARGING

SmartEVSE calculates with currents per phase; a problem arises in Solar mode, because there Isum (the sum of the currents of all phases) has to be guarded; to calculate with it per phase you have to know the number of phases you are charging.
We try to detect the number of phases you are charging with, with the help of the settings of the second contactor C2 and the EVMeter, if present.
This detection can fail easily; not always an EVMeter is present, and even if there is, an EV could determine to start charging at one phase and later on add more phases (Teslas are known to do this); an EV could even decide during the charging process to stop charging on certain phases.
We could introduce a setting "1phase/3phase" charging, but this setting would be EV dependent if you are on a 3 phase grid; so you would have to change the setting every time another EV connects to your SmartEVSE.

Currently the most reliable way to get the correct behaviour at Solar mode is:
- if you are on a 3 phase grid and you are 3 phase charging, you have no problem
- if you are on a 3 phase grid and you are 1 phase charging in Solar mode, set CONTACT2 to "Always Off", even if you don't have a second contactor installed; it will tell the algorithm to calculate with single phase
- if you are on a 1 phase grid, set CONTACT2 to "Always Off" since you will always be charging single phase

If you are at Smart mode you just set CONTACT2 to the appropriate setting as documented above.

# Multiple SmartEVSE controllers on one mains supply (Power Share)
Up to eight SmartEVSE modules can share one mains supply.

## Hardware connections
- Connect the A, B and GND connections from the Master to the Node(s).
- So A connects to A, B goes to B etc.
- If you are using Smart/Solar mode, you should connect the A, B, +12V and GND wires from the sensorbox to the same screw terminals of the SmartEVSE! Make sure that the +12V wire from the sensorbox is connected to only -one- SmartEVSE.

## Software configuration
- Set one SmartEVSE PWR SHARE setting to MASTER, and the others to NODE 1-7. Make sure there is only one Master, and the Node numbers are unique.
- See [Master setup](#master-setup-power-share) and [Node setup](#node-setup-power-share) above for step-by-step instructions.

# OCPP (you want your company to pay for your electricity charges, or you want to exploit your SmartEVSE as a public charger)
To charge a company or a user for your electricity cost, you need a Backend Provider (BP). The BP will monitor your charger usage and will bill the appropriate user and/or company, and will pay you your part.
Your SmartEVSE can be connected to any BP by the OCPP protocol.
See the OCPP section in the SmartEVSE dashboard for setting up identifiers and configuring the OCPP interface.
Connect to the OCPP server using the credentials set up in the SmartEVSE dashboard. To use
the RFID reader with OCPP, set the mode Rmt/OCPP in the RFID menu. Note that the other
RFID modes overrule the OCPP access control. OCPP SmartCharging requires the SmartEVSE
internal load balancing needs to be turned off.
For user experiences with back-end providers, see [OCPP Backends](ocpp.md)

# REST API

For the specification of the REST API, see [REST API](REST_API.md)

# MQTT API
Your SmartEVSE can now export the most important data to your MQTT-server. Just fill in the configuration data on the webserver and the data will automatically be announced to your MQTT server.

You can easily show all the MQTT topics published:
```
mosquitto_sub -v -h ip-of-mosquitto-server -u username -P password  -t '#'
```

You can feed the SmartEVSE data by publishing to a topic:
```
mosquitto_pub  -h ip-of-mosquitto-server -u username -P password -t 'SmartEVSE-xxxxx/Set/CurrentOverride' -m 150
```
...where xxxxx your SmartEVSE's serial number, will set your Override Current to 15.0A.

Valid topics you can publish to are:
```
/Set/Mode
/Set/CurrentOverride
/Set/CurrentMaxSumMains
/Set/CPPWMOverride
/Set/MainsMeter
/Set/EVMeter
/Set/HomeBatteryCurrent
/Set/RequiredEVCCID
/Set/ColorOff
/Set/ColorNormal
/Set/ColorSmart
/Set/ColorSolar
/Set/CableLock
/Set/EnableC2  0 "Not present", 1 "Always Off", 2 "Solar Off", 3 "Always On", 4 "Auto" ; do not change during charging to prevent unexpected errors of your EV!
               You can send either the number or the string, SmartEVSE will accept both!
/Set/RFID      Hex string representing RFID card UID (12 or 14 hex characters for 6 or 7 byte UIDs)
               Example: "112233445566" (6 bytes) or "11223344556677" (7 bytes)
               This will simulate an RFID card swipe and start/stop a charging session using all existing RFID checks
               (whitelist verification, OCPP authorization, etc.)
/Set/PrioStrategy       0=Modbus Address, 1=First Connected, 2=Last Connected (Master only)
/Set/RotationInterval   0=disabled, 30-1440 minutes (Master only)
/Set/IdleTimeout        30-300 seconds (Master only)
```

For details on the scheduling topics, see [Priority-Based Power Scheduling](priority-scheduling.md).
Your mains kWh meter data can be fed with:
```
mosquitto_pub  -h ip-of-mosquitto-server -u username -P password -t 'SmartEVSE-xxxxx/Set/MainsMeter' -m L1:L2:L3
```
...where L1 - L3 are the currents in deci-Amperes. So 100 means 10.0A importing, -5 means 0.5A exporting.
...These should be fed at least every 10 seconds.

Your EV kWh meter data can be fed with:
```
mosquitto_pub  -h ip-of-mosquitto-server -u username -P password -t 'SmartEVSE-xxxxx/Set/EVMeter' -m L1:L2:L3:P:E
```
...where L1 - L3 are the currents in deci-Amperes. So 100 means 10.0A.
...where P is the Power in W,
...where E is the Energy in Wh.

You can simulate an RFID card swipe via MQTT to start/stop a charging session:
```
mosquitto_pub  -h ip-of-mosquitto-server -u username -P password -t 'SmartEVSE-xxxxx/Set/RFID' -m '112233445566'
```
...where 112233445566 is the hex representation of your RFID card's UID (6 byte example).
...For a 7 byte UID, use 14 hex characters (e.g., '11223344556677').
...The RFID will be processed using all existing checks: whitelist verification, OCPP authorization, etc.
...Swiping the same card again will typically stop the session (behavior depends on RFID Reader mode setting).

You can find test scripts in the [test directory](https://github.com/SmartEVSE/SmartEVSE-3/tree/master/SmartEVSE-3/test) that feed EV and MainsMeter data to your MQTT server.

The `/Set/CurrentMaxSumMains` topic can be used to set the [CAPACITY](#capacity) setting.

# Home Battery Integration
In a normal EVSE setup, a sensorbox is used to read the P1 information to deduce if there is sufficient solar energy available. This however can give unwanted results when also using a home battery as this will result in one battery charging the other one.

For this purpose the settings endpoint allows you to pass through the battery current information:
* A positive current means the battery is charging
* A negative current means the battery is discharging

The EVSE will use the battery current to neutralize the impact of a home battery on the P1 information.

**Regular updates from the consumer are required to keep this working as values cannot be older than 11 seconds.**

**The battery currents are ONLY taken into account in Solar mode!**

### Example
* Home battery is charging at 2300W -> 10A
* P1 has an export value of 230W -> -1A
* EVSE will neutralize the battery and P1 will be "exporting" -11A

The sender has several options when sending the home battery current:
* Send the current AS-IS -> EVSE current will be maximized
* Only send when battery is discharging -> AS-IS operation but EVSE will not discharge the home battery
* Reserve an amount of current for the home battery (e.g. 10A) -> Prioritize the home battery up to a specific limit

# Integration with Home Assistant
There are three options to integrate your SmartEVSE with Home Assistant:

## By MQTT (preferred)

If you already use MQTT in your Home Assistant setup, this is the easiest and fastest way to integrate your SmartEVSE into HA. As soon as you have MQTT configured correctly in the SmartEVSE, the device will automatically be discovered by Home Assistant!

> [!TIP]
> Just add the MQTT details in the SmartEVSE and you're good! There is no further integration needed to set up, you will find the SmartEVSE listed on the [MQTT integration page](https://my.home-assistant.io/redirect/integration/?domain=mqtt). Not even a HA restart needed!

## Through the HA-integration - DEPRECATED

If you cannot (or do not want to) use MQTT to integrate your SmartEVSE with Home Assistant, please have a look at [the SmartEVSE `custom_component` for Home Assistant](https://github.com/dingo35/ha-SmartEVSEv3). This `custom_component` uses the REST API to share data from the SmartEVSE to Home Assistant, and enables you to set SmartEVSE settings from Home Assistant. You will need SmartEVSE firmware version 1.5.2 or higher to use this integration.

> [!WARNING]
>  Because of how this `custom_component` and the REST API works, data updates will arrive considerably slower in HA when compared to the MQTT integration. When possible, consider using MQTT.

## By manually configuring your configuration.yaml

It's a lot of work, but you can have everything exactly your way. See examples in the integrations directory of our GitHub repository.

# EU Capacity Rate Limiting

In line with a EU directive, electricity providers can implement a "capacity rate" for consumers, encouraging more balanced energy consumption. This approach aims to smooth out usage patterns and reduce peak demand.

For further details, please refer to [serkri#215](https://github.com/serkri/SmartEVSE-3/issues/215).

* The menu item "Capacity" can be set from 10-600A. (sum of all phases)
* This setting applies only in Smart or Solar mode.
* Beyond existing limits (Mains, MaxCircuit), the charging current will be controlled to ensure that the total of all Mains phase currents does not exceed the Capacity setting.
* If you are unfamiliar with this setting or do not fall under the applicable regulations, it is advisable to keep the setting at its default setting. (disabled)
