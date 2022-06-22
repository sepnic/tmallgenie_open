## How to build tmallgenie on macosx/ubuntu/raspberrypi

### Environment dependencies

Ubuntu/raspberrypi：`sudo apt-get install libatlas-base-dev libasound2-dev`

### Build the project and run

``` bash
mkdir -p example/unix/out
cd example/unix/out
cmake ..
make
./GenieMain
```
Speaking "Jarvis" or pressing enter could wakeup Genie SDK for interaction
