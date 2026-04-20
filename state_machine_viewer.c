#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "raylib.h"

typedef enum DiagramState {
    STATE_READY = 0,
    STATE_QUEUED_RESET,
    STATE_SWEEP_RUNNING,
    STATE_TUBES_RUNNING,
    STATE_SWEEP_STOPPED,
    STATE_TUBES_STOPPED,
    STATE_PUSHER_RUNNING,
    STATE_COUNT
} DiagramState;

typedef enum TransitionId {
    TRANSITION_NONE = 0,
    TRANS_READY_TO_QUEUED,
    TRANS_READY_TO_SWEEP,
    TRANS_SWEEP_TO_TUBES,
    TRANS_SWEEP_TO_SWEEP_STOPPED,
    TRANS_TUBES_TO_SWEEP_STOPPED,
    TRANS_SWEEP_STOPPED_TO_TUBES_STOPPED,
    TRANS_TUBES_STOPPED_TO_PUSHER,
    TRANS_PUSHER_TO_READY,
    TRANS_PUSHER_TO_SWEEP,
    TRANS_TUBES_RUNNING_SELF,
    TRANS_SWEEP_STOPPED_SELF
} TransitionId;

#define MAX_EVENT_LOG 12
#define LOG_LINE_CAPACITY 196

typedef struct NodeVisual {
    const char *name;
    Rectangle rect;
} NodeVisual;

typedef struct Machine {
    bool sweepStopFlop;
    bool tubeStopFlop;
    bool pusherStopFlop;
    bool holdResetValue;

    bool sweepRelay;
    bool tubeRelay;
    bool pusherRelay;
    bool liftRelay;
    bool statusLed;
} Machine;

typedef struct InputButton {
    const char *label;
    Rectangle rect;
} InputButton;

static void StartCycle(Machine *m) {
    m->sweepStopFlop = false;
    m->tubeStopFlop = false;
    m->pusherStopFlop = false;
    m->statusLed = true;
    m->sweepRelay = true;
}

static void StartTubes(Machine *m) {
    m->tubeRelay = true;
}

static void StopSweep(Machine *m) {
    m->sweepStopFlop = true;
    m->sweepRelay = false;
}

static void StopTubes(Machine *m) {
    m->tubeStopFlop = true;
    m->statusLed = false;
    m->tubeRelay = false;
}

static void StopLiftStartPusher(Machine *m) {
    m->liftRelay = false;
    m->pusherRelay = true;
}

static void StopLift(Machine *m) {
    m->liftRelay = false;
}

static void StopPusherStartLift(Machine *m) {
    m->pusherStopFlop = true;
    m->pusherRelay = false;
    m->liftRelay = true;

    if (m->holdResetValue) {
        m->holdResetValue = false;
        StartCycle(m);
    }
}

static void InitMachine(Machine *m) {
    memset(m, 0, sizeof(*m));

    // Start in a safe idle state and allow reset to start a cycle.
    m->pusherStopFlop = true;
    m->liftRelay = true;
}

static DiagramState DetectPrimaryState(const Machine *m) {
    if (m->pusherRelay) {
        return STATE_PUSHER_RUNNING;
    }
    if (m->tubeStopFlop) {
        return STATE_TUBES_STOPPED;
    }
    if (m->sweepRelay && m->tubeRelay) {
        return STATE_TUBES_RUNNING;
    }
    if (!m->sweepRelay && m->tubeRelay) {
        return STATE_SWEEP_STOPPED;
    }
    if (m->sweepRelay) {
        return STATE_SWEEP_RUNNING;
    }
    return STATE_READY;
}

static bool IsQueuedResetActive(const Machine *m) {
    return m->holdResetValue;
}

static bool IsStateActive(const Machine *m, DiagramState state) {
    if (state == STATE_QUEUED_RESET) {
        return IsQueuedResetActive(m);
    }
    return DetectPrimaryState(m) == state;
}

static const char *StateName(DiagramState state) {
    switch (state) {
        case STATE_READY:
            return "Ready";
        case STATE_QUEUED_RESET:
            return "Queued Reset";
        case STATE_SWEEP_RUNNING:
            return "Sweep Running";
        case STATE_TUBES_RUNNING:
            return "Tubes Running";
        case STATE_SWEEP_STOPPED:
            return "Sweep Stopped";
        case STATE_TUBES_STOPPED:
            return "Tubes Stopped";
        case STATE_PUSHER_RUNNING:
            return "Pusher Running";
        default:
            return "Unknown";
    }
}

static const char *TransitionName(TransitionId transition) {
    switch (transition) {
        case TRANS_READY_TO_QUEUED:
            return "Ready -> Queued Reset";
        case TRANS_READY_TO_SWEEP:
            return "Ready -> Sweep Running";
        case TRANS_SWEEP_TO_TUBES:
            return "Sweep Running -> Tubes Running";
        case TRANS_SWEEP_TO_SWEEP_STOPPED:
            return "Sweep Running -> Sweep Stopped";
        case TRANS_TUBES_TO_SWEEP_STOPPED:
            return "Tubes Running -> Sweep Stopped";
        case TRANS_SWEEP_STOPPED_TO_TUBES_STOPPED:
            return "Sweep Stopped -> Tubes Stopped";
        case TRANS_TUBES_STOPPED_TO_PUSHER:
            return "Tubes Stopped -> Pusher Running";
        case TRANS_PUSHER_TO_READY:
            return "Pusher Running -> Ready";
        case TRANS_PUSHER_TO_SWEEP:
            return "Pusher Running -> Sweep Running (queued)";
        case TRANS_TUBES_RUNNING_SELF:
            return "Tubes Running self-loop (liftStop wait)";
        case TRANS_SWEEP_STOPPED_SELF:
            return "Sweep Stopped self-loop (liftStop wait)";
        case TRANSITION_NONE:
        default:
            return "none";
    }
}

static void FormatTimestamp(double seconds, char *buffer, size_t bufferSize) {
    int minutes = (int)(seconds / 60.0);
    double remainingSeconds = seconds - (double)(minutes * 60);
    snprintf(buffer, bufferSize, "%02d:%06.3f", minutes, remainingSeconds);
}

static void PushEventLog(char logLines[MAX_EVENT_LOG][LOG_LINE_CAPACITY], int *count, int *nextIndex,
                         const char *message, double timestampSeconds) {
    char ts[16];
    FormatTimestamp(timestampSeconds, ts, sizeof(ts));
    snprintf(logLines[*nextIndex], LOG_LINE_CAPACITY, "[%s] %s", ts, message);

    *nextIndex = (*nextIndex + 1) % MAX_EVENT_LOG;
    if (*count < MAX_EVENT_LOG) {
        (*count)++;
    }
}

static void ProcessInputEvent(Machine *m, int buttonIndex, char *lastAction, size_t actionSize,
                              TransitionId *lastTransition) {
    const DiagramState stateBefore = DetectPrimaryState(m);
    *lastTransition = TRANSITION_NONE;

    switch (buttonIndex) {
        case 0: {
            if (m->pusherStopFlop) {
                StartCycle(m);
                snprintf(lastAction, actionSize, "reset -> startCycle()");
                *lastTransition = TRANS_READY_TO_SWEEP;
            } else {
                m->holdResetValue = true;
                m->statusLed = true;
                snprintf(lastAction, actionSize, "reset queued (holdResetValue=true)");
                *lastTransition = TRANS_READY_TO_QUEUED;
            }
            break;
        }
        case 1: {
            if (!m->sweepStopFlop) {
                StartTubes(m);
                snprintf(lastAction, actionSize, "tubeStart -> startTubes()");
                *lastTransition = TRANS_SWEEP_TO_TUBES;
            } else {
                snprintf(lastAction, actionSize, "tubeStart ignored (sweep already stopped)");
            }
            break;
        }
        case 2: {
            if (!m->tubeStopFlop) {
                const bool tubeWasRunning = m->tubeRelay;
                StopSweep(m);
                snprintf(lastAction, actionSize, "sweepStop -> stopSweep()");
                *lastTransition = tubeWasRunning ? TRANS_TUBES_TO_SWEEP_STOPPED : TRANS_SWEEP_TO_SWEEP_STOPPED;
            } else {
                snprintf(lastAction, actionSize, "sweepStop ignored (tubes already stopped)");
            }
            break;
        }
        case 3: {
            if (m->sweepStopFlop) {
                StopTubes(m);
                snprintf(lastAction, actionSize, "tubeStop -> stopTubes()");
                *lastTransition = TRANS_SWEEP_STOPPED_TO_TUBES_STOPPED;
            } else {
                snprintf(lastAction, actionSize, "tubeStop ignored (sweep not stopped yet)");
            }
            break;
        }
        case 4: {
            if (m->tubeStopFlop) {
                StopLiftStartPusher(m);
                snprintf(lastAction, actionSize, "liftStop -> stopLiftStartPusher()");
                *lastTransition = TRANS_TUBES_STOPPED_TO_PUSHER;
            } else {
                StopLift(m);
                snprintf(lastAction, actionSize, "liftStop -> stopLift() (waiting for tubes)");
                *lastTransition = (stateBefore == STATE_SWEEP_STOPPED) ? TRANS_SWEEP_STOPPED_SELF : TRANS_TUBES_RUNNING_SELF;
            }
            break;
        }
        case 5: {
            if (m->pusherRelay || m->holdResetValue || !m->pusherStopFlop) {
                const bool queuedReset = m->holdResetValue;
                StopPusherStartLift(m);
                snprintf(lastAction, actionSize, "pusherStop -> stopPusherStartLift()");
                *lastTransition = queuedReset ? TRANS_PUSHER_TO_SWEEP : TRANS_PUSHER_TO_READY;
            } else {
                snprintf(lastAction, actionSize, "pusherStop ignored (pusher not active)");
            }
            break;
        }
        default: {
            snprintf(lastAction, actionSize, "unknown input");
            break;
        }
    }
}

static void DrawNode(const NodeVisual *node, bool active) {
    const Color fill = active ? (Color){35, 119, 63, 255} : (Color){237, 240, 245, 255};
    const Color border = active ? (Color){23, 74, 40, 255} : (Color){90, 96, 109, 255};
    const Color text = active ? RAYWHITE : (Color){38, 43, 53, 255};

    DrawRectangleRounded(node->rect, 0.08f, 8, fill);
    DrawRectangleRoundedLinesEx(node->rect, 0.08f, 8, 2.0f, border);

    const int fontSize = 18;
    const int textWidth = MeasureText(node->name, fontSize);
    const int x = (int)(node->rect.x + (node->rect.width - (float)textWidth) * 0.5f);
    const int y = (int)(node->rect.y + (node->rect.height - (float)fontSize) * 0.5f);
    DrawText(node->name, x, y, fontSize, text);
}

static void DrawArrow(Vector2 start, Vector2 end, const char *label, Color color, float thickness) {
    DrawLineEx(start, end, thickness, color);

    Vector2 dir = Vector2Normalize((Vector2){end.x - start.x, end.y - start.y});
    Vector2 perp = (Vector2){-dir.y, dir.x};

    const float headLen = 10.0f;
    const float headWidth = 6.0f;

    Vector2 tip = end;
    Vector2 base = (Vector2){end.x - dir.x * headLen, end.y - dir.y * headLen};
    Vector2 left = (Vector2){base.x + perp.x * headWidth, base.y + perp.y * headWidth};
    Vector2 right = (Vector2){base.x - perp.x * headWidth, base.y - perp.y * headWidth};

    DrawTriangle(tip, left, right, color);

    if (label != NULL && label[0] != '\0') {
        const int fontSize = 14;
        Vector2 mid = (Vector2){(start.x + end.x) * 0.5f, (start.y + end.y) * 0.5f};
        DrawText(label, (int)mid.x + 4, (int)mid.y - 10, fontSize, (Color){65, 68, 75, 255});
    }
}

static void DrawSignalLight(int x, int y, const char *label, bool on, Color onColor) {
    const Color offColor = (Color){70, 73, 80, 255};
    DrawCircle(x, y, 11.0f, on ? onColor : offColor);
    DrawCircleLines(x, y, 11.0f, BLACK);
    DrawText(label, x + 18, y - 8, 18, (Color){29, 33, 41, 255});
}

int main(void) {
    const int screenWidth = 1500;
    const int screenHeight = 900;

    InitWindow(screenWidth, screenHeight, "Candlepin Pinsetter State Machine Explorer");
    SetTargetFPS(60);

    Machine machine;
    InitMachine(&machine);

    char lastAction[160] = "none";
    TransitionId lastTransition = TRANSITION_NONE;
    char eventLog[MAX_EVENT_LOG][LOG_LINE_CAPACITY] = {{0}};
    int eventLogCount = 0;
    int eventLogNext = 0;
    PushEventLog(eventLog, &eventLogCount, &eventLogNext, "startup -> Ready", GetTime());

    NodeVisual nodes[STATE_COUNT] = {
        {"Ready", {420, 90, 220, 64}},
        {"Queued Reset", {720, 90, 240, 64}},
        {"Sweep Running", {420, 230, 220, 64}},
        {"Tubes Running", {420, 370, 220, 64}},
        {"Sweep Stopped", {720, 370, 240, 64}},
        {"Tubes Stopped", {720, 510, 240, 64}},
        {"Pusher Running", {720, 650, 240, 64}}
    };

    InputButton inputs[] = {
        {"reset", {30, 110, 250, 46}},
        {"tubeStart", {30, 170, 250, 46}},
        {"sweepStop", {30, 230, 250, 46}},
        {"tubeStop", {30, 290, 250, 46}},
        {"liftStop", {30, 350, 250, 46}},
        {"pusherStop", {30, 410, 250, 46}}
    };

    while (!WindowShouldClose()) {
        const Vector2 mouse = GetMousePosition();

        if (IsKeyPressed(KEY_R)) {
            InitMachine(&machine);
            lastTransition = TRANSITION_NONE;
            snprintf(lastAction, sizeof(lastAction), "machine reset to startup state");
            PushEventLog(eventLog, &eventLogCount, &eventLogNext, "manual reset -> Ready", GetTime());
        }

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            for (int i = 0; i < (int)(sizeof(inputs) / sizeof(inputs[0])); i++) {
                if (CheckCollisionPointRec(mouse, inputs[i].rect)) {
                    ProcessInputEvent(&machine, i, lastAction, sizeof(lastAction), &lastTransition);

                    {
                        char logMessage[LOG_LINE_CAPACITY];
                        const DiagramState primaryState = DetectPrimaryState(&machine);
                        snprintf(logMessage, sizeof(logMessage), "%s | state=%s%s", lastAction,
                                 StateName(primaryState), machine.holdResetValue ? " + Queued Reset" : "");
                        PushEventLog(eventLog, &eventLogCount, &eventLogNext, logMessage, GetTime());
                    }
                    break;
                }
            }
        }

        BeginDrawing();
        ClearBackground((Color){248, 249, 252, 255});

        DrawText("Bowl-Mor Pinsetter State Machine", 26, 18, 34, (Color){20, 30, 48, 255});
        DrawText("Click an input to simulate a switch trigger", 30, 58, 20, (Color){69, 77, 93, 255});

        // Input panel
        DrawRectangleRounded((Rectangle){18, 90, 275, 360}, 0.06f, 8, (Color){226, 232, 241, 255});
        DrawRectangleRoundedLinesEx((Rectangle){18, 90, 275, 360}, 0.06f, 8, 2.0f, (Color){106, 116, 132, 255});
        DrawText("Machine Inputs", 36, 96, 24, (Color){24, 35, 55, 255});

        for (int i = 0; i < (int)(sizeof(inputs) / sizeof(inputs[0])); i++) {
            const bool hover = CheckCollisionPointRec(mouse, inputs[i].rect);
            const Color fill = hover ? (Color){198, 214, 236, 255} : (Color){212, 223, 239, 255};
            DrawRectangleRounded(inputs[i].rect, 0.2f, 6, fill);
            DrawRectangleRoundedLinesEx(inputs[i].rect, 0.2f, 6, 1.6f, (Color){84, 100, 124, 255});
            DrawText(inputs[i].label, (int)inputs[i].rect.x + 16, (int)inputs[i].rect.y + 12, 22, (Color){29, 35, 48, 255});
        }

        DrawText("Press R to reset simulator", 30, 462, 18, (Color){80, 87, 98, 255});

        // Edge legend
        DrawRectangleRounded((Rectangle){310, 90, 700, 92}, 0.08f, 8, (Color){233, 237, 245, 255});
        DrawRectangleRoundedLinesEx((Rectangle){310, 90, 700, 92}, 0.08f, 8, 2.0f, (Color){106, 116, 132, 255});
        DrawText("Edge Legend", 328, 102, 22, (Color){24, 35, 55, 255});

        DrawLineEx((Vector2){330, 136}, (Vector2){430, 136}, 2.2f, (Color){106, 110, 120, 255});
        DrawText("inactive transition", 440, 126, 18, (Color){55, 63, 79, 255});

        DrawLineEx((Vector2){620, 136}, (Vector2){720, 136}, 3.8f, (Color){223, 108, 22, 255});
        DrawText("last triggered transition", 730, 126, 18, (Color){55, 63, 79, 255});

        DrawText(TextFormat("Active edge: %s", TransitionName(lastTransition)), 328, 154, 18, (Color){55, 63, 79, 255});

        // Edges from Mermaid model (simplified visual map)
        const Color edgeBase = (Color){106, 110, 120, 255};
        const Color edgeActive = (Color){223, 108, 22, 255};

        DrawArrow((Vector2){640, 122}, (Vector2){720, 122}, "reset queued",
              lastTransition == TRANS_READY_TO_QUEUED ? edgeActive : edgeBase,
              lastTransition == TRANS_READY_TO_QUEUED ? 3.8f : 2.2f);
        DrawArrow((Vector2){530, 154}, (Vector2){530, 230}, "reset",
              lastTransition == TRANS_READY_TO_SWEEP ? edgeActive : edgeBase,
              lastTransition == TRANS_READY_TO_SWEEP ? 3.8f : 2.2f);
        DrawArrow((Vector2){530, 294}, (Vector2){530, 370}, "tubeStart",
              lastTransition == TRANS_SWEEP_TO_TUBES ? edgeActive : edgeBase,
              lastTransition == TRANS_SWEEP_TO_TUBES ? 3.8f : 2.2f);

        DrawArrow((Vector2){640, 402}, (Vector2){720, 402}, "sweepStop",
              lastTransition == TRANS_TUBES_TO_SWEEP_STOPPED ? edgeActive : edgeBase,
              lastTransition == TRANS_TUBES_TO_SWEEP_STOPPED ? 3.8f : 2.2f);
        DrawArrow((Vector2){640, 260}, (Vector2){720, 384}, "sweepStop",
              lastTransition == TRANS_SWEEP_TO_SWEEP_STOPPED ? edgeActive : edgeBase,
              lastTransition == TRANS_SWEEP_TO_SWEEP_STOPPED ? 3.8f : 2.2f);

        DrawArrow((Vector2){840, 434}, (Vector2){840, 510}, "tubeStop",
              lastTransition == TRANS_SWEEP_STOPPED_TO_TUBES_STOPPED ? edgeActive : edgeBase,
              lastTransition == TRANS_SWEEP_STOPPED_TO_TUBES_STOPPED ? 3.8f : 2.2f);
        DrawArrow((Vector2){840, 574}, (Vector2){840, 650}, "liftStop",
              lastTransition == TRANS_TUBES_STOPPED_TO_PUSHER ? edgeActive : edgeBase,
              lastTransition == TRANS_TUBES_STOPPED_TO_PUSHER ? 3.8f : 2.2f);

        DrawArrow((Vector2){720, 682}, (Vector2){530, 122}, "pusherStop",
              lastTransition == TRANS_PUSHER_TO_READY ? edgeActive : edgeBase,
              lastTransition == TRANS_PUSHER_TO_READY ? 3.8f : 2.2f);
        DrawArrow((Vector2){840, 714}, (Vector2){840, 780}, "pusherStop",
              lastTransition == TRANS_PUSHER_TO_SWEEP ? edgeActive : edgeBase,
              lastTransition == TRANS_PUSHER_TO_SWEEP ? 3.8f : 2.2f);
        DrawArrow((Vector2){840, 780}, (Vector2){530, 780}, "auto startCycle when queued",
              lastTransition == TRANS_PUSHER_TO_SWEEP ? edgeActive : edgeBase,
              lastTransition == TRANS_PUSHER_TO_SWEEP ? 3.8f : 2.2f);
        DrawArrow((Vector2){530, 780}, (Vector2){530, 294}, "",
              lastTransition == TRANS_PUSHER_TO_SWEEP ? edgeActive : edgeBase,
              lastTransition == TRANS_PUSHER_TO_SWEEP ? 3.8f : 2.2f);

        DrawArrow((Vector2){470, 400}, (Vector2){470, 355}, "liftStop wait",
              lastTransition == TRANS_TUBES_RUNNING_SELF ? edgeActive : edgeBase,
              lastTransition == TRANS_TUBES_RUNNING_SELF ? 3.8f : 2.2f);
        DrawArrow((Vector2){760, 400}, (Vector2){760, 355}, "liftStop wait",
              lastTransition == TRANS_SWEEP_STOPPED_SELF ? edgeActive : edgeBase,
              lastTransition == TRANS_SWEEP_STOPPED_SELF ? 3.8f : 2.2f);

        DrawArrow((Vector2){840, 122}, (Vector2){530, 122}, "queued until pusherStop",
              lastTransition == TRANS_PUSHER_TO_SWEEP ? edgeActive : edgeBase,
              lastTransition == TRANS_PUSHER_TO_SWEEP ? 3.8f : 2.2f);

        for (int i = 0; i < STATE_COUNT; i++) {
            DrawNode(&nodes[i], IsStateActive(&machine, (DiagramState)i));
        }

        // Output and relay panel
        DrawRectangleRounded((Rectangle){1040, 90, 430, 350}, 0.06f, 8, (Color){236, 239, 231, 255});
        DrawRectangleRoundedLinesEx((Rectangle){1040, 90, 430, 350}, 0.06f, 8, 2.0f, (Color){111, 122, 89, 255});
        DrawText("Active Components", 1060, 102, 28, (Color){33, 49, 26, 255});

        DrawSignalLight(1080, 160, "Status LED", machine.statusLed, (Color){252, 195, 0, 255});
        DrawSignalLight(1080, 205, "Sweep Relay", machine.sweepRelay, (Color){32, 167, 90, 255});
        DrawSignalLight(1080, 250, "Tube Relay", machine.tubeRelay, (Color){20, 139, 204, 255});
        DrawSignalLight(1080, 295, "Pusher Relay", machine.pusherRelay, (Color){222, 72, 72, 255});
        DrawSignalLight(1080, 340, "Lift Relay", machine.liftRelay, (Color){140, 95, 224, 255});

        DrawText("Control Flops", 1060, 390, 24, (Color){45, 53, 33, 255});
        DrawText(TextFormat("sweepStopFlop: %s", machine.sweepStopFlop ? "true" : "false"), 1060, 422, 20, (Color){45, 53, 33, 255});
        DrawText(TextFormat("tubeStopFlop:  %s", machine.tubeStopFlop ? "true" : "false"), 1060, 448, 20, (Color){45, 53, 33, 255});
        DrawText(TextFormat("pusherStopFlop:%s", machine.pusherStopFlop ? " true" : " false"), 1060, 474, 20, (Color){45, 53, 33, 255});
        DrawText(TextFormat("holdResetValue:%s", machine.holdResetValue ? " true" : " false"), 1060, 500, 20, (Color){45, 53, 33, 255});

        DrawRectangleRounded((Rectangle){18, 710, 1452, 172}, 0.08f, 8, (Color){227, 231, 238, 255});
        DrawRectangleRoundedLinesEx((Rectangle){18, 710, 1452, 172}, 0.08f, 8, 2.0f, (Color){111, 118, 132, 255});
        DrawText("Last action:", 38, 724, 26, (Color){25, 32, 45, 255});
        DrawText(lastAction, 188, 728, 24, (Color){37, 44, 58, 255});
        DrawText("Event Log (newest first)", 38, 760, 22, (Color){25, 32, 45, 255});

        for (int i = 0; i < eventLogCount && i < 5; i++) {
            const int idx = (eventLogNext - 1 - i + MAX_EVENT_LOG) % MAX_EVENT_LOG;
            DrawText(eventLog[idx], 44, 790 + i * 17, 16, (Color){49, 56, 69, 255});
        }

        EndDrawing();
    }

    CloseWindow();
    return 0;
}
