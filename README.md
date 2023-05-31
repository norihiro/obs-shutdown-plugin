# OBS Shutdown Plugin

<!-- TODO: Reconsider names
- OBS Shutdown Plugin
- OBS Termination Plugin
- OBS Quitting Plugin
- OBS Quit Plugin
-->

## Introduction

This is a simple plugin for OBS Studio to shutdown OBS Studio requested by websocket.

## API

- OBS websocket request name: `CallVendorRequest`
- Pass these data
  ```
  "vendorName": "obs-shutdown-plugin",
  "requestType": "shutdown",
  "requestData": request_object
  ```

The `request_object` should have these contents.

| Name | Type | Description |
| ---- | ---- | ----------- |
| `reason` | `string` | Reason why OBS Studio need to shutdown. [Required] |
| `support_url` | `string` | A valid URL if a user complains sudden termination of OBS Studio. [Required] |
| `force` | `bool` | Make effort to shutdown without confirmation dialogs such as stop recording and streaming. [Optional] |

See [example/shutdown.py](example/shutdown.py) for details.

## Build and install
### Linux
Make sure `libobsConfig.cmake` is found by cmake.
After checkout, run these commands.
```
mkdir build && cd build
cmake \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib/x86_64-linux-gnu \
	-DQT_VERSION=6 \
	..
make -j2
sudo make install
```
You might need to adjust `CMAKE_INSTALL_LIBDIR` for your system.

### macOS
Make sure `libobsConfig.cmake` is found by cmake.
After checkout, run these commands.
```
mkdir build && cd build
cmake \
    -DCMAKE_PREFIX_PATH="$PWD/release/" \
    -DQT_VERSION=6 \
	..
make -j2
```
Finally, copy `obs-shutdown-plugin.so` and `data` to the obs-plugins folder.

See a file [.github/workflows/main.yml](.github/workflows/main.yml) for details.

## See also
- [Suggestion](https://ideas.obsproject.com/posts/2225/allow-clean-shutdown-via-api)
- [obs-studio PR 8888](https://github.com/obsproject/obs-studio/pull/8888)
- [obs-studio PR 8889](https://github.com/obsproject/obs-studio/pull/8889)
