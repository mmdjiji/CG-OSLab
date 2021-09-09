#include <stdio.h>

int fibo (int n) {
  int i;
  int a = 1, b = 1, c = 2;
  for(i=0; i<n; ++i) {
    printf("%d ", a);
    a = b;
    b = c;
    c = a + b;
  }
  return 0;
}

int main () {
  int var;
  scanf("%d", &var);
  fibo(var);
  return 0;
}