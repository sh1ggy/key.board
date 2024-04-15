# IDF Build version

## TODO
- [x] Add rfid reader lib
- [x] Read preferences value from nvs 
- [x] setup HID and CDC interfaces for printing to serial as well
    - Track this issue https://github.com/espressif/esp-idf/issues/13240
- [ ] Send and recieve HID data from rfid reader
    - https://github.com/hathach/tinyusb/blob/master/examples/device/hid_composite/src/main.c#L111
    - Use this to setup secondary HID control, this is exclusively for sending back bytes of cards (send_hid_report) (this is called seting up a new interface)
    - Use tud_hid_report(REPORT_ID_CONSUMER_CONTROL, &volume_down, 2); as example
    - Use tud_hid_set_report_cb to setup callback for recieving data (i.e for switching modes)

### Future
- [ ] Reach feature parity with arduino version
- [ ] Fix build-workflow to stop erroring

## Tips
- Run `idf.py partition-table` to get the partition table deets

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
