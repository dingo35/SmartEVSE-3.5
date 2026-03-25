#if defined(ESP32)

#include <unordered_map>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <WiFi.h>

#include "esp32.h"
#include "network_common.h"
#include "http_api.h"
#include "session_log.h"
#include "utils.h"
#include "glcd.h"
#include "meter.h"
#include "modbus.h"
#include "mqtt_publish.h"
#include "OneWire.h"
#include "diag_sampler.h"
#include "diag_storage.h"
#include "capacity_peak.h"
#include <LittleFS.h>

//OCPP includes
#if ENABLE_OCPP && defined(SMARTEVSE_VERSION) //run OCPP only on ESP32
#include <MicroOcpp.h>
#include <MicroOcppMongooseClient.h>
#include <MicroOcpp/Core/Configuration.h>
#include "ocpp_logic.h"
#include "ocpp_telemetry.h"
#endif //ENABLE_OCPP

// Externs for globals not exposed via headers
extern unsigned char RFID[8];
extern uint8_t pilot;
extern bool CustomButton;
extern uint32_t CurrentPWM;
extern bool CPDutyOverride;
extern uint8_t Lock;
extern uint8_t Config;
extern uint8_t LCDlock;
extern uint8_t CableLock;
extern EnableC2_t EnableC2;
extern uint8_t BacklightSet;
extern uint16_t OverrideCurrent;
extern uint8_t MaxSumMainsTime;
extern uint8_t PrioStrategy;
extern uint16_t RotationInterval;
extern uint16_t IdleTimeout;
extern int16_t IrmsOriginal[3];
extern int16_t homeBatteryCurrent;
extern time_t homeBatteryLastUpdate;
extern int phasesLastUpdate;
extern uint8_t ColorOff[3];
extern uint8_t ColorNormal[3];
extern uint8_t ColorSmart[3];
extern uint8_t ColorSolar[3];
extern uint8_t ColorCustom[3];
extern uint8_t OcppMode;
extern uint16_t MaxMains;
extern uint16_t MaxSumMains;
extern uint16_t CapacityLimit;
extern capacity_state_t CapacityState;
extern uint16_t MaxCurrent;
extern uint16_t MinCurrent;
extern uint16_t MaxCircuit;
extern uint16_t MaxCircuitMains;
extern uint16_t StartCurrent;
extern uint16_t StopTime;
extern uint16_t ImportCurrent;
extern struct DelayedTimeStruct DelayedStopTime;
extern uint8_t DelayedRepeat;
extern uint8_t RFIDReader;
extern uint16_t maxTemp;
extern uint8_t ScheduleState[];
extern uint8_t BalancedState[];   // PLAN-07: per-node state for node overview
extern uint16_t RotationTimer;
extern int8_t TempEVSE;
extern const char StrRFIDReader[7][10];
extern Switch_Phase_t Switching_Phases_C2;

#if MQTT
extern String MQTTHost;
extern uint16_t MQTTPort;
extern String MQTTprefix;
extern String MQTTuser;
extern String MQTTpassword;
extern bool MQTTtls;
extern bool MQTTSmartServer;
extern bool MQTTChangeOnly;
extern uint16_t MQTTHeartbeat;
#endif

#if ENABLE_OCPP && defined(SMARTEVSE_VERSION)
extern MicroOcpp::MOcppMongooseClient *OcppWsClient;
extern float OcppCurrentLimit;
extern ocpp_telemetry_t OcppTelemetry;
#endif

//make mongoose 7.14 compatible with 7.13
#define mg_http_match_uri(X,Y) mg_match(X->uri, mg_str(Y), NULL)

/*
 * Validate a filename for safe filesystem access.
 * Returns true if filename contains only safe characters:
 * alphanumeric, underscore, dash, dot.
 * Rejects empty names, path separators, '..' sequences,
 * and any non-printable or non-ASCII characters.
 */
static bool is_safe_filename(const char *name, size_t len)
{
    if (len == 0)
        return false;
    /* Reject '..' anywhere in the name */
    for (size_t i = 0; i + 1 < len; i++) {
        if (name[i] == '.' && name[i + 1] == '.')
            return false;
    }
    for (size_t i = 0; i < len; i++) {
        char ch = name[i];
        /* Allow alphanumeric, underscore, dash, dot */
        if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
            (ch >= '0' && ch <= '9') || ch == '_' || ch == '-' || ch == '.')
            continue;
        return false;
    }
    return true;
}

// handles URI, returns true if handled, false if not
bool handle_URI(struct mg_connection *c, struct mg_http_message *hm,  webServerRequest* request) {
//    if (mg_match(hm->uri, mg_str("/settings"), NULL)) {               // REST API call?
    if (mg_http_match_uri(hm, "/settings")) {                            // REST API call?
      if (!memcmp("GET", hm->method.buf, hm->method.len)) {                     // if GET
        String mode = "N/A";
        int modeId = -1;
        if(AccessStatus == OFF)  {
            mode = "OFF";
            modeId=0;
        } else if(AccessStatus == PAUSE)  {
            mode = "PAUSE";
            modeId=4;
        } else {
            switch(Mode) {
                case MODE_NORMAL: mode = "NORMAL"; modeId=1; break;
                case MODE_SOLAR: mode = "SOLAR"; modeId=2; break;
                case MODE_SMART: mode = "SMART"; modeId=3; break;
            }
        }
        if (mode == "N/A") { //this should never happen, but it does
            _LOG_A("ERROR: mode=%s, Mode=%u, modeId=%d, AccessStatus=%u.\n", mode.c_str(), Mode, modeId, AccessStatus);
        }
        String backlight = "N/A";
        switch(BacklightSet) {
            case 0: backlight = "OFF"; break;
            case 1: backlight = "ON"; break;
            case 2: backlight = "DIMMED"; break;
        }
        String evstate = StrStateNameWeb[State];
        String error = getErrorNameWeb(ErrorFlags);
        int errorId = getErrorId(ErrorFlags);

        if (ErrorFlags & LESS_6A) {
            evstate += " - " + error;
            error = "None";
            errorId = 0;
        }

        boolean evConnected = pilot != PILOT_12V;                    //when access bit = 1, p.ex. in OFF mode, the STATEs are no longer updated

        DynamicJsonDocument doc(4096); // https://arduinojson.org/v6/assistant/ (3200 + nodes + circuit_meter)
        doc["version"] = String(VERSION);
        doc["serialnr"] = serialnr;
        doc["mode"] = mode;
        doc["mode_id"] = modeId;
        doc["car_connected"] = evConnected;

        if(WiFi.isConnected()) {
            switch(WiFi.status()) {
                case WL_NO_SHIELD:          doc["wifi"]["status"] = "WL_NO_SHIELD"; break;
                case WL_IDLE_STATUS:        doc["wifi"]["status"] = "WL_IDLE_STATUS"; break;
                case WL_NO_SSID_AVAIL:      doc["wifi"]["status"] = "WL_NO_SSID_AVAIL"; break;
                case WL_SCAN_COMPLETED:     doc["wifi"]["status"] = "WL_SCAN_COMPLETED"; break;
                case WL_CONNECTED:          doc["wifi"]["status"] = "WL_CONNECTED"; break;
                case WL_CONNECT_FAILED:     doc["wifi"]["status"] = "WL_CONNECT_FAILED"; break;
                case WL_CONNECTION_LOST:    doc["wifi"]["status"] = "WL_CONNECTION_LOST"; break;
                case WL_DISCONNECTED:       doc["wifi"]["status"] = "WL_DISCONNECTED"; break;
                default:                    doc["wifi"]["status"] = "UNKNOWN"; break;
            }

            doc["wifi"]["ssid"] = WiFi.SSID();
            doc["wifi"]["rssi"] = WiFi.RSSI();
            doc["wifi"]["bssid"] = WiFi.BSSIDstr();
        }

        doc["evse"]["temp"] = TempEVSE;
        doc["evse"]["temp_max"] = maxTemp;
        doc["evse"]["connected"] = evConnected;
        doc["evse"]["access"] = AccessStatus;
        doc["evse"]["mode"] = Mode;
        doc["evse"]["loadbl"] = LoadBl;
        doc["evse"]["pwm"] = CurrentPWM;
        doc["evse"]["custombutton"] = CustomButton;
        doc["evse"]["solar_stop_timer"] = SolarStopTimer;
        doc["evse"]["state"] = evstate;
        doc["evse"]["state_id"] = State;
        doc["evse"]["error"] = error;
        doc["evse"]["error_id"] = errorId;
        doc["evse"]["rfidreader"] = StrRFIDReader[RFIDReader];
        doc["evse"]["nrofphases"] = Nr_Of_Phases_Charging;
        doc["evse"]["rfid"] = !RFIDReader ? "Not Installed" : RFIDstatus >= 8 ? "NOSTATUS" : StrRFIDStatusWeb[RFIDstatus];
        char iec_buf[2] = {evse_state_to_iec61851(State, ErrorFlags), '\0'};
        doc["evse"]["iec61851_state"] = iec_buf;
        doc["evse"]["charging_enabled"] = evse_charging_enabled(State);
        if (RFIDReader) {
            char buf[15];
            printRFID(buf, sizeof(buf));
            doc["evse"]["rfid_lastread"] = buf;
        }

        doc["settings"]["charge_current"] = Balanced[0];
        doc["settings"]["override_current"] = OverrideCurrent;
        doc["settings"]["current_min"] = MinCurrent;
        doc["settings"]["current_max"] = MaxCurrent;
        doc["settings"]["current_main"] = MaxMains;
        doc["settings"]["current_max_circuit"] = MaxCircuit;
        doc["settings"]["current_max_sum_mains"] = MaxSumMains;
        doc["settings"]["circuit_meter_type"] = CircuitMeter.Type;
        doc["settings"]["circuit_meter_address"] = CircuitMeter.Address;
        doc["settings"]["max_circuit_mains"] = MaxCircuitMains;
        doc["settings"]["max_sum_mains_time"] = MaxSumMainsTime;
        doc["settings"]["solar_max_import"] = ImportCurrent;
        doc["settings"]["solar_start_current"] = StartCurrent;
        doc["settings"]["solar_stop_time"] = StopTime;
        doc["settings"]["enable_C2"] = StrEnableC2[EnableC2];
        doc["settings"]["mains_meter"] = EMConfig[MainsMeter.Type].Desc;
        doc["settings"]["starttime"] = (DelayedStartTime.epoch2 ? DelayedStartTime.epoch2 + EPOCH2_OFFSET : 0);
        doc["settings"]["stoptime"] = (DelayedStopTime.epoch2 ? DelayedStopTime.epoch2 + EPOCH2_OFFSET : 0);
        doc["settings"]["repeat"] = DelayedRepeat;
        doc["settings"]["lcdlock"] = LCDlock;
        doc["settings"]["lock"] = Lock;
        doc["settings"]["cablelock"] = CableLock;
        doc["settings"]["prio_strategy"] = PrioStrategy;
        doc["settings"]["rotation_interval"] = RotationInterval;
        doc["settings"]["idle_timeout"] = IdleTimeout;
        doc["settings"]["capacity_limit"] = CapacityLimit;
        doc["settings"]["capacity_window_avg"] = capacity_get_window_avg_w(&CapacityState);
        doc["settings"]["capacity_monthly_peak"] = capacity_get_monthly_peak_w(&CapacityState);
        doc["settings"]["capacity_headroom"] = capacity_get_headroom_w(&CapacityState);

        if (LoadBl == 1) {
            static const char *StrSchedState[] = {"Inactive", "Active", "Paused"};
            for (int i = 0; i < NR_EVSES; i++) {
                doc["schedule"]["state"][i] = (ScheduleState[i] <= 2) ? StrSchedState[ScheduleState[i]] : "N/A";
            }
            doc["schedule"]["rotation_timer"] = RotationTimer;
            // BEGIN PLAN-07: Per-node load balancing data
            static const char *StrBalState[] = {"Idle", "Request", "Charging"};
            for (int i = 0; i < NR_EVSES; i++) {
                doc["nodes"][i]["current"] = Balanced[i];
                doc["nodes"][i]["state"] = (BalancedState[i] <= 2) ? StrBalState[BalancedState[i]] : "N/A";
                doc["nodes"][i]["sched"] = (ScheduleState[i] <= 2) ? StrSchedState[ScheduleState[i]] : "N/A";
            }
            // END PLAN-07
        }
#if MODEM
            doc["settings"]["required_evccid"] = RequiredEVCCID;
#if SMARTEVSE_VERSION < 40
            doc["settings"]["modem"] = "Experiment";
#else
            doc["settings"]["modem"] = "QCA7000";
#endif
            doc["ev_state"]["initial_soc"] = InitialSoC;
            doc["ev_state"]["remaining_soc"] = RemainingSoC;
            doc["ev_state"]["full_soc"] = FullSoC;
            doc["ev_state"]["energy_capacity"] = EnergyCapacity > 0 ? EnergyCapacity : -1; // Wh
            doc["ev_state"]["energy_request"] = EnergyRequest > 0 ? EnergyRequest : -1; // Wh
            doc["ev_state"]["computed_soc"] = ComputedSoC;
            doc["ev_state"]["evccid"] = EVCCID;
            doc["ev_state"]["time_until_full"] = TimeUntilFull;
#endif

#if MQTT
        doc["mqtt"]["host"] = MQTTHost;
        doc["mqtt"]["port"] = MQTTPort;
        doc["mqtt"]["topic_prefix"] = MQTTprefix;
        doc["mqtt"]["username"] = MQTTuser;
        doc["mqtt"]["password_set"] = MQTTpassword != "";
        doc["mqtt"]["tls"] = MQTTtls;
        if (MQTTclient.connected) {
            doc["mqtt"]["status"] = "Connected";
        } else {
            doc["mqtt"]["status"] = "Disconnected";
        }
        doc["mqtt"]["smartevse_server"] = MQTTSmartServer;
        doc["mqtt"]["change_only"] = MQTTChangeOnly;
        doc["mqtt"]["heartbeat"] = MQTTHeartbeat;
#endif

#if ENABLE_OCPP && defined(SMARTEVSE_VERSION) //run OCPP only on ESP32
        doc["ocpp"]["mode"] = OcppMode ? "Enabled" : "Disabled";
        doc["ocpp"]["backend_url"] = OcppWsClient ? OcppWsClient->getBackendUrl() : "";
        doc["ocpp"]["cb_id"] = OcppWsClient ? OcppWsClient->getChargeBoxId() : "";
        doc["ocpp"]["auth_key"] = OcppWsClient ? OcppWsClient->getAuthKey() : "";

        {
            auto freevendMode = MicroOcpp::getConfigurationPublic(MO_CONFIG_EXT_PREFIX "FreeVendActive");
            doc["ocpp"]["auto_auth"] = freevendMode && freevendMode->getBool() ? "Enabled" : "Disabled";
            auto freevendIdTag = MicroOcpp::getConfigurationPublic(MO_CONFIG_EXT_PREFIX "FreeVendIdTag");
            doc["ocpp"]["auto_auth_idtag"] = freevendIdTag ? freevendIdTag->getString() : "";
        }

        if (OcppWsClient && OcppWsClient->isConnected()) {
            doc["ocpp"]["status"] = "Connected";
        } else {
            doc["ocpp"]["status"] = "Disconnected";
        }

        // OCPP telemetry
        doc["ocpp"]["tx_active"] = OcppTelemetry.tx_active;
        doc["ocpp"]["tx_starts"] = OcppTelemetry.tx_start_count;
        doc["ocpp"]["tx_stops"] = OcppTelemetry.tx_stop_count;
        doc["ocpp"]["auth_accepts"] = OcppTelemetry.auth_accept_count;
        doc["ocpp"]["auth_rejects"] = OcppTelemetry.auth_reject_count;
        doc["ocpp"]["auth_timeouts"] = OcppTelemetry.auth_timeout_count;
        doc["ocpp"]["smart_charging_active"] = (!LoadBl && OcppCurrentLimit >= 0.0f);
        doc["ocpp"]["current_limit_a"] = OcppCurrentLimit >= 0.0f ? OcppCurrentLimit : -1;
        doc["ocpp"]["lb_conflict"] = OcppTelemetry.lb_conflict;
#endif //ENABLE_OCPP

        doc["home_battery"]["current"] = homeBatteryCurrent;
        doc["home_battery"]["last_update"] = homeBatteryLastUpdate;

        doc["ev_meter"]["description"] = EMConfig[EVMeter.Type].Desc;
        doc["ev_meter"]["address"] = EVMeter.Address;
        doc["ev_meter"]["import_active_power"] = EVMeter.PowerMeasured; // Watt
        doc["ev_meter"]["total_wh"] = EVMeter.Energy; // Wh
        doc["ev_meter"]["charged_wh"] = EVMeter.EnergyCharged; // Wh
        doc["ev_meter"]["currents"]["TOTAL"] = EVMeter.Irms[0] + EVMeter.Irms[1] + EVMeter.Irms[2];
        doc["ev_meter"]["currents"]["L1"] = EVMeter.Irms[0];
        doc["ev_meter"]["currents"]["L2"] = EVMeter.Irms[1];
        doc["ev_meter"]["currents"]["L3"] = EVMeter.Irms[2];
        doc["ev_meter"]["import_active_energy"] = EVMeter.Import_active_energy; // Wh
        doc["ev_meter"]["export_active_energy"] = EVMeter.Export_active_energy; // Wh

        doc["mains_meter"]["import_active_energy"] = MainsMeter.Import_active_energy; // Wh
        doc["mains_meter"]["export_active_energy"] = MainsMeter.Export_active_energy; // Wh
        if (MainsMeter.Type == EM_HOMEWIZARD_P1) {
            doc["mains_meter"]["host"] = !homeWizardHost.isEmpty() ? homeWizardHost : "HomeWizard P1 Not Found";
        }

        // BEGIN PLAN-14: CircuitMeter data in /settings GET
        if (CircuitMeter.Type) {
            doc["circuit_meter"]["description"] = EMConfig[CircuitMeter.Type].Desc;
            doc["circuit_meter"]["address"] = CircuitMeter.Address;
            doc["circuit_meter"]["import_active_energy"] = CircuitMeter.Import_active_energy;
            doc["circuit_meter"]["export_active_energy"] = CircuitMeter.Export_active_energy;
            doc["circuit_meter"]["power"] = CircuitMeter.PowerMeasured;
            doc["circuit_meter"]["currents"]["TOTAL"] = CircuitMeter.Irms[0] + CircuitMeter.Irms[1] + CircuitMeter.Irms[2];
            doc["circuit_meter"]["currents"]["L1"] = CircuitMeter.Irms[0];
            doc["circuit_meter"]["currents"]["L2"] = CircuitMeter.Irms[1];
            doc["circuit_meter"]["currents"]["L3"] = CircuitMeter.Irms[2];
        }
        // END PLAN-14

        doc["phase_currents"]["TOTAL"] = MainsMeter.Irms[0] + MainsMeter.Irms[1] + MainsMeter.Irms[2];
        doc["phase_currents"]["L1"] = MainsMeter.Irms[0];
        doc["phase_currents"]["L2"] = MainsMeter.Irms[1];
        doc["phase_currents"]["L3"] = MainsMeter.Irms[2];
        doc["phase_currents"]["last_data_update"] = phasesLastUpdate;
        doc["phase_currents"]["original_data"]["TOTAL"] = IrmsOriginal[0] + IrmsOriginal[1] + IrmsOriginal[2];
        doc["phase_currents"]["original_data"]["L1"] = IrmsOriginal[0];
        doc["phase_currents"]["original_data"]["L2"] = IrmsOriginal[1];
        doc["phase_currents"]["original_data"]["L3"] = IrmsOriginal[2];

        doc["backlight"]["timer"] = BacklightTimer;
        doc["backlight"]["status"] = backlight;

        doc["color"]["off"]["R"] = ColorOff[0];
        doc["color"]["off"]["G"] = ColorOff[1];
        doc["color"]["off"]["B"] = ColorOff[2];
        doc["color"]["normal"]["R"] = ColorNormal[0];
        doc["color"]["normal"]["G"] = ColorNormal[1];
        doc["color"]["normal"]["B"] = ColorNormal[2];
        doc["color"]["smart"]["R"] = ColorSmart[0];
        doc["color"]["smart"]["G"] = ColorSmart[1];
        doc["color"]["smart"]["B"] = ColorSmart[2];
        doc["color"]["solar"]["R"] = ColorSolar[0];
        doc["color"]["solar"]["G"] = ColorSolar[1];
        doc["color"]["solar"]["B"] = ColorSolar[2];
        doc["color"]["custom"]["R"] = ColorCustom[0];
        doc["color"]["custom"]["G"] = ColorCustom[1];
        doc["color"]["custom"]["B"] = ColorCustom[2];

        String json;
        serializeJson(doc, json);
        mg_http_reply(c, 200, "Content-Type: application/json\r\n", "%s\n", json.c_str());    // Yes. Respond JSON
        return true;
      } else if (!memcmp("POST", hm->method.buf, hm->method.len)) {
#if MQTT
        // Process MQTT publish settings before mqtt_update early return,
        // because configureMqtt() bundles these with mqtt_update=1
        if(request->hasParam("mqtt_heartbeat")) {
            int val = request->getParam("mqtt_heartbeat")->value().toInt();
            if(!http_api_validate_mqtt_heartbeat(val)) {
                MQTTHeartbeat = val;
                mqtt_cache.heartbeat_s = MQTTHeartbeat;
            }
        }
        if(request->hasParam("mqtt_change_only")) {
            int val = request->getParam("mqtt_change_only")->value().toInt();
            if(!http_api_validate_mqtt_change_only(val)) {
                MQTTChangeOnly = (val == 1);
            }
        }
#endif
        if(request->hasParam("mqtt_update")) {
#if MQTT
            request_write_settings();  // persist mqtt_heartbeat/mqtt_change_only if changed above
#endif
            return false;                                                       // handled in network.cpp
        }
        DynamicJsonDocument doc(512); // https://arduinojson.org/v6/assistant/

        if(request->hasParam("backlight")) {
            int backlight = request->getParam("backlight")->value().toInt();
            BacklightTimer = backlight * BACKLIGHT;
            doc["Backlight"] = backlight;
        }

        if(request->hasParam("current_min")) {
            int current = request->getParam("current_min")->value().toInt();
            const char *err = http_api_validate_current_min(current, LoadBl);
            if(!err) {
                MinCurrent = current;
                doc["current_min"] = MinCurrent;
            } else {
                doc["current_min"] = err;
            }
        }

        if(request->hasParam("current_max_sum_mains")) {
            int current = request->getParam("current_max_sum_mains")->value().toInt();
            const char *err = http_api_validate_max_sum_mains(current, LoadBl);
            if(!err) {
                MaxSumMains = current;
                doc["current_max_sum_mains"] = MaxSumMains;
            } else {
                doc["current_max_sum_mains"] = err;
            }
        }

        // BEGIN PLAN-14: MaxCircuitMains via REST
        if(request->hasParam("max_circuit_mains")) {
            int current = request->getParam("max_circuit_mains")->value().toInt();
            if (LoadBl < 2 && (current == 0 || (current >= 10 && current <= 600))) {
                MaxCircuitMains = current;
                doc["max_circuit_mains"] = MaxCircuitMains;
            } else {
                doc["max_circuit_mains"] = "Value not allowed!";
            }
        }
        // END PLAN-14

        if(request->hasParam("max_sum_mains_timer")) {
            int time = request->getParam("max_sum_mains_timer")->value().toInt();
            if(time >= 0 && time <= 60 && LoadBl < 2) {
                MaxSumMainsTime = time;
                doc["max_sum_mains_time"] = MaxSumMainsTime;
            } else {
                doc["max_sum_mains_time"] = "Value not allowed!";
            }
        }

        if(request->hasParam("capacity_limit")) {
            int val = request->getParam("capacity_limit")->value().toInt();
            if (val >= 0 && val <= 25000) {
                CapacityLimit = (uint16_t)val;
                capacity_set_limit(&CapacityState, (int32_t)CapacityLimit);
                doc["capacity_limit"] = CapacityLimit;
            } else {
                doc["capacity_limit"] = "Value not allowed! (0-25000)";
            }
        }

        if(request->hasParam("disable_override_current")) {
            setOverrideCurrent(0);
            doc["disable_override_current"] = "OK";
        }

        if(request->hasParam("custombutton")) {
            CustomButton = request->getParam("custombutton")->value().toInt() > 0;
            doc["custombutton"] = CustomButton;
        }

        if(request->hasParam("mode")) {
            String mode = request->getParam("mode")->value();

            //first check if we have a delayed mode switch
            if(request->hasParam("starttime")) {
                String DelayedStartTimeStr = request->getParam("starttime")->value();
                //string time_str = "2023-04-14T11:31";
                if (!StoreTimeString(DelayedStartTimeStr, &DelayedStartTime)) {
                    //parse OK
                    if (DelayedStartTime.diff > 0)
                        setAccess(OFF);                         //switch to OFF, we are Delayed Charging
                    else {//we are in the past so no delayed charging
                        DelayedStartTime.epoch2 = DELAYEDSTARTTIME;
                        DelayedStopTime.epoch2 = DELAYEDSTOPTIME;
                        DelayedRepeat = 0;
                    }
                }
                else {
                    //we couldn't parse the string, so we are NOT Delayed Charging
                    DelayedStartTime.epoch2 = DELAYEDSTARTTIME;
                    DelayedStopTime.epoch2 = DELAYEDSTOPTIME;
                    DelayedRepeat = 0;
                }

                // so now we might have a starttime and we might be Delayed Charging
                if (DelayedStartTime.epoch2) {
                    //we only accept a DelayedStopTime if we have a valid DelayedStartTime
                    if(request->hasParam("stoptime")) {
                        String DelayedStopTimeStr = request->getParam("stoptime")->value();
                        //string time_str = "2023-04-14T11:31";
                        if (!StoreTimeString(DelayedStopTimeStr, &DelayedStopTime)) {
                            //parse OK
                            if (DelayedStopTime.diff <= 0 || DelayedStopTime.epoch2 <= DelayedStartTime.epoch2)
                                //we are in the past or DelayedStopTime before DelayedStartTime so no DelayedStopTime
                                DelayedStopTime.epoch2 = DELAYEDSTOPTIME;
                        }
                        else
                            //we couldn't parse the string, so no DelayedStopTime
                            DelayedStopTime.epoch2 = DELAYEDSTOPTIME;
                        doc["stoptime"] = (DelayedStopTime.epoch2 ? DelayedStopTime.epoch2 + EPOCH2_OFFSET : 0);
                        if(request->hasParam("repeat")) {
                            int Repeat = request->getParam("repeat")->value().toInt();
                            if (Repeat >= 0 && Repeat <= 1) {                                   //boundary check
                                DelayedRepeat = Repeat;
                                doc["repeat"] = Repeat;
                            }
                        }
                    }

                }
                doc["starttime"] = (DelayedStartTime.epoch2 ? DelayedStartTime.epoch2 + EPOCH2_OFFSET : 0);
            } else
                DelayedStartTime.epoch2 = DELAYEDSTARTTIME;


            switch(mode.toInt()) {
                case 0: // OFF
#if SMARTEVSE_VERSION >=40 //v4
                    Serial1.printf("@ResetModemTimers\n");
#endif
                    setAccess(OFF);
                    break;
                case 1:
                    setMode(MODE_NORMAL);
                    break;
                case 2:
                    setMode(MODE_SOLAR);
                    break;
                case 3:
                    setMode(MODE_SMART);
                    break;
                case 4: // PAUSE
                    setAccess(PAUSE);
                    break;
                default:
                    mode = "Value not allowed!";
            }
            doc["mode"] = mode;
        }

        if(request->hasParam("enable_C2")) {
            EnableC2 = (EnableC2_t) request->getParam("enable_C2")->value().toInt();
            doc["settings"]["enable_C2"] = StrEnableC2[EnableC2];
        }

        if(request->hasParam("phases")) {
            int phases = request->getParam("phases")->value().toInt();
            http_phase_switch_request_t req = { .phases = phases };
            const char *err = http_api_validate_phase_switch(&req, EnableC2, LoadBl);
            if (!err) {
                bool switching = false;
                int prev = Nr_Of_Phases_Charging;
                if (phases == 1 && Nr_Of_Phases_Charging != 1) {
                    Switching_Phases_C2 = GOING_TO_SWITCH_1P;
                    switching = true;
                } else if (phases == 3 && Nr_Of_Phases_Charging != 3) {
                    Switching_Phases_C2 = GOING_TO_SWITCH_3P;
                    switching = true;
                }
                doc["phases"] = phases;
                doc["switching"] = switching;
                doc["previous_phases"] = prev;
            } else {
                doc["phases"] = err;
            }
        }

        if(request->hasParam("stop_timer")) {
            int stop_timer = request->getParam("stop_timer")->value().toInt();
            const char *err = http_api_validate_stop_timer(stop_timer);
            if(!err) {
                StopTime = stop_timer;
                doc["stop_timer"] = true;
            } else {
                doc["stop_timer"] = false;
            }
        }

        if(Mode == MODE_NORMAL || Mode == MODE_SMART) {
            if(request->hasParam("override_current")) {
                int current = request->getParam("override_current")->value().toInt();
                const char *err = http_api_validate_override_current(current, MinCurrent, MaxCurrent, LoadBl);
                if (!err) {
                    setOverrideCurrent(current);
                    doc["override_current"] = OverrideCurrent;
                } else {
                    doc["override_current"] = err;
                }
            }
        }

        if(request->hasParam("solar_start_current")) {
            int current = request->getParam("solar_start_current")->value().toInt();
            const char *err = http_api_validate_solar_start(current);
            if(!err) {
                StartCurrent = current;
                doc["solar_start_current"] = StartCurrent;
            } else {
                doc["solar_start_current"] = err;
            }
        }

        if(request->hasParam("solar_max_import")) {
            int current = request->getParam("solar_max_import")->value().toInt();
            const char *err = http_api_validate_solar_max_import(current);
            if(!err) {
                ImportCurrent = current;
                doc["solar_max_import"] = ImportCurrent;
            } else {
                doc["solar_max_import"] = err;
            }
        }

        //special section to post stuff for experimenting with an ISO15118 modem
        if(request->hasParam("override_pwm")) {
            int pwm = request->getParam("override_pwm")->value().toInt();
            if (pwm == 0){
                PILOT_DISCONNECTED;
                CPDutyOverride = true;
            } else if (pwm < 0){
                PILOT_CONNECTED;
                CPDutyOverride = false;
                pwm = 100; // 10% until next loop, to be safe, corresponds to 6A
            } else{
                PILOT_CONNECTED;
                CPDutyOverride = true;
            }

            SetCPDuty(pwm);
            doc["override_pwm"] = pwm;
        }
#if MODEM
        //allow basic plug 'n charge based on evccid
        //if required_evccid is set to a value, SmartEVSE will only allow charging requests from said EVCCID
        if(request->hasParam("required_evccid")) {
            if (request->getParam("required_evccid")->value().length() <= 32) {
                strncpy(RequiredEVCCID, request->getParam("required_evccid")->value().c_str(), sizeof(RequiredEVCCID) - 1);
                RequiredEVCCID[sizeof(RequiredEVCCID) - 1] = '\0';
                doc["required_evccid"] = RequiredEVCCID;
                Serial1.printf("@RequiredEVCCID:%s\n", RequiredEVCCID);
            } else {
                doc["required_evccid"] = "EVCCID too long (max 32 char)";
            }
        }
#endif
        if(request->hasParam("prio_strategy")) {
            int val = request->getParam("prio_strategy")->value().toInt();
            const char *err = http_api_validate_prio_strategy(val, LoadBl);
            if(!err) {
                setItemValue(MENU_PRIO, val);
                doc["prio_strategy"] = PrioStrategy;
            } else {
                doc["prio_strategy"] = err;
            }
        }

        if(request->hasParam("rotation_interval")) {
            int val = request->getParam("rotation_interval")->value().toInt();
            const char *err = http_api_validate_rotation_interval(val, LoadBl);
            if(!err) {
                setItemValue(MENU_ROTATION, val);
                doc["rotation_interval"] = RotationInterval;
            } else {
                doc["rotation_interval"] = err;
            }
        }

        if(request->hasParam("idle_timeout")) {
            int val = request->getParam("idle_timeout")->value().toInt();
            const char *err = http_api_validate_idle_timeout(val, LoadBl);
            if(!err) {
                setItemValue(MENU_IDLE_TIMEOUT, val);
                doc["idle_timeout"] = IdleTimeout;
            } else {
                doc["idle_timeout"] = err;
            }
        }

        if(request->hasParam("lcdlock")) {
            int lock = request->getParam("lcdlock")->value().toInt();
            if (lock >= 0 && lock <= 1) {                                   //boundary check
                LCDlock = lock;
                doc["lcdlock"] = lock;
            }
        }

        if(request->hasParam("cablelock")) {
            int c_lock = request->getParam("cablelock")->value().toInt();
            if (c_lock >= 0 && c_lock <= 1) {                               //boundary check
                CableLock = c_lock;
                doc["cablelock"] = c_lock;
            }
        }

#if ENABLE_OCPP && defined(SMARTEVSE_VERSION) //run OCPP only on ESP32
        if(request->hasParam("ocpp_update")) {
            if (request->getParam("ocpp_update")->value().toInt() == 1) {

                if(request->hasParam("ocpp_mode")) {
                    OcppMode = request->getParam("ocpp_mode")->value().toInt();
                    doc["ocpp_mode"] = OcppMode;
                }

                if(request->hasParam("ocpp_backend_url")) {
                    const char *url = request->getParam("ocpp_backend_url")->value().c_str();
                    ocpp_validate_result_t vr = ocpp_validate_backend_url(url);
                    if (vr != OCPP_VALIDATE_OK) {
                        doc["ocpp_backend_url"] = vr == OCPP_VALIDATE_EMPTY ? "URL is empty"
                                                : vr == OCPP_VALIDATE_BAD_SCHEME ? "URL must start with ws:// or wss://"
                                                : "Invalid URL";
                    } else if (OcppWsClient) {
                        OcppWsClient->setBackendUrl(url);
                        doc["ocpp_backend_url"] = OcppWsClient->getBackendUrl();
                    } else {
                        doc["ocpp_backend_url"] = "Can only update when OCPP enabled";
                    }
                }

                if(request->hasParam("ocpp_cb_id")) {
                    const char *cb_id = request->getParam("ocpp_cb_id")->value().c_str();
                    ocpp_validate_result_t vr = ocpp_validate_chargebox_id(cb_id);
                    if (vr != OCPP_VALIDATE_OK) {
                        doc["ocpp_cb_id"] = vr == OCPP_VALIDATE_EMPTY ? "ChargeBoxId is empty"
                                          : vr == OCPP_VALIDATE_TOO_LONG ? "ChargeBoxId exceeds 20 characters"
                                          : vr == OCPP_VALIDATE_BAD_CHARS ? "ChargeBoxId contains invalid characters"
                                          : "Invalid ChargeBoxId";
                    } else if (OcppWsClient) {
                        OcppWsClient->setChargeBoxId(cb_id);
                        doc["ocpp_cb_id"] = OcppWsClient->getChargeBoxId();
                    } else {
                        doc["ocpp_cb_id"] = "Can only update when OCPP enabled";
                    }
                }

                if(request->hasParam("ocpp_auth_key")) {
                    const char *auth_key = request->getParam("ocpp_auth_key")->value().c_str();
                    ocpp_validate_result_t vr = ocpp_validate_auth_key(auth_key);
                    if (vr != OCPP_VALIDATE_OK) {
                        doc["ocpp_auth_key"] = vr == OCPP_VALIDATE_TOO_LONG ? "Auth key exceeds 40 characters"
                                             : "Invalid auth key";
                    } else if (OcppWsClient) {
                        OcppWsClient->setAuthKey(auth_key);
                        doc["ocpp_auth_key"] = OcppWsClient->getAuthKey();
                    } else {
                        doc["ocpp_auth_key"] = "Can only update when OCPP enabled";
                    }
                }

                if(request->hasParam("ocpp_auto_auth")) {
                    auto freevendMode = MicroOcpp::getConfigurationPublic(MO_CONFIG_EXT_PREFIX "FreeVendActive");
                    if (freevendMode) {
                        freevendMode->setBool(request->getParam("ocpp_auto_auth")->value().toInt());
                        doc["ocpp_auto_auth"] = freevendMode->getBool() ? 1 : 0;
                    } else {
                        doc["ocpp_auto_auth"] = "Can only update when OCPP enabled";
                    }
                }

                if(request->hasParam("ocpp_auto_auth_idtag")) {
                    auto freevendIdTag = MicroOcpp::getConfigurationPublic(MO_CONFIG_EXT_PREFIX "FreeVendIdTag");
                    if (freevendIdTag) {
                        freevendIdTag->setString(request->getParam("ocpp_auto_auth_idtag")->value().c_str());
                        doc["ocpp_auto_auth_idtag"] = freevendIdTag->getString();
                    } else {
                        doc["ocpp_auto_auth_idtag"] = "Can only update when OCPP enabled";
                    }
                }

                // Apply changes in OcppWsClient
                if (OcppWsClient) {
                    OcppWsClient->reloadConfigs();
                }
                MicroOcpp::configuration_save();
            }
        }
#endif //ENABLE_OCPP

        String json;
        serializeJson(doc, json);
        mg_http_reply(c, 200, "Content-Type: application/json\r\n", "%s\n", json.c_str());    // Yes. Respond JSON
        request_write_settings();
        return true;
      }
    } else if (mg_http_match_uri(hm, "/color_off") && !memcmp("POST", hm->method.buf, hm->method.len)) {
        DynamicJsonDocument doc(200);
        if (request->hasParam("R") && request->hasParam("G") && request->hasParam("B")) {
            uint8_t r, g, b;
            if (http_api_parse_color(request->getParam("R")->value().toInt(),
                                     request->getParam("G")->value().toInt(),
                                     request->getParam("B")->value().toInt(), &r, &g, &b)) {
                ColorOff[0] = r; ColorOff[1] = g; ColorOff[2] = b;
                doc["color"]["off"]["R"] = r; doc["color"]["off"]["G"] = g; doc["color"]["off"]["B"] = b;
            }
        }
        String json;
        serializeJson(doc, json);
        mg_http_reply(c, 200, "Content-Type: application/json\r\n", "%s\r\n", json.c_str());
        return true;
    } else if (mg_http_match_uri(hm, "/color_normal") && !memcmp("POST", hm->method.buf, hm->method.len)) {
        DynamicJsonDocument doc(200);
        if (request->hasParam("R") && request->hasParam("G") && request->hasParam("B")) {
            uint8_t r, g, b;
            if (http_api_parse_color(request->getParam("R")->value().toInt(),
                                     request->getParam("G")->value().toInt(),
                                     request->getParam("B")->value().toInt(), &r, &g, &b)) {
                ColorNormal[0] = r; ColorNormal[1] = g; ColorNormal[2] = b;
                doc["color"]["normal"]["R"] = r; doc["color"]["normal"]["G"] = g; doc["color"]["normal"]["B"] = b;
            }
        }
        String json;
        serializeJson(doc, json);
        mg_http_reply(c, 200, "Content-Type: application/json\r\n", "%s\r\n", json.c_str());
        return true;
    } else if (mg_http_match_uri(hm, "/color_smart") && !memcmp("POST", hm->method.buf, hm->method.len)) {
        DynamicJsonDocument doc(200);
        if (request->hasParam("R") && request->hasParam("G") && request->hasParam("B")) {
            uint8_t r, g, b;
            if (http_api_parse_color(request->getParam("R")->value().toInt(),
                                     request->getParam("G")->value().toInt(),
                                     request->getParam("B")->value().toInt(), &r, &g, &b)) {
                ColorSmart[0] = r; ColorSmart[1] = g; ColorSmart[2] = b;
                doc["color"]["smart"]["R"] = r; doc["color"]["smart"]["G"] = g; doc["color"]["smart"]["B"] = b;
            }
        }
        String json;
        serializeJson(doc, json);
        mg_http_reply(c, 200, "Content-Type: application/json\r\n", "%s\r\n", json.c_str());
        return true;
    } else if (mg_http_match_uri(hm, "/color_solar") && !memcmp("POST", hm->method.buf, hm->method.len)) {
        DynamicJsonDocument doc(200);
        if (request->hasParam("R") && request->hasParam("G") && request->hasParam("B")) {
            uint8_t r, g, b;
            if (http_api_parse_color(request->getParam("R")->value().toInt(),
                                     request->getParam("G")->value().toInt(),
                                     request->getParam("B")->value().toInt(), &r, &g, &b)) {
                ColorSolar[0] = r; ColorSolar[1] = g; ColorSolar[2] = b;
                doc["color"]["solar"]["R"] = r; doc["color"]["solar"]["G"] = g; doc["color"]["solar"]["B"] = b;
            }
        }
        String json;
        serializeJson(doc, json);
        mg_http_reply(c, 200, "Content-Type: application/json\r\n", "%s\r\n", json.c_str());
        return true;
    } else if (mg_http_match_uri(hm, "/color_custom") && !memcmp("POST", hm->method.buf, hm->method.len)) {
        DynamicJsonDocument doc(200);
        if (request->hasParam("R") && request->hasParam("G") && request->hasParam("B")) {
            uint8_t r, g, b;
            if (http_api_parse_color(request->getParam("R")->value().toInt(),
                                     request->getParam("G")->value().toInt(),
                                     request->getParam("B")->value().toInt(), &r, &g, &b)) {
                ColorCustom[0] = r; ColorCustom[1] = g; ColorCustom[2] = b;
                doc["color"]["custom"]["R"] = r; doc["color"]["custom"]["G"] = g; doc["color"]["custom"]["B"] = b;
            }
        }
        String json;
        serializeJson(doc, json);
        mg_http_reply(c, 200, "Content-Type: application/json\r\n", "%s\r\n", json.c_str());
        return true;
    } else if (mg_http_match_uri(hm, "/currents") && !memcmp("POST", hm->method.buf, hm->method.len)) {
        DynamicJsonDocument doc(200);

        if(request->hasParam("battery_current")) {
            if (LoadBl < 2) {
                homeBatteryCurrent = request->getParam("battery_current")->value().toInt();
                homeBatteryLastUpdate = time(NULL);
                doc["battery_current"] = homeBatteryCurrent;
            } else
                doc["battery_current"] = "not allowed on slave";
        }

        if(MainsMeter.Type == EM_API) {
            if(request->hasParam("L1") && request->hasParam("L2") && request->hasParam("L3")) {
                if (LoadBl < 2) {
#if SMARTEVSE_VERSION < 40 //v3
                    MainsMeter.Irms[0] = request->getParam("L1")->value().toInt();
                    MainsMeter.Irms[1] = request->getParam("L2")->value().toInt();
                    MainsMeter.Irms[2] = request->getParam("L3")->value().toInt();

                    CalcIsum();
                    MainsMeter.setTimeout(COMM_TIMEOUT);
#else  //v4
                    Serial1.printf("@Irms:%03u,%d,%d,%d\n", MainsMeter.Address, (int16_t) request->getParam("L1")->value().toInt(), (int16_t) request->getParam("L2")->value().toInt(), (int16_t) request->getParam("L3")->value().toInt()); //Irms:011,312,123,124 means: the meter on address 11(dec) has Irms[0] 312 dA, Irms[1] of 123 dA, Irms[2] of 124 dA
#endif
                    for (int x = 0; x < 3; x++) {
                        doc["original"]["L" + x] = IrmsOriginal[x];
                        doc["L" + x] = MainsMeter.Irms[x];
                    }
                    doc["TOTAL"] = Isum;

                } else
                    doc["TOTAL"] = "not allowed on slave";
            }
        }

        // BEGIN PLAN-14: CircuitMeter API feed via /currents POST
        if(CircuitMeter.Type == EM_API) {
            if(request->hasParam("circuit_L1") && request->hasParam("circuit_L2") && request->hasParam("circuit_L3")) {
                if (LoadBl < 2) {
#if SMARTEVSE_VERSION < 40 //v3
                    CircuitMeter.Irms[0] = request->getParam("circuit_L1")->value().toInt();
                    CircuitMeter.Irms[1] = request->getParam("circuit_L2")->value().toInt();
                    CircuitMeter.Irms[2] = request->getParam("circuit_L3")->value().toInt();
                    CircuitMeter.CalcImeasured();
                    CircuitMeter.setTimeout(COMM_TIMEOUT);
#else //v4
                    Serial1.printf("@Irms:%03u,%d,%d,%d\n", CircuitMeter.Address, (int16_t) request->getParam("circuit_L1")->value().toInt(), (int16_t) request->getParam("circuit_L2")->value().toInt(), (int16_t) request->getParam("circuit_L3")->value().toInt());
#endif
                    for (int x = 0; x < 3; x++)
                        doc["circuit"]["L" + x] = CircuitMeter.Irms[x];
                    doc["circuit"]["TOTAL"] = CircuitMeter.Irms[0] + CircuitMeter.Irms[1] + CircuitMeter.Irms[2];
                } else
                    doc["circuit"]["TOTAL"] = "not allowed on slave";
            }
        }
        // END PLAN-14

        String json;
        serializeJson(doc, json);
        mg_http_reply(c, 200, "Content-Type: application/json\r\n", "%s\r\n", json.c_str());    // Yes. Respond JSON
        return true;
    } else if (mg_http_match_uri(hm, "/ev_meter") && !memcmp("POST", hm->method.buf, hm->method.len)) {
        DynamicJsonDocument doc(200);

        if(EVMeter.Type == EM_API) {
            if(request->hasParam("L1") && request->hasParam("L2") && request->hasParam("L3")) {
#if SMARTEVSE_VERSION < 40 //v3
                EVMeter.Irms[0] = request->getParam("L1")->value().toInt();
                EVMeter.Irms[1] = request->getParam("L2")->value().toInt();
                EVMeter.Irms[2] = request->getParam("L3")->value().toInt();
                EVMeter.CalcImeasured();
                EVMeter.Timeout = COMM_EVTIMEOUT;
#else //v4
                Serial1.printf("@Irms:%03u,%d,%d,%d\n", EVMeter.Address, (int16_t) request->getParam("L1")->value().toInt(), (int16_t) request->getParam("L2")->value().toInt(), (int16_t) request->getParam("L3")->value().toInt()); //Irms:011,312,123,124 means: the meter on address 11(dec) has Irms[0] 312 dA, Irms[1] of 123 dA, Irms[2] of 124 dA
#endif
                for (int x = 0; x < 3; x++)
                    doc["ev_meter"]["currents"]["L" + x] = EVMeter.Irms[x];
                doc["ev_meter"]["currents"]["TOTAL"] = EVMeter.Irms[0] + EVMeter.Irms[1] + EVMeter.Irms[2];
            }

            if(request->hasParam("import_active_energy") && request->hasParam("export_active_energy") && request->hasParam("import_active_power")) {

                EVMeter.Import_active_energy = request->getParam("import_active_energy")->value().toInt();
                EVMeter.Export_active_energy = request->getParam("export_active_energy")->value().toInt();
#if SMARTEVSE_VERSION < 40 //v3
                EVMeter.PowerMeasured = request->getParam("import_active_power")->value().toInt();
#else //v4
                Serial1.printf("@PowerMeasured:%03u,%d\n", EVMeter.Address, (int16_t) request->getParam("import_active_power")->value().toInt());
#endif
                EVMeter.UpdateEnergies(); //we dont send the energies to CH32 because they are not used there
                doc["ev_meter"]["import_active_power"] = EVMeter.PowerMeasured;
                doc["ev_meter"]["import_active_energy"] = EVMeter.Import_active_energy;
                doc["ev_meter"]["export_active_energy"] = EVMeter.Export_active_energy;
                doc["ev_meter"]["total_kwh"] = EVMeter.Energy;
                doc["ev_meter"]["charged_kwh"] = EVMeter.EnergyCharged;
            }
        }

        String json;
        serializeJson(doc, json);
        mg_http_reply(c, 200, "Content-Type: application/json\r\n", "%s\r\n", json.c_str());    // Yes. Respond JSON
        return true;

    } else if (mg_http_match_uri(hm, "/lcd")) {
        if (strncmp("POST", hm->method.buf, hm->method.len) == 0) {
            DynamicJsonDocument doc(100);
            if (LCDPasswordOK) {
                const String btnName = request->getParam("button")->value();
                const bool btnDown = request->getParam("state")->value() == "1";

                // Button state bitmasks.
                static constexpr uint8_t RIGHT_MASK = 0b100;
                static constexpr uint8_t MIDDLE_MASK = 0b010;
                static constexpr uint8_t LEFT_MASK = 0b001;
                static constexpr uint8_t ALL_BUTTONS_UP = 0b111;
                static const std::unordered_map<std::string, uint8_t> btnMasks = {
                    {"right", RIGHT_MASK},
                    {"middle", MIDDLE_MASK},
                    {"left", LEFT_MASK}
                };

                xSemaphoreTake(buttonMutex, portMAX_DELAY);
                auto it = btnMasks.find(btnName.c_str());
                if (it != btnMasks.end()) {
                    // Clear bits if button is pressed, set bits if up.
                    const uint8_t mask = it->second;
                    if (btnDown) {
                        ButtonStateOverride = ALL_BUTTONS_UP & ~mask;
                    } else {
                        ButtonStateOverride = ALL_BUTTONS_UP | mask;
                    }
                    // Prevent stuck button in case we forget to reset to a 'down' button state.
                    LastBtnOverrideTime = millis();
                }
                xSemaphoreGive(buttonMutex);

                // Create JSON response
                doc["button"]["right"] = ButtonStateOverride & 4 ? "up" : "down";
                doc["button"]["middle"] = ButtonStateOverride & 2 ? "up" : "down";
                doc["button"]["left"] = ButtonStateOverride & 1 ? "up" : "down";
            } else { //LCDPasswordOK is false
                // Create JSON response; buttons are not pressed if we don't have the right password!
                doc["button"]["right"] = "down";
                doc["button"]["middle"] = "down";
                doc["button"]["left"] = "down";
            }
            // Serialize and send response
            String json;
            serializeJson(doc, json);
            mg_http_reply(c, 200, "Content-Type: application/json\r\n", "%s\r\n", json.c_str());
        } else {
            // Generate BMP image from LCD buffer.
    		const std::vector<uint8_t> bmpImage = createImageFromGLCDBuffer();
		    const size_t bmpImageSize = bmpImage.size();

            // Start the HTTP response with chunked encoding
            mg_printf(c,
                      "HTTP/1.1 200 OK\r\n"
                      "Content-Type: image/bmp\r\n"
                      "Connection: keep-alive\r\n"
                      "Cache-Control: no-cache\r\n"
                      "Transfer-Encoding: chunked\r\n"
                      "\r\n");

            // Using chunked transfer encoding to get rid of content-len + keep-alive problems.
            mg_http_write_chunk(c, reinterpret_cast<const char *>(bmpImage.data()), bmpImageSize);

            // Send an empty chunk to signal the end of the response.
            mg_http_write_chunk(c, "", 0);
        }
        return true;

    } else if (mg_http_match_uri(hm, "/lcd-verify-password") && !memcmp("POST", hm->method.buf, hm->method.len)) {
        char password[32];
        mg_http_get_var(&hm->body, "password", password, sizeof(password));
        DynamicJsonDocument doc(256);

        LCDPasswordOK = (atoi(password) == LCDPin);
        if (LCDPasswordOK) {
            doc["success"] = true;
        } else {
            doc["success"] = false;
        }

        String json;
        serializeJson(doc, json);
        mg_http_reply(c, 200, "Content-Type: application/json\r\n", "%s\r\n", json.c_str());
        return true;


    } else if (mg_http_match_uri(hm, "/cablelock") && !memcmp("POST", hm->method.buf, hm->method.len)) {
        DynamicJsonDocument doc(200);

        if(request->hasParam("1")) {
            CableLock = 1;
            doc["cablelock"] = CableLock;
        } else {
            CableLock = 0;
            doc["cablelock"] = CableLock;
        }

        String json;
        serializeJson(doc, json);
        mg_http_reply(c, 200, "Content-Type: application/json\r\n", "%s\r\n", json.c_str());    // Yes. Respond JSON
        return true;

    } else if (mg_http_match_uri(hm, "/rfid") && !memcmp("POST", hm->method.buf, hm->method.len)) {
        DynamicJsonDocument doc(200);

        uint8_t RFIDReader = getItemValue(MENU_RFIDREADER);
        if (!RFIDReader) {
            doc["rfid_status"] = "RFID reader not enabled";
        } else if (request->hasParam("rfid")) {
            String hexString = request->getParam("rfid")->value();
            hexString.trim();

            // Check if payload is valid hex and correct length
            bool validHex = true;
            for (size_t i = 0; i < hexString.length(); i++) {
                if (!isxdigit(hexString[i])) {
                    validHex = false;
                    break;
                }
            }

            if (!validHex) {
                doc["rfid_status"] = "Invalid RFID hex string";
            } else if (hexString.length() == 12 || hexString.length() == 14) {
                // Parse hex string into RFID array
                memset(RFID, 0, 8);

                if (hexString.length() == 12) {
                    // 6 byte UID (old reader format, starts at RFID[1])
                    RFID[0] = 0x01; // Family code for old reader
                    for (int i = 0; i < 6; i++) {
                        RFID[i + 1] = (uint8_t)strtol(hexString.substring(i * 2, i * 2 + 2).c_str(), NULL, 16);
                    }
                    RFID[7] = crc8((unsigned char *)RFID, 7);
                } else {
                    // 7 byte UID (new reader format)
                    for (int i = 0; i < 7; i++) {
                        RFID[i] = (uint8_t)strtol(hexString.substring(i * 2, i * 2 + 2).c_str(), NULL, 16);
                    }
                    RFID[7] = crc8((unsigned char *)RFID, 7);
                }

                _LOG_A("RFID received via REST API: %s\n", hexString.c_str());

                // Reset RFIDstatus so CheckRFID processes the card as new
                RFIDstatus = 0;

                // Process RFID using existing logic (whitelist check, OCPP, etc.)
                CheckRFID();

                doc["rfid"] = hexString;
                doc["rfid_status"] = !RFIDReader ? "Not Installed" : RFIDstatus >= 8 ? "NOSTATUS" : StrRFIDStatusWeb[RFIDstatus];
            } else {
                doc["rfid_status"] = "Invalid RFID length";
            }
        } else {
            doc["rfid_status"] = "Missing rfid parameter";
        }

        String json;
        serializeJson(doc, json);
        mg_http_reply(c, 200, "Content-Type: application/json\r\n", "%s\r\n", json.c_str());
        return true;

#if MODEM && SMARTEVSE_VERSION < 40
    } else if (mg_http_match_uri(hm, "/ev_state") && !memcmp("POST", hm->method.buf, hm->method.len)) {
        DynamicJsonDocument doc(200);

        //State of charge posting
        int current_soc = request->getParam("current_soc")->value().toInt();
        int full_soc = request->getParam("full_soc")->value().toInt();

        // Energy requested by car
        int energy_request = request->getParam("energy_request")->value().toInt();

        // Total energy capacity of car's battery
        int energy_capacity = request->getParam("energy_capacity")->value().toInt();

        // Update EVCCID of car
        if (request->hasParam("evccid")) {
            if (request->getParam("evccid")->value().length() <= 32) {
                strncpy(EVCCID, request->getParam("evccid")->value().c_str(), sizeof(EVCCID) - 1);
                EVCCID[sizeof(EVCCID) - 1] = '\0';
                doc["evccid"] = EVCCID;
            }
        }

        if (full_soc >= FullSoC) // Only update if we received it, since sometimes it's there, sometimes it's not
            FullSoC = full_soc;

        if (energy_capacity >= EnergyCapacity) // Only update if we received it, since sometimes it's there, sometimes it's not
            EnergyCapacity = energy_capacity;

        if (energy_request >= EnergyRequest) // Only update if we received it, since sometimes it's there, sometimes it's not
            EnergyRequest = energy_request;

        if (current_soc >= 0 && current_soc <= 100) {
            // We set the InitialSoC for our own calculations
            InitialSoC = current_soc;

            // We also set the ComputedSoC to allow for app integrations
            ComputedSoC = current_soc;

            // Skip waiting, charge since we have what we've got
            if (State == STATE_MODEM_REQUEST || State == STATE_MODEM_WAIT || State == STATE_MODEM_DONE){
                _LOG_A("Received SoC via REST. Shortcut to State Modem Done\n");
                setState(STATE_MODEM_DONE); // Go to State B, which means in this case setting PWM
            }
        }

        RecomputeSoC();

        doc["current_soc"] = current_soc;
        doc["full_soc"] = full_soc;
        doc["energy_capacity"] = energy_capacity;
        doc["energy_request"] = energy_request;

        String json;
        serializeJson(doc, json);
        mg_http_reply(c, 200, "Content-Type: application/json\r\n", "%s\r\n", json.c_str());    // Yes. Respond JSON
        return true;
#endif
    } else if (mg_http_match_uri(hm, "/session/last") && !memcmp("GET", hm->method.buf, hm->method.len)) {
        const session_record_t *last = session_get_last();
        if (!last) {
            mg_http_reply(c, 204, "", "");
        } else {
            char json[384];
            if (session_to_json(last, json, sizeof(json)) > 0) {
                mg_http_reply(c, 200, "Content-Type: application/json\r\n", "%s\r\n", json);
            } else {
                mg_http_reply(c, 500, "", "");
            }
        }
        return true;

#if MODEM && SMARTEVSE_VERSION >= 40
    } else if (mg_http_match_uri(hm, "/ev_state") && !memcmp("GET", hm->method.buf, hm->method.len)) {
        //this can be activated by: curl -X GET "http://smartevse-xxxx.lan/ev_state?update_ev_state=1" -d ''
        uint8_t GetState = 0;
        if(request->hasParam("update_ev_state")) {
            GetState = strtol(request->getParam("update_ev_state")->value().c_str(),NULL,0);
            if (GetState)
                setState(STATE_MODEM_REQUEST);
        }
        _LOG_A("DEBUG: GetState=%u.\n", GetState);
        mg_http_reply(c, 200, "Content-Type: application/json\r\n", "%s\r\n", ""); //json request needs json response
        return true;
#endif

#if FAKE_RFID
    //this can be activated by: http://smartevse-xxx.lan/debug?showrfid=1
    } else if (mg_http_match_uri(hm, "/debug") && !memcmp("GET", hm->method.buf, hm->method.len)) {
        if(request->hasParam("showrfid")) {
            Show_RFID = strtol(request->getParam("showrfid")->value().c_str(),NULL,0);
        }
        _LOG_A("DEBUG: Show_RFID=%u.\n",Show_RFID);
        mg_http_reply(c, 200, "Content-Type: application/json\r\n", "%s\r\n", ""); //json request needs json response
        return true;
#endif

#if AUTOMATED_TESTING
    //this can be activated by: http://smartevse-xxx.lan/automated_testing?current_max=100
    //WARNING: because of automated testing, no limitations here!
    //THAT IS DANGEROUS WHEN USED IN PRODUCTION ENVIRONMENT
    //FOR SMARTEVSE's IN A TESTING BENCH ONLY!!!!
    } else if (mg_http_match_uri(hm, "/automated_testing") && !memcmp("POST", hm->method.buf, hm->method.len)) {
        if(request->hasParam("current_max")) {
            MaxCurrent = strtol(request->getParam("current_max")->value().c_str(),NULL,0);
            SEND_TO_CH32(MaxCurrent)
        }
        if(request->hasParam("current_main")) {
            MaxMains = strtol(request->getParam("current_main")->value().c_str(),NULL,0);
            SEND_TO_CH32(MaxMains)
        }
        if(request->hasParam("current_max_circuit")) {
            MaxCircuit = strtol(request->getParam("current_max_circuit")->value().c_str(),NULL,0);
            SEND_TO_CH32(MaxCircuit)
        }
        if(request->hasParam("mainsmeter")) {
            MainsMeter.Type = strtol(request->getParam("mainsmeter")->value().c_str(),NULL,0);
            Serial1.printf("@MainsMeterType:%u\n", MainsMeter.Type);
        }
        if(request->hasParam("evmeter")) {
            EVMeter.Type = strtol(request->getParam("evmeter")->value().c_str(),NULL,0);
            Serial1.printf("@EVMeterType:%u\n", EVMeter.Type);
        }
        if(request->hasParam("config")) {
            Config = strtol(request->getParam("config")->value().c_str(),NULL,0);
            SEND_TO_CH32(Config)
            setState(STATE_A);                                                  // so the new value will actually be read
        }
        if(request->hasParam("loadbl")) {
            int LBL = strtol(request->getParam("loadbl")->value().c_str(),NULL,0);
#if SMARTEVSE_VERSION >=30 && SMARTEVSE_VERSION < 40
            ConfigureModbusMode(LBL);
#endif
            LoadBl = LBL;
            SEND_TO_CH32(LoadBl)
        }
        mg_http_reply(c, 200, "Content-Type: application/json\r\n", "%s\r\n", ""); //json request needs json response
        return true;
#endif

    // BEGIN PLAN-06: Diagnostic telemetry REST endpoints
    } else if (mg_http_match_uri(hm, "/diag/status") && !memcmp("GET", hm->method.buf, hm->method.len)) {
        char json[256];
        int n = diag_status_json(json, sizeof(json));
        if (n > 0)
            mg_http_reply(c, 200, "Content-Type: application/json\r\n", "%s\r\n", json);
        else
            mg_http_reply(c, 500, "", "status error\r\n");
        return true;

    } else if (mg_http_match_uri(hm, "/diag/start") && !memcmp("POST", hm->method.buf, hm->method.len)) {
        /* Parse profile from query: /diag/start?profile=general */
        char pbuf[16] = {0};
        mg_http_get_var(&hm->query, "profile", pbuf, sizeof(pbuf));

        diag_profile_t profile = DIAG_PROFILE_GENERAL;
        if (strcmp(pbuf, "solar") == 0)        profile = DIAG_PROFILE_SOLAR;
        else if (strcmp(pbuf, "loadbal") == 0)  profile = DIAG_PROFILE_LOADBAL;
        else if (strcmp(pbuf, "modbus") == 0)   profile = DIAG_PROFILE_MODBUS;
        else if (strcmp(pbuf, "fast") == 0)     profile = DIAG_PROFILE_FAST;
        /* else default to GENERAL */

        diag_start(profile);
        mg_http_reply(c, 200, "Content-Type: application/json\r\n",
                      "{\"started\":true,\"profile\":\"%s\"}\r\n", pbuf[0] ? pbuf : "general");
        return true;

    } else if (mg_http_match_uri(hm, "/diag/stop") && !memcmp("POST", hm->method.buf, hm->method.len)) {
        diag_stop();
        mg_http_reply(c, 200, "Content-Type: application/json\r\n",
                      "{\"stopped\":true}\r\n");
        return true;

    } else if (mg_http_match_uri(hm, "/diag/download") && !memcmp("GET", hm->method.buf, hm->method.len)) {
        diag_ring_t *ring = diag_get_ring();
        /* Freeze for consistent download */
        bool was_frozen = ring->frozen;
        diag_ring_freeze(ring, true);

        /* Serialize to binary .diag format */
        size_t max_sz = sizeof(diag_file_header_t) + (size_t)ring->count * sizeof(diag_snapshot_t) + 4;
        uint8_t *out = (uint8_t *)malloc(max_sz);
        if (!out) {
            if (!was_frozen) diag_ring_freeze(ring, false);
            mg_http_reply(c, 500, "", "out of memory\r\n");
            return true;
        }

        size_t n = diag_ring_serialize(ring, out, max_sz, VERSION, serialnr);
        if (!was_frozen) diag_ring_freeze(ring, false);

        if (n > 0) {
            mg_printf(c,
                      "HTTP/1.1 200 OK\r\n"
                      "Content-Type: application/octet-stream\r\n"
                      "Content-Disposition: attachment; filename=\"smartevse_%u.diag\"\r\n"
                      "Content-Length: %u\r\n\r\n",
                      (unsigned)serialnr, (unsigned)n);
            mg_send(c, out, n);
            c->is_resp = 0;
        } else {
            mg_http_reply(c, 500, "", "serialize error\r\n");
        }
        free(out);
        return true;

    } else if (mg_http_match_uri(hm, "/diag/dump") && !memcmp("POST", hm->method.buf, hm->method.len)) {
        bool ok = diag_storage_dump(DIAG_TRIGGER_MANUAL);
        mg_http_reply(c, ok ? 200 : 500, "Content-Type: application/json\r\n",
                      "{\"dumped\":%s}\r\n", ok ? "true" : "false");
        return true;

    } else if (mg_http_match_uri(hm, "/diag/files") && !memcmp("GET", hm->method.buf, hm->method.len)) {
        char json[384];
        int n = diag_storage_list_json(json, sizeof(json));
        if (n > 0)
            mg_http_reply(c, 200, "Content-Type: application/json\r\n", "%s\r\n", json);
        else
            mg_http_reply(c, 500, "", "list error\r\n");
        return true;

    } else if (mg_http_match_uri(hm, "/diag/file/*") && !memcmp("GET", hm->method.buf, hm->method.len)) {
        /* Extract filename from URI: /diag/file/<filename> */
        struct mg_str uri = hm->uri;
        const char *prefix = "/diag/file/";
        size_t prefix_len = strlen(prefix);
        if (uri.len <= prefix_len) {
            mg_http_reply(c, 400, "", "missing filename\r\n");
            return true;
        }
        char fname[48];
        size_t fname_len = uri.len - prefix_len;
        if (fname_len >= sizeof(fname)) fname_len = sizeof(fname) - 1;
        memcpy(fname, uri.buf + prefix_len, fname_len);
        fname[fname_len] = '\0';

        /* Validate filename: reject path traversal and unsafe characters */
        if (!is_safe_filename(fname, fname_len)) {
            mg_http_reply(c, 400, "", "invalid filename\r\n");
            return true;
        }

        /* Read file from LittleFS */
        char filepath[64];
        snprintf(filepath, sizeof(filepath), "%s/%s", DIAG_DIR, fname);
        File file = LittleFS.open(filepath, "r");
        if (!file) {
            mg_http_reply(c, 404, "", "file not found\r\n");
            return true;
        }
        size_t fsize = file.size();
        uint8_t *fbuf = (uint8_t *)malloc(fsize);
        if (!fbuf) {
            file.close();
            mg_http_reply(c, 500, "", "out of memory\r\n");
            return true;
        }
        file.read(fbuf, fsize);
        file.close();

        mg_printf(c,
                  "HTTP/1.1 200 OK\r\n"
                  "Content-Type: application/octet-stream\r\n"
                  "Content-Disposition: attachment; filename=\"%s\"\r\n"
                  "Content-Length: %u\r\n\r\n",
                  fname, (unsigned)fsize);
        mg_send(c, fbuf, fsize);
        c->is_resp = 0;
        free(fbuf);
        return true;

    } else if (mg_http_match_uri(hm, "/diag/file/*") && !memcmp("DELETE", hm->method.buf, hm->method.len)) {
        struct mg_str uri = hm->uri;
        const char *prefix = "/diag/file/";
        size_t prefix_len = strlen(prefix);
        if (uri.len <= prefix_len) {
            mg_http_reply(c, 400, "", "missing filename\r\n");
            return true;
        }
        char fname[48];
        size_t fname_len = uri.len - prefix_len;
        if (fname_len >= sizeof(fname)) fname_len = sizeof(fname) - 1;
        memcpy(fname, uri.buf + prefix_len, fname_len);
        fname[fname_len] = '\0';

        /* Validate filename: reject path traversal and unsafe characters */
        if (!is_safe_filename(fname, fname_len)) {
            mg_http_reply(c, 400, "", "invalid filename\r\n");
            return true;
        }

        bool ok = diag_storage_delete(fname);
        mg_http_reply(c, ok ? 200 : 404, "Content-Type: application/json\r\n",
                      "{\"deleted\":%s}\r\n", ok ? "true" : "false");
        return true;
    // END PLAN-06
  }
  return false;
}

#endif // defined(ESP32)
