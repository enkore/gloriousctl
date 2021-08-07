gloriousctl
===========

A utility to adjust the settings of Model O/D mice on Linux/BSD, but it probably
works for other Sinowealth-made mice as well.

    Usage:
     gloriousctl --help
            Show this help text.
     gloriousctl --info
            Show the current configuration of the mouse.
     gloriousctl --listen
            Listen for and show DPI profile changes.
     gloriousctl [--set-...]
            Change persistent mouse settings.

    Available settings:
     --set-debounce-time 4-16
            Change click debounce time in milliseconds. Only use even numbers.
     --set-dpi DPI1,...
            Up to six DPIs can be configured.
     --set-dpi-color RRGGBB,...
            For each DPI the RGB color can be set.
     --set-effect effect-name
            Available RGB effects: off, glorious, breathing, wave, tail,
            single, breathing7, breathing1, rave
            single and breathing1 use one color, breathing7 seven, rave two.
     --set-colors RRGGBB,...
            Set the color(s) of the effect. Only effective with --set-effect.
     --set-brightness 0-4
            Set the brightness of the effect. Only effective with --set-effect.
     --set-speed 0-3
            Set the speed of the effect. Only effective with --set-effect.

    Supported mice:
     - Glorious Model D (VID 258a PID 0033)
     - Glorious Model O/O- (VID 258a PID 0036) (untested)

Build requirements
------------------

C compiler and libhidapi-hidraw. The libusb-based HIDAPI backend
should technically work, but it requires exclusive control over the
USB device, which is impractical for a mouse.

Caveats
-------

Not tested with Model O but probably just works. Possibly works with
minor alterations with the wealth of sinowealth mice, since they all
seem to use the exact same Windows utility to configure them (except a
config file telling it which VID/PID to look for and what options
exist).

Since this is not the official OEM/ODM (sinowealth would be an
interesting case study...) software, and this appears to modify the
EEPROM/flash of the controller in your mouse, there is a chance that
using this in some way could brick your mouse.
