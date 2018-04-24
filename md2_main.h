#ifndef MD2_MAIN
#define MD2_MAIN

static void md2_exit_with_message(char const* fmt, ...);

#define md2_fatal(...) assert(0), md2_exit_with_message(__VA_ARGS__)


#endif