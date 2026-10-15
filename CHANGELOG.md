# Changelog

## 1.0.0 (2026-10-15)


### Features

* **core:** persist settings, guard risky changes and move the console out of main ([eca1c7b](https://github.com/alexmtos/gbs-reimagined/commit/eca1c7b1fe2597d31c320f80a5976f72c1e95b6c))
* **input:** sampling controls with a live sync-quality meter ([#6](https://github.com/alexmtos/gbs-reimagined/issues/6)) ([ee7a5f6](https://github.com/alexmtos/gbs-reimagined/commit/ee7a5f6670a5fbce077b55b7b1d0f72e3ea15b3f))
* **installer:** browser installer with Improv Wi-Fi, published on Pages (M3) ([#11](https://github.com/alexmtos/gbs-reimagined/issues/11)) ([7bb7ad1](https://github.com/alexmtos/gbs-reimagined/commit/7bb7ad11c07a52a69077b59e822a8cb8b82715ee))
* lay the foundations of GBS Reimagined (milestone M0) ([212c57d](https://github.com/alexmtos/gbs-reimagined/commit/212c57d803da8f3c7f4c1d75e20e4afad7c6a2bf))
* milestone M1, first picture monitored everywhere ([#2](https://github.com/alexmtos/gbs-reimagined/issues/2)) ([5b58fde](https://github.com/alexmtos/gbs-reimagined/commit/5b58fde3389412d8dde28ab714ba2b172a58bc88))
* **net:** serve the web UI and the JSON API over Wi-Fi ([ffd2f9f](https://github.com/alexmtos/gbs-reimagined/commit/ffd2f9fa787b1583b3800c6f0d105ea0d866a2c4))
* **output:** force 60 Hz for 50 Hz sources; drop blank on change ([#7](https://github.com/alexmtos/gbs-reimagined/issues/7)) ([34e32f6](https://github.com/alexmtos/gbs-reimagined/commit/34e32f6bb425d8bb82a4df225e9e65b2d064f6d5))
* **output:** integer scale and computed output modes (M2) ([#9](https://github.com/alexmtos/gbs-reimagined/issues/9)) ([3949ce8](https://github.com/alexmtos/gbs-reimagined/commit/3949ce862db5335ee0f30716e5bd8b5746a098e6))
* **profiles:** saved profiles with auto-load, switcher triggers and backup (M2) ([#8](https://github.com/alexmtos/gbs-reimagined/issues/8)) ([233a00a](https://github.com/alexmtos/gbs-reimagined/commit/233a00a9f56876899865de23aba260eae8421445))
* **ui:** run the board from its own screen and knob ([d4514a9](https://github.com/alexmtos/gbs-reimagined/commit/d4514a970323a10d8b40660c1da2a0810ac7232f))
* **update:** firmware update over the network (M3) ([#10](https://github.com/alexmtos/gbs-reimagined/issues/10)) ([182b259](https://github.com/alexmtos/gbs-reimagined/commit/182b2594998ca6dbb4586493c2e12ead51a2f0ca))
* **video:** add the video engine and calibrate ADC black levels at boot ([42996a7](https://github.com/alexmtos/gbs-reimagined/commit/42996a70f7f27171fe3c113ea46e7dca1cfd8869))
* **video:** calibrate input levels on a white screen and add auto gain ([8bdfc86](https://github.com/alexmtos/gbs-reimagined/commit/8bdfc8621fcf481666c10a99053a4ec564745691))
* **video:** output geometry - aspect, position and size (M2) ([#4](https://github.com/alexmtos/gbs-reimagined/issues/4)) ([7a2e502](https://github.com/alexmtos/gbs-reimagined/commit/7a2e5021e92a183393e5f74d6b37b7b25265aa3a))
* **video:** pass HD and computer sources through (experimental) ([ea40553](https://github.com/alexmtos/gbs-reimagined/commit/ea405539fff8c7439ce85fe830872ede23dc7641))
* **video:** scanlines and the deinterlacer choice ([#5](https://github.com/alexmtos/gbs-reimagined/issues/5)) ([bb98c79](https://github.com/alexmtos/gbs-reimagined/commit/bb98c799896efb95b05100554f18a091b85ac826))
* **web:** edit settings, confirm risky changes and join a network ([22fab3c](https://github.com/alexmtos/gbs-reimagined/commit/22fab3c0b0598c52171057c6fa3fccaaca62e5ae))


### Bug Fixes

* **net:** deliver the whole page and say why a Wi-Fi join failed ([92c995f](https://github.com/alexmtos/gbs-reimagined/commit/92c995f4c72f6168712c6cd81446e0531ab96a77))
* **video:** calibrate white to 0x7A to keep clear of the ADC ceiling ([5d37d1b](https://github.com/alexmtos/gbs-reimagined/commit/5d37d1b38465aa8faa81320c3c55523e43f9335d))
* **video:** show the whole 576-line picture from PAL sources ([5c2696c](https://github.com/alexmtos/gbs-reimagined/commit/5c2696c5f795bcb5f7edaa040e8ffd47d2e3b47c))
* **video:** switch to the input the user chooses ([7c6bf60](https://github.com/alexmtos/gbs-reimagined/commit/7c6bf60c8f0439f7f99ec7cfd5a6b0b111ea9254))


### Performance Improvements

* **video:** probe a discrete VSync across ticks instead of in one ([24627bd](https://github.com/alexmtos/gbs-reimagined/commit/24627bd965fd73c751773de186408ac74167be07))
