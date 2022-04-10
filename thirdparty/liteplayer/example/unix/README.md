## How to build liteplayer for macosx/ubuntu

### Environment dependencies

Ubuntu：`sudo apt-get install libasound2-dev`

### Build the project and run

``` bash
mkdir -p example/unix/out
cd example/unix/out
cmake ..
make
./basic_demo <HTTP_URL|FILE_PATH>
```
