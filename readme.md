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

Fixes include issues
```
sudo ln -s /usr/include/pipewire-0.3/pipewire /usr/local/include/pipewire
sudo ln -s /usr/include/spa-0.2/spa /usr/local/include/spa
sudo ln -s /usr/include/opencv4/opencv2/ /usr/local/include/opencv2
```

If you're in a Distrobox, you might get a Dbus error, and need to run `sudo dbus-daemon --system` to make things work (this still applies even if the xdg portal is running on the host)

# Tips

## My screencap is laggy
The Hyprland XDG Portal Screencap feature seems somewhat broken with high refresh rates right now. You can recompile it yourself, by _recursively_ cloning the (xdg-desktop-portal-hyprland)[https://github.com/hyprwm/xdg-desktop-portal-hyprland] repo, modifying line 807 (or thereabouts) in `src/portals/Screencopy.cpp` to be `PSTREAM->pSession->sharingData.framerate = 120;`, where 120 is your glasses display's refresh rate, and then compiling. 

## But how do I get a "headless" display to place into this AR environment?
To use a headless display with Hyprland, just run `hyprctl output create headless`, check the ID, then config it (you can find the ID of the display with `hyprctl monitors`) to any res/rr (1920x1080@120 is ideal). It should match your glasses.

## My colours are weird, like Red and Blue are swapped!
There will soon be an option that you can add to a config in order to set this correctly.

## Going fullscreen prevents rendering while I'm focused on another window!
If you want a fullscreen look, but without properly being fullscreen, and you're using a Window Manager, you may be able to just remove your borders and bar.

In waybar, you'll add this to the top of your config
```
"output": ["eDP-1", "HEADLESS-2"],
```
Where you'll want your physical displays and your headless display here, but NOT your glasses display, which will prevent Waybar from opening on them.