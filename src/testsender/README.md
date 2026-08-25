# Test Sender Tutorial

The project provides two UDP sender executables:

- `lar-test-sender` runs maintained, deterministic built-in scenarios.
- `lar-custom-test-sender` is a continuously compiled source template intended for local edits.

Both send mapped `Plane`/`Target` values and the atomic three-field DLZ
telemetry envelope through a JSON mapping. Start the viewer in Online mode with
the same mapping and UDP port before running a sender.

## Build and run

Configure and build both senders:

```bash
cmake -S . -B build
cmake --build build --target lar-test-sender lar-custom-test-sender
```

List the built-in scenarios. This command does not require a mapping:

```bash
./build/lar-test-sender --list-scenarios
```

Send 300 figure-eight packets to the local viewer:

```bash
./build/lar-test-sender \
  --map maps/full-state.json \
  --scenario figure-eight \
  --host 127.0.0.1 \
  --port 45454 \
  --count 300 \
  --interval 50
```

The options are:

| Option | Default | Meaning |
| --- | ---: | --- |
| `--map`, `-m` | required | JSON mapping used to encode each datagram |
| `--scenario`, `-s` | `default` | Built-in scenario name |
| `--list-scenarios` | off | Print names and descriptions, then exit |
| `--host`, `-H` | `127.0.0.1` | Numeric destination IP address |
| `--port`, `-p` | `45454` | Destination UDP port in `1..65535` |
| `--count`, `-c` | `100` | Number of packets, at least one |
| `--interval`, `-i` | `100` | Milliseconds between packets; defaults to `10` for `hundred-hz` |

The first packet is sent immediately. Scenario time is deterministic: packet `n` uses `elapsedSeconds = n * interval / 1000`. No wall-clock time or randomness enters built-in state generation.

## Built-in scenarios

| Scenario | Behavior |
| --- | --- |
| `default` | Preserves the original sender's slow eastward longitude drift and heading sweep. `Target.ir_r` remains `26000`. |
| `steady` | Holds all spatial, attitude, velocity, and LAR values fixed while packet time advances. |
| `straight-line` | Moves the aircraft east at a constant 200 m/s, wrapping longitude for long runs. |
| `coordinated-turn` | Flies a bounded circular track with tangent heading, constant speed, and coordinated roll around fixed, locally coherent IR/IZ centers. |
| `figure-eight` | Changes position, altitude, heading, pitch, roll, horizontal speed, and vertical rate on a bounded figure-eight. |
| `moving-target` | Moves both target/LAR centers on a bounded orbit while the aircraft remains steady. |
| `crossing-target` | Sends an eastbound aircraft while the target repeatedly crosses its track. |
| `pulsing-lar` | Independently varies IZ angular boundaries, inner/outer IZ ranges, and IR range. |
| `dateline-crossing` | Wraps aircraft and target longitudes between values near `+pi` and `-pi`. |
| `polar-lar-stress` | Moves one coherent near-pole IR/IZ center pair while independently resizing both large full-circle regions across the Mercator latitude limit and antimeridian. |
| `pole-crossing-route` | Flies the aircraft across the north and south poles with the required longitude reversal after each crossing. |
| `mixed-high-dynamics` | Combines bounded aircraft, target, altitude, attitude, velocity, angle, and range changes. |
| `hundred-hz` | Uses mixed high-dynamics state changes and a precise 10 ms timer by default, producing 100 packets per second. |
| `dlz-head-on-20` | Sends DLZ range 20 nm, aspect 0°, altitude 30 kft. |
| `dlz-beam-20` | Sends DLZ range 20 nm, aspect 90°, altitude 30 kft. |
| `dlz-tail-chase-13` | Sends DLZ range 13 nm, aspect 180°, altitude 55 kft. |
| `dlz-inside-rmin` | Sends DLZ range 1.2 nm, aspect 0°, altitude 30 kft. |
| `dlz-far-60` | Sends DLZ range 60 nm, aspect 0°, altitude 30 kft. |
| `dlz-aspect-sweep` | Sends 50 nm/55 kft while sweeping aspect 0°↔180°. |

Every built-in scenario treats `ir_pos` and `iz_pos` as one coherent target
pair. IZ retains a small, fixed local offset from IR (less than one kilometer)
so both centers remain visually associated while still exercising distinct
mapped fields. Moving, dateline, and polar scenarios derive IZ from the current
IR anchor instead of integrating the two positions independently.

Examples:

```bash
# Baseline packet compatibility
./build/lar-test-sender -m maps/full-state.json -s default -c 10 -i 100

# Exercise longitude wrapping quickly
./build/lar-test-sender -m maps/full-state.json -s dateline-crossing -c 400 -i 50

# Exercise all changing display inputs
./build/lar-test-sender -m maps/full-state.json -s mixed-high-dynamics -c 1000 -i 20

# Exercise large moving polar IR and IZ fills
./build/lar-test-sender -m maps/full-state.json -s polar-lar-stress -c 500 -i 50

# Exercise aircraft routing across both poles
./build/lar-test-sender -m maps/full-state.json -s pole-crossing-route -c 80 -i 50

# Send 100 packets per second for ten seconds
./build/lar-test-sender -m maps/full-state.json -s hundred-hz -c 1000

# Send the three-field DLZ protocol to the viewer
./build/lar-test-sender -m maps/dlz-inputs.json \
  -s dlz-head-on-20 -c 100 -i 100

# Combine legacy LAR fields and DLZ telemetry in one datagram
./build/lar-test-sender -m maps/full-state-dlz.json \
  -s dlz-aspect-sweep -c 200 -i 50
```

## Units and fields

The names and indexes in a map resolve to these values:

| Mapped field | Unit and meaning |
| --- | --- |
| `Plane.location[0]` | Latitude in radians |
| `Plane.location[1]` | Longitude in radians |
| `Plane.location[2]` | Altitude in meters |
| `Plane.euler[0..2]` | Yaw/heading, pitch, and roll in radians |
| `Plane.velocity[0..2]` | X, Y, and Z axis velocity in kilometers/hour |
| `Target.iz_pos[0..1]` | IZ center latitude and longitude in radians |
| `Target.iz_pos[2]` | IZ center radius in meters |
| `Target.ir_pos[0..1]` | IR/target latitude and longitude in radians |
| `Target.ir_pos[2]` | IR/target radius in meters |
| `Target.iz_theta1`, `Target.iz_theta2` | IZ angular boundaries in radians; `theta` is the protocol field spelling |
| `Target.iz_r1`, `Target.iz_r2` | Inner and outer IZ range in meters |
| `Target.ir_r` | IR range in meters |
| `Target.time` | Scenario time in seconds |
| `dlz_range_nm` | DLZ range in nautical miles |
| `dlz_aspect_deg` | DLZ aspect in degrees, `0..180` |
| `dlz_altitude_ft` | DLZ altitude in feet |

Geographic movement in `scenarios.cpp` uses a spherical Earth radius of 6,371,000 meters to turn local north/east meter offsets into latitude/longitude changes. That is suitable for repeatable display testing, not navigation or geodesic simulation.

## How mapping changes packets

The sender does not transmit the in-memory `Plane` and `Target` structures directly. `PacketMapping::encode` applies each JSON entry:

```json
{"name": "iz_r1", "index": 0, "offset": 24, "size": 8}
```

- `name` and `index` select one state field.
- `offset` selects its first byte in the datagram.
- `size: 4` writes a little-endian IEEE-754 float.
- `size: 8` writes a little-endian IEEE-754 double.
- Datagram size is the largest mapped `offset + size`; unmapped gaps contain zero bytes.
- Reordering entries changes no values, only their packet locations.
- Omitting a field means the receiver reports it unavailable and cannot display geometry that requires it.
- A mapping may include none of the DLZ fields or all three; partial DLZ mappings are rejected so a frame cannot mix old and new DLZ values.

A sender and viewer must use the same mapping. Float mappings have less precision than double mappings. Built-in longitude values retain a boundary margin so float conversion cannot round a valid longitude beyond `+/-pi`.

## Add a built-in scenario

Built-in scenario declarations are in `scenarios.h`; implementation is in
`scenarios.cpp`. The six `dlz-*` scenarios populate the three-field telemetry
output and are intended to be exercised over UDP with `maps/dlz-inputs.json` or
`maps/full-state-dlz.json`.

1. Add the command-line name and definition to `src/testsender/scenarios.cpp`.
2. Provide deterministic initialization and elapsed-time updates.
3. For a DLZ scenario, write only `rangeNm`, `aspectDegrees`, and `altitudeFeet` to the telemetry output; Mach values remain viewer-side defaults.
4. Keep paths bounded or wrap longitude so an arbitrarily long run remains valid.
5. Build and run the scenario test shown below.

A minimal bounded rotation branch looks like this:

```cpp
if (name == QStringLiteral("heading-sweep")) {
    plane->euler[0] = std::fmod(0.4 * elapsedSeconds, 2.0 * Pi);
    plane->euler[1] = 0.12 * std::sin(0.7 * elapsedSeconds);
    plane->euler[2] = 0.35 * std::sin(0.3 * elapsedSeconds);
    return;
}
```

Then verify it:

```bash
cmake --build build --target lar-test-sender lar-testsender-scenario-tests
./build/lar-testsender-scenario-tests
./build/lar-test-sender -m maps/full-state.json -s heading-sweep -c 200 -i 50
```

`scenario_tests.cpp` runs every listed scenario at short and very large packet times, validates each generated state with `StateValidator`, encodes it through `PacketMapping`, and checks deterministic reproduction.

## Edit the custom sender

The editable template is `src/testsender/custom_sender.cpp`. Its three customization functions are isolated at the top:

1. `CustomScenario::initialize()` assigns a complete valid initial state, including default DLZ telemetry.
2. `CustomScenario::update()` changes state before each packet and before validation.
3. `CustomScenario::mutateDatagram()` optionally changes encoded bytes after validation.

Edit only those functions for most experiments. Build and run the template with the same mapping, host, port, count, and interval options:

```bash
cmake --build build --target lar-custom-test-sender
./build/lar-custom-test-sender \
  --map maps/full-state.json \
  --host 127.0.0.1 \
  --port 45454 \
  --count 500 \
  --interval 20
```

The custom executable is a development tool and is intentionally not installed by CMake.

### Movement example

Use elapsed time for repeatable motion. This example moves east at about 150 m/s around the template latitude:

```cpp
constexpr double earthRadius = 6371000.0;
constexpr double latitude = 0.71558;
plane->location[0] = latitude;
plane->location[1] = 0.50490
    + (150.0 * elapsedSeconds) / (earthRadius * std::cos(latitude));
plane->velocity[0] = 150.0 * 3.6; // km/h
target->time = elapsedSeconds;
```

For long runs, wrap the resulting longitude into `[-pi, pi]` as `scenarios.cpp` does. A target moves the same way by updating both `target->iz_pos` and `target->ir_pos`.

### Rotation example

```cpp
constexpr double twoPi = 6.28318530717958647692;
plane->euler[0] = std::fmod(0.75 + 0.2 * elapsedSeconds, twoPi);
plane->euler[1] = 0.08 * std::sin(0.9 * elapsedSeconds);
plane->euler[2] = 0.45 * std::sin(0.4 * elapsedSeconds);
```

### LAR example

Keep the inner range lower than the outer range for every phase:

```cpp
const double pulse = std::sin(0.5 * elapsedSeconds);
target->iz_theta1 = -0.6 - 0.15 * pulse;
target->iz_theta2 =  0.7 + 0.15 * pulse;
target->iz_r1 = 1500.0 + 500.0 * pulse;   // 1000..2000 m
target->iz_r2 = 15000.0 + 2500.0 * pulse; // 12500..17500 m
target->ir_r = 26000.0 + 4000.0 * pulse;  // 22000..30000 m
```

### Time and rate example

`elapsedSeconds` is simulated packet time. Derive state and rates from the same formula rather than accumulating updates:

```cpp
const double altitudeRateMps = 40.0 * std::cos(0.2 * elapsedSeconds);
plane->location[2] = 3200.0 + 200.0 * std::sin(0.2 * elapsedSeconds);
plane->velocity[2] = altitudeRateMps * 3.6;
target->time = elapsedSeconds;
```

`packetIndex` is useful for exact packet patterns, such as changing a value every tenth datagram.

### Invalid packet example

State updates are validated before encoding, so malformed-packet tests belong in `mutateDatagram()`. To send every tenth packet one byte short:

```cpp
void mutateDatagram(int packetIndex, QByteArray *datagram)
{
    if ((packetIndex + 1) % 10 == 0) datagram->chop(1);
}
```

This reliably violates the selected mapping's minimum packet size. To inject NaN, infinity, reversed ranges, or another invalid mapped value, write the desired little-endian float/double bytes at the offset declared by the mapping. Such mutation code is mapping-specific; for `maps/full-state.json`, for example, `iz_r2` starts at byte 112 and is an 8-byte double.

## Validation constraints

Before encoding, both senders validate all state fields with `StateValidator`; `PacketMapping` then applies the selected mapping and encodes the validated state:

- Every value must be finite. NaN and positive/negative infinity are rejected.
- Aircraft, IZ, and IR latitude must be in `[-pi/2, pi/2]`.
- Aircraft, IZ, and IR longitude must be in `[-pi, pi]`.
- `Target.iz_r1` must be non-negative.
- `Target.iz_r2` must be positive.
- `Target.iz_r1 < Target.iz_r2` must hold.
- `Target.ir_r` must be positive.

There are currently no codec range constraints on altitude, Euler angles, velocity, target position element 2, IZ angular boundaries, or time beyond finiteness. Use physically meaningful values anyway when testing display behavior.

Mutation runs after this validation by design. It can therefore create packets that the receiver should reject without weakening normal sender checks.

## Practical test checklist

- Build `lar-test-sender`, `lar-custom-test-sender`, and `lar-testsender-scenario-tests`.
- Run `./build/lar-testsender-scenario-tests` and confirm exit code zero.
- Confirm `--list-scenarios` works without `--map`.
- Load the exact same mapping in sender and viewer.
- Verify sender host/port matches the viewer's listening interface and port.
- Start with `steady` and confirm every mapped Current Values field is stable except time.
- Run `figure-eight` and `moving-target` to inspect aircraft and target motion independently.
- Run `pulsing-lar` and confirm inner range never crosses outer range.
- Run `dateline-crossing` and inspect the `+pi` to `-pi` longitude transition.
- Run `mixed-high-dynamics` at a short interval and check rendering, recording, and replay.
- Test both 4-byte and 8-byte field mappings when precision matters.
- For malformed packets, establish a valid baseline first, then add one mutation at a time.
- Confirm rejected malformed packets produce diagnostics and do not replace the last valid state.
