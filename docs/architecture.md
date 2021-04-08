# Architecture

## Nested Compositor

The FDO backend exposes a minimal Wayland compositor to the `WPEWebProcess`
instances which take care of the actual rendering of web content. The Wayland
protocol is thus used between the “UI process” (your application) and the
processes that take care of rendering—Wayland is, after all, an IPC protocol
designed to pass graphics buffers around.

* WPEBackend-fdo takes care of creating a Wayland compositor, and will
  act as the “server” side. The main entry point for the server side is
  in the `src/ws.cpp` file.

* Each `WPEWebProcess` spawned by WPE WebKit acts as a Wayland client,
  see `src/ws-client.cpp` for its implementation. The connection process
  roughly works as follows:

  1. In the UI process side the backend uses [socketpair(2)][man-socketpair]
     to create an unique socket, which is left open so the `WPEWebProcess`
     can inherit it.
  2. The socket file descriptor number for the client side of the connection
     is passed to the `WPEWebProcess` on its command line to let it know which
     file descriptor to use.
  3. The part of the backend that runs in the `WPEWebProcess` uses received
     file descriptor number as its connection to the nested compositor.

* For each `WebKitWebView` to be rendered by a given `WPEWebProcess`, the
  client part of backend creates a new Wayland surface. Each time a new
  frame with rendered web content is ready, a new buffer is attached to
  the surface and committed—which effectively “passes it” to the UI process.

Note that the UI Process does *not* need itself to use the Wayland protocol
to display the rendered content on some output device. For example the
[Cog][cog] `drm` platform module takes the graphics buffers “exported” by
WPEBackend-fdo and uses the DRM/KMS devices exposed by the kernel to drive
video outputs itself without relying on a running compositor.

[cog]: https://github.com/Igalia/cog
[man-socketpair]: https://man7.org/linux/man-pages/man2/socketpair.2.html
