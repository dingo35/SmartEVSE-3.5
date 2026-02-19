Feature: Authorization & Access Control
  The charger supports RFID, OCPP, and switch-based access control.
  Access can be ON, OFF, or PAUSE. When access is revoked or paused
  during an active session, the charger suspends charging gracefully
  by transitioning to the corresponding waiting state (B1 or C1).

  Background:
    Given the charger is initialized

  Scenario: Revoking access during charging suspends the session
    Given the charger is charging in State C
    When access is set to OFF
    Then the charger transitions to State C1
    And charging is suspended

  Scenario: Pausing access during charging suspends the session
    Given the charger is charging in State C
    When access is set to PAUSE
    Then the charger transitions to State C1

  Scenario: Revoking access while connected stops charging preparation
    Given the charger is connected in State B
    When access is set to OFF
    Then the charger transitions to State B1

  Scenario: RFID lock timer expires and revokes access
    Given the charger is idle with RFID access granted
    And the RFID lock timer is set
    When the lock timer expires
    Then access is revoked
