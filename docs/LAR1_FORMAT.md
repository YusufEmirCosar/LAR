# LAR1 session format

`.lar` files are self-contained streams of validated UDP datagrams. They embed
the exact canonical JSON mapping needed to decode every record. All integers
are unsigned and little-endian.

## File layout

```text
+0  4 bytes   ASCII magic "LAR1"
+4  4 bytes   uint32 packetSectionOffset
+8  N bytes   UTF-8 compact JSON mapping
+packetSectionOffset zero or more packet records through exact EOF
```

The mapping length is derived, not separately stored:

```text
mappingLength = packetSectionOffset - 8
```

`packetSectionOffset` must be greater than 8, at or before EOF, and imply a
mapping no larger than 16 MiB. Empty mappings are invalid, so an offset of 8 is
rejected. A valid session may contain zero packet records.

## Record layout

```text
+0   8 bytes   uint64 relativeTimeMilliseconds
+8   4 bytes   uint32 packetSize
+12  N bytes   original UDP datagram bytes
```

Records are concatenated without alignment or padding. `packetSize` preserves
exact datagram boundaries, variable lengths, mapping gaps, and trailing
unmapped bytes.

## Resource limits

| Item | Limit |
| --- | ---: |
| Fixed header | 8 bytes |
| Embedded mapping | 16 MiB |
| One datagram payload | 16 MiB |
| Record count | No arbitrary product limit; signed 64-bit index/storage constraints apply |
| Supported relative duration | 365 days (31,536,000,000 ms) |
| Timestamp ordering | Non-decreasing; equal adjacent times are valid |

The disk timestamp field can represent the full `uint64` range, but values
beyond 365 days are outside the application domain and rejected. This keeps
playback math, formatting, and UI ratios exact and bounded on supported
platforms. Record counts and API indices use signed 64-bit values. Available
storage, valid file layout, duration, and that representation are natural
limits; the application imposes no separate session-record ceiling.

## Writer contract

`LarSessionWriter` implements `IRecordingTransaction`:

1. `begin(mappingJson)` parses and canonicalizes the mapping before creating
   transaction state.
2. It opens an auto-removing temporary file and writes `LAR1`, the computed
   packet offset, and compact JSON.
3. `append(timestamp, packet)` rejects empty/oversized packets, decreasing time,
   and any packet that does not decode and validate under the embedded mapping.
4. A record is encoded in memory, then appended. A short/failed write truncates
   back to the previous file size.
5. `reset()` opens a fresh temporary stream with the same canonical mapping.
6. `cancel()` closes/removes transaction storage and clears all state.

Recording timestamps represent active recording time. Paused wall-clock
intervals are excluded by `RecordingService`; receipt timestamps and drain
boundaries share the process-wide monotonic nanosecond domain before conversion
to exact milliseconds.

## Immutable snapshots

`createSnapshot()` flushes the transaction and captures:

- the temporary file path;
- its exact byte count at the barrier;
- a shared lifetime token that keeps the temporary file alive.

`FileSessionSnapshot::writeTo` reads only that stable prefix. Later recording
appends are outside the snapshot, and ending the live transaction cannot delete
the temporary source while persistence still owns the snapshot.

`QtSessionPersistence` writes the snapshot through `QSaveFile` and calls
`commit()`. The observable contract is atomic replacement: success exposes the
complete new file; failure preserves an existing destination.

## Reader validation pass

`LarSessionReader::loadFile` and `loadData` clear previous state before parsing.
A session becomes visible only after one complete pass succeeds:

1. Verify minimum header size and exact `LAR1` magic.
2. Read and validate `packetSectionOffset` and mapping size.
3. Parse the embedded JSON with `JsonMappingRepository`.
4. For every record, verify a complete 12-byte record header.
5. Enforce duration, non-decreasing time, payload size, and remaining bytes.
6. Read and decode every packet with the embedded `PacketMapping`; any invalid
   stored domain value rejects the entire session.
7. Store the timestamp and header offset of every 4,096th record as a sparse
   checkpoint. In parallel, retain every record location while the complete
   index remains within its 128 MiB budget.
8. For `loadFile`, retain a validated file as an immutable byte snapshot and
   close the original handle when the source is no larger than 512 MiB.
9. Commit the mapping, checkpoint table, optional complete index, selected
   random-access source, count, duration, and valid flag atomically.

The source and index budgets are independent. A file no larger than 512 MiB is
read from its immutable in-memory snapshot after validation; a larger file
remains file-backed. `loadData` retains the caller-supplied byte array because
it is already an in-memory source. The reader retains a complete per-record
location index while it fits within 128 MiB, regardless of source type. This
fast path makes `recordAt` direct and lets timestamp selection binary-search the
complete index without reconstructing pages.

If the complete index would exceed its budget, the reader discards it and uses
the checkpoint table plus one cached page of at most 4,096 locations. A lookup
then binary-searches checkpoint timestamps, materializes only the selected
page, and binary-searches that page. Sequential playback normally reuses the
same page. A file-backed source is size-checked during random access so growth
or truncation after validation is rejected; a cached source is independent of
later changes to the original file.

## Playback interpretation

- Duration is the final record timestamp, or zero for an empty session.
- Seek selects the last record whose timestamp is less than or equal to the
  requested exact time.
- Stop publishes the first record and resets position to zero.
- Play advances an exact cursor by `(1000 / 60) * positiveRate` milliseconds on
  every presentation tick.
- Each play tick performs one native resident-index or sparse-fallback lookup
  and decodes at most the last record strictly before the advanced cursor. It
  does not iterate through skipped records or probe unrelated file pages.
- Playback finishes after the cursor advances beyond the final timestamp.
  With Repeat enabled and a positive final timestamp, each newly calculated
  cursor is first reduced modulo that final timestamp and the wrapped value is
  looked up. This preserves overshoot across one or multiple loops.
- Burst is currently an inert presentation placeholder and has no LAR1 read or
  playback semantics.
- The online mapping is neither required nor modified by loading a session.

## Compatibility and evolution

The four-byte magic is the format version. Current readers accept only `LAR1`.
There are no optional header fields or extension chunks. A future incompatible
layout must use a new magic and a separate parser path; silently reinterpreting
existing bytes would break deterministic replay and corruption checks.

Compatible changes do not alter layout, for example:

- adding a new mapping field that an updated `StateField` registry recognizes;
- omitting fields from a mapping;
- choosing binary32 or binary64 per field;
- retaining extra unmapped/trailing datagram bytes.

An older reader that does not recognize a newly embedded mapping key will
reject that session instead of producing partial or misinterpreted state.

## Worked hexadecimal example

For a compact mapping of 64 bytes, the first record begins at offset 72
(`0x48`):

```text
4c 41 52 31             # "LAR1"
48 00 00 00             # packetSectionOffset = 72
... 64 JSON bytes ...
e8 03 00 00 00 00 00 00 # relativeTimeMilliseconds = 1000
08 00 00 00             # packetSize = 8
... 8 original packet bytes ...
```

The actual canonical JSON length determines the offset; implementations must
not assume the illustrative value above.
