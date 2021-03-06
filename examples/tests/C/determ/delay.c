#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <C4SNet.h>

void *delay( void *hnd, c4snet_data_t *x)
{
  int int_x = *(int *)C4SNetGetData( x);
  c4snet_data_t *result;

  usleep(int_x * 1000);

  result = C4SNetCreate(CTYPE_int, 1, &int_x);

  C4SNetFree(x);

  C4SNetOut( hnd, 1, result);

  return hnd;
}

