Feature: Complete Charging Cycle
  End-to-end scenarios covering real-world charging workflows
  from vehicle connection through active charging to disconnection.
  These scenarios validate the interaction between the state machine,
  authorization, contactor control, and modem negotiation subsystems.

  Scenario: Normal standalone charge session
    Given a standalone charger in normal mode
    And authorization is granted
    When a vehicle plugs in
    Then the charger enters State B
    When the vehicle requests charging
    And the diode check passes
    Then the charger enters State C after debounce
    And contactor 1 is on
    When the vehicle finishes charging
    Then the charger returns to State B
    When the vehicle disconnects
    Then the charger returns to State A
    And all contactors are off

  Scenario: Modem negotiation before charging
    Given a charger requiring modem authentication
    When a vehicle plugs in
    Then modem negotiation begins
    When the modem authentication completes
    Then the charger enters State B
    And the modem stage is set to authenticated
