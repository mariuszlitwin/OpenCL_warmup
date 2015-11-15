#include <stdlib.h>
#include <string.h>

#include <CL/cl.h>

#include "mlcl.h"

cl_int mlclInitializePlatform(mlcl_platform *platform,
                              cl_uint platform_bs) {
    platform->id = 0;
    platform->name = NULL;
    platform->vendor = NULL;

    platform->_raw_str_len = platform_bs;
    platform->_raw_str = malloc(sizeof(char) * platform_bs);

    if (platform->_raw_str)
        return CL_SUCCESS;
    else
        return CL_OUT_OF_HOST_MEMORY;
}

cl_int mlclGetPlatform(cl_uint num_entries,
                       mlcl_platform *platforms,
                       cl_uint *num_platforms) {
    cl_int retcode = 0;

    cl_uint i = 0;
    size_t byte_counter = 0,
           last_len = 0;
    char *strptr = NULL;

    cl_platform_id *platforms_raw = NULL;

    if (platforms)
        platforms_raw = (cl_platform_id *) malloc(sizeof(cl_platform_id) * num_entries);

    retcode = clGetPlatformIDs(num_entries,
                               platforms_raw,
                               num_platforms);

    if (platforms) {
        for (i = 0; i < num_entries; i++) {
            byte_counter = 0;
            strptr = platforms[i]._raw_str;
            platforms[i].id = platforms_raw[i];

            clGetPlatformInfo(platforms[i].id,
                              CL_PLATFORM_VENDOR,
                              platforms[i]._raw_str_len - byte_counter,
                              strptr,
                              &last_len);

            platforms[i].vendor = strptr;

            strptr += last_len;
            byte_counter += last_len;

            clGetPlatformInfo(platforms[i].id,
                              CL_PLATFORM_NAME,
                              platforms[i]._raw_str_len - byte_counter,
                              strptr,
                              &last_len);

            platforms[i].name = strptr;

            strptr += last_len;
            byte_counter += last_len;
        }
    }

    free(platforms_raw);
    return retcode;
}

void mlclDropPlatform(mlcl_platform *platform) {
    cl_uint i = 0;

    for (i = 0; i < platform->device_len; i++)
        free((platform->device)[i]._raw_str);

    free(platform->device);

    free(platform->_raw_str);
    free(platform);
}

cl_int mlclInitializeDevice(mlcl_device *device,
                            cl_uint device_bs) {
    device->id = 0;
    device->name = NULL;
    device->vendor = NULL;

    device->type = 0;

    device->global_memory_size = 0;
	device->local_memory_size = 0;
	device->max_compute_units = 0;

	device->max_work_group_size = 0;

	device->_raw_str_len = device_bs;
	device->_raw_str = malloc(sizeof(char) * device_bs);

    if (device->_raw_str)
        return CL_SUCCESS;
    else
        return CL_OUT_OF_HOST_MEMORY;
}

cl_int mlclPopulateDevices(mlcl_platform *platform,
                           cl_device_type type,
                           cl_uint device_bs) {
    cl_uint i = 0;
    cl_uint num_devices = 0,
            t_num_devices = 0;

    size_t byte_counter = 0,
           last_len = 0;
    char *strptr = NULL;

    cl_int retcode = clGetDeviceIDs(platform->id,
                                    type,
                                    NULL,
                                    NULL,
                                    &num_devices);

    mlcl_device *devices = NULL;
    cl_device_id *devices_raw = (cl_device_id *) malloc(sizeof(cl_device_id) * num_devices);


    retcode = clGetDeviceIDs(platform->id,
                             type,
                             num_devices,
                             devices_raw,
                             &t_num_devices);

    num_devices = t_num_devices;

    devices = (mlcl_device *) malloc(sizeof(mlcl_device) * num_devices);
    for (i = 0; i < num_devices; i++)
        retcode = mlclInitializeDevice(&(devices[i]), device_bs);

    if (retcode == CL_SUCCESS) {
        for (i = 0; i < num_devices; i++) {
            byte_counter = 0;
            strptr = devices[i]._raw_str;
            devices[i].id = devices_raw[i];

            clGetDeviceInfo(devices_raw[i],
                            CL_DEVICE_VENDOR,
                            (devices[i]._raw_str_len - byte_counter),
                            strptr,
                            &last_len);

            devices[i].vendor = strptr;

            strptr += last_len;
            byte_counter += last_len;

            clGetDeviceInfo(devices[i].id,
                            CL_DEVICE_NAME,
                            devices[i]._raw_str_len - byte_counter,
                            strptr,
                            &last_len);

            devices[i].name = strptr;

            strptr += last_len;
            byte_counter += last_len;

            clGetDeviceInfo(devices[i].id,
                            CL_DEVICE_TYPE,
                            sizeof(cl_device_type),
                            &(devices[i].type),
                            &last_len);

            clGetDeviceInfo(devices[i].id,
                            CL_DEVICE_GLOBAL_MEM_SIZE,
                            sizeof(cl_ulong),
                            &(devices[i].global_memory_size),
                            &last_len);

            clGetDeviceInfo(devices[i].id,
                            CL_DEVICE_LOCAL_MEM_SIZE,
                            sizeof(cl_ulong),
                            &(devices[i].local_memory_size),
                            &last_len);

            clGetDeviceInfo(devices[i].id,
                            CL_DEVICE_MAX_COMPUTE_UNITS,
                            sizeof(cl_uint),
                            &(devices[i].max_compute_units),
                            &last_len);

            clGetDeviceInfo(devices[i].id,
                            CL_DEVICE_MAX_WORK_GROUP_SIZE,
                            sizeof(size_t),
                            &(devices[i].max_work_group_size),
                            &last_len);
        }
    }

    free(devices_raw);

    platform->device = devices;
    platform->device_len = num_devices;

    return retcode;
}

