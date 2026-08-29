# Future Patches

## Priority 1: Direct LVGL Image Access from the SD Card

The next planned improvement is direct and more efficient access to images on
the SD card through LVGL paths such as `S:/Bild1.png`.

Implementing this requires extending the external component with:

- an LVGL file-system driver that maps the `S:` drive to the existing
  `/sdcard` mount point;
- a runtime PNG decoder compatible with LVGL;
- shared access synchronization between LVGL and the HTTP server so that image
  reads cannot conflict with uploads, replacements, deletions, or directory
  operations.

This will allow LVGL to read and decode images directly from the SD card,
without routing them through the local HTTP server and the ESPHome
`online_image` component.
