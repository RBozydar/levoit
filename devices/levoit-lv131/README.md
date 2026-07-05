[← Back](../../README.md)

# Levoit LV-PUR131 - Custom Firmware (ESPHome)

The LV-PUR131 model is very similar to the [LV-131S](../levoit-lv131s/README.md) (later called the S board / model) except it doesn't havethe wireless capabilities.
They look identical and have mostly the same components.

**Hardware Upgrades:**
- MCU: Original On-Bright OB39R08A3U20SP -> ESP32-C3 Super Mini
- Sensor upgrade to PM5003 is theoretically possible and should be implemented the same way it is on the S board, but I don't have the money to buy one right now. Please make a PR if you tested whether it works or not.

## Features
- WiFi on a machine that never was supposed to have it!!
- Everything available on the S model.

## Why this works?
Because we replace the old UC with one that can do wireless and levoit reused the same components between this and the wireless model. 

## Interesting findings
The main differences between the models are the ones related to the microcontroller and the voltage it uses. The S board uses an ESP32 and runs on 3.3V whereas this model runs on a microcontroller that works fully on 5V - even the display is the same, thanks to which this mod is able to use the wifi icon which normally is not (I think?) used by the machine at all. The routing also changes a bit between the boards but not really by much.

## Hardware Upgrade Project ("The Hack")
### Required Parts
- Broken or working Levoit LV-PUR131
- Any cheap ESP32 board really. A cheap chinese C3 Super Mini works fine.
- Wires
- 3x 10k ohm resistor
- 2x 4.7k ohm resistor
- BAT43 diode

### Required Tools
- Something to cut a trace with
- Soldering iron

### Disassembly
Same as the S model.

### Hack / Modify PCB
We need to force the board to behave like the S model so the S mod works here as well. To acomplish this we need to add two voltage dividers, a resistor, a diode, cut a trace, and desolder the old UC.

This is what the board looks like:
![Board](./images/board.gif)

This is what the mod schematics look like. Only the parts interesting us are laid down, I did not copy the whole board. The mod parts are marked in red:
![Schematics](./images/schematics.png)

This is what example solder points look like:
![Solder points](./images/solder_points.jpg)

This is what the finished board looks like. Sorry for the flux, I ran out of IPA: 
![Mod](./images/mod.png)

#### Important?
- I didn't have the schottky diode on hand so I used a 1N4148 diode instead. Because of it's forward voltage I had to add another 10k resistor on the Q101's base. This part is marked in purple and is in the bottom right corner of the solder_points.jpg. The schematic does not include this part, and so shouldn't you.
- The original UC is stuck in place with some kind of glue. You might have to apply a bit of force to get it to move.

### How, why, etc.
The board is designed to work on 5V only. Our ESP32 runs on 3.3V and if we feed it's GPIO 5V it's gonna blow up. Every time there is a risk of 5V going into out new controller we need to somehow convert it to ~3.3V. This is why the voltage dividers are needed, and this is also why we need to cut the trace pulling up the TM1628's lines to 5V and replace it with 3.3V as well.

## Notes

- There still is one GPIO left free on the board. Somebody, please put an LED strip in the vent and share what it looks like.
