# Asynchronous content

`AsyncResource<T>` owns request generation, cooperative cancellation and a retained
result slot. Workers produce data through the bounded `runtime::Executor`; they
never access UI nodes, resource registries or renderers. Owner-thread publication
rejects superseded results. Executor refusal is an observable error. A stopped task ticket makes queued
work discarded by executor shutdown observable as Cancelled, even if its loader
never ran. Completion
queue saturation cannot discard the only result copy.

`AsyncSnapshot<T>` exposes Pending, Ready, Error or Cancelled and an optional
previous successful value. `AsyncView<T>` creates pending/ready/error content on
the owner thread. Refresh retains previous successful content by default. Initial
pending fallback waits 150 ms; success is never delayed to force a spinner to show.
Cancellation and errors remain observable while old content is retained.

View disposal cancels its observation; request lifetime belongs to the resource.
Retry creates a new generation. Optional `TransitionHost` reveal is independent
of data correctness. Animation preferences never prevent publication.

These boundaries model explicit loading state, not React concurrent rendering or
implicit suspension. Disk UI loading and an editor remain [planned](DOCUMENTS.md);
automatic resource discovery is not part of this API.

Call `AsyncView::refresh()` after starting/retrying a request on a settled view.
While pending, the view polls the retained slot every 16 ms through the UI scheduler;
settled views stop polling. An optional worker-safe wake callback can notify the
host, but never owns the result. Disposal disconnects observation without
cancelling a resource shared by other views.

## Worker admission

`Executor::submit` returns `TaskSubmission`: `admission` is `Accepted`, `Busy`,
`TooLarge`, or `Closed`; only acceptance carries a `TaskTicket`. Busy is temporary
capacity pressure. Oversized work and closed executors require a changed request
or executor, not automatic retry. Cancellation retains capacity until work retires.
Services own retry deadlines and result channels; executor admission is atomic and
does not require a preceding statistics check.

## Future file and asset browser

A file/asset browser is future optional UI composed from virtualized lists/grids,
selection and bounded asynchronous providers. `BrowserQuery` contains an explicit
content root, directory/item identity, filter/sort policy, query generation and
page limit. `BrowserPage` returns stable item IDs, metadata revisions and an opaque
continuation cursor. Cursor validity belongs to the provider; changed directory
contents may invalidate the enumeration and request a restart.

Selection binds item identity, not visible row index. A file is not automatically
an AssetDefinition: import, catalog lookup and opening are separate application
commands. File access goes through the configured storage/root provider; path
normalization, symlink and root-escape policy remain there rather than in widgets.
A root handle is an access boundary, not a claim that trusted native code is sandboxed.

Workers stage owned pages and thumbnail results. Owner-thread publication validates
view lifetime, query generation, item revision and requested size. Refresh, sort,
root change and detach cancel old demand without accepting late results. Bound
page count, metadata bytes, thumbnail decode work and the weighted thumbnail cache;
visible resource leases survive eviction until their frame retires. Errors remain
separate from an empty directory. A slow root cannot block UI event processing.

[Qt's filesystem model](https://doc.qt.io/qt-6/qfilesystemmodel.html) provides a
reference for asynchronous population and model/view separation. File watching,
remote browse protocols and OS-native file dialogs are separate extensions. The
initial viewer can select configured datasets without implementing this browser.
