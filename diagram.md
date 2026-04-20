stateDiagram-v2
    [*] --> Ready

    state "Ready (Lift ON, Sweep OFF, Tube OFF, Pusher OFF)" as Ready
    state "Queued Reset (holdResetValue = true)" as QueuedReset
    state "Sweep Running" as SweepRunning
    state "Tubes Running" as TubesRunning
    state "Sweep Stopped / Tubes Still Running" as SweepStopped
    state "Tubes Stopped" as TubesStopped
    state "Pusher Running (Lift OFF)" as PusherRunning

    Ready --> SweepRunning: reset pressed AND pusherStopFlop == true / startCycle()
    Ready --> QueuedReset: reset pressed AND pusherStopFlop == false / holdResetValue = true, statusLED ON

    QueuedReset --> SweepRunning: pusherStop event / stopPusherStartLift() then auto startCycle()

    SweepRunning --> TubesRunning: tubeStart event AND sweepStopFlop == false / startTubes()

    SweepRunning --> SweepStopped: sweepStop event AND tubeStopFlop == false / stopSweep()
    TubesRunning --> SweepStopped: sweepStop event AND tubeStopFlop == false / stopSweep()

    SweepStopped --> TubesStopped: tubeStop level AND sweepStopFlop == true / stopTubes()

    TubesRunning --> TubesRunning: liftStop level AND tubeStopFlop == false / stopLift()
    SweepStopped --> SweepStopped: liftStop level AND tubeStopFlop == false / stopLift()

    TubesStopped --> PusherRunning: liftStop level AND tubeStopFlop == true / stopLiftStartPusher()

    PusherRunning --> Ready: pusherStop event / stopPusherStartLift() (no queued reset)
    PusherRunning --> SweepRunning: pusherStop event AND holdResetValue == true / stopPusherStartLift() + startCycle()

    