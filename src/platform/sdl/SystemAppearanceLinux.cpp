#include <optional>

#include <SDL3/SDL_timer.h>
#include <dbus/dbus.h>

#include <platform/sdl/SystemAppearance.hpp>

namespace playground::sdl {
namespace {
struct PortalSettings {
  const char *key;
  DBusConnection *connection{};
  DBusPendingCall *pending{};
  std::optional<bool> high;
  Uint64 nextRead{};

  explicit PortalSettings(const char *setting) : key{setting} {
    DBusError error;
    dbus_error_init(&error);
    connection = dbus_bus_get_private(DBUS_BUS_SESSION, &error);
    dbus_error_free(&error);
    if (connection)
      dbus_connection_set_exit_on_disconnect(connection, false);
  }

  ~PortalSettings() {
    if (pending) {
      dbus_pending_call_cancel(pending);
      dbus_pending_call_unref(pending);
    }
    if (connection) {
      dbus_connection_close(connection);
      dbus_connection_unref(connection);
    }
  }

  void poll() {
    if (!connection)
      return;
    dbus_connection_read_write_dispatch(connection, 0);
    if (pending && dbus_pending_call_get_completed(pending)) {
      auto *reply = dbus_pending_call_steal_reply(pending);
      dbus_pending_call_unref(pending);
      pending = nullptr;
      high.reset();
      DBusMessageIter it;
      if (reply && dbus_message_iter_init(reply, &it)) {
        while (dbus_message_iter_get_arg_type(&it) == DBUS_TYPE_VARIANT) {
          DBusMessageIter inner;
          dbus_message_iter_recurse(&it, &inner);
          it = inner;
        }
        if (dbus_message_iter_get_arg_type(&it) == DBUS_TYPE_UINT32) {
          dbus_uint32_t v{};
          dbus_message_iter_get_basic(&it, &v);
          if (v <= 1)
            high = v == 1;
        }
      }
      if (reply)
        dbus_message_unref(reply);
    }
    const auto now = SDL_GetTicks();
    if (pending || now < nextRead)
      return;
    nextRead = now + 1000;
    auto *request = dbus_message_new_method_call(
        "org.freedesktop.portal.Desktop", "/org/freedesktop/portal/desktop",
        "org.freedesktop.portal.Settings", "Read");
    if (!request)
      return;
    const char *group = "org.freedesktop.appearance";
    if (dbus_message_append_args(request, DBUS_TYPE_STRING, &group,
                                 DBUS_TYPE_STRING, &key, DBUS_TYPE_INVALID))
      dbus_connection_send_with_reply(connection, request, &pending, 500);
    dbus_message_unref(request);
  }
};
} // namespace

void platformContrast(ui::SystemAppearance &value) {
  static PortalSettings settings{"contrast"};
  static PortalSettings motion{"reduced-motion"};
  motion.poll();
  value.reducedMotion = motion.high;
  settings.poll();
  value.highContrast = settings.high;
}
} // namespace playground::sdl
