#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "sealed-code-pal-priv.h"
#include <stdio.h>
void scp_register(void)
{
  struct scode_params_info scp_params_info =
  {
    .params_num = 4,
    .pm_str =
    {
      {.type = SCODE_PARAM_TYPE_POINTER,
       .size = 1+sizeof(struct scp_in_msg)/(sizeof(int))}, /* FIXME rounding */
      {.type = SCODE_PARAM_TYPE_INTEGER,
       .size = 1},
      {.type = SCODE_PARAM_TYPE_POINTER,
       .size = 1+sizeof(struct scp_out_msg)/(sizeof(int))}, /* FIXME rounding */
      {.type = SCODE_PARAM_TYPE_POINTER,
       .size = 1},
    }
  };

  struct scode_sections_info scode_info;
  scode_sections_info_init(&scode_info,
                           &__scode_start, scode_ptr_diff(&__scode_end, &__scode_start),
                           &__sdata_start, scode_ptr_diff(&__sdata_end, &__sdata_start),
                           sizeof(struct scp_in_msg)+sizeof(struct scp_out_msg)+PAGE_SIZE /* XXX fudge factor */,
                           2*PAGE_SIZE);
  scode_sections_info_print(&scode_info);
  fflush(NULL);
  assert(!scode_register(&scode_info, &scp_params_info, scp_entry));
}

void scp_unregister(void)
{
  scode_unregister(scp_entry);
}

size_t scp_sealed_size_of_unsealed_size(size_t in)
{
  return in + 80; /* FIXME: calculate this for real */
}

int scp_seal(uint8_t *incode, size_t incode_len, uint8_t *outcode, size_t *outcode_len)
{
  struct scp_in_msg *in;
  struct scp_out_msg *out;
  size_t out_len = sizeof(*out);
  
  assert((in = malloc(sizeof(struct scp_in_msg))) != NULL);
  assert((out = malloc(sizeof(struct scp_out_msg))) != NULL);

  /* XXX need to make sure these are swapped in,
     since we aren't necessarily writing to all of the
     pages inside.
  */
  scode_touch_range(in, sizeof(*in), 1);
  scode_touch_range(out, sizeof(*out), 1);

  in->command = SCP_SEAL;

  assert(incode_len <= SCP_MAX_UNSEALED_LEN);
  memcpy(in->m.seal.code, incode, incode_len);
  in->m.seal.code_len = incode_len;

  scp_register();
  scp_entry(in, sizeof(*in), out, &out_len);
  scp_unregister();

  if(out->status != 0) {
    return out->status;
  }

  assert(out->r.seal.code_len <= *outcode_len);
  *outcode_len = out->r.seal.code_len;

  memcpy(outcode, out->r.seal.code, out->r.seal.code_len);

  return 0;
}

int scp_run(uint8_t *incode, size_t incode_len,
            uint8_t *params, size_t params_len,
            uint8_t *output, size_t *output_len)
{
  struct scp_in_msg *in;
  struct scp_out_msg *out;
  size_t out_len = sizeof(*out);

  assert((in = malloc(sizeof(struct scp_in_msg))) != NULL);
  assert((out = malloc(sizeof(struct scp_out_msg))) != NULL);

  /* XXX need to make sure these are swapped in,
     since we aren't necessarily writing to all of the
     pages inside.
  */
  scode_touch_range(in, sizeof(*in), 1);
  scode_touch_range(out, sizeof(*out), 1);

  in->command = SCP_LOAD;

  assert(incode_len <= SCP_MAX_SEALED_LEN);
  memcpy(in->m.load.code, incode, incode_len);
  in->m.load.code_len = incode_len;

  assert(params_len <= SCP_MAX_PARAM_LEN);
  memcpy(in->m.load.params, params, params_len);
  in->m.load.params_len = params_len;

  scp_register();
  scp_entry(in, sizeof(*in), out, &out_len);
  scp_unregister();

  if(out->status != 0) {
    return out->status;
  }

  assert(out->r.load.output_len <= *output_len);
  *output_len = out->r.load.output_len;

  memcpy(output, out->r.load.output, out->r.load.output_len);

  return 0;
}
