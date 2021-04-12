#include <stdio.h>
#include <stdlib.h>
#include "mm.h"
#include "memlib.h"

void test(void* (*mal)(size_t)){
  mm_init();

}

int main() {
  printf("Testing my implementation\n");
  test(&mm_malloc);

  printf("Testing C implementation\n");
  test(&malloc);
}
