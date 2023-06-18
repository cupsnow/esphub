
# Setup

Board **Sparkfun ESP32 Thing**

[Install IDF for Linux](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/linux-macos-setup.html)

[Set up the environment variables](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/linux-macos-setup.html#step-4-set-up-the-environment-variables) and/or continuous coding

```sh
. ./env.sh

# Original method
# . $HOME/esp/esp-idf/export.sh
```

Sparkfun ESP32 Thing Main XTAL 26MHz

```sh
`idf.py menuconfig`

# Component config → Hardware Settings → Main XTAL Config → Main XTAL frequency
```

# Coding

  - Task stack at least 2048 when use printf
