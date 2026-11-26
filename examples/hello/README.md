# hello

The smallest program of [dgx-ui](https://github.com/jef-sure/esp32-component-dgx-ui), and a project to start your own
from. A text that never changes, and in a frame a count that another task sends once a second. The task that owns the
display sleeps on its queue; when a count comes, its field is drawn again and nothing else is.

The project takes dgx-ui from the ESP Component Registry, `jef-sure/dgx-ui: "^0.1.4"` in `main/idf_component.yml`,
and [DGX](https://github.com/jef-sure/esp32-component-dgx), the display library, comes with it from the registry.
Inside the repository of dgx-ui, where it lies as `examples/hello`, it builds with the checkout around it and asks the
registry for DGX only.
Where dgx-ui is to be had, [Quick start](https://github.com/jef-sure/esp32-component-dgx-ui#quick-start) of its README
tells. The fonts are two of those DGX ships, so the build needs nothing beside ESP-IDF, 5.2 or later.

## On a CYD

The display named in `main/main.c` is that of a CYD (ESP32-2432S028R): an ILI9341 on SPI, standing, 240 by 320.

```sh
idf.py set-target esp32
idf.py build flash monitor
```

What you should see: `Hello, dgx-ui` at the top, and under it a frame with a number that grows by one every second.

## In a window on the computer

Without a board the same program runs in a window of DGX of the same size, built by ESP-IDF for its `linux` target.
It needs a C compiler of the computer's own and SDL2: on Debian and Ubuntu,
`sudo apt install build-essential libsdl2-dev`.

```sh
idf.py --preview set-target linux
idf.py build
./build/hello.elf
```

[The window of DGX](https://github.com/jef-sure/esp32-component-dgx/blob/main/docs/host-window-en.md) tells more of
it: the keys and the mouse, a run with nobody at the computer, a picture of the window kept in a file.

## Another display

Two places name the display:

- `display_init()` in `main/main.c`: the bus with its pins, and the driver of the panel;
- `sdkconfig.defaults`: the bus and the driver DGX builds, its options `CONFIG_DGX_ENABLE_...`. A `sdkconfig` made
  before the change keeps the old ones: delete it, and the next build makes it again.

The README of DGX lists the displays and the buses it drives, under
[What's in the box](https://github.com/jef-sure/esp32-component-dgx#whats-in-the-box), and its
[Getting started](https://github.com/jef-sure/esp32-component-dgx#getting-started) tells how they are turned on.
Nothing else in the program knows the size of the display: the tree lays itself out in the screen it is given.

## What is where

| File | What it is |
| ---- | ---------- |
| `main/main.c` | the program: the display, the message, the texts, the widgets, the task that counts, the loop |
| `main/idf_component.yml` | the components the program asks for: dgx-ui |
| `sdkconfig.defaults` | a tick of 1 ms, and the bus and the driver of the display |
| `sdkconfig.defaults.linux` | the stack the window needs |
| `CMakeLists.txt` | the project; its first block does something only inside the repository of dgx-ui, where the component is the checkout around the example |

## What next

- [Letters with soft edges](https://github.com/jef-sure/esp32-component-dgx-ui/blob/0.1.4/docs/en/cookbook.md#letters-with-soft-edges):
  fonts DGX makes as it builds the program, of the letters the program writes.
- [Getting started](https://github.com/jef-sure/esp32-component-dgx-ui/blob/0.1.4/docs/en/getting-started.md): a
  program explained piece by piece, a round CO2 scale fed by a sensor.
- [How it works](https://github.com/jef-sure/esp32-component-dgx-ui/blob/0.1.4/docs/en/how-it-works.md): the tree,
  the messages, the painter and the looks.
- [Cookbook](https://github.com/jef-sure/esp32-component-dgx-ui/blob/0.1.4/docs/en/cookbook.md): buttons, sliders,
  lists, popups, keys and touch.
- [API reference](https://github.com/jef-sure/esp32-component-dgx-ui/blob/0.1.4/docs/en/api.md).
