#include <luisa/runtime/context.h>
#include <luisa/runtime/device.h>
#include <luisa/runtime/stream.h>

using namespace luisa::compute;

int main(int argc, char** argv) {

    Context context{ argv[1] }; 
    Device  device = context.create_device(argv[2]);
    Stream  stream = device.create_stream();

    return 0;
}