#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <CL/cl.h>

#include "ml_typedef.h"
#include "mlcl.h"
#include "pz_file.h"

const ml_byte PS_OPEN_SOURCE = 0,
              PS_GET_PLATFORM_COUNT = 1,
              PS_INITIALIZE_PLATFORM_STRUCT = 2,
              PS_FETCH_PLATFORM = 3,
              PS_FIND_DEVICE = 4,
              PS_PREPARE_SOURCE = 5,
              PS_CONTEXT_ALOCATION = 6,
              PS_CREATE_CONTEXT = 7,
              PS_QUEUE_ALOCATION = 8,
              PS_CREATE_QUEUE = 9,
              PS_OPEN_KERNEL = 10,
              PS_READ_KERNEL = 11,
              PS_PROGRAM_ALOCATION = 12,
              PS_PROGRAM_CREATE = 13,
              PS_PROGRAM_BUILD = 14,
              PS_KERNEL_ALOCATION = 15,
              PS_KERNEL_CREATE = 16,
              PS_IBUFFER_ALOCATION = 17,
              PS_IBUFFER_PREPARE = 18,
              PS_OBUFFER_ALOCATION = 19,
              PS_OBUFFER_PREPARE = 20,
              PS_ADD_ARGUMENTS = 21,
              PS_RUN_KERNEL = 22,
              PS_ENQUEUE_RESULTS = 23;
const ml_byte PS_MAX = 24;

const char error_message[30][255] = {/*  0 */ "Cannot open file with randomness source, error code: %d\n",
                                     /*  1 */ "Failed getting platform count, error code: %d\n",
                                     /*  2 */ "Failed initializing structures for platforms, error code: %d\n",
                                     /*  3 */ "Cannot fetch platform from OpenCL driver, error code: %d\n",
                                     /*  4 */ "Failed fetching devices for chosen platform, error code: %d\n",
                                     /*  5 */ "Error initializing memory for dynamic load estimation, error code: %d\n",
                                     /*  6 */ "Cannot instantiate memory for clContext, error code: %d\n",
                                     /*  7 */ "Cannot instantiate clContext, error code: %d\n",
                                     /*  8 */ "Cannot instantiate memory for clQueue, error code: %d\n",
                                     /*  9 */ "Cannot get queues, error code: %d\n",
                                     /* 10 */ "Cannot open kernel file, error code: %d\n",
                                     /* 11 */ "Failed reading kernel file, error code: %d\n",
                                     /* 12 */ "Cannot instantiate memory for cl_program, error code: %d\n",
                                     /* 13 */ "Cannot create cl_program structs, error code: %d\n",
                                     /* 14 */ "Cannot compile cl_program, error code: %d\n",
                                     /* 15 */ "Cannot instantiate memory for cl_kernel, error code: %d\n",
                                     /* 16 */ "Cannot create kernel, error code: %d\n",
                                     /* 17 */ "Cannot allocate memory for cl_mem input buffer, error code: %d\n",
                                     /* 18 */ "Error creating cl_device(s) input memory buffers, error code: %d\n",
                                     /* 19 */ "Cannot allocate memory for cl_mem output buffer, error code: %d\n",
                                     /* 20 */ "Error creating cl_device(s) output memory buffers, error code: %d\n",
                                     /* 21 */ "Error adding arguments for kernels, error code: %d\n",
                                     /* 22 */ "Error running kernels, error_code: %d\n",
                                     /* 23 */ "Error receiving results from device(s), error code: %d\n"};

int main(int argc, char *argv[]) {
    cl_uint i = 0, j = 0;
    int c = 0;

    cl_uint platforms_count = 0,
            t_platforms_count = 0;

    mlcl_platform *platform = NULL;
    cl_context *context = NULL;
    cl_command_queue *queue = NULL;
    cl_program *program = NULL;
    cl_kernel *kernel = NULL;

    char *host_byte_stream_proxy = NULL;
    cl_mem *dev_byte_stream = NULL;
    cl_mem *dev_output = NULL;

    FILE* f_source = NULL;
    pz_fp fp_source;

    FILE* f_kernel = NULL;
    pz_fp fp_kernel;
    char *s_kernel = NULL;

    cl_int error = CL_SUCCESS;
    cl_uint program_step = 0;


    cl_uint chosen_platform = 0,                                                // platform to use
            subrange_count = 100,                                               // overall count of subranges
            word_size = 4;                                                      // word size in bytes
    float top_percent = 0.9,                                                    // top boundary (percent of maximum word value)
          bot_percent = 0.1;                                                    // bottom boundary (percent of maximum word value)
    //char source_file_name[] = "/home/hadouken/Desktop/test.hex";                // file with random stream to validate
    char *source_file_name = NULL;

    while ((c = getopt(argc, argv, "p:s:w:t:b:")) != -1) {
        switch(c) {
            case 'p':
                chosen_platform = atoi(optarg);
                break;
            case 's':
                subrange_count = atoi(optarg);
                break;
            case 'w':
                word_size = atoi(optarg);
                break;
            case 't':
                top_percent = atof(optarg);
                break;
            case 'b':
                bot_percent = atof(optarg);
                break;
        }
    }
    for (i = optind; i < argc; i++)
        source_file_name = argv[i];

    if (source_file_name == NULL ||
        top_percent < bot_percent) {
        printf("Usage: %s -p chosen_platform -s subrange_count -w word_size -t top_percent_value -b bottom_percent_value <path to randomness source>\n", argv[0]);
        printf("top must be greater than bottom\n");
        return 1;
    }

    cl_uint top_value = 1,
            bot_value = 0;

    for (i = 0; i < word_size * 8; i++)
        top_value *= 2;
    top_value -= 1;

    bot_value = top_value * bot_percent;
    top_value = top_value * top_percent;

    cl_uint compute_units_count = 0;
    cl_uint *compute_units_subranges = NULL;

    cl_uint words_per_subrange = 0;                                             // words per one compute unit
    cl_uint excessive_bytes = 0;                                                // bytes that will not be analyzed (because they can't be assigned to any subrange)
    cl_uint bytes_per_subrange = 0;                                             // bytes per compute unit
    cl_uint bytes_to_analyze = 0;                                               // count of bytes that will be analyzed
    cl_uint subranges_per_compute_unit = 0;
    cl_uint excessive_subranges = 0;

    cl_uint host_bytes_count_proxy = 0;
    size_t global_work_size_proxy = 0;

    cl_uint *results = NULL;

    const char kernel_func_name[] = "subrange_hits";

    printf("Howdy! I'll be using following arguments:\n");
    printf("\tchosen_platform: %u\n", chosen_platform);
    printf("\tsubrange_count: %u\n", subrange_count);
    printf("\tword_size: %u\n", word_size);
    printf("\ttop: %f (%u)\n", top_percent, top_value);
    printf("\tbottom: %f (%u)\n", bot_percent, bot_value);
    printf("\trandomness source: %s\n\n", source_file_name);

    while (program_step < PS_MAX &&
           error == CL_SUCCESS) {
        if (program_step == PS_OPEN_SOURCE) {
            printf("[info] \t[%d]: Opening source file: %s\n", program_step, source_file_name);
            f_source = fopen(source_file_name, "r");
            if (f_source == NULL)
                error = CL_INVALID_VALUE;
            else {
                error = CL_SUCCESS;
                fp_source = pz_wrap_fp(f_source, 0);
            }
        } else if (program_step == PS_GET_PLATFORM_COUNT) {
            printf("[info] \t[%d]: Source file is %u bytes long\n", program_step, fp_source.len);
            printf("[info] \t[%d]: Obtaining platform count\n", program_step);

            error = mlclGetPlatform(NULL, NULL, &platforms_count);
            platform = (mlcl_platform *) malloc(sizeof(mlcl_platform) * platforms_count);

            if (error != CL_SUCCESS)
                error = CL_SUCCESS;
            else if (platform == NULL)
                error = CL_OUT_OF_HOST_MEMORY;
            else
                error = CL_SUCCESS;
        } else if (program_step == PS_INITIALIZE_PLATFORM_STRUCT) {
            printf("[info] \t[%d]: %d platforms available\n", program_step, platforms_count);
            printf("[info] \t[%d]: Initializing platform structures\n", program_step);

            for (i = 0; i < platforms_count; i++)
                if (error == CL_SUCCESS)
                    error = mlclInitializePlatform(&(platform[i]), 255);
        } else if (program_step == PS_FETCH_PLATFORM) {
            printf("[info] \t[%d]: Fetching platform IDs\n", program_step);

            error = mlclGetPlatform(platforms_count,
                                    platform,
                                    &t_platforms_count);

            platforms_count = t_platforms_count;
        } else if (program_step == PS_FIND_DEVICE) {
            printf("[info] \t[%d]: Available platforms:\n", program_step);
            for (i = 0; i < platforms_count; i++) {
                if (chosen_platform == i)
                    printf(" ---> ");
                else
                    printf("      ");
                printf("\t[%d]: [%d] %s %s\n", program_step,
                                             i,
                                             platform[i].vendor,
                                             platform[i].name);
            }

            printf("[info] \t[%d]: Fetching devices for chosen platform\n", program_step);

            if (chosen_platform <= platforms_count)
                error = mlclPopulateDevices(&(platform[chosen_platform]),
                                            CL_DEVICE_TYPE_ALL,
                                            255);
            else {
                printf("[error]\tInvalid platform: %d!\n", chosen_platform);
                error = CL_INVALID_ARG_VALUE;
            }
        } else if (program_step == PS_PREPARE_SOURCE) {
            printf("[info] \t[%d]: %d available devices:\n", program_step,
                                                            platform[chosen_platform].device_len);
            for (j = 0; j < platform[chosen_platform].device_len; j++) {
                printf("      \t[%d]:|-> [%d] %s %s\n", program_step,
                                                        j,
                                                        platform[chosen_platform].device[j].vendor,
                                                        platform[chosen_platform].device[j].name);
                printf("      \t[%d]:|       mem (l/g): %u/%u\n", program_step,
                                                                  platform[chosen_platform].device[j].local_memory_size,
                                                                  platform[chosen_platform].device[j].global_memory_size);
                printf("      \t[%d]:|       max compute units: %u\n", program_step,
                                                                       platform[chosen_platform].device[j].max_compute_units);
                printf("      \t[%d]:|       max workgroup size: %u\n", program_step,
                                                                        platform[chosen_platform].device[j].max_work_group_size);
            }

            printf("[info] \t[%d]: Spliting source file per compute unit\n", program_step);

            for (i = 0; i < platforms_count; i++)
                for (j = 0; j < platform[i].device_len; j++)
                    compute_units_count += platform[i].device[j].max_compute_units;

            compute_units_subranges = (cl_uint *) malloc(sizeof(cl_uint) * compute_units_count);

            if (compute_units_subranges == NULL) {
                error = CL_OUT_OF_HOST_MEMORY;
            } else {
                words_per_subrange = fp_source.len / (word_size * subrange_count);
                excessive_bytes = fp_source.len % (word_size * subrange_count);
                bytes_per_subrange = words_per_subrange * word_size;
                bytes_to_analyze = fp_source.len - excessive_bytes;

                subranges_per_compute_unit = subrange_count / compute_units_count;
                excessive_subranges = subrange_count % compute_units_count;

                for (i = 0; i < compute_units_count; i++) {
                    compute_units_subranges[i] = subranges_per_compute_unit;
                    if (excessive_subranges) {
                        compute_units_subranges[i]++;
                        excessive_subranges--;
                    }
                }
            }
        } else if (program_step == PS_CONTEXT_ALOCATION) {
            printf("[info] \t[%d]: Bytes per subrange: %d\n", program_step, bytes_per_subrange);
            printf("[info] \t[%d]: Subranges per compute unit: \n", program_step);
            for (i = 0; i < compute_units_count; i++) {
                printf("       \t[%d]: CU%d: %d subranges\n", program_step,
                                                                        i,
                                                                        compute_units_subranges[i]);
            }
            printf("[info] \t[%d]: Alocating memory for context\n", program_step);

            context = (cl_context *) malloc(sizeof(cl_context) * platform[chosen_platform].device_len);

            if (context == NULL)
                error = CL_OUT_OF_HOST_MEMORY;
            else {
                for (i = 0; i < platform[chosen_platform].device_len; i++)
                    context[i] = NULL;
                error = CL_SUCCESS;
            }
        } else if (program_step == PS_CREATE_CONTEXT) {
            printf("[info] \t[%d]: Creating context\n", program_step);

            for (i = 0; i < platform[chosen_platform].device_len; i++)
                if (error == CL_SUCCESS)
                    context[i] = clCreateContext(NULL,
                                                 1,
                                                 &(platform[chosen_platform].device[i]),
                                                 NULL,
                                                 NULL,
                                                 &error);
        } else if (program_step == PS_QUEUE_ALOCATION) {
            printf("[info] \t[%d]: Alocating memory for queue\n", program_step);

            queue = (cl_command_queue *) malloc(sizeof(cl_command_queue) * platform[chosen_platform].device_len);

            if (queue == NULL)
                error = CL_OUT_OF_HOST_MEMORY;
            else {
                for (i = 0; i < platform[chosen_platform].device_len; i++)
                    queue[i] = NULL;
                error = CL_SUCCESS;
            }
        } else if (program_step == PS_CREATE_QUEUE) {
            printf("[info] \t[%d]: Creating queues\n", program_step);

            for (i = 0; i < platform[chosen_platform].device_len; i++)
                if (error == CL_SUCCESS)
                    queue[i] = clCreateCommandQueue(context[i],
                                                    platform[chosen_platform].device[i].id,
                                                    NULL,
                                                    &error);
        } else if (program_step == PS_OPEN_KERNEL) {
            printf("[info] \t[%d]: Opening kernel file to read, kernel filename: ./pz_kernel.cl\n", program_step);

            f_kernel = fopen("./pz_kernel.cl", "r");
            if (f_kernel == NULL)
                error = CL_INVALID_VALUE;
            else {
                error = CL_SUCCESS;
                fp_kernel = pz_wrap_fp(f_kernel, 0);
            }
        } else if (program_step == PS_READ_KERNEL) {
            printf("[info] \t[%d]: Reading kernel file\n", program_step);

            s_kernel = (char *) malloc(sizeof(char) * fp_kernel.len);
            if (s_kernel == NULL)
                error = CL_OUT_OF_HOST_MEMORY;
            else {
                fp_kernel.readed = fread(s_kernel,
                                         sizeof(char),
                                         fp_kernel.len,
                                         fp_kernel.fp);
            }
        } else if (program_step == PS_PROGRAM_ALOCATION) {
            printf("[info] \t[%d]: Allocating memory for cl_program(s)\n", program_step);

            program = (cl_program *) malloc(sizeof(cl_program) *
                                            platform[chosen_platform].device_len);
            if (program == NULL)
                error = CL_OUT_OF_HOST_MEMORY;
            else {
                for (i = 0; i < platform[chosen_platform].device_len; i++)
                    program[i] = NULL;
                error = CL_SUCCESS;
            }
        } else if (program_step == PS_PROGRAM_CREATE) {
            printf("[info] \t[%d]: Initializing cl_program(s) structures\n", program_step);

            for (i = 0; i < platform[chosen_platform].device_len; i++)
                if (error == CL_SUCCESS)
                    program[i] = clCreateProgramWithSource(context[i],
                                                           1,
                                                           (const char**) &s_kernel,
                                                           &fp_kernel.readed,
                                                           &error);
        } else if (program_step == PS_PROGRAM_BUILD) {
            printf("[info] \t[%d]: Building program(s)\n", program_step);

            for (i = 0; i < platform[chosen_platform].device_len; i++) {
                if (error == CL_SUCCESS) {
                    error = clBuildProgram(program[i], NULL, NULL, NULL, NULL, NULL);

                    if (error == CL_BUILD_PROGRAM_FAILURE) {
                        size_t log_size;
                        clGetProgramBuildInfo(program[i],
                                              platform[chosen_platform].device[i].id,
                                              CL_PROGRAM_BUILD_LOG,
                                              0,
                                              NULL,
                                              &log_size);
                        char *log = (char *) malloc(log_size);
                        clGetProgramBuildInfo(program[i],
                                              platform[chosen_platform].device[i].id,
                                              CL_PROGRAM_BUILD_LOG,
                                              log_size,
                                              log,
                                              NULL);
                        printf("Error building program for device %d:\n%s\n",  i,
                                                                               log);
                        free(log);
                    }
                }
            }
        } else if (program_step == PS_KERNEL_ALOCATION) {
            printf("[info] \t[%d]: Allocating memory for cl_kernel(s)\n", program_step);

            kernel = (cl_program *) malloc(sizeof(cl_kernel) *
                                           platform[chosen_platform].device_len);
            if (kernel == NULL)
                error = CL_OUT_OF_HOST_MEMORY;
            else {
                for (i = 0; i < platform[chosen_platform].device_len; i++)
                    kernel[i] = NULL;
                error = CL_SUCCESS;
            }
        } else if (program_step == PS_KERNEL_CREATE) {
            printf("[info] \t[%d]: Creating cl_kernel(s)\n", program_step);

            for (i = 0; i < platform[chosen_platform].device_len; i++)
                if (error == CL_SUCCESS)
                    kernel[i] = clCreateKernel(program[i],
                                               kernel_func_name,
                                               &error);
        } else if (program_step == PS_IBUFFER_ALOCATION) {
            printf("[info] \t[%d]: Allocating memory for cl_mem(s) input buffer\n", program_step);

            dev_byte_stream = (cl_mem *) malloc(sizeof(cl_mem) *
                                                platform[chosen_platform].device_len);

            if (dev_byte_stream == NULL)
                error = CL_OUT_OF_HOST_MEMORY;
            else {
                for (i = 0; i < platform[chosen_platform].device_len; i++)
                    dev_byte_stream[i] = NULL;
                error = CL_SUCCESS;
            }
        } else if (program_step == PS_IBUFFER_PREPARE) {
            printf("[info] \t[%d]: Creating data for device(s)\n", program_step);

            for (i = 0; i < platform[chosen_platform].device_len; i++) {
                if (error == CL_SUCCESS) {
                    host_bytes_count_proxy = platform[chosen_platform].device[i].max_compute_units *
                                             subranges_per_compute_unit *
                                             bytes_per_subrange;
                    host_byte_stream_proxy = (char *) malloc(sizeof(char) *
                                                             host_bytes_count_proxy);

                    if (host_byte_stream_proxy == NULL)
                        error = CL_OUT_OF_HOST_MEMORY;
                    else {
                        fp_source.readed = fread(host_byte_stream_proxy,
                                                 sizeof(char),
                                                 host_bytes_count_proxy,
                                                 fp_source.fp);

                        if (host_bytes_count_proxy != fp_source.readed) {
                            printf("[error]\t[%d]: Read %d bytes, should be %d\n", program_step,
                                                                                   fp_source.readed,
                                                                                   host_bytes_count_proxy);
                            error = CL_OUT_OF_RESOURCES;
                        } else
                            dev_byte_stream[i] = clCreateBuffer(context[i],
                                                                CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                                                                sizeof(char) * host_bytes_count_proxy,
                                                                host_byte_stream_proxy,
                                                                &error);

                        free(host_byte_stream_proxy);
                    }
                }
            }
        } else if (program_step == PS_OBUFFER_ALOCATION) {
            printf("[info] \t[%d]: Allocating memory for cl_mem(s) output buffer\n", program_step);

            dev_output = (cl_mem *) malloc(sizeof(cl_mem) *
                                           platform[chosen_platform].device_len);

            if (dev_output == NULL)
                error = CL_OUT_OF_HOST_MEMORY;
            else {
                for (i = 0; i < platform[chosen_platform].device_len; i++)
                    dev_output[i] = NULL;
                error = CL_SUCCESS;
            }
        } else if (program_step == PS_OBUFFER_PREPARE) {
            printf("[info] \t[%d]: Creating output buffers for device(s)\n", program_step);

            for (i = 0; i < platform[chosen_platform].device_len; i++)
                if (error == CL_SUCCESS)
                    dev_output[i] = clCreateBuffer(context[i],
                                                   CL_MEM_WRITE_ONLY,
                                                   sizeof(cl_uint) * subranges_per_compute_unit * platform[chosen_platform].device[i].max_compute_units,
                                                   NULL,
                                                   &error);
        } else if (program_step == PS_ADD_ARGUMENTS) {
            printf("[info] \t[%d]: Creating output buffers for device(s)\n", program_step);

            for (i = 0; i < platform[chosen_platform].device_len; i++) {
                if (error == CL_SUCCESS) {
                    if (error == CL_SUCCESS)
                        error = clSetKernelArg(kernel[i],
                                               0,
                                               sizeof(cl_mem),
                                               &(dev_byte_stream[i]));
                    if (error == CL_SUCCESS)
                        error = clSetKernelArg(kernel[i],
                                               1,
                                               sizeof(cl_mem),
                                               &(dev_output[i]));
                    if (error == CL_SUCCESS)
                        error = clSetKernelArg(kernel[i],
                                               2,
                                               sizeof(cl_uint),
                                               &word_size);
                    if (error == CL_SUCCESS)
                        error = clSetKernelArg(kernel[i],
                                               3,
                                               sizeof(cl_uint),
                                               &bytes_per_subrange);
                    if (error == CL_SUCCESS)
                        error = clSetKernelArg(kernel[i],
                                               4,
                                               sizeof(cl_uint),
                                               &top_value);
                    if (error == CL_SUCCESS)
                        error = clSetKernelArg(kernel[i],
                                               5,
                                               sizeof(cl_uint),
                                               &bot_value);
                }
            }
        } else if (program_step == PS_RUN_KERNEL) {
             printf("[info] \t[%d]: Running kernels\n", program_step);

            for (i = 0; i < platform[chosen_platform].device_len; i++) {
                global_work_size_proxy = platform[chosen_platform].device[i].max_compute_units * subranges_per_compute_unit;

                if (error == CL_SUCCESS)
                    error = clEnqueueNDRangeKernel(queue[i],
                                                   kernel[i],
                                                   1,
                                                   NULL,
                                                   (const size_t*) &global_work_size_proxy,
                                                   NULL,
                                                   0,
                                                   NULL,
                                                   NULL);
            }

            if (error == CL_SUCCESS)
                for (i = 0; i < platform[chosen_platform].device_len; i++)
                        clFinish(queue[i]);
        } else if (program_step = PS_ENQUEUE_RESULTS) {
            printf("[info] \t[%d]: Enqueueing results from device(s)\n", program_step);
            results = (cl_uint *) malloc(sizeof(cl_uint) * subrange_count);

            if (results == NULL)
                error = CL_OUT_OF_HOST_MEMORY;
            else {
                for (i = 0; i < subrange_count; i++)
                    results[i] = NULL;

                j = 0;

                for (i = 0; i < platform[chosen_platform].device_len; i++) {
                    if (error == CL_SUCCESS) {
                        error = clEnqueueReadBuffer(queue[i],
                                                    dev_output[i],
                                                    CL_TRUE,
                                                    0,
                                                    sizeof(cl_uint) * subranges_per_compute_unit * platform[chosen_platform].device[i].max_compute_units,
                                                    results + j,
                                                    0,
                                                    NULL,
                                                    NULL);
                        j += (subranges_per_compute_unit * platform[chosen_platform].device[i].max_compute_units);
                    }
                }
            }
        }

        program_step++;
    }

    if(program_step == PS_MAX) {
        printf("[info] \t[%d]: Finally on finish...\n", program_step);
        printf("[info] \t[%d]: Received results: \n", program_step);
        for (i = 0; i < subrange_count; i++)
            printf("       \t      Subrange %u. = %u/%u\n", i + 1,
                                                            results[i],
                                                            words_per_subrange);
    }

    // visual error handling
    // at this point `error` variable MUST be CL_SUCCESS
    // in other situation we have error in OpenCL operations
    if (error != CL_SUCCESS) {
        printf("[error]\t[%d]: ", program_step - 1);
        printf(error_message[program_step - 1], error);
    }

    // cleaning after execution
    if (program_step - 1 >= PS_ENQUEUE_RESULTS)
        if (results != NULL)
            free(results);

    if (program_step - 1 >= PS_OBUFFER_PREPARE)
        if (dev_output != NULL)
            for (i = 0; i < platform[chosen_platform].device_len; i++)
                if (dev_output[i] != NULL)
                    clReleaseMemObject(dev_output[i]);
    if (program_step - 1 >= PS_OBUFFER_ALOCATION)
        if (dev_output != NULL)
            free(dev_output);

    if (program_step - 1 >= PS_IBUFFER_PREPARE)
        if (dev_byte_stream != NULL)
            for (i = 0; i < platform[chosen_platform].device_len; i++)
                if (dev_byte_stream[i] != NULL)
                    clReleaseMemObject(dev_byte_stream[i]);
    if (program_step - 1 >= PS_IBUFFER_ALOCATION)
        if (dev_byte_stream != NULL)
            free(dev_byte_stream);

    if (program_step - 1 >= PS_KERNEL_CREATE)
        if (kernel != NULL)
            for (i = 0; i < platform[chosen_platform].device_len; i++)
                if (kernel[i] != NULL)
                    clReleaseKernel(kernel[i]);
    if (program_step - 1 >= PS_KERNEL_ALOCATION)
        if (kernel != NULL)
            free(kernel);

    if (program_step - 1 >= PS_PROGRAM_CREATE)
        if (program != NULL)
            for (i = 0; i < platform[chosen_platform].device_len; i++)
                if (program[i] != NULL)
                    clReleaseProgram(program[i]);

    if (program_step - 1 >= PS_PROGRAM_ALOCATION)
        if (program != NULL)
            free(program);

    if (program_step - 1 >= PS_CREATE_QUEUE)
        if (queue != NULL)
            for (i = 0; i < platform[chosen_platform].device_len; i++)
                if (queue[i] != NULL)
                    clRetainCommandQueue(queue[i]);

    if (program_step - 1 >= PS_QUEUE_ALOCATION)
        if (queue != NULL)
            free(queue);

    if (program_step - 1 >= PS_CREATE_CONTEXT)
        if (context != NULL)
            for (i = 0; i < platform[chosen_platform].device_len; i++)
                if (context[i] != NULL)
                    clRetainContext(context[i]);

    if (program_step - 1 >= PS_CONTEXT_ALOCATION)
        if (context != NULL)
            free(context);

    if (program_step - 1 >= PS_READ_KERNEL)
        if (s_kernel != NULL)
            free(s_kernel);

    if (program_step - 1 >= PS_INITIALIZE_PLATFORM_STRUCT)
        if (platform != NULL)
            for (i = 0; i < platforms_count; i++)
                mlclDropPlatform(&(platform[i]));

    return error;
}
