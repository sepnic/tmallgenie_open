## How to build tmallgenie for esp32

### Supported development boards

 - ESP32-LyraT V4.3

 Other official esp32 boards may also work, but have not been verified

### Setup ESP-ADF

See https://docs.espressif.com/projects/esp-adf/zh_CN/latest/get-started/index.html

### Setup the environment variants

``` bash
export ADF_PATH="<YOUR ADF PATH>"
export IDF_PATH="$ADF_PATH/esp-idf"
export IDF_TOOLS_PATH="<YOUR IDF TOOLS PATH>"
. $IDF_PATH/export.sh
```

### Build the project

``` bash
# config your wifi ssid&password and audio board, check sdkconfig.defaults for details
idf.py menuconfig
idf.py --no-ccache build       # build
idf.py -p <PORT> flash monitor # flash image and monitor serial message
```
Speaking "Hi LeXin" or pressing 'REC' key could wakeup Genie SDK for interaction
