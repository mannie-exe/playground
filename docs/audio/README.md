# Audio (planned)

Status: future design. No audio engine, synthesis or spatial renderer is
implemented. These contracts reserve a direction without adding runtime APIs.

AppHost owns an `AudioEngine`; SDL audio streams handle device output. A mixer
produces PCM independently of graphical frame pacing, UI sleep and window resize.

| Planned primitive | Responsibility |
|---|---|
| AudioClip | Immutable decoded samples for short sounds |
| StreamingSource | Bounded buffering and worker decoding |
| SynthSource | Procedural samples, initially oscillators and envelopes |
| VoiceHandle | Playback instance, pause/stop/loop/gain/pitch |
| AudioBus | Master/UI/Effects/Music volume, mute and routing |
| SoundCue | Semantic sound description independent of an asset |
| AudioEmitter / AudioListener | Spatial state independent of spatializer |

## Timing, ownership and limits

Audio uses its sample clock. Timestamped bounded commands transfer application
intent to the mixer. Parameter ramps avoid discontinuities; visual easing is not
a substitute for audio envelopes. Our rendering path performs no file access,
logging, application-lock waits, arbitrary user callbacks or heap allocation.
SDL callbacks need not use one particular thread. Decoders run separately.

One-shot sounds remain engine-owned until finished. Scoped looping voices stop
when their owner disappears. Resource retirement and completion publication occur
outside rendering. Source exhaustion and physical output completion are distinct.

Account decoded bytes, buffered duration, voices, processing time and underruns.
Bound work per block and queued commands; define refusal and priority-based voice
replacement explicitly. Device failure/default changes must be observable and
recoverable. UI cues follow accepted semantic actions, never paint calls; mute,
volume and repetition limits are shared settings.

## Spatialization and verification

Begin with attenuation and stereo positioning. HRTF, occlusion and reverberation
remain later replaceable processing stages. Acoustic simulation runs separately
and publishes bounded parameter updates; no ray tracing in the audio callback.

First implement playback, buses, fades and accounting, with deterministic offline
mixer tests and device smoke tests. Add synthesis and spatial processing afterward.
Do not implement a general DSP graph before concrete workloads require one.

References: [SDL stream setup](https://wiki.libsdl.org/SDL3/SDL_OpenAudioDeviceStream),
[callback contract](https://wiki.libsdl.org/SDL3/SDL_AudioStreamCallback).
