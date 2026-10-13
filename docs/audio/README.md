# Audio services and FMOD

The optional audio module provides scoped playback and spatial control independently
of graphics, application update cadence and window focus. FMOD Core and Studio are
the concrete native integration; the runtime does not require a second SDL mixer.

## Ownership and API

| Value/API | Contract |
|---|---|
| `AudioEngine` | Host-owned adapter lifetime, device policy, scope creation and diagnostics |
| `AudioCapabilities` | Available clip/stream/Studio-event, spatial, scheduling, device and output capabilities |
| `AudioScope` | Instance or explicitly host-owned playback, admission limits and pause/close policy |
| `AudioAsset` | Immutable encoded source and clip/stream preparation settings |
| `AudioClip` | Retained prepared clip identity; native realization remains adapter-owned |
| `AudioBankAsset` | Exact bank bytes, platform/build compatibility and dependency closure |
| `AudioEventRef` | Bank-set identity plus Studio event GUID; optional authored path for inspection |
| `VoiceHandle / AudioEventHandle` | Engine/scope generation and slot identity, never native pointers |
| `PlaybackProps / PlaybackPatch` | Gain, pause, loop/seek where supported, spatial binding and routing |
| `AudioEventProps` | Event reference, bounded typed parameters and initial spatial attributes |
| `AudioScope::play / startEvent / stop / apply` | Owner-side bounded commands with explicit admission and outcomes |
| `AudioSnapshot` | Device generation, preparation/playback state, errors, queue age and measured/estimated resource usage |

Clip playback and Studio events share lifetime conventions but retain different
capabilities. Do not flatten authored Studio parameters, timelines, buses or events
into a clip-only interface. Missing features return Unsupported before execution.
Native FMOD objects and callbacks stay behind the adapter. Applications may omit
sound entirely; requesting required audio on an unavailable adapter fails activation.

Reuse AssetReader, ServiceScope, generational handles and ResourceLedger. Shared bank
and clip storage is charged once; demand is attributed separately to scopes. A
host policy owns device changes and master gain. Application routing/parameters stay
within its configured bank set and scope. The baseline admits one resident app;
concurrent independent Studio projects and cross-app mixing are future work.

## FMOD adapter and clock

The adapter uses FMOD Core for ordinary clip/stream playback and exposes Studio
banks/events as a separate capability. When Studio is enabled, use its underlying
Core system rather than starting a second device engine. Studio's asynchronous
command processing requires regular update calls. See the
[Studio guide](https://www.fmod.com/docs/2.03/api/studio-guide.html).

A scoped service supplies update deadlines even when no view paints. Apply admitted
control batches and observe preparation/retirement on the adapter's designated
control thread. Native mixing and streaming remain FMOD responsibilities. Configure
format/channel layout and buffering explicitly; 48 kHz stereo may be a test profile,
but is not a portable fixed device contract. Core scheduling exposes a device/DSP
clock only when supported; Studio event timelines are a distinct time domain.

Requested, admitted, ready, playing, stopped and retired are distinct observations.
An accepted start does not prove audible output or sample readiness. Report native
lateness/errors instead of claiming sample-accurate correspondence with model ticks.
Changing device or restoring an engine increments its clock/resource generation.

## Bank and event preparation

Bank loading provides metadata separately from non-streaming sample readiness.
Streaming instruments have their own demand. The content closure records master,
referenced and sample banks; path lookup additionally requires the strings bank.
An FMOD bank is not a world cell or permission boundary. See
[Studio bank organization](https://www.fmod.com/docs/2.03/api/studio-guide.html#bank-layout).

`AudioBankRequest` names exact content, required readiness, deadline and byte limits.
Use nonblocking loading where supported and expose metadata/sample states through
retained request outcomes. Do not call blocking flushCommands or flushSampleLoading
from ordinary host update/render callbacks. A private preparation/control operation
that can block has a bounded admission policy and responsive host-side cancellation.
Native execution may still require retirement before resources can be released.
[Studio system API](https://www.fmod.com/docs/2.03/api/studio-api-system.html).

Retain file/mount/memory storage for the entire native read lifetime. Memory loading
must respect the selected copy/point mode and alignment. Package streams use explicit
file callbacks or a validated staging strategy; FMOD never resolves arbitrary pack
paths through the current working directory. Compressed entries cannot claim cheap
seeks without bounded indexed access or admitted decompression. Decode progress,
source storage and native memory are reported separately.

`BankLease` pins metadata and required sample dependencies while descriptions,
events or native reads remain in use. Stop/release owned events before final bank
unload. Unloading invalidates associated native descriptions/instances; a cache
trim cannot do this under another scope's live lease. Track asynchronous unload
completion before retiring referenced memory. [Bank API](https://www.fmod.com/docs/2.03/api/studio-api-bank.html).

Event release marks native disposal eligibility; it is not necessarily immediate
destruction. Playground preserves a stable terminal outcome while native callbacks
and resources retire. Dropping a caller handle does not accidentally stop a one-shot;
closing its owning scope does stop or explicitly transfer playback. Stops distinguish
immediate and authored fade/tail policies. [Event instance API](https://www.fmod.com/docs/2.03/api/studio-api-eventinstance.html).

## Callback and control discipline

FMOD callback threading depends on callback kind and initialization flags. Configure
and test that choice; never assume all callbacks run during the host update. Copy
bounded records into adapter-owned storage, then wake the service. Native callbacks
do not call app code, publish world state, log through blocking paths or retain raw
view pointers. User-data mappings survive through final callback retirement.
Callback features requiring synchronous app-provided objects, such as programmer
sounds, are unsupported in the baseline unless a registered native adapter can
satisfy them within the callback contract; they cannot await owner-thread delivery.
[FMOD threading](https://www.fmod.com/docs/2.03/api/studio-api-threads.html).

Reserve control capacity for stop, scope close and device recovery. Coalesce eligible
parameter updates; do not silently discard accepted lifecycle commands. Notification
overflow retains terminal state for reconciliation and reports diagnostics. Native
command-buffer capacity is not an allocator or latency guarantee; service admission
bounds Playground's work before calls enter FMOD.

## Spatial work and acceptance

`AudioEmitter` and `AudioListener` consume committed world epoch, SpaceId, pose,
velocity and discontinuity with explicit units. The app selects a listener; a camera
is only one possible source. Resolve frame motion once. Convert positions from
double world coordinates to a shared nearby audio origin before float conversion.
Choose handedness at adapter initialization and transform every position, direction
and velocity consistently. Preserve orientation and units; do not scale velocity by
frame delta. See [FMOD spatial attributes](https://www.fmod.com/docs/2.03/api/spatializing-sounds-in-the-core-api.html).

Baseline spatial playback supports one selected space/listener domain. Emitters in
another space require explicit routing, suppression or a later multi-domain adapter;
unrelated SpaceIds must not share distance calculations. Origin shifts change
representation without generating Doppler velocity. Teleport/recovery uses explicit
discontinuity handling. Missing required pose data has a declared retain/mute/stop
policy with bounded age.

World residency requests banks independently of visibility. Playing events and
streams pin their required source lifetime. Level transitions prepare destination
content before exchanging scene/audio bindings; fading events may retain old banks.
Physics contacts become app-chosen sound commands after completed-step publication.
Automatic occlusion, room acoustics, propagation and material-to-cue inference are
future/application features, not consequences of loading collision geometry.

## Limits, settings and recovery

Bound voices/events, banks, pending requests, command/callback records, streams,
source bytes and preparation estimates. Report FMOD memory/CPU/virtualization metrics
where available, separately from tracked Playground allocations. Virtual playback
is not equivalent to resource retirement. FMOD-private allocations are not a hard
process-wide budget merely because the adapter reserves an estimate.
[Core resource management](https://www.fmod.com/docs/2.03/api/managing-resources-in-the-core-api.html).

[Audio settings](../platform/SETTINGS.md#shared-audio) retain master gain, device
preference and background policy. UI/effects/music controls are optional logical
routes supplied by an app's routing profile, not mandatory Studio bus names.
Settings affect requested/effective state explicitly. No device yields Unavailable;
a silent test adapter does not claim audible playback capability.

Device loss stops new native admission and reports recovery. Recreate eligible
resources from retained definitions; do not replay completed one-shots. Pause,
mute, presentation suspension and instance disposal have separate behavior. Updates
needed for native loading/control/retirement continue while paused. The adapter
reports whether decoder work actually pauses; it cannot promise this for every
Studio-authored stream. Shutdown retires callbacks/readers before releasing systems.

## Build, content and verification

FMOD is opt-in through a supplied SDK root. Validate headers, libraries, architecture,
Debug/Release selection and the supported API version; record the exact SDK and
Studio bank build in build/content metadata. Documentation references the 2.03 API
family as an integration reference, not a claim about an installed SDK or latest
release. Native acceptance fixes the exact tested revision. Preserve vendor notices
and distribute only permitted runtime artifacts. Non-game deployment requires review
of the applicable FMOD terms; do not assume a game license covers every host app.
[FMOD licensing](https://www.fmod.com/licensing).

Use generated PCM for Core fixtures and a small redistributable Studio project/bank
set for events, parameters, loops, fades, spatial attributes and missing dependencies.
Record source/build compatibility for generated banks. Test load failure, callback
pressure, settings pause, close while loading/playing, bank release, device loss and
far-origin equivalence. Offline/fake tests establish control ordering; real FMOD
execution and device observation establish native/output coverage. SDK-disabled
builds must still compile the runtime and non-audio apps.

## Future audio capabilities

Arbitrary native DSP plugins, runtime synthesis APIs, voice capture/chat, acoustic
simulation, simultaneous independent app mixes and remote audio authoring remain
future. FMOD authoring tools remain external; Playground consumes compatible built
banks and optional adapter-specific values without reimplementing Studio.
