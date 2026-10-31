// Only used by the PlatformIO integration: options come from platformio.ini / "// sim:" comments
// via the build script instead of from the sketchgen tool.
#ifdef SIM_PLATFORMIO
#include "sim_args.h"
#ifndef SIM_ARGS
#define SIM_ARGS ""
#endif
#ifndef SIM_NAME
#define SIM_NAME "sketch"
#endif
const char *sim_sketch_args = SIM_ARGS;
const char *sim_sketch_name = SIM_NAME;
#endif
