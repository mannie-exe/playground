# Motion and content transitions

Motion is owner-thread UI work. `UIRoot::motion()` owns playback; specifications
contain values and timing, while bindings resolve live targets. Simulation and
audio clocks remain independent. Native window size/position requests are not
interpolated: window managers may defer, constrain or deny them, and resizing can
recreate presentation resources. Animate content inside the established window.

## Values and playback

`MotionSpec` defines duration/delay in seconds, easing, repeat count and direction.
Linear, steps and cubic Bézier easing are supported. `Keyframes<T>` contains finite,
ordered offsets in [0,1], including both endpoints. Supported values are float,
2D vectors and colors. `TimelineSpec` combines tracks at explicit offsets, in
sequence or in parallel. Overlapping tracks for the same property are rejected;
sequential tracks may reuse a binding.

`MotionBinding<T>` identifies one property through a lifetime-safe target.
Built-in bindings expose opacity, translation, scale, rotation, background and
border color. Paint-color bindings address generic node chrome; controls with
their own painting use a custom MotionValue instead. Transform components compose
with the authored affine transform;
they do not decompose arbitrary matrices. `MotionValue<T>` supports custom paint
parameters with an invalidation callback. Property IDs and specifications are
separate from runtime target bindings; no editor or disk loader is provided.

`transition(binding, target, spec)` authors the destination once and interpolates
from the displayed value. Keyframe playback is a temporary presentation effect.
Finishing or cancelling removes the effect and reveals the authored value. A
transition therefore reveals its destination on cancellation. Retargeting starts
from the displayed value. Direct property writes replace active effects, including
writes of the existing authored value through the property-specific setter.

One playback owns a property at a time. Replacing/writing one property retires
only that property's tracks; unrelated tracks keep their original clock. A
partially interrupted timeline remains Running/Paused until its remaining tracks
finish, then reports Replaced (or TargetGone if any target disappeared). Explicit
cancel reports Cancelled. Destroying/reassigning the timeline handle cancels
all of its remaining tracks. Move-only `AnimationHandle` cancels on
destruction and supports pause, resume, seek, finish, cancel and status. Outcomes
are Completed, Cancelled, Replaced and TargetGone. Detached nodes invalidate their
bindings. Custom value invalidation callbacks only mark paint dirty; they must
not mutate playback.
Completion notifications run outside sampling and may enqueue structural
changes through `UIRoot::defer`. Callback exceptions must not repeat notification.

Playback admission defaults to 1024 tracks and 16384 keyframes per engine;
`MotionLimits` configures these bounds. No-op transitions settle immediately.
Pending completion records are bounded alongside active playback. Statistics
expose live playback,
track/keyframe counts and known retained storage, not process memory or residency.
No allocation-free sampling or free opacity-compositing guarantee is implied.

## Time and presentation

Advancing elapsed time and sampling properties are separate. Input dispatch can
advance timers without resampling every animation. The session samples motion at
presentation opportunities, before preparation; transformed hit geometry matches
the last sampled visual state. Active visual motion requests frames through normal
frame admission. Delayed starts and completion deadlines wake idle sessions.
Paused motion creates no frame demand. Final invalidation remains pending until
successful presentation. Blocked presentation skips intermediate samples rather
than extending elapsed-time animations. Explicit pause freezes elapsed time.

`ThemeMotion` supplies Feedback (120 ms), Reveal (180 ms) and Dismiss (120 ms).
Shared `MotionPreference` is System, Full, Reduced or None. System follows the
available native reduced-motion preference; unknown uses Full. Reduced settles
spatial/decorative effects and permits short opacity/color feedback; None settles all
motion immediately. Preference changes apply to existing playback. Functional
timers and game simulation remain independent.

## Presence and replacement

`Presence` retains one child across entering, present, exiting and hidden states.
Exit collapses it after playback; reversal starts from the current presentation.
`TransitionHost` replaces keyed content and retains at most incoming and outgoing
trees. Incoming determines layout; outgoing is clipped to the host and removed
after exit. Rapid replacement discards an obsolete outgoing tree.

Outgoing content is inert: it remains painted but cannot receive input, focus or
accessibility actions. Inertness is separate from authored enabled/visibility.
Captured gestures are cancelled; callers explicitly choose focus transfer. Failed
candidate construction, attachment or animation admission preserves usable
existing content. Admission rollback restores its key, authored opacity, local
inert state and scheduled focus; interrupted prior animations settle rather than
resume. Cancelled pointer gestures are not recreated.

## Extension boundaries

Springs, additive mixing, layout interpolation, shared-element snapshots and
native-window animation remain planned. These APIs do not implement React
concurrent reconciliation or suspend arbitrary C++ execution. Async data ownership
and reveal policy are defined in [ASYNC.md](ASYNC.md).

## References

- [Web Animations effect composition](https://www.w3.org/TR/web-animations-1/#combining-effects)
- [Qt animation lifecycle](https://doc.qt.io/qt-6/qabstractanimation.html)
- [Anime.js retargeting](https://animejs.com/documentation/animatable/)
- [React transitions](https://react.dev/reference/react/useTransition)
- [SDL window size requests](https://wiki.libsdl.org/SDL3/SDL_SetWindowSize)
- [SDL window position requests](https://wiki.libsdl.org/SDL3/SDL_SetWindowPosition)
