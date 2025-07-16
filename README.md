# ZET/ARDOR GAMING Edge mouse configuration app

Linux configuration app for ZET/ARDOR GAMING Edge mouse.  
done by reverse engineering the HID protocol used by the official software.

## what can it do and what it can't:

### it can:  
- change LED backlight (mode, speed, brightness, colors)  
- change poll rate  
- turning on/off angle snap, ripple
- configuring debounce time  
- 7 DPI levels tuning and turning on/off  
- reset to factory settings  

### later:
- button remapping  
- macros  
- loading profiles from official app  

# udev rule setup

to run the tool without sudo, create a udev rule.

1. create a new file: `sudo nano /etc/udev/rules.d/99-edge.rules`
2. add this:
   ```
   SUBSYSTEM=="hidraw", ATTRS{idVendor}=="2ea8", ATTRS{idProduct}=="2203", MODE="0666"
   ```
3. reload the udev rules:
   ```bash
   sudo udevadm control --reload-rules && sudo udevadm trigger
   ```
4. you may need to replug the mouse for the changes to take effect.