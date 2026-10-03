# Audio

AudioEngine owns playback, mixing, synthesis and spatial parameters. Playback
is independent of UI motion, graphical frame pacing and window event/resize
processing.

## Ownership and API

AppHost owns one AudioEngine/device service. Applications obtain scoped playback
access; engine configuration and the default device remain host policy. A
headless authority can omit audio and emit semantic sound events for clients.

| Primitive | Contract |
|---|---|
| AudioAsset | Catalog source plus validated decode/stream properties; source lifetime follows AssetReader |
| AudioClip | Immutable PCM, sample rate/channel layout, frame count and source identity |
| StreamingSource | Seekable source, worker decoder, bounded PCM ring and explicit underrun/end state |
| SynthSource | Bounded built-in oscillators/envelopes; no arbitrary callback in the mixer |
| VoiceId / VoiceHandle | Engine identity + slot/generation; control playback without owning a device |
| PlaybackProps / PlaybackPatch | Loop, gain, pitch, bus, timing and priority; validated owner-thread changes |
| AudioBus | Master/UI/Effects/Music routing, gain/mute and smooth parameter ramps |
| SoundCue | Semantic cue mapped to clips and finite variation/repetition policy |
| AudioEmitter / AudioListener | Spatial values independent of the spatializer |
| AudioSnapshot | Device state, sample clock, voices, queues, storage, underruns and errors |

`play` returns an accepted voice or explicit admission failure; accepted is not
proof that sound has reached the listener. One-shots remain engine-owned after
the returned control handle is dropped. An explicitly scoped looping handle
requests stop on owner disposal. App deactivation stops its voices unless the
host explicitly transfers a music/session scope. UI cues follow accepted semantic
actions, never paints or every pointer-motion sample. No default cue asset is
assumed or downloaded by the engine.

## SDL adapter and clock

One engine mixer produces interleaved float PCM at 48,000 frames/second
with stereo output. SDL_AudioStream adapts it to the device
format. Source resampling and pitch use bounded state across mixer blocks.
[SDL_OpenAudioDeviceStream](https://wiki.libsdl.org/SDL3/SDL_OpenAudioDeviceStream)
creates a paused stream/device pair; configure state before resuming. Its callback
is not tied to frame update. Keep device creation/destruction outside rendering.

Use a monotonically increasing mixed-frame index with a clock generation after
reset/device discontinuity. Scheduled commands name that generation and frame;
late starts execute at the next available frame and report lateness, while stale
generations reject. Production time and queued duration estimate latency; neither
is an exact audible presentation timestamp. A streaming seek has its own request
generation, clears obsolete buffered data and ramps across the discontinuity.

SDL [stream callbacks](https://wiki.libsdl.org/SDL3/SDL_AudioStreamCallback) can
run on different threads and requests vary in byte count. The adapter must not
assume a fixed callback size, thread identity or one block per callback. Process
whole PCM frames in bounded blocks, including zero/small requests, using a tested
cap on callback work and observable underruns. SDL may buffer/resample internally;
only the project's mixer can claim its own allocation/locking properties, not an
end-to-end hard real-time guarantee for SDL or the OS.

## Render-path discipline

The mixer uses preallocated voice slots, command storage, PCM buffers and a
bounded bus topology. It performs no file access, logging, waits on application
locks, general callbacks or heap allocation. Finite gains/pitch/positions and
validated channel layouts prevent invalid samples. Accumulate bounded float
samples and clamp final output to [-1, 1]; silence denormals and reject non-finite
source samples with SourceError. Test silence and saturation. A limiter is not a
replacement for bounded arithmetic or voice admission.

The owner/control thread is the single producer of a bounded command queue;
workers publish decode results to owned source buffers, not mixer callbacks.
Commands carry retained slot IDs, not borrowed app objects. Parameter changes may
coalesce by voice/property; play refusal is explicit. Reserve control capacity
or per-slot stop flags so saturation cannot prevent stop, deactivation or mute.

Finished voices enter a preallocated retirement mailbox with capacity derived
from admitted slots. The mixer never drops the final shared reference or frees
large buffers; the control side reclaims only after acknowledgment. Completed,
Stopped, Stolen, SourceError and DeviceLost remain distinct results, published
once outside rendering. A full notification queue cannot lose the retirement
record. Shutdown stops admission, quiesces callbacks and joins decoder work before
releasing its resources. Test these lifetime rules under device replacement.

## Limits, settings and recovery

Account PCM/source bytes, stream-ring capacity, scratch, active/reserved voices,
command occupancy, decode work, callback work and underruns. Integrate buffer
charges with shared managed accounting outside the callback; do not add audio
bytes to both an audio total and the process total as separate allocations.
Voice/stream limits are explicit configurable bounds, validated using offline
and device benchmarks. Priority stealing selects a documented victim and fades;
a voice that must not be stolen fails admission when no capacity exists.

Shared [audio settings](../platform/SETTINGS.md#shared-audio) define bus gains,
mute, output preference, background policy and draft preview/Apply/Save behavior.
Device IDs are transient; persist an adapter preference key, never a native ID.

Default-device changes and failure publish Recovering/Unavailable states and a
causal diagnostic. Reopen at a control boundary and invalidate obsolete device
clock mappings; avoid replaying already-fired one-shots on recovery. Decode EOF,
stream drained and physical output completion are separate observations. Silent
operation remains usable when no device is available.

Pause is explicit per scope/bus: UI remains active while Settings is open; local
gameplay loops may pause, music may continue, and online session time continues.
Mute changes output gain, not simulation or network time. Streaming sources must
define whether pausing retains bounded buffered data or stops decoding.

## Spatial work and acceptance

Clips, buses, fades, accounting, bounded streaming and built-in synthesis use the
same voice lifecycle. Oscillators and envelopes produce finite samples under a
bounded per-voice work contract; phase and envelope state survive block boundaries.
Tests use deterministic offline signals and the native device separately.

Spatialization consumes listener/emitter snapshots identified by world epoch,
SpaceId, tick and discontinuity. [World coordinates](../platform/WORLDS.md) use
meters and +Y up; legacy/authored scale converts at the input boundary. Resolve
moving-frame poses consistently, subtract precise listener position before any
float conversion, then apply distance attenuation and stereo positioning.
Unconnected spaces do not mix spatial voices; cross-space audio requires an
explicit app mix policy rather than subtracting unrelated coordinates. Teleports
reset position history; render-origin shifts do not move emitters or restart voices.
Validate positive distance bounds and finite poses. Mono
sources pan by listener-relative position; stereo sources preserve their authored
image unless explicitly downmixed for spatialization. Processing stages accept
bounded parameter updates. HRTF, Doppler, occlusion, reverberation and acoustic
simulation require a separately specified processor; no scene raycasts run in the
audio callback. The voice API does not imply those effects are enabled.

World/cell streaming can retain audio source data independently of visible geometry.
The app declares whether emitter deactivation stops, fades or virtualizes playback;
CPU/GPU scene eviction alone does not destroy a logical emitter. Publish bounded
copied spatial snapshots on the control path, never query the live world, navigate
or load cells in the audio callback. Multi-listener mixing is explicit per output,
not inferred from whichever viewport rendered last.

Tests cover sample-accurate starts/loops/fades, seek generations, invalid values,
queue saturation, repeated stop, stale handles, stealing, streaming starvation,
retirement and device loss. Vary block sizes and sample rates. Compare offline
output to reference signals and measure peak callback duration/underruns while
UI sleeps, windows resize, assets decode and rendering is under pressure. No
general DSP graph, audio editor, microphone capture or voice chat is required
by the playback contract.
