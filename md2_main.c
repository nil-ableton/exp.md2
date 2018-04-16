#include <stdio.h>

int test_buf(int, char const**);
int test_map(int, char const**);

int main(int argc, char const* argv[])
{
  test_buf(argc, argv);
  test_map(argc, argv);
  puts("md2: done");
  return 0;
}
