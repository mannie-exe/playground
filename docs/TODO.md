# Baseline C/C++ SDL3 Application Architecture TODO

This is the original learning outline, not an implementation-status checklist.
Names and unchecked items below are conceptual exercises, not requirements to
duplicate existing modules. Current contracts live in [ui/REFERENCE.md](ui/REFERENCE.md),
[ui/CONTRACTS.md](ui/CONTRACTS.md), and [render/GPU.md](render/GPU.md).

This document describes baseline structs, processes, algorithms, and references for building small cross-platform C/C++ programs using SDL3, SDL_GPU, SDL audio, optional SDL_image, optional SDL_net, and CMake/CPM.

It does not assume a specific experiment, game, simulation, renderer, or UI.

---

## TODO: Core Structs To Model

### Application Ownership

- [ ] Define `App`
  - Owns all long-lived application state.
  - Contains platform, input, timing, resources, world/state, scheduler, renderer, and audio state.
  - Should be created once at startup and destroyed once at shutdown.

- [ ] Define `Env` or `Platform`
  - Owns SDL initialization state.
  - Owns window.
  - Owns GPU device/context if using `SDL_GPU`.
  - Owns audio device/stream state if using audio.
  - Stores platform paths, display info, DPI info, and runtime flags.

- [ ] Define `AppConfig`
  - Window title.
  - Initial width/height.
  - VSync preference.
  - Target simulation tick rate.
  - Asset path roots.
  - Debug/profiling flags.

---

## TODO: Time And Loop State

- [ ] Define `Clock`
  - Tracks real time.
  - Tracks frame delta.
  - Tracks accumulated simulation time.
  - Tracks frame count.
  - Tracks fixed timestep.

- [ ] Implement fixed timestep update
  - Simulation updates should run with stable `dt`.
  - Rendering may run once per frame using current state.
  - Clamp large frame deltas to avoid runaway catch-up loops.

- [ ] Define `FrameState`
  - Per-frame temporary state.
  - Frame index.
  - Delta time.
  - Interpolation alpha.
  - Debug counters.

---

## TODO: Input And Events

- [ ] Define `InputState`
  - Current keyboard state.
  - Current mouse position.
  - Mouse button state.
  - Gamepad state if needed.
  - Text input state if needed.

- [ ] Define `InputEvent`
  - Normalized input event copied from SDL events.
  - Avoid letting raw SDL events spread through the whole program.

- [ ] Define `EventQueue`
  - Ring buffer or vector of events.
  - Used for cross-system messages.
  - Cleared or advanced once per frame.

- [ ] Define `Command`
  - Intentional action derived from input, UI, scripts, networking, timers, or tools.
  - Examples: quit app, change mode, load asset, toggle debug panel, start playback.

- [ ] Define `CommandBuffer`
  - Stores commands for the current frame.
  - Executed in a predictable order.

---

## TODO: State And Data Containers

- [ ] Define `World` or `Model`
  - Holds the durable state being edited, simulated, displayed, or processed.
  - Should not own SDL window/device resources.
  - Should be serializable later if possible.

- [ ] Define `EntityId` / `Handle`
  - Prefer IDs over raw pointers for objects that may move or be deleted.
  - Consider generation counters for dynamic containers.

- [ ] Define fixed-capacity containers first
  - Arrays plus counts.
  - Easier to reason about.
  - Good for learning memory layout and ownership.

- [ ] Define dynamic containers later
  - `std::vector`
  - Handle tables
  - Free lists
  - Slot maps
  - Resource registries

---

## TODO: Resource Management

- [ ] Define `ResourceStore`
  - Owns loaded textures, shaders, fonts, buffers, sounds, and other assets.
  - Returns handles instead of exposing raw resources everywhere.

- [ ] Define resource handle types
  - `TextureHandle`
  - `ShaderHandle`
  - `BufferHandle`
  - `SoundHandle`
  - `FontHandle`

- [ ] Implement asset lifecycle
  - Load
  - Validate
  - Store
  - Retrieve by handle
  - Destroy at shutdown

- [ ] Add hot reload later
  - Shaders
  - Images
  - Config files
  - Layout files

---

## TODO: Rendering

- [ ] Define `Renderer`
  - Owns GPU pipeline state.
  - Owns swapchain interaction.
  - Owns render passes.
  - Owns frame-local rendering data.

- [ ] Define `RenderCommand`
  - A draw request independent of high-level world logic.
  - Examples: draw rect, draw image, draw mesh, draw text, draw debug line.

- [ ] Define `RenderQueue`
  - Stores render commands for the frame.
  - Can be sorted by layer, material, texture, depth, or pass.

- [ ] Implement render phases
  - Begin frame.
  - Build render commands.
  - Sort render commands.
  - Upload/update GPU buffers.
  - Encode GPU commands.
  - Submit/present.

---

## TODO: Audio

- [ ] Define `AudioSystem`
  - Owns SDL audio stream/device state.
  - Owns sample buffers or procedural generators.

- [ ] Define `AudioCommand`
  - Play sound.
  - Stop sound.
  - Set volume.
  - Set parameter.
  - Push generated samples.

- [ ] Implement audio update boundary
  - Keep audio data flow explicit.
  - Avoid unpredictable allocation inside the audio path.

---

## TODO: Scheduling

- [ ] Define `SystemTask`
  - Function pointer or callable.
  - Phase.
  - Optional dependencies.
  - Optional enabled flag.

- [ ] Define `Scheduler`
  - Stores ordered systems.
  - Runs them in phase order.
  - Later may support dependency ordering.

- [ ] Define baseline phases
  - `Input`
  - `Commands`
  - `Simulation`
  - `Audio`
  - `RenderPrep`
  - `Render`
  - `Cleanup`

- [ ] Add timer scheduling
  - Delayed commands.
  - Repeating commands.
  - Timeouts.

---

## TODO: Algorithms To Know And Implement

- [ ] Fixed timestep loop
- [ ] Ring buffer
- [ ] Stable sort
- [ ] Priority queue / min-heap
- [ ] Free list
- [ ] Handle table with generation counters
- [ ] Dirty flags
- [ ] Double buffering
- [ ] State machine
- [ ] Command pattern
- [ ] Basic dependency graph
- [ ] Topological sort
- [ ] Spatial grid, only when proximity queries are needed
- [ ] Serialization boundary, later

---

## TODO: Inline State Design

Use this for the first version.

```cpp
constexpr uint32_t MAX_EVENTS = 256;
constexpr uint32_t MAX_COMMANDS = 256;
constexpr uint32_t MAX_RENDER_COMMANDS = 4096;
constexpr uint32_t MAX_RESOURCES = 1024;

struct App {
    AppConfig config;
    Env env;
    Clock clock;
    FrameState frame;

    InputState input;
    EventQueue events;
    CommandBuffer commands;

    World world;
    ResourceStore resources;
    AudioSystem audio;
    Renderer renderer;
    Scheduler scheduler;

    bool running;
};
```

Characteristics:

- State is directly owned.
- Fewer allocations.
- Easier shutdown.
- Easier debugging.
- Good baseline for C-style and embedded-style thinking.

---

## TODO: Dynamic State Design

Use this once the inline version becomes limiting.

```cpp
struct App {
    AppConfig config;
    std::unique_ptr<Env> env;

    Clock clock;
    FrameState frame;

    InputState input;
    std::vector<InputEvent> events;
    std::vector<Command> commands;

    std::unique_ptr<World> world;
    ResourceStore resources;
    AudioSystem audio;
    Renderer renderer;
    Scheduler scheduler;

    bool running;
};
```

Recommended dynamic handle shape:

```cpp
struct Handle {
    uint32_t index;
    uint32_t generation;
};
```

Characteristics:

- Flexible counts.
- Better for editors, tools, and loading arbitrary files.
- Requires careful lifetime handling.
- Avoid storing raw pointers to objects inside resizable arrays.

---

# Appendix A: Glossary

## App

The root object. Owns the program state and coordinates startup, frame execution, and shutdown.

## Env / Platform

The boundary between your application and the operating system. SDL windowing, GPU device, audio device, file paths, and platform-specific facts belong here.

## World / Model

The durable state your program is actually about. It should be independent from SDL where practical.

## Event

Something that happened. Usually factual and low-level.

Example: mouse moved, key pressed, window resized.

## Command

Something the program intends to do.

Example: quit, load file, create object, switch mode, play sound.

## System

A process that updates one part of the application.

Example: input system, simulation system, audio system, render system.

## Scheduler

The code that decides which systems run, and in what order.

## Resource

A loaded external or GPU-backed object.

Example: texture, shader, font, sound, mesh, buffer.

## Handle

A small ID used to refer to a resource or object without exposing a raw pointer.

## Ring Buffer

A fixed-size queue where old entries are overwritten or discarded as new entries arrive.

Useful for events, logs, samples, and history.

## Free List

A list of unused slots inside a fixed or dynamic pool.

Useful for reusing object storage without constantly allocating.

## Dirty Flag

A boolean or bit saying “this derived data must be rebuilt.”

Useful for transforms, render batches, layout, caches, and resource uploads.

## Fixed Timestep

A loop strategy where simulation advances in consistent increments, such as `1.0 / 60.0`.

This gives more stable behavior than using variable frame deltas everywhere.

## Render Command

A small description of something to draw.

The world produces render commands; the renderer consumes them.

## Double Buffering

Keeping separate read and write buffers so one system does not mutate data another system is currently reading.

---

# Appendix B: Optional Pseudo-Code

## Core State

```cpp
struct AppConfig {
    const char* title;
    int window_width;
    int window_height;
    bool vsync;
    double fixed_dt;
    const char* asset_root;
    bool debug_tools;
};

struct Env {
    SDL_Window* window;
    SDL_GPUDevice* gpu;
    SDL_AudioStream* audio_stream;
    int drawable_width;
    int drawable_height;
    float dpi_scale;
};
```

## Main Loop

```cpp
init_app(&app);

while (app.running) {
    clock_begin_frame(&app.clock, &app.frame);

    collect_platform_events(&app.env, &app.events);
    update_input_state(&app.input, &app.events);

    translate_events_to_commands(&app.input, &app.events, &app.commands);
    execute_commands(&app, &app.commands);

    while (clock_should_simulate(&app.clock)) {
        run_systems(&app.scheduler, Phase_Simulation, &app);
        clock_advance_fixed_tick(&app.clock);
    }

    run_systems(&app.scheduler, Phase_Audio, &app);
    run_systems(&app.scheduler, Phase_RenderPrep, &app);
    run_systems(&app.scheduler, Phase_Render, &app);

    clear_frame_temporaries(&app);
}

shutdown_app(&app);
```

## Fixed Timestep

```cpp
struct Clock {
    double now;
    double last;
    double frame_dt;
    double accumulator;
    double fixed_dt;
    uint64_t frame_index;
};

struct FrameState {
    float dt;
    float alpha;
    uint32_t simulation_ticks;
    uint32_t draw_calls;
};

double fixed_dt = 1.0 / 60.0;
double max_frame_dt = 0.25;

frame_dt = min(measure_time_since_last_frame(), max_frame_dt);
accumulator += frame_dt;

while (accumulator >= fixed_dt) {
    tick_world(world, fixed_dt);
    accumulator -= fixed_dt;
}

alpha = accumulator / fixed_dt;
render(world, alpha);
```

## Command Buffer

```cpp
struct InputState {
    bool keys[SDL_SCANCODE_COUNT];
    bool mouse_buttons[8];
    float mouse_x;
    float mouse_y;
    float mouse_dx;
    float mouse_dy;
    float wheel_x;
    float wheel_y;
};

struct InputEvent {
    enum Type { Quit, KeyDown, KeyUp, MouseDown, MouseUp, MouseMove, MouseWheel, Resize } type;
    int key;
    int button;
    float x;
    float y;
};

struct EventQueue {
    InputEvent items[MAX_EVENTS];
    uint32_t begin;
    uint32_t count;
};

struct Command {
    enum Type { QuitApp, LoadAsset, ToggleDebug, SetMode } type;
    uint32_t id;
    float value;
};

struct CommandBuffer {
    Command items[MAX_COMMANDS];
    uint32_t count;
};

void push_command(CommandBuffer* buffer, Command command) {
    if (buffer->count < MAX_COMMANDS) {
        buffer->items[buffer->count++] = command;
    }
}

void clear_commands(CommandBuffer* buffer) {
    buffer->count = 0;
}
```

## Handle Table

```cpp
struct Handle {
    uint32_t index;
    uint32_t generation;
};

using EntityId = Handle;

template <typename T>
struct Slot {
    T value;
    uint32_t generation;
    bool occupied;
};

struct Pool {
    Slot<Entity> slots[MAX_ENTITIES];
    uint32_t free_indices[MAX_ENTITIES];
    uint32_t free_count;
};

Handle allocate_entity(Pool* pool);
void release_entity(Pool* pool, Handle handle);
Entity* get_entity(Pool* pool, Handle handle);
```

## Resource Store

```cpp
using TextureHandle = Handle;
using ShaderHandle = Handle;
using BufferHandle = Handle;
using SoundHandle = Handle;
using FontHandle = Handle;

struct ResourceStore {
    Slot<Texture> textures[MAX_RESOURCES];
    Slot<Shader> shaders[MAX_RESOURCES];
    Slot<Sound> sounds[MAX_RESOURCES];
};

TextureHandle load_texture(ResourceStore* store, Env* env, const char* path);
Texture* get_texture(ResourceStore* store, TextureHandle handle);
void destroy_resources(ResourceStore* store, Env* env);
```

## Render Queue

```cpp
struct RenderCommand {
    enum Type { Rect, Image, Text, Mesh, DebugLine } type;
    uint32_t layer;
    TextureHandle texture;
    float x;
    float y;
    float w;
    float h;
};

struct RenderQueue {
    RenderCommand items[MAX_RENDER_COMMANDS];
    uint32_t count;
};

struct Renderer {
    SDL_GPUGraphicsPipeline* pipelines[32];
    RenderQueue queue;
};

void sort_render_queue(RenderQueue* queue) {
    std::stable_sort(queue->items, queue->items + queue->count, [](auto& a, auto& b) {
        return a.layer < b.layer;
    });
}
```

## Audio System

```cpp
struct AudioCommand {
    enum Type { Play, Stop, SetVolume, PushSamples } type;
    SoundHandle sound;
    float volume;
};

struct AudioSystem {
    SDL_AudioStream* stream;
    AudioCommand commands[256];
    uint32_t command_count;
};
```

## Scheduler

```cpp
enum Phase {
    Phase_Input,
    Phase_Commands,
    Phase_Simulation,
    Phase_Audio,
    Phase_RenderPrep,
    Phase_Render,
    Phase_Cleanup
};

struct SystemTask {
    Phase phase;
    void (*run)(App* app, float dt);
    bool enabled;
};

void run_systems(Scheduler* scheduler, Phase phase, App* app, float dt) {
    for (SystemTask& task : scheduler->tasks) {
        if (task.enabled && task.phase == phase) {
            task.run(app, dt);
        }
    }
}
```

## Timer Scheduling

```cpp
struct TimerEvent {
    double fire_at;
    double repeat_interval;
    Command command;
};

struct TimerLess {
    bool operator()(const TimerEvent& a, const TimerEvent& b) const {
        return a.fire_at > b.fire_at;
    }
};

std::priority_queue<TimerEvent, std::vector<TimerEvent>, TimerLess> timers;

void pump_timers(double now, CommandBuffer* commands) {
    while (!timers.empty() && timers.top().fire_at <= now) {
        TimerEvent timer = timers.top();
        timers.pop();
        push_command(commands, timer.command);

        if (timer.repeat_interval > 0.0) {
            timer.fire_at += timer.repeat_interval;
            timers.push(timer);
        }
    }
}
```

## Dirty Flags

```cpp
struct DirtyValue {
    Mat4 local;
    Mat4 world;
    bool world_dirty;
};

void mark_dirty(DirtyValue* value) {
    value->world_dirty = true;
}

Mat4 get_world(DirtyValue* value) {
    if (value->world_dirty) {
        value->world = recompute_world(value->local);
        value->world_dirty = false;
    }
    return value->world;
}
```

## Double Buffering

```cpp
template <typename T>
struct DoubleBuffer {
    T buffers[2];
    uint32_t read_index;
    uint32_t write_index;
};

template <typename T>
void swap_buffers(DoubleBuffer<T>* buffer) {
    std::swap(buffer->read_index, buffer->write_index);
}
```

## State Machine

```cpp
enum AppMode {
    Mode_Running,
    Mode_Paused,
    Mode_Menu,
    Mode_Shutdown
};

void transition(AppMode* mode, AppMode next) {
    *mode = next;
}
```

## Dependency Graph

```cpp
struct DependencyGraph {
    std::vector<SystemTask> tasks;
    std::vector<std::pair<uint32_t, uint32_t>> edges;
};

std::vector<uint32_t> topological_sort(const DependencyGraph& graph);
```

## Spatial Grid

```cpp
struct SpatialGrid {
    float cell_size;
    std::vector<EntityId> cells[GRID_CELL_COUNT];
};

uint32_t cell_index(float x, float y);
void insert(SpatialGrid* grid, EntityId id, float x, float y);
void query_radius(SpatialGrid* grid, float x, float y, float radius, std::vector<EntityId>* out);
```

## Serialization Boundary

```cpp
struct SaveHeader {
    uint32_t magic;
    uint32_t version;
    uint64_t world_revision;
};

bool save_world(const World* world, const char* path);
bool load_world(World* world, const char* path);
```

---

# Bibliography And Research Links

## SDL

- SDL Wiki
  https://wiki.libsdl.org/

- SDL3 API
  https://wiki.libsdl.org/SDL3/FrontPage

- SDL3 examples
  https://examples.libsdl.org/

- SDL3 GPU API
  https://wiki.libsdl.org/SDL3/CategoryGPU

- SDL3 audio API
  https://wiki.libsdl.org/SDL3/CategoryAudio

- SDL3 main callbacks / application lifecycle
  https://wiki.libsdl.org/SDL3/README-main-functions

- SDL3 migration guide
  https://wiki.libsdl.org/SDL3/README-migration

- SDL CMake documentation
  https://github.com/libsdl-org/SDL/blob/main/docs/README-cmake.md

## SDL Companion Libraries

- SDL_image documentation
  https://wiki.libsdl.org/SDL3_image

- SDL_image GitHub
  https://github.com/libsdl-org/SDL_image

- SDL_net documentation
  https://wiki.libsdl.org/SDL3_net

- SDL_net GitHub
  https://github.com/libsdl-org/SDL_net

- SDL_shadercross GitHub
  https://github.com/libsdl-org/SDL_shadercross

## SDL_GPU Examples And Tutorials

- TheSpydog SDL_gpu_examples
  https://github.com/TheSpydog/SDL_gpu_examples

- GPU For Beginners: SDL_GPU
  https://gpuforbeginners.com/chapter01/

- Muddling through SDL GPU
  https://www.jonathanfischer.net/gpu-by-example-part1/

## AI-Generated Navigation Aids

Use these for orientation, then verify details against official docs or source.

- DeepWiki: libsdl-org/SDL
  https://deepwiki.com/libsdl-org/SDL

- DeepWiki: libsdl-org/SDL_shadercross
  https://deepwiki.com/libsdl-org/SDL_shadercross

- Context7: SDL_shadercross
  https://context7.com/libsdl-org/sdl_shadercross

- Context7: SDL_gpu_examples
  https://context7.com/thespydog/sdl_gpu_examples

## CMake And Dependency Management

- CMake getting started
  https://cmake.org/getting-started/

- Modern CMake
  https://cliutils.gitlab.io/modern-cmake/

- CPM.cmake
  https://github.com/cpm-cmake/CPM.cmake

## Memory And Allocation

- mimalloc GitHub
  https://github.com/microsoft/mimalloc

- mimalloc documentation
  https://microsoft.github.io/mimalloc/

## Math

- cglm documentation
  https://cglm.readthedocs.io/

- cglm GitHub
  https://github.com/recp/cglm

- 3Blue1Brown: Essence of Linear Algebra
  https://www.youtube.com/playlist?list=PLZHQObOWTQDPD3MizzM2xVFitgF8hE_ab

- Immersive Linear Algebra
  https://immersivemath.com/

- Scratchapixel
  https://www.scratchapixel.com/

- Scratchapixel: LookAt camera
  https://www.scratchapixel.com/lessons/mathematics-physics-for-computer-graphics/lookat-function/framing-lookat-function.html

## Program Structure And Game/System Design

- Game Programming Patterns
  https://gameprogrammingpatterns.com/

- Game Loop
  https://gameprogrammingpatterns.com/game-loop.html

- Event Queue
  https://gameprogrammingpatterns.com/event-queue.html

- Command Pattern
  https://gameprogrammingpatterns.com/command.html

- State Pattern
  https://gameprogrammingpatterns.com/state.html

- Component Pattern
  https://gameprogrammingpatterns.com/component.html

## Debugging And Profiling

- RenderDoc
  https://renderdoc.org/

- RenderDoc GitHub
  https://github.com/baldurk/renderdoc

- Tracy Profiler
  https://github.com/wolfpld/tracy
