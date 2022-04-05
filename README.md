# rotator
Basic rotation control tray icon

I made this pretty much for my own use only, so I didn't really put in a lot of failsafes in it.


## usage

At the moment, it's extremely basic. It creates a tray icon that you can click to toggle automatic rotation on and off. It utilizes the accelerometer to automatically rotate the screen, and also rotates the *input* devices described in `config::XDevs` (eg. touchscreen, touchpad)

## compiling

BEFORE COMPILING, make sure to modify `config::XDevs` for the names of the *input* devices you want to rotate! You can find these by using `xinput`

### dependencies

You need the following libraries, and their respective headers:

 - gio (>=2.0)
 - gtk+ (>=2.0)
 - x11
 - xrandr
 - xinput

Additionally, you'd need:

 - `pkg-config`
 - a c++ compiler

Then, you can compile it with the following command:

```
c++ rotate.cpp $(pkg-config --libs --cflags glib-2.0 gio-2.0 gtk+-2.0 xrandr xi) -mtune=native -march=native -Ofast -o rotate
```

It then will create a binary, `rotate`, in the current directory. Run that, and you'll get your rotation control!

---

***spin***
