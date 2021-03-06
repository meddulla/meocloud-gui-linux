import os
from gi.repository import GObject, Gtk, GLib, Gdk


class TrayIcon (GObject.Object):
    __gtype_name__ = 'TrayIcon'

    def __init__(self, app):
        self.app = app
        self.syncing = 0
        self.timeout = None
        self.last_icon = None
        self.icon_name = "meocloud-init"
        self.icon_theme = Gtk.IconTheme()

        self.icon = Gtk.StatusIcon()
        self.icon.connect("activate", self.tray_popup)
        self.icon.connect("popup-menu", self.tray_popup)

        self.menu = Gtk.Menu()

    def wrapper_run(self, func):
        Gdk.threads_enter()
        try:
            func()
        finally:
            Gdk.threads_leave()

    def wrapper(self, func):
        GLib.idle_add(self.wrapper_run, func)

    def set_icon(self, name):
        self.last_icon = name

        if self.app.icon_type != "":
            name += "-" + self.app.icon_type
            use_icon_name = False
        else:
            icon_info = self.icon_theme.lookup_icon(name, 32, 0)
            if icon_info is not None:
                use_icon_name = True
            else:
                use_icon_name = False

        self.icon_name = name

        if self.syncing > 0 and "sync" not in name:
            self.syncing = 0
        elif self.syncing < 1 and "sync" in name:
            self.syncing = 2
            if self.timeout is not None:
                GLib.source_remove(self.timeout)
            self.timeout = GLib.timeout_add(500, self.cycle_sync_icon)

        if use_icon_name:
            GLib.idle_add(self.icon.set_from_icon_name, name)
        else:
            icon_file = os.path.join(
                self.app.app_path, "icons/" + name + ".svg")
            GLib.idle_add(self.icon.set_from_file, icon_file)

    def cycle_sync_icon(self):
        if self.syncing < 1:
            GLib.source_remove(self.timeout)
            self.timeout = None
            return False

        icon_name = "meocloud-sync-" + str(self.syncing)

        self.set_icon(icon_name)

        if self.syncing < 4:
            self.syncing += 1
        else:
            self.syncing = 1

        return True

    def show(self):
        self.icon.set_visible(True)
        self.menu.show_all()

    def tray_quit(self, widget, data=None):
        self.app.release()
        self.app.quit()

    def tray_popup(self, widget, button=None, time=None, data=None):
        if time is None:
            time = Gtk.get_current_event().get_time()

        self.menu.show_all()
        self.menu.popup(None, None, lambda w, x:
                        self.icon.position_menu(self.menu, self.icon),
                        self.icon, 3, time)

    def add_menu_item(self, menuitem, hide=False):
        self.menu.append(menuitem)

        if hide:
            menuitem.set_no_show_all(True)
            menuitem.hide()

    def hide(self):
        self.icon.set_visible(False)
