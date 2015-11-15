#ifndef MLCL_H
#define MLCL_H

#include <CL/cl.h>

typedef struct {
    // see: https://www.khronos.org/registry/cl/sdk/1.0/docs/man/xhtml/clGetDeviceInfo.html
    cl_device_id id;

    char *_raw_str;                                                             // all strings below in one...
    cl_uint _raw_str_len;                                                       // length of buffer for strings

    char *vendor;                                                               // CL_DEVICE_VENDOR
    char *name;                                                                 // CL_DEVICE_NAME

    cl_device_type type;                                                        // CL_DEVICE_TYPE

    cl_ulong        global_memory_size;                                         // CL_DEVICE_GLOBAL_MEM_SIZE
	cl_ulong        local_memory_size;                                          // CL_DEVICE_LOCAL_MEM_SIZE
	cl_uint         max_compute_units;                                          // CL_DEVICE_MAX_COMPUTE_UNITS

	size_t          max_work_group_size;                                        // CL_DEVICE_MAX_WORK_GROUP_SIZE
} mlcl_device;

typedef struct {
    // see: https://www.khronos.org/registry/cl/sdk/1.0/docs/man/xhtml/clGetPlatformInfo.html
    cl_platform_id id;                                                          // platform id/handler

    char *_raw_str;                                                             // all strings below in one...
    cl_uint _raw_str_len;                                                       // length of buffer for strings

    char *vendor;                                                               // CL_PLATFORM_VENDOR
    char *name;                                                                 // CL_PLATFORM_NAME

    mlcl_device *device;                                                        // devices from platform (check `mlclPopulateDevices`)
    cl_uint device_len;                                                         // length of `device` array
} mlcl_platform;

extern cl_int mlclInitializePlatform(mlcl_platform *platform,                   // platform to initialize
                                     cl_uint platform_bs);                      // size of internal complex string system

extern cl_int mlclGetPlatform(cl_uint num_entries,                              // limits fetched `platforms` count
                              mlcl_platform *platforms,                         // address to fetch `platforms`
                              cl_uint *num_platforms);                          // count of `platforms feteched will be placed here`

extern void mlclDropPlatform(mlcl_platform *platform);                          // platform to drop

extern cl_int mlclPopulateDevices(mlcl_platform *platform,
                                  cl_device_type type,
                                  cl_uint device_bs);

#endif /* MLCL_H */
