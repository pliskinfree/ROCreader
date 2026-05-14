# URL online shelf transport

ROCreader's URL entry is a normal online shelf module. It supports OPDS/Kavita
sources and `manual_web` sources through the same shelf, cover, and download
interfaces.

Release packages must keep `online_sources.release.ini` template-only. Device or
developer sources belong in `online_sources.ini` on the memory card.

## Manual web helper

`manual_web` sources may use an optional external helper for sites that need a
custom HTTP/TLS transport. The helper is intentionally behind environment
variables so the UI and source parser stay generic:

```sh
export ROCREADER_MANUAL_WEB_TRANSPORT=1
export ROCREADER_MANUAL_WEB_FETCH=/path/to/wn04_fetch
export ROCREADER_MANUAL_WEB_CURL=/path/to/curl-impersonate
export ROCREADER_MANUAL_WEB_CATALOG_ONLY=1
```

The legacy WN04 names are still accepted for compatibility:

```sh
export ROCREADER_EXPERIMENTAL_WN04_TRANSPORT=1
export ROCREADER_WN04_FETCH=/path/to/wn04_fetch
export ROCREADER_WN04_CATALOG_ONLY=1
```

The helper command contract is:

```text
wn04_fetch fetch URL [REFERER]
wn04_fetch download URL OUTPUT [REFERER]
wn04_fetch resolve DETAIL_URL TITLE SOURCE_URL
```

The URL shelf currently uses the helper successfully for catalog pages and
remote covers. Real book download remains behind the same `resolve` and
`download` interfaces, but can be disabled with
`ROCREADER_MANUAL_WEB_CATALOG_ONLY=1` while the download transport is still
being researched.

## WN04 status

Confirmed on device:

- `https://www.wn04.cfd/...` catalog pages load through the Chrome120 HTTP/3
  helper.
- Detail pages load.
- Cover images from `wnacgimg` load.

Known blocker:

- `https://d1.wcdn.date/api/generate-link` and signed
  `https://d1.wcdn.date/download?...` URLs are not reachable from the tested
  device network, even when forcing IPv4 and known Cloudflare IPs. Windows
  `curl_cffi` can resolve and download, so future work should attach to the
  preserved `ManualWebResolveDownload` / `ManualWebDownload` interfaces rather
  than changing the UI or shelf modules.

## Build notes

The low-glibc helper runtime currently lives under:

```text
experimental/wn04_v155_device_pkg/
```

It contains only runtime files and compatibility notes. Old probe scripts,
matrix logs, one-off signed URLs, and full build caches should not be kept in
the project tree.
