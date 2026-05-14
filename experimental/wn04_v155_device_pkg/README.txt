WN04 manual_web helper runtime

This is the retained low-glibc aarch64 helper runtime used by the URL online
shelf module when ROCREADER_MANUAL_WEB_TRANSPORT=1.

Runtime files:

  wn04_v155_device_pkg/bin/wn04_fetch
  wn04_v155_device_pkg/bin/curl-impersonate
  wn04_v155_device_pkg/lib/libstdc++.so.6
  wn04_v155_device_pkg/lib/libgcc_s.so.1

Confirmed device scope:

  - WN04 catalog page fetch works.
  - WN04 detail page fetch works.
  - WN04/wnacg cover fetch works.
  - d1.wcdn.date generate-link and signed download URLs do not work on the
    tested device network.

Use in ROCreader:

  export ROCREADER_MANUAL_WEB_TRANSPORT=1
  export ROCREADER_MANUAL_WEB_FETCH=/path/to/wn04_fetch
  export ROCREADER_MANUAL_WEB_CURL=/path/to/curl-impersonate
  export ROCREADER_MANUAL_WEB_CATALOG_ONLY=1

Do not add this helper to release packages by default. Keep release online
sources template-only.
