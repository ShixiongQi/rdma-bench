#ifndef CONFIG_H_
#define CONFIG_H_

#include <stdint.h>
struct user_param
{
    uint32_t device_idx;
    uint32_t sgid_idx;
    uint32_t qp_num;
    uint32_t mr_num;
    uint8_t ib_port;
};
#endif /* CONFIG_H_*/
