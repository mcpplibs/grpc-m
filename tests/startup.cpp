// DIAGNOSTIC (temporary branch): the module imported, nothing called.
// If this one also dies, the failure is in static initialisation of what links.
#include <cstdio>

import mcpplibs.mwrap.grpc;

int main() {
    std::fputs("startup: main entered\n", stderr);
    return 0;
}
