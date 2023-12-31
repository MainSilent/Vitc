🔴 Project is no longer active

# PS Vita - Stream to PC via Wifi (Beta)
Resolution is currently 480x272, Low Quality is expected for beta version

## [Setup Tutorial](https://www.youtube.com/watch?v=TF5XHjNtOG8)

### Vita WiFi is Terrible

The issue is not the WiFi chip but rather the firmware, It has a very limited network buffer (64KiB)

For example I want to send a frame which is 960x544 in YUV format, The size will be:

    960 * 544 * 3/2 = 783360 bytes

If I send the data in a chunk of `8160` then:
 
    783360 / 8160 = 96 messages

And for each message I have to wait 1000us so that the buffer doesn't overflow

That means I'm only getting ~10 FPS:

    1s = 1000ms => 1000ms / 96ms = ~10 FPS

Make sure no other app or plugin is using your network

Overclocking is recommended for better FPS

## Todo

- Use LZ4 compression to achieve native resolution with 30 FPS
- Package the client (.exe, .dmg, .AppImage)
- Improve resolution up to 720p
- Android TV client
- Audio Support
- Config app for optional settings like turning display off
- Add VP9 format to get +30 FPS

## Credit

[xerpi/vita-udcd-uvc](https://github.com/xerpi/vita-udcd-uvc)
