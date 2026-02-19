Feature: IEC 61851-1 State Machine Transitions
  The charger implements the IEC 61851-1 standard for electric vehicle
  charging state management. States are driven by pilot signal voltage.
  State A is disconnected, State B is connected (not charging),
  State C is actively charging, and waiting states B1/C1 indicate
  the charger is connected but blocked (errors, delays, or access revoked).

  Background:
    Given the charger is initialized
    And authorization is granted

  Scenario: Vehicle connection triggers State A to B transition
    Given the charger is idle in State A
    And the modem is authenticated
    When a vehicle plugs in
    Then the charger transitions to State B
    And the contactors remain off

  Scenario: Full charge cycle from connection to disconnection
    Given the charger is in standalone normal mode
    And the modem is authenticated
    When a vehicle plugs in
    Then the charger transitions to State B
    When the vehicle requests charging after diode check
    Then the charger transitions to State C
    And contactor 1 is energized
    When the vehicle stops requesting charge
    Then the charger returns to State B
    When the vehicle disconnects
    Then the charger returns to State A
    And all contactors are off

  Scenario: Errors prevent charging and route to waiting state
    Given the charger has a temperature error
    When a vehicle plugs in
    Then the charger transitions to State B1
    And charging does not start

  Scenario: Charge delay routes to waiting state
    Given a charge delay is active
    When a vehicle plugs in
    Then the charger transitions to State B1

  Scenario: Unauthorized charger ignores vehicle connection
    Given authorization is revoked
    When a vehicle plugs in
    Then the charger remains in State A
