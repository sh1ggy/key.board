# IDF Build version

## TODO
- [x] Add rfid reader lib
- [x] Read preferences value from nvs 
- [x] setup HID and CDC interfaces for printing to serial as well
    - Track this issue https://github.com/espressif/esp-idf/issues/13240
- [x] Render cards 
- [x] Listen for RFID
- [ ] add global error event listener
- [ ] Manual program button entry ( us gpio 0)
- [ ] mark etchings on the enclosure
    - Scan here


### Future
- [x] Reach feature parity with arduino version
- [ ] Fix build-workflow to stop erroring

### Not doing
- [ ] Send and recieve HID data from rfid reader
    - https://github.com/hathach/tinyusb/blob/master/examples/device/hid_composite/src/main.c#L111
    - Use this to setup secondary HID control, this is exclusively for sending back bytes of cards (send_hid_report) (this is called seting up a new interface)
    - Use tud_hid_report(REPORT_ID_CONSUMER_CONTROL, &volume_down, 2); as example
    - Use tud_hid_set_report_cb to setup callback for recieving data (i.e for switching modes)

## Learns
- Run `esptool.py erase_flash` to erase the flash before flashing with new boot[source](https://docs.espressif.com/projects/esptool/en/latest/esp32s3/esptool/basic-commands.html)
- Run `idf.py partition-table` to get the partition table deets
### Timers
- The [general purpose timer](https://docs.espressif.com/projects/esp-idf/en/v4.3/esp32/api-reference/peripherals/timer.html#general-purpose-timer) is just an abstraction layer over a hardware ISR timer, 
similar to the teensy. It has a divider, a hardware counter frequency, a max count and an alarm (ISR callback);
- The [high resolution timer](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/system/esp_timer.html) (misappropriately named, this is more of a general purpose, higher level timer) 
is an abstraction over the general purpose timer. It can use software tasks and ISRS and other great functionality.
- https://www.eevblog.com/forum/microcontrollers/esp32-timers/msg3955919/#msg3955919

## Launch instructions

```bash
idf.py -p PORT flash monitor
```

(Replace PORT with the name of the serial port to use.)

(To exit the serial monitor, type ``Ctrl-]``.)

See the Getting Started Guide for full steps to configure and use ESP-IDF to build projects.


## Example Output

After the flashing you should see the output at idf monitor:

```
I (290) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (310) example: USB initialization
I (310) tusb_desc:
┌─────────────────────────────────┐
│  USB Device Descriptor Summary  │
├───────────────────┬─────────────┤
│bDeviceClass       │ 0           │
├───────────────────┼─────────────┤
│bDeviceSubClass    │ 0           │
├───────────────────┼─────────────┤
│bDeviceProtocol    │ 0           │
├───────────────────┼─────────────┤
│bMaxPacketSize0    │ 64          │
├───────────────────┼─────────────┤
│idVendor           │ 0x303a      │
├───────────────────┼─────────────┤
│idProduct          │ 0x4004      │
├───────────────────┼─────────────┤
│bcdDevice          │ 0x100       │
├───────────────────┼─────────────┤
│iManufacturer      │ 0x1         │
├───────────────────┼─────────────┤
│iProduct           │ 0x2         │
├───────────────────┼─────────────┤
│iSerialNumber      │ 0x3         │
├───────────────────┼─────────────┤
│bNumConfigurations │ 0x1         │
└───────────────────┴─────────────┘
I (480) TinyUSB: TinyUSB Driver installed
I (480) example: USB initialization DONE
I (2490) example: Sending Keyboard report
I (3040) example: Sending Mouse report
```
