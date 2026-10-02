# Asynchronous content

`AsyncResource<T>` owns request generation, cooperative cancellation and a durable
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
While pending, the view polls the durable slot every 16 ms through the UI scheduler;
settled views stop polling. An optional worker-safe wake callback can notify the
host, but never owns the result. Disposal disconnects observation without
cancelling a resource shared by other views.
