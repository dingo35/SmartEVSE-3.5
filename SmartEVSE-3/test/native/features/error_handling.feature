Feature: Error Handling & Safety
  The charger monitors temperature, meter communication, and current
  availability to ensure safe operation. When limits are exceeded,
  the charger sets error flags and suspends charging. Recovery happens
  automatically once conditions return to safe levels with hysteresis.

  Background:
    Given the charger is initialized

  Scenario: Temperature exceeding maximum triggers shutdown
    Given the charger is charging
    And the EVSE temperature exceeds the maximum
    When the 1-second tick fires
    Then a temperature error flag is set
    And charging is suspended

  Scenario: Temperature recovery with hysteresis
    Given a temperature error is active
    And the maximum temperature is 65 degrees
    When the temperature drops to 55 degrees
    Then the temperature error remains active
    When the temperature drops to 54 degrees
    Then the temperature error is cleared

  Scenario: Mains meter communication timeout
    Given the charger is in Smart mode with a mains meter
    When the mains meter stops communicating
    Then a CT_NOCOMM error is raised
    When communication is restored
    Then the CT_NOCOMM error is cleared
