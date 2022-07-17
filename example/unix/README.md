## How to build tmallgenie for macosx/ubuntu/raspberry-pi

### Environment dependencies

Ubuntu/raspberry-pi：`sudo apt-get install libatlas-base-dev libasound2-dev`

### Build the project and run

``` bash
mkdir -p example/unix/out
cd example/unix/out
cmake ..
make
./GenieMain
```
Speaking "Jarvis" or pressing enter could wakeup Genie SDK for interaction
