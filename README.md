gloriousctl
===========

A utility to adjust the settings of Model O/D mice, but it probably
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
     --set-dpi DPI1,...
            Up to six DPIs can be configured.
     --set-dpi-color RRGGBB,...
            For each DPI the RGB color can be set.
     --set-effect effect-name
            Available RGB effects: off, ...

    Supported mice:
     - Glorious Model D (VID 258a PID 0033)
     - Glorious Model O/O- (VID 258a PID 0036) (untested)
