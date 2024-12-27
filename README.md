Xscreensaver-run
================

This tiny program may be used to run screensavers from
[XScreenSaver](https://www.jwz.org/xscreensaver/) collection synchronously,
in fullscreen and without additional effects like screen locks or timers.

This is a fork of [xscreensaver-run](https://github.com/sergei-mironov/xscreensaver-run) which allow you to provide a list of screensavers to run in a file. This is nice because there is no flashing when transitioning between multiple screensavers.

See also <https://unix.stackexchange.com/questions/290698/how-to-manually-start-an-xscreensaver-application-in-root-window-full-screen>


Build
-----

Just `make`

    make

Install with

    make install


NixOS users may use the following commands to install the application in the
current user's profile:

    nix-build
    nix-env -i ./result

Usage
-----

Generate a list of screensavers to run. Example:

    find /usr/lib/xscreensaver/ -type f | grep -v xscreensaver- | shuf > xscreensaver-list

Your distro might install screensaver files to a different location. You can edit the file and provide arguments to each screensaver if need. 

Run like this:

    ./xscreensaver-run ./xscreensaver-list

Press ESC to go to the next screensaver. Many screensavers accept mouse and keyboard input. ENTER typically resets the current animation.

Usage (gamescope)
-----

[sway](https://github.com/swaywm/sway) forces X11 windows to be scaled. You can use [gamescope](https://github.com/ValveSoftware/gamescope) ensure that a fullscreen X11 window is not scaled.

    ./gamescope-xscreensaver


Development notes
-----------------

### Environment

To test the application in Nix:

1. `$ nix-shell`
2. `(shell) $ nix-build '<nixpkgs>' -A pkgs.xscreensaver`
3. `(shell) $ make`
4. `(shell) $ ./xscreensaver-run ./result/libexec/xscreensaver/apple2 -text`

### Hints

* To reset root window's cursor (just in case): `xsetroot -cursor_name left_ptr`

### References

* [X Event definitions](https://fossies.org/dox/tightvnc-1.3.10_unixsrc/X_8h.html)
* [X Event documentation](https://tronche.com/gui/x/xlib/events/processing-overview.html)
* https://linux.die.net/man/3/xwindowevent
* https://linux.die.net/man/3/xgrabkeyboard

Bugs?
-----

Yes! Check FIXMEs in the source code. Feel free to fix them and file PRs!

