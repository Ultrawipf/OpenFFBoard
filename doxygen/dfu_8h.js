var dfu_8h =
[
    [ "dfu_status_response_t", "dfu_8h.html#aaf8f42c0268914a5be70977d261bd58e", null ],
    [ "dfu_descriptor_type_t", "dfu_8h.html#a37c9cfb22efe8e280af662ace4330a3e", [
      [ "DFU_DESC_FUNCTIONAL", "dfu_8h.html#a37c9cfb22efe8e280af662ace4330a3ea104febb2c4ae0a2bdd3a326681489f29", null ]
    ] ],
    [ "dfu_requests_t", "dfu_8h.html#a3f8c3355e116b6b3e1f4456d78e591ae", [
      [ "DFU_REQUEST_DETACH", "dfu_8h.html#a3f8c3355e116b6b3e1f4456d78e591aead62eb3475664d46507fedfd42c5d887d", null ],
      [ "DFU_REQUEST_DNLOAD", "dfu_8h.html#a3f8c3355e116b6b3e1f4456d78e591aeae1dda5da816e42c62cd7242a9dd24646", null ],
      [ "DFU_REQUEST_UPLOAD", "dfu_8h.html#a3f8c3355e116b6b3e1f4456d78e591aea2107138fe930491b1b44839976c1ac47", null ],
      [ "DFU_REQUEST_GETSTATUS", "dfu_8h.html#a3f8c3355e116b6b3e1f4456d78e591aea2a605ea936737a87fa38bf4f7b64426e", null ],
      [ "DFU_REQUEST_CLRSTATUS", "dfu_8h.html#a3f8c3355e116b6b3e1f4456d78e591aeaad306d79f583680fd0b8dac302f8d7c0", null ],
      [ "DFU_REQUEST_GETSTATE", "dfu_8h.html#a3f8c3355e116b6b3e1f4456d78e591aea4fb42c4e4f2dde0b3725e36cae2770b0", null ],
      [ "DFU_REQUEST_ABORT", "dfu_8h.html#a3f8c3355e116b6b3e1f4456d78e591aea2eab07a6ed314e1d1d0fcf360554eddd", null ]
    ] ],
    [ "dfu_state_t", "dfu_8h.html#a187dfde3b0fa8047e6c97b2317bce21f", [
      [ "APP_IDLE", "dfu_8h.html#a187dfde3b0fa8047e6c97b2317bce21fab7ba8c8ed2c179fec88a9b6aafc663f0", null ],
      [ "APP_DETACH", "dfu_8h.html#a187dfde3b0fa8047e6c97b2317bce21fa1b6cb3fe0a26dce8a6fce9f179aa23ef", null ],
      [ "DFU_IDLE", "dfu_8h.html#a187dfde3b0fa8047e6c97b2317bce21fa6750bb1bc3aa5e183a24e5f1eebbc134", null ],
      [ "DFU_DNLOAD_SYNC", "dfu_8h.html#a187dfde3b0fa8047e6c97b2317bce21fac112f5ac931419fca913cc4791c768cc", null ],
      [ "DFU_DNBUSY", "dfu_8h.html#a187dfde3b0fa8047e6c97b2317bce21fa6c7e0d5ef61f350a4719b0d637dae263", null ],
      [ "DFU_DNLOAD_IDLE", "dfu_8h.html#a187dfde3b0fa8047e6c97b2317bce21faf6596caed39e4fc9a86c86fc48fb04ea", null ],
      [ "DFU_MANIFEST_SYNC", "dfu_8h.html#a187dfde3b0fa8047e6c97b2317bce21fadf68d3fd97017e0caba53637e530fc1d", null ],
      [ "DFU_MANIFEST", "dfu_8h.html#a187dfde3b0fa8047e6c97b2317bce21fa7b90825d89b39d39d150336fe251982c", null ],
      [ "DFU_MANIFEST_WAIT_RESET", "dfu_8h.html#a187dfde3b0fa8047e6c97b2317bce21faeda26ae591d2de9fc8dfbcf28285613c", null ],
      [ "DFU_UPLOAD_IDLE", "dfu_8h.html#a187dfde3b0fa8047e6c97b2317bce21faf56d835eb6704702c3ddf5134402f82a", null ],
      [ "DFU_ERROR", "dfu_8h.html#a187dfde3b0fa8047e6c97b2317bce21fafc200a88839dcdee3b90417f52b35657", null ]
    ] ],
    [ "dfu_status_t", "dfu_8h.html#aa74586a3991c492fcb4fa3ba5adeea2b", [
      [ "DFU_STATUS_OK", "dfu_8h.html#aa74586a3991c492fcb4fa3ba5adeea2ba3a550674419538042b365d365595dc40", null ],
      [ "DFU_STATUS_ERR_TARGET", "dfu_8h.html#aa74586a3991c492fcb4fa3ba5adeea2ba9a7a7639b542fa571c5bf230e811afa5", null ],
      [ "DFU_STATUS_ERR_FILE", "dfu_8h.html#aa74586a3991c492fcb4fa3ba5adeea2ba507bb0dd505ecf8e1ad31f3ad59ca595", null ],
      [ "DFU_STATUS_ERR_WRITE", "dfu_8h.html#aa74586a3991c492fcb4fa3ba5adeea2ba714bc30afe28582ddb38e90702beb1d3", null ],
      [ "DFU_STATUS_ERR_ERASE", "dfu_8h.html#aa74586a3991c492fcb4fa3ba5adeea2ba69f5911f3539cf83984e2220e069d9d0", null ],
      [ "DFU_STATUS_ERR_CHECK_ERASED", "dfu_8h.html#aa74586a3991c492fcb4fa3ba5adeea2ba1b79f547824d24336bd32c4facf90e35", null ],
      [ "DFU_STATUS_ERR_PROG", "dfu_8h.html#aa74586a3991c492fcb4fa3ba5adeea2ba334d3e00203a36c177a36c4f20ca4cd3", null ],
      [ "DFU_STATUS_ERR_VERIFY", "dfu_8h.html#aa74586a3991c492fcb4fa3ba5adeea2bab209da0e18d2bdac90cce75446f5e284", null ],
      [ "DFU_STATUS_ERR_ADDRESS", "dfu_8h.html#aa74586a3991c492fcb4fa3ba5adeea2bab7abfa9c48bb721cdbc63a220e0a34a2", null ],
      [ "DFU_STATUS_ERR_NOTDONE", "dfu_8h.html#aa74586a3991c492fcb4fa3ba5adeea2ba9f0b8e1bdb57da25a84e499db614fa38", null ],
      [ "DFU_STATUS_ERR_FIRMWARE", "dfu_8h.html#aa74586a3991c492fcb4fa3ba5adeea2baaebd82c354d409a6a918f103bbfe6c26", null ],
      [ "DFU_STATUS_ERR_VENDOR", "dfu_8h.html#aa74586a3991c492fcb4fa3ba5adeea2bad5c270162ee2af751292540632a538c2", null ],
      [ "DFU_STATUS_ERR_USBR", "dfu_8h.html#aa74586a3991c492fcb4fa3ba5adeea2babc69336005f3df189e7df6d0f0ad69b2", null ],
      [ "DFU_STATUS_ERR_POR", "dfu_8h.html#aa74586a3991c492fcb4fa3ba5adeea2ba4b8d062347f43c1d75a58ed4e74d63bb", null ],
      [ "DFU_STATUS_ERR_UNKNOWN", "dfu_8h.html#aa74586a3991c492fcb4fa3ba5adeea2bafeee9594f62632238db9053c37a99465", null ],
      [ "DFU_STATUS_ERR_STALLEDPKT", "dfu_8h.html#aa74586a3991c492fcb4fa3ba5adeea2ba8d963dfec7ec03c538c5a38944f63fe3", null ]
    ] ],
    [ "TU_VERIFY_STATIC", "dfu_8h.html#ade151499e67494a9bc5276676e4b2fd3", null ],
    [ "C", "dfu_8h.html#aaa53ca0b650dfd85c4f59fa156f7a2cc", null ]
];