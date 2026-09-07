# Location Demo

This demo combines the CC1161W UART GNSS receiver with reverse geocoding.

## Current flow

1. Boot the standalone location demo firmware.
2. The page waits for a valid CC1161W RMC fix and displays its coordinates.
3. Tap `Locate` to send an HTTPS GET to the reverse-geocode service.
4. The page parses `success` and `data.displayName` from the JSON response.
5. The page displays the server response.

Request example:

```text
https://ai.iwg.senthink.com/api/reverse-geocode?longitude=120.1551000&latitude=30.2741500
```

Coordinates use signed degrees multiplied by 10,000,000. This avoids floating-point
format differences between the GNSS driver, protocol, and server.

## Integration points

- `location_source_get()` reads the latest valid fix through
  `demo_gnss_get_location()`.
- `location_transport_request()` uses SIM 0, PDP CID 1 and HTTPS with certificate
  verification disabled for this demo.
- A real network transport must support cancellation or request-generation checks when
  the page exits while a request is in progress.
- Parse the server response into `description`; do not update LVGL objects from the
  network task. The 100 ms UI timer owns all LVGL updates.

Build this standalone demo with:

```text
./build.bat build PROJECT=L_CT4IT02_1698W BUILD_MODE=demo MODEM=NT26F9D0 MODEMPKG=F9D_A
```

The demo starts the GNSS receiver and location page as independent tasks from
`0demo_main.c`.
