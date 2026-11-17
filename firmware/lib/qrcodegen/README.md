# qrcodegen (vendored)

The C version of Project Nayuki's
[QR Code generator library](https://www.nayuki.io/page/qr-code-generator-library), MIT licence
(see [LICENSE](LICENSE)). It draws the QR codes on the board's screen
([ADR 0017](../../../docs/adr/0017-qr-codes-from-nayukis-library.md)).

| File | Source | SHA-256 |
|---|---|---|
| `qrcodegen.c` | `c/qrcodegen.c` at tag v1.8.0 (`720f62bddb7226106071d4728c292cb1df519ceb`) of [nayuki/QR-Code-generator](https://github.com/nayuki/QR-Code-generator) | `300eff07ee25baaa7578f20284411638154716379437391e7e689c0e6ce81403` |
| `qrcodegen.h` | `c/qrcodegen.h`, same tag | `e82df4bff37d18b5863b9e7486fe6bda1b6cda8c3b9ecebfec473907265cb589` |
| `LICENSE` | the licence section of `Readme.markdown`, same tag | |

The files are unmodified. `library.json` builds them with `NDEBUG`, so their `assert` texts do not
take RAM, and without the firmware's C++-only warning flag. To update: download both files from a newer tag, record the tag, commit
and checksums here, and run the host tests (`firmware/test/test_qr`). Dependencies are updated by
hand ([ADR 0016](../../../docs/adr/0016-dependencies-updated-by-hand.md)).
