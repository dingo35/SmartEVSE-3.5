# User experiences with OCPP backend providers

## Tap Electric

You can connect your charging station to the Tap Electric platform at no cost. This allows you to make your charging station available to other users through the app. Users without a subscription pay a 10% transaction fee per charging session to Tap Electric (this is their business case to keep the connection free of charge). This fee is automatically added to the cost of the charging session the start at your station. The charging station owner receives his earnings monthly, minus any applicable service fees, as agreed with Tap Electric. There is no upfront amount held by Tap Electric.

- Sign up and download the app
- Go to "Beheer"
- Connect a new charger and select the option "my charger is not in the list"
- Copy the link wss://ocpp.tapelectric.app/XXXXXXXX and paste it into your OCPP settings of the SmartEVSE (Backend URL)
- For "Charge Box Id," enter an id of your choice
- You can leave the password blank
- Save the settings and you should see your charging station appear in TapElectric


In the TapElectric app, you can adjust various settings, including the rate you charge, etc.

You decide the price you want to charge for charging sessions. You can set rates based on your electricity contract (for example, day and night tariffs) and include hardware costs in your pricing. Tap Electric ensures that charging and payment are fully automated via the app. You link your bank account to the Tap Electric platform.
Tap Electric can pay the VAT to the tax authorities on your behalf for the charging sessions, depending on how your account is set up. This is especially relevant if you don’t have a VAT number but still want to pay VAT on the transactions. In that case, Tap Electric collects VAT over the full transaction amount and remits it to the authorities. Every month you receive a clear invoice from Tap Electric, showing what has been paid and to whom. You do not need to file a separate VAT return for these charging sessions yourself.
You can determine who has access to your charging station (for example, residents, guests, or employees) and whether you want to offer free or discounted charging to specific user groups. You have real-time insight into the usage of your charging station and can manage its status and availability via the app.

Everything is working in combination with a business Shell Recharge charge card, by using the RFID reader.

If you don't want to use an RFID card (e.g. your charger is mounted inside your garage and you only have one EV):
- on the web-dashboard of your SmartEVSE, check the checkbox Auto Authorize
- add the ID of your charge card. You can get the ID by scanning the card with your phone using an app like "NFC tools"


Only issue I encounter is that sometimes the connection between the SmartEVSE and Tap is terminated. I have to reboot the SmartEVSE and check in the TAP app that the connection is restored. A disconnected SmartEVSE can still be used to charge. It will upload the latests reading when the connection is restored. If there are gaps in charge sessions, you can contact the TAP support desk to solve it.



## Tibber

I'm using it in combination with Tibber and Myenergi Zappi (v2.1).

I googled on pictures regarding a correct serial number from a zappi and i modified the last digit, and it was possible to be used in the Tibber App.

Open Tibber app and follow these steps:
- Open the "Power-up" menu.
- Press "Laden".
- Select "Myenergi Zappi (v2.1).
- Enter a serial number from a zappi (adjusted serial numer)

Enter the information provided on screen in the evse OCPP configuration:
- BackendURL: wss://zappi.ocpp-s.tibber.com/
- Charge Box ID: Already filled in from "serial number" step.
- Password: Provided from Tibber
- I also enabled auto-authorize but used a code of my own (I don't have an rfid reader connected to SmartEvse).

To enable gridrewards you also need to add a car:
- Open the "Power-up" menu.
- Press "Laden".
- Select car you want, i selected a dummy as i don't want to connect my car to the Tibber app.

As final step you need to select the chargepoint in the "grid rewards" part of the app. Otherwise it will not enable it for you and allow you to charge using grid rewards. The detection of the car being connected is used from the SmartEvse, when you connect the car.

## SteVe
[@jpiscaer](https://github.com/jpiscaer) is using [SteVe](https://github.com/steve-community/steve), a free and open source OCPP back-end, available to self-host (running the default docker compose config in his case). The use case for using SteVe is to keep a list of charging transactions for reimbursement of charging costs from his company.
With a monthly list of transactions and a read-out of my (Inepro PRO380-Mod) kWh meter, he can satisfy the Belastingdienst's requirements. Note that he's not automating the payouts; he does the financial transactions manually, so he doesn't need a payment provider (nor does SteVe support this). If you need automatic payouts, look for Tap Electric or Monta. Since the EVSE is on a private driveway, he doesn't want or need to use RFID tags; he's has set the OCPP configuration to 'auto-authorize', and has configured SteVe to accept all requests through a dummy rfid tag. This way, he only uses SteVe to keep a list of charging transactions, and he doesn't allow (keep track of) guest usage, public usage etc.

---

## OCPP Configuration Reference

### Settings

| Parameter | Description | Validation |
|-----------|-------------|------------|
| Backend URL | WebSocket endpoint for the OCPP backend | Must start with `ws://` or `wss://` |
| Charge Box ID | Identifier for this charge point | Max 20 characters, printable ASCII |
| Password | WebSocket Basic Auth key | Max 40 characters (optional) |
| Auto Authorize | Enable FreeVend (auto-start transactions) | On/Off |
| Auto Authorize ID Tag | ID tag used for auto-authorized transactions | String |

### OCPP and Load Balancing

OCPP Smart Charging and SmartEVSE internal load balancing are **mutually exclusive**:

- **LoadBl = 0 (Standalone):** OCPP Smart Charging is active. The backend can set current limits via `SetChargingProfile`.
- **LoadBl = 1+ (Master/Node):** OCPP Smart Charging is disabled. The internal load balancer controls current distribution. OCPP still tracks transactions and authorization but cannot control current.

If you change LoadBl while OCPP is active, the firmware detects the conflict and disables OCPP current limiting. You must disable and re-enable OCPP for the new setting to take full effect.

### OCPP and Solar/Smart Mode

When Auto Authorize (FreeVend) is enabled together with Solar mode, the firmware defers granting charge permission until solar surplus is available. This prevents FreeVend from bypassing the solar surplus check. The same applies when a ChargeDelay is active.

### OCPP Telemetry

The `/settings` API endpoint includes OCPP diagnostics under the `ocpp` key:

| Field | Description |
|-------|-------------|
| `tx_active` | Whether a transaction is currently running |
| `tx_starts` / `tx_stops` | Transaction start/stop counters since boot |
| `auth_accepts` / `auth_rejects` / `auth_timeouts` | Authorization result counters |
| `smart_charging_active` | Whether OCPP Smart Charging is controlling current |
| `current_limit_a` | Current OCPP limit in amps (-1 = no limit) |
| `lb_conflict` | Whether a LoadBl/OCPP conflict is detected |

MQTT topics published:
- `<prefix>/OCPPTxActive` — `true` / `false`
- `<prefix>/OCPPCurrentLimit` — float or `none`
- `<prefix>/OCPPSmartCharging` — `Active` / `Inactive` / `Conflict`

---

## FAQ / Troubleshooting

### "OCPP shows Disconnected but charging still works"

This is normal. MicroOcpp has an offline transaction queue. When the WebSocket connection drops, transactions are cached locally and sent to the backend when the connection is restored. Charging continues using the last known authorization state.

### "Car charges immediately without RFID"

FreeVend (Auto Authorize) is enabled. When active, MicroOcpp automatically starts a transaction when a vehicle is plugged in, without requiring an RFID card. To require RFID authentication, disable Auto Authorize in the OCPP settings.

### "OCPP current limit is not working"

Check these in order:
1. **LoadBl must be 0 (Standalone).** OCPP Smart Charging only works when the internal load balancer is disabled. Check the `/settings` API — `lb_conflict` should be `false`.
2. **Backend must send SetChargingProfile.** The limit is set by the backend, not configured locally. Check `current_limit_a` in `/settings` — if it shows `-1`, no limit has been received.
3. **MinCurrent applies.** If the OCPP limit is below MinCurrent (typically 6A), the charge current is zeroed rather than set to the limit value.

### "Dual chargers report wrong energy"

SmartEVSE initializes MicroOcpp with a single connector. Each ESP32 runs its own OCPP instance. If both connect to the same backend, ensure each has a unique Charge Box ID. There is no multi-connector OCPP coordination between two SmartEVSEs — each reports independently.

### "Connection drops every few hours"

Common causes:
- **WiFi stability:** Check WiFi signal strength (RSSI) via MQTT or `/settings`. Consider a WiFi repeater if RSSI is below -70 dBm.
- **Backend timeouts:** Some backends disconnect idle WebSocket connections. MicroOcpp has internal reconnect logic and will reconnect automatically.
- **Router settings:** Some routers aggressively close idle TCP connections. Check your router's TCP timeout settings.
- If the problem persists, check the OCPP telemetry counters in `/settings` — the `ws_connects` and `ws_disconnects` fields show reconnection frequency since boot.

### "Settings validation rejects my URL/ChargeBoxId"

- **URL:** Must start with `ws://` or `wss://` and include a host after the scheme. Example: `wss://ocpp.provider.com/charger123`
- **Charge Box ID:** Max 20 characters (OCPP 1.6 CiString20 limit). Only printable ASCII characters are allowed. Special characters like `<`, `>`, `&`, `"`, `'` are rejected.
- **Password:** Max 40 characters (OCPP 1.6 AuthorizationKey limit). Empty password is valid (no auth).
