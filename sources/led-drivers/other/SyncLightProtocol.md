# SyncLight HID protocol notes

The details were reconstructed from the official SyncLight traffic captures and
validated against a 27 inch controller.

## USB HID devices

Known VID/PID pairs:

- `0x1a86:0xfe07`
- `0x1a86:0xfe0c`

The driver sends Windows HID output reports. Each report is 65 bytes:

- byte `0`: HID report id, currently `0x00`
- bytes `1..64`: protocol payload chunk

Frames longer than 64 protocol bytes are split into sequential HID reports.
Continuation reports do not add protocol headers; they carry the next 64 bytes
of the same frame.

## Checksum

Both `RB` and `SC` frames use an 8-bit additive checksum:

```text
checksum = sum(frame bytes before checksum) & 0xff
```

The checksum is stored in the last protocol byte of the frame.

## RB service frames

`RB` frames are used for controller service commands and global color output.

Frame layout:

```text
offset  size  description
0       2     ASCII "RB"
2       1     frame length, including checksum
3       1     frame id
4       1     action
5       n     payload
last    1     checksum
```

Known actions:

```text
0x86  global/section color
0x87  brightness
0x97  keepalive
```

Brightness payload:

```text
offset  size  description
0       1     brightness, 0..255
```

Global color payload:

```text
offset  size  description
0       1     section, `0x01` for global output
1       1     red byte
2       1     green byte
3       1     blue byte
4       1     `0x47`
5       1     `0x48`
6       1     `0x00`
7       1     `0x00`
8       1     `0x00`
9       1     `0xfe`
```

The working global color sequence is:

1. send `RB` keepalive (`0x97`)
2. wait about 20 ms
3. send `RB` color (`0x86`)

## SC per-led frames

`SC` frames are used for per-led output.

Frame layout:

```text
offset  size  description
0       2     ASCII "SC"
2       2     frame length, big endian, including checksum
4       1     frame id
5       n*5   color records
...     1     max controller address
last    1     checksum
```

Each color record is five bytes:

```text
offset  size  description
0       1     first controller position
1       1     second controller position
2       1     red byte
3       1     green byte
4       1     blue byte
```

The first record sets bit `0x80` on the first position byte.
The controller interprets the two position bytes as two physical LED positions,
not as a continuous range. For example:

```text
record 0: 0x80, 0x01, R, G, B
record 1: 0x02, 0x03, R, G, B
record 2: 0x04, 0x05, R, G, B
```

`Controller LEDs` in the UI is the physical LED count. The driver derives:

```text
maxAddress = controllerLedCount
recordCount = (controllerLedCount + 3) / 2
frameLength = 5 + recordCount * 5 + 1 + 1
```

For a 65 LED controller:

```text
maxAddress = 65
recordCount = 34
frameLength = 177 bytes (`0x00b1`)
```


Validated layout for the 27 inch 65 LED U-shaped strip:

```text
LED Layout:
Top: 31
Bottom: 0
Left: 17
Right: 17
Input position: 48
Reverse direction: enabled

LED Controller:
Output mode: Per-LED
Controller LEDs: 65
```

This matches the physical controller order:

```text
right side: bottom -> top
top side: right -> left
left side: top -> bottom
```

## Color order

The driver writes the color bytes it receives from HyperHDR. RGB byte order is
already applied by HyperHDR's color processing pipeline before
`LedDevice::write()` reaches this driver. Applying RGB order again inside the
SyncLight frame builder would double-swap channels.

## Shutdown

The driver sends a black frame from:

- `powerOff()`
- `close()`
- `~DriverOtherSyncLight()`

The destructor path is required because quitting HyperHDR from the tray may
destroy the driver without going through the normal instance-disable path.
