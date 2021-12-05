## Testing

```plantuml
@startuml
allowmixing
title Test Structure

package C_stack{
    component iso14229 #red
    component "test_harness_iso14229.so" as test_harness
    iso14229 <-down-> test_harness
}

() C

package Python_stack{
    () "can.VirtualBus" as CAN

    component test_suite #red
    component pytest #lightgrey
    component ctypes #lightgrey
    component "python-can" as python_can #lightgrey
    component "udsoncan\nUDS client implementation" as udsoncan #lightgrey
    udsoncan<-down->python_can

    test_suite <-> ctypes: read iso14229 state,\ncall iso14229UserInit(...)
    test_suite -left> pytest
    test_suite <-down-> udsoncan
    CAN <-> ctypes: setup callback functions\ncall init functions
}

python_can <-> CAN
ctypes<-right->C
C<-right->test_harness

@enduml
```

## Note

`ASSUMPTION` marks where a simplifying assumption was made about the use of this library 

## TODO
- hook calls to `time.time()` in `python-isotp` and `udsoncan` to eliminate host computer latency effects