# Status API

The effect owns `org.kde.LumaSave` on the session bus while loaded and exports
`org.kde.LumaSave.Status` at `/LumaSave`.

- `statusJson() → string` returns one JSON object.
- `statusChanged()` is emitted when mode, state, reduction, or statistics change.

Schema 2 reports `mode`, `state`, `blockedReason`, requested and effective
brightness, relative and full-scale current reduction, and saved-backlight
seconds for the session, current day, and all time. The `*SavedBacklightSeconds`
fields are canonical. Older `*EquivalentFullReductionSeconds` aliases remain
for alpha widget compatibility.

Consumers must tolerate unknown keys and states. When the effect is unloaded,
the bus name disappears; this means Off/unavailable, not an error that should
be polled rapidly. Subscribe to D-Bus owner and status changes, with an
occasional refresh only for clock-like statistics.

