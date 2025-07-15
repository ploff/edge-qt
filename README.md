# ZET/ARDOR GAMING Edge mouse configuration app

Linux configuration app for ZET/ARDOR GAMING Edge mouse.

ensure udev rules or run with sudo.  
```SUBSYSTEM=="hidraw", ATTRS{idVendor}=="2ea8", ATTRS{idProduct}=="2203", MODE="0666"```

## what can it do and what it can't:

### it can:  
- change LED backlight (mode, speed, brightness)  
- change polling rate  
- turning on/off angle snap, ripple, debounce  
- DPI levels turning on/off and tuning  
- reset to factory settings  

### it can't:
- color changing on some LED modes

### later:
- button remapping  
- macros  
- loading profiles from official app  