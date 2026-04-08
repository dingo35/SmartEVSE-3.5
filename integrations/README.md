# Easy integration with Home Assistant

> [!TIP]
> If you want to integrate your SmartEVSE with Home Assistant, the preferred way is to have the HA MQTT integration installed, and interface with SmartEVSE through MQTT. Home Assistant will automatically discover SmartEVSE when it connects to MQTT.

> [!WARNING]
> The [SmartEVSE custom component for Home Assistant](https://github.com/dingo35/ha-SmartEVSEv3) is considered deprecated.

# Use ESPHome to provide current information to SmartEVSE
Your SmartEVSE has an API endpoint to send L1/L2/L3 data, which means that you don't need to connect a SensorBox to retrieve the required information for load-balancing or solar-charging.
If you are using a P1 reader (often also called DSMR-reader) with your electricity meter, you might want to send the current information (L1/L2/L3) directly from your ESPHome device to your SmartEVSE.

## ESPHome examples
- [integrations/esphome/ET-SM01%20smart%20meter%20module.yaml](esphome/ET-SM01%20smart%20meter%20module.yaml) - ESP8266 ET-SM01 P1 DSMR module; reads meter data and posts L1/L2/L3 currents to SmartEVSE over HTTP.
- [integrations/esphome/SlimmeLezer-with-DSMR-reader-API-and-MQTT-with-HTTP-fallback.yaml](esphome/SlimmeLezer-with-DSMR-reader-API-and-MQTT-with-HTTP-fallback.yaml) - SlimmeLezer configuration that pushes DSMR data to a DSMR-reader API and sends currents to SmartEVSE via MQTT with HTTP fallback.
- [integrations/esphome/WT32-ETH01-SlimmeLezer.yaml](esphome/WT32-ETH01-SlimmeLezer.yaml) - WT32-ETH01 SlimmeLezer Ethernet device; reads DSMR data and posts currents to SmartEVSE.

## Home Assistant examples
- [integrations/home-assistant/HA_P1_DSMR_to_SmartEVSE_API.yaml](home-assistant/HA_P1_DSMR_to_SmartEVSE_API.yaml) - Home Assistant package that reads P1 DSMR sensors and pushes currents to SmartEVSE via REST.
- [integrations/home-assistant/HA_SmartEVSE_MQTT_automations.yaml](home-assistant/HA_SmartEVSE_MQTT_automations.yaml) - Automations that publish currents and battery data to SmartEVSE via MQTT.
- [integrations/home-assistant/HA_SmartEVSE_REST_API.yaml](home-assistant/HA_SmartEVSE_REST_API.yaml) - REST API sensors and commands for SmartEVSE settings and status.
- [integrations/home-assistant/HASS-Node-RED.json](home-assistant/HASS-Node-RED.json) - Node-RED flow example for Home Assistant to integrate SmartEVSE.
