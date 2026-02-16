#include "graph.h"

bench_graph g;

int main() {
    g.init();
    g.run(1);
    g.end();
    return 0;
}