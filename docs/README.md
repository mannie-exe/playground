# Playground contracts

These documents define the target architecture, behavior and APIs for
implementation. Sections marked **future** are excluded from the implementation
scope. Contract text is not a report of implementation or test status.

| Area | Contracts |
|---|---|
| Build, install and release | [Contributing](../CONTRIBUTING.md), [manifest generation](platform/MANIFESTS.md) |
| Applications and models | [Applications](platform/APPLICATIONS.md), [runtime](platform/RUNTIME.md), [activity](platform/ACTIVITY.md) |
| Content | [Asset ownership](platform/ASSETS.md), [packs](platform/PACKAGES.md), [versions/manifests](platform/MANIFESTS.md) |
| Audio | [Playback, synthesis, spatial state and accounting](audio/README.md) |
| Networking | [Transport, shared sessions and hosting](platform/NETWORKING.md) |
| Desktop policy | [Windows](platform/WINDOWING.md), [settings](platform/SETTINGS.md), [graphics](platform/GRAPHICS.md) |
| Rendering | [Architecture](render/ARCHITECTURE.md), [coordinates and values](render/CONTRACTS.md), [resources](render/RESOURCES.md) |
| UI composition | [Guide](ui/GUIDE.md), [reference](ui/REFERENCE.md), [controls](ui/CONTROLS.md) |
| UI services | [Themes](ui/THEMING.md), [motion](ui/MOTION.md), [async](ui/ASYNC.md), [accessibility](ui/ACCESSIBILITY.md), [testing](ui/TESTING.md) |

## Future capabilities

- [App authoring, executable bundles and dynamic loading](platform/DISTRIBUTED_APPS.md)
- [Executable-app permissions and isolation](platform/SECURITY.md)
- [Runtime UI documents and editor](ui/DOCUMENTS.md)
- Combobox and Autocomplete in [control contracts](ui/CONTROLS.md)

[C++ notes](NOTES.md), the [historical migration guide](CATCH_UP.md), and
[original learning outline](TODO.md) are learning material, not architecture
requirements or implementation backlogs.
