## How to build liteplayer for esp32

### Setup ESP-ADF

See https://docs.espressif.com/projects/esp-adf/zh_CN/latest/get-started/index.html

My esp-adf info:
 - repo: https://github.com/espressif/esp-adf.git
 - branch: master
 - head: fde2bb04a9e5873cd0a1657529b14c719c03466e
 - idf_ver: v3.3

### Setup the environment variants

``` bash
export ADF_PATH="<YOUR ADF PATH>"
export IDF_PATH="$ADF_PATH/esp-idf"
export IDF_TOOLS_PATH="<YOUR IDF TOOLS PATH>"
. $IDF_PATH/export.sh
```

### Build the project

``` bash
idf.py menuconfig                # config your wifi ssid&password, audio board
idf.py build                     # build
idf.py -p <PORT> flash monitor   # flash image and monitor serial message
```

