#include <iostream>
#include "vec3.h"

int main() {
    Vec3 v1(1, 1, 1);
    Vec3 v2(2, 2, 2);

    std::cout << v1 + v2 << std::endl;

    return 0;
}
