Feature: Solar Balancing
  In solar mode, charging current follows solar surplus with
  automatic phase switching between 1-phase and 3-phase operation.
  A SolarStopTimer manages the hysteresis for switching decisions.
  During session startup, the charger forces minimum current to
  allow the vehicle to begin its charging handshake.

  Background:
    Given the charger is in solar mode
    And the charger is charging

  Scenario: Shortage in 3-phase mode triggers switch to 1-phase
    Given the charger is in 3-phase mode with AUTO phase switching
    And there is a current shortage
    When the solar stop timer expires
    Then the charger switches to 1-phase mode

  Scenario: Surplus in 1-phase mode triggers switch to 3-phase
    Given the charger is in 1-phase mode at maximum current
    And there is sufficient solar surplus
    When the solar stop timer expires
    Then the charger switches to 3-phase mode

  Scenario: Insufficient surplus resets the solar stop timer
    Given the charger is in 1-phase mode below maximum current
    And the solar stop timer is running
    When the surplus is insufficient for 3-phase upgrade
    Then the solar stop timer is reset to zero

  Scenario: Solar startup forces minimum current
    Given a new charging session has just started
    When current is calculated during the startup period
    Then the EVSE receives exactly the minimum current
