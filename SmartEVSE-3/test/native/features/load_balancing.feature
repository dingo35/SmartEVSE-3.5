Feature: Load Balancing
  Multiple EVSEs share available current fairly while respecting
  individual and circuit-level limits. The master EVSE calculates
  the balanced current distribution across all connected nodes,
  enforcing minimum current thresholds and maximum capacities.

  Background:
    Given a master EVSE with load balancing enabled

  Scenario: Two EVSEs share current equally
    Given 2 EVSEs are charging
    When current is calculated
    Then both EVSEs receive equal current

  Scenario: Individual EVSE maximum is respected
    Given 2 EVSEs are charging
    And EVSE 2 has a lower maximum capacity
    When current is calculated
    Then EVSE 2 does not exceed its maximum
    And EVSE 1 receives the remainder

  Scenario: Minimum current is enforced during shortage
    Given 2 EVSEs are charging with limited total current
    When current is calculated
    Then each active EVSE receives at least the minimum current

  Scenario: OCPP current limit reduces charge current
    Given OCPP mode is enabled
    And the OCPP current limit is 10A
    When current is calculated
    Then the charge current does not exceed 10A
