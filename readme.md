Clone this repo with submodules
(add command)

Go into src/arenv, make build dir, cd in
cmake..
make
If you see errors, but a binary has been made, all is fine, the same happens for me.

Fixes include issues
sudo ln -s /usr/local/include/opencv4/opencv2 /usr/local/include/opencv2
sudo ln -s /usr/include/pipewire-0.3/pipewire /usr/local/include/pipewire
sudo ln -s /usr/include/spa-0.2/spa /usr/local/include/spa

If you're in a Distrobox, you might get a Dbus error, and need to run `sudo dbus-daemon --system` to make things work (this still applies even if the xdg portal is running on the host)