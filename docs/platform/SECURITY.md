# App permissions and isolation (future)

This future contract governs untrusted executable apps. Trusted native C++ apps
execute in the host process. Asset path validation, resource ledgers and activation
generations do not establish this security boundary. Pack validation and network
security remain required by their respective subsystem contracts.

## Threat model and enforcement

Assume a creator can submit malicious logic, manifests, media and protocol data.
Protect host files/credentials, other apps and sessions, user input/privacy, and
availability. A compromised OS, trusted host/runtime bug and hardware side channels
remain residual risks; permissions do not establish cheat-free gameplay or safe
social behavior. Data-only packs also reach complex native decoders.

A declaration records requested access. A grant is host authority after user and
platform policy. Every sensitive operation must cross an enforcing broker.
Untrusted native code loaded into the host could bypass a C++ facade and invoke
the OS directly; do not offer that as a restricted app mode.

Use a Wasm runtime in a separately supervised worker process, with only
explicit host imports, plus platform-specific process restrictions. Wasm narrows
code/memory authority; process isolation contains crashes and enables forced
termination. A subprocess alone does not restrict its OS privileges. Exact
macOS/Windows/Linux process controls and packaging implications require prototypes
before claiming support. Do not expose blanket WASI filesystem, environment,
process or socket access. [Wasmtime's security model](https://docs.wasmtime.dev/security.html)
provides a candidate isolation foundation, not the broker implementation.

Rendering/audio devices and permission UI stay in the trusted host. Batch bounded
value commands across IPC to avoid per-node/per-sample crossings. Validate all
requests in the host even if the worker SDK already validated them. Untrusted
media preparation should run in an isolated decoder worker; restricting app logic
does not protect a host font/image parser from hostile input. Custom native shaders
and arbitrary DSP/native extensions are not granted in the initial public profile;
use vetted material/cue primitives with bounded work.

## Capability contracts

| Capability | Scoped authority |
|---|---|
| content.read | Exact mounted dependency closure; no host path or implicit URL fetch |
| storage.private | App/account namespace, operation set and quota |
| storage.user-selected | Opaque user-selected file/directory grant; revocable, no path-based expansion |
| session.connect | Approved session/provider and protocol; scoped participant credentials |
| network.request | Explicit scheme/origin/port/method policy and byte/rate/time limits |
| network.listen / LAN discovery | Separate explicit grant; outbound access does not imply listening or broadcast |
| input | Foreground assigned input only; no global keyboard hooks |
| clipboard | Distinct read/write grants with platform/user-gesture requirements |
| audio.play | Scoped buses/voices/gain limits; recording is a separate grant |
| device.capture | Explicit mic/camera/device permission with visible host indication |
| presentation | Bounded UI/scene commands and resources; no native device pointers |
| jobs | Bounded tasks and runtime-owned scheduling; no arbitrary native thread creation |

Distribution policy may narrow permitted grants; it cannot expand user/platform
authority. No capability is implied by a package dependency, signature, downloaded
asset or manifest's requested limits. Unknown required capabilities reject activation;
optional denial is an ordinary typed outcome. The permission surface is owned by
the host and cannot be replaced or impersonated by the app's theme/documents.
Host-controlled chrome identifies the app and its active sensitive grants.

Grants bind instance/principal, resource scope, operations, quota, expiry and
revocation generation. Handles are validated against the caller's broker table;
guessable integer indexes alone confer no authority. Delegation is explicit and
can only narrow rights. A library dependency executes with the caller's selected
service scope, never its own manifest's broader request.

Revocation rejects new calls and cancels/quiesces pending operations where
possible. Every async delivery rechecks grant generation and instance lifetime;
closing a dialog is not sufficient revocation. Already-transmitted bytes or
completed external effects cannot be recalled; report that boundary honestly.
Shutdown retires resources and credentials without waiting forever for guest
cooperation. Policy/authentication errors stay distinguishable from I/O failure.

## Network policy

Enforce through the broker that performs the connection/request. Validate parsed
origins and approved resolved addresses, redirects, protocol upgrades and every
new connection. Deny loopback/private/link-local destinations by default in the
public profile; LAN use is an explicit separate grant. Check IPv4/IPv6 and bind
resolution validation to the actual connection to avoid DNS rebinding races.
Keep TLS hostname/certificate validation; never invent application cryptography.
These concerns follow [OWASP's SSRF guidance](https://cheatsheetseries.owasp.org/cheatsheets/Server_Side_Request_Forgery_Prevention_Cheat_Sheet.html).

Session access can expose only typed game/collaboration operations instead of
arbitrary internet sockets. Broker-owned credentials never enter guest memory.
Rate-limit and audit service requests, but do not log secrets or full user content
by default. Allowing an app to read data and send it to an approved destination
permits that combination; origin permissions cannot prove benign data use.
Consent must describe meaningful data access and destinations.

## Resource and failure containment

Bound guest linear memory, tables, stack, CPU execution, host calls, IPC bytes,
handles, timers, storage, network traffic, voices, decoded media and GPU work.
Account shared physical resources once and separately limit each instance's
entitlement/outstanding work. Existing rendering accounting is not per-app
isolation and does not cover dependency-private allocations.

Use VM execution interruption and a supervisor deadline, with bounded host-call
work. [Wasmtime fuel/epochs](https://docs.wasmtime.dev/examples-interrupting-wasm.html)
can bound guest execution; they do not cancel a blocked native host function.
Host services therefore require cancellation/deadlines and process-level recovery.
Quota reduction blocks growth, then follows an explicit reclaim/termination
policy. A malicious worker must not prevent another app or trusted permission UI
from being serviced. GPU driver calls can still fail/hang; bounded command counts
and vetted shaders reduce risk but are not hard GPU execution-time guarantees.

## Release gates

Before enabling untrusted distributions:

- Define the actual VM/ABI, broker allowlist and OS isolation profile per target;
  fail closed on targets without the promised boundary.
- Prove denied file/network/device operations cannot bypass the broker, including
  paths, links, DNS/redirects, forged handles, IPC framing and dependency grants.
- Test stale results after revocation, restart, app switch and identity changes;
  verify one app cannot read another's storage, input, credentials or private view.
- Fuzz manifests, archive readers, codecs and broker messages; isolate native
  decoders and test malformed/oversized media and decompression exhaustion.
- Exercise infinite loops, allocation/handle floods, blocked calls, crashes and
  shutdown under load with bounded effects on the host and other apps.
- Establish signed distribution/update trust, key rotation/revocation, rollback
  policy, vulnerability response and independent security review.

Tests and manifests alone cannot certify security. Publish the implemented threat
model, supported profiles, residual risks and verification evidence with a release.
Local pack loading must not be advertised as this future sandbox.
