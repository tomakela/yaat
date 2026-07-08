# yaat

YAAT is an early Windows 95-compatible point-and-click adventure engine experiment.

## Demo intro room scaffold

A first demo room scaffold lives under `assets/rooms/intro` with its matching script at `assets/scripts/room_intro.yaat`.
The scaffold intentionally allows missing graphics and sounds: a missing background, object sprite, hotspot sprite, or sound should be treated as a visible placeholder/warning instead of a hard failure.

For a quick browser preview of the data-driven placeholder behavior, serve the repository root and open `demo/intro_room.html`:

```sh
python3 -m http.server 8000
# then open http://127.0.0.1:8000/demo/intro_room.html
```

The browser preview is not the final Win95 runtime. It is a lightweight authoring/demo harness that shows the intended room composition rules before the C renderer and asset loader exist.
