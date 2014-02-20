/** THIS IS AN AUTOMATICALLY GENERATED FILE.  DO NOT MODIFY
 * BY HAND!!
 *
 * Generated by lcm-gen
 **/

#include <stdint.h>
#include <stdlib.h>
#include <lcm/lcm_coretypes.h>
#include <lcm/lcm.h>

#ifndef _mav_map_measurement_function_t_h
#define _mav_map_measurement_function_t_h

#ifdef __cplusplus
extern "C" {
#endif

#include "lcmtypes/occ_map_pixel_map_t.h"
#include "lcmtypes/occ_map_pixel_map_t.h"
#include "lcmtypes/occ_map_pixel_map_t.h"
#include "lcmtypes/occ_map_pixel_map_t.h"
#include "lcmtypes/occ_map_pixel_map_t.h"
#include "lcmtypes/occ_map_pixel_map_t.h"
typedef struct _mav_map_measurement_function_t mav_map_measurement_function_t;
struct _mav_map_measurement_function_t
{
    int64_t    utime;
    double     z_height;
    double     theta;
    occ_map_pixel_map_t phi_psi_xy_cov_map;
    occ_map_pixel_map_t phi_psi_xy_information_map;
    occ_map_pixel_map_t xy_max_information;
    occ_map_pixel_map_t xy_min_information;
    int32_t    num_xy_maps;
    occ_map_pixel_map_t *xy_information_maps;
    occ_map_pixel_map_t *xy_cov_maps;
};

mav_map_measurement_function_t   *mav_map_measurement_function_t_copy(const mav_map_measurement_function_t *p);
void mav_map_measurement_function_t_destroy(mav_map_measurement_function_t *p);

typedef struct _mav_map_measurement_function_t_subscription_t mav_map_measurement_function_t_subscription_t;
typedef void(*mav_map_measurement_function_t_handler_t)(const lcm_recv_buf_t *rbuf,
             const char *channel, const mav_map_measurement_function_t *msg, void *user);

int mav_map_measurement_function_t_publish(lcm_t *lcm, const char *channel, const mav_map_measurement_function_t *p);
mav_map_measurement_function_t_subscription_t* mav_map_measurement_function_t_subscribe(lcm_t *lcm, const char *channel, mav_map_measurement_function_t_handler_t f, void *userdata);
int mav_map_measurement_function_t_unsubscribe(lcm_t *lcm, mav_map_measurement_function_t_subscription_t* hid);
int mav_map_measurement_function_t_subscription_set_queue_capacity(mav_map_measurement_function_t_subscription_t* subs,
                              int num_messages);


int  mav_map_measurement_function_t_encode(void *buf, int offset, int maxlen, const mav_map_measurement_function_t *p);
int  mav_map_measurement_function_t_decode(const void *buf, int offset, int maxlen, mav_map_measurement_function_t *p);
int  mav_map_measurement_function_t_decode_cleanup(mav_map_measurement_function_t *p);
int  mav_map_measurement_function_t_encoded_size(const mav_map_measurement_function_t *p);

// LCM support functions. Users should not call these
int64_t __mav_map_measurement_function_t_get_hash(void);
int64_t __mav_map_measurement_function_t_hash_recursive(const __lcm_hash_ptr *p);
int     __mav_map_measurement_function_t_encode_array(void *buf, int offset, int maxlen, const mav_map_measurement_function_t *p, int elements);
int     __mav_map_measurement_function_t_decode_array(const void *buf, int offset, int maxlen, mav_map_measurement_function_t *p, int elements);
int     __mav_map_measurement_function_t_decode_array_cleanup(mav_map_measurement_function_t *p, int elements);
int     __mav_map_measurement_function_t_encoded_array_size(const mav_map_measurement_function_t *p, int elements);
int     __mav_map_measurement_function_t_clone_array(const mav_map_measurement_function_t *p, mav_map_measurement_function_t *q, int elements);

#ifdef __cplusplus
}
#endif

#endif