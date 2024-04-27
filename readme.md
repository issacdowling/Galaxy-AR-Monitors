The only current dependencies should be libpipewire, opencv and raylib, though I want to statically compile things, and package as a Flatpak and Appimage to solve any portability issues.

Clone this repo, then go into src/arenv, make and cd into build dir, and compile.

```
git clone https://gitlab.com/issacdowling/galaxy.git
cd galaxy/src/arenv/

mkdir build
cd build

cmake..
make
```

If you see errors, but a binary has been made, all is fine, the same happens for me.

Fixes include issues
```
sudo ln -s /usr/include/pipewire-0.3/pipewire /usr/local/include/pipewire
sudo ln -s /usr/include/spa-0.2/spa /usr/local/include/spa
```

If you're in a Distrobox, you might get a Dbus error, and need to run `sudo dbus-daemon --system` to make things work (this still applies even if the xdg portal is running on the host)