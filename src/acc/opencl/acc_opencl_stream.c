/*------------------------------------------------------------------------------------------------*
 * Copyright (C) by the DBCSR developers group - All rights reserved                              *
 * This file is part of the DBCSR library.                                                        *
 *                                                                                                *
 * For information on the license, see the LICENSE file.                                          *
 * For further information please visit https://dbcsr.cp2k.org                                    *
 * SPDX-License-Identifier: GPL-2.0+                                                              *
 *------------------------------------------------------------------------------------------------*/
#if defined(__OPENCL)
#include "acc_opencl.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#if defined(_OPENMP)
# include <omp.h>
#endif

#if defined(CL_VERSION_2_0)
# define ACC_OPENCL_CREATE_COMMAND_QUEUE(CTX, DEV, PROPS, RESULT) \
    clCreateCommandQueueWithProperties(CTX, DEV, PROPS, RESULT)
#else
# define ACC_OPENCL_CREATE_COMMAND_QUEUE(CTX, DEV, PROPS, RESULT) \
    clCreateCommandQueue(CTX, DEV, (cl_command_queue_properties) \
      (NULL != (PROPS) ? ((PROPS)[1]) : 0), RESULT)
#endif

#if defined(CL_VERSION_1_2)
# define ACC_OPENCL_WAIT_EVENT(QUEUE, EVENT) clEnqueueMarkerWithWaitList(QUEUE, 1, EVENT, NULL)
#else
# define ACC_OPENCL_WAIT_EVENT(QUEUE, EVENT) clEnqueueWaitForEvents(QUEUE, 1, EVENT)
#endif


#if defined(__cplusplus)
extern "C" {
#endif

int c_dbcsr_acc_opencl_stream_counter;


int c_dbcsr_acc_opencl_stream_create(int thread_id, cl_command_queue* stream_p,
  const char* name, const ACC_OPENCL_COMMAND_QUEUE_PROPERTIES* properties)
{
  const cl_context context = c_dbcsr_acc_opencl_config.contexts[thread_id];
  cl_int result = (NULL != context ? EXIT_SUCCESS : EXIT_FAILURE);
  assert(NULL != stream_p);
  if (EXIT_SUCCESS == result) {
    cl_command_queue *const streams = c_dbcsr_acc_opencl_config.streams
      + ACC_OPENCL_STREAMS_MAXCOUNT * thread_id;
    int i = 0;
    for (; i < ACC_OPENCL_STREAMS_MAXCOUNT; ++i) if (NULL == streams[i]) break;
    if (i < ACC_OPENCL_STREAMS_MAXCOUNT) { /* register stream */
      cl_device_id device = NULL;
      result = clGetContextInfo(context, CL_CONTEXT_DEVICES,
        sizeof(cl_device_id), &device, NULL);
      if (CL_SUCCESS == result) {
        streams[i] = ACC_OPENCL_CREATE_COMMAND_QUEUE(context, device, properties, &result);
        ++c_dbcsr_acc_opencl_config.stream_stats[thread_id];
        assert(CL_SUCCESS == result || NULL == streams[i]);
      }
      *stream_p = streams[i];
    }
    else {
      result = EXIT_FAILURE;
      *stream_p = NULL;
    }
  }
  else *stream_p = NULL;
  ACC_OPENCL_RETURN_CAUSE(result, name);
}


int c_dbcsr_acc_stream_create(void** stream_p, const char* name, int priority)
{
  int result;
  ACC_OPENCL_COMMAND_QUEUE_PROPERTIES properties[8] = {
    CL_QUEUE_PROPERTIES, 0/*placeholder*/,
    0 /* terminator */
  };
  cl_command_queue queue = NULL;
#if !defined(ACC_OPENCL_STREAM_PRIORITIES) || !defined(CL_QUEUE_PRIORITY_KHR)
  LIBXSMM_UNUSED(priority);
#else
  if (0 <= priority) {
    properties[2] = CL_QUEUE_PRIORITY_KHR;
    properties[3] = ((  CL_QUEUE_PRIORITY_HIGH_KHR <= priority
                      && CL_QUEUE_PRIORITY_LOW_KHR >= priority)
      ? priority : CL_QUEUE_PRIORITY_MED_KHR);
    properties[4] = 0; /* terminator */
  }
#endif
  if (3 <= c_dbcsr_acc_opencl_config.verbosity
    || 0 > c_dbcsr_acc_opencl_config.verbosity)
  {
    properties[1] = CL_QUEUE_PROFILING_ENABLE;
  }
#if defined(_OPENMP)
  if (1 < omp_get_num_threads()) {
    int c, tid;
    assert(0 < c_dbcsr_acc_opencl_config.nthreads);
# if (201107/*v3.1*/ <= _OPENMP)
#   pragma omp atomic capture
# else
#   pragma omp critical(c_dbcsr_acc_opencl_stream)
# endif
    c = c_dbcsr_acc_opencl_stream_counter++;
    tid = (c < c_dbcsr_acc_opencl_config.nthreads
      ? c : (c % c_dbcsr_acc_opencl_config.nthreads));
    result = (NULL != c_dbcsr_acc_opencl_config.contexts[tid]
      ? c_dbcsr_acc_opencl_stream_create(tid, &queue, name, properties)
      : EXIT_FAILURE);
  }
  else
#endif
  result = c_dbcsr_acc_opencl_stream_create(0/*master*/,
    &queue, name, properties);
  assert(NULL != stream_p);
  if (EXIT_SUCCESS == result) {
    assert(NULL != queue);
#if defined(ACC_OPENCL_STREAM_NOALLOC)
    assert(sizeof(void*) >= sizeof(cl_command_queue));
    *stream_p = (void*)queue;
#else
    *stream_p = malloc(sizeof(cl_command_queue));
    if (NULL != *stream_p) {
      *(cl_command_queue*)*stream_p = queue;
    }
    else {
      clReleaseCommandQueue(queue);
      result = EXIT_FAILURE;
    }
#endif
  }
  else {
    *stream_p = NULL;
  }
  ACC_OPENCL_RETURN_CAUSE(result, name);
}


int c_dbcsr_acc_stream_destroy(void* stream)
{
  int result = EXIT_SUCCESS;
  if (NULL != stream) {
    const cl_command_queue queue = *ACC_OPENCL_STREAM(stream);
    int tid = 0, i = ACC_OPENCL_STREAMS_MAXCOUNT;
    cl_command_queue* streams = NULL;
    for (; tid < c_dbcsr_acc_opencl_config.nthreads; ++tid) { /* unregister */
      streams = c_dbcsr_acc_opencl_config.streams + ACC_OPENCL_STREAMS_MAXCOUNT * tid;
      for (i = 0; i < ACC_OPENCL_STREAMS_MAXCOUNT; ++i) if (queue == streams[i]) {
        tid = c_dbcsr_acc_opencl_config.nthreads; /* break outer loop */
        break;
      }
    }
    if (i < ACC_OPENCL_STREAMS_MAXCOUNT) {
      const int j = i + 1;
      result = clReleaseCommandQueue(queue);
      assert(NULL != streams);
      streams[i] = NULL;
      /* compacting streams is not thread-safe */
      if (j < ACC_OPENCL_STREAMS_MAXCOUNT && NULL != streams[j]) {
        memmove(streams + i, streams + j,
          sizeof(cl_command_queue) * (ACC_OPENCL_STREAMS_MAXCOUNT - j));
      }
    }
    else {
      ACC_OPENCL_EXPECT(CL_SUCCESS, clReleaseCommandQueue(queue));
      result = EXIT_FAILURE;
    }
#if defined(_OPENMP)
# if (201107/*v3.1*/ <= _OPENMP)
#   pragma omp atomic write
# else
#   pragma omp critical(c_dbcsr_acc_opencl_stream)
# endif
#endif
    c_dbcsr_acc_opencl_stream_counter = 0; /* reset */
#if defined(ACC_OPENCL_STREAM_NOALLOC)
    assert(sizeof(void*) >= sizeof(cl_command_queue));
#else
    free(stream);
#endif
  }
  ACC_OPENCL_RETURN(result);
}


int c_dbcsr_acc_stream_priority_range(int* least, int* greatest)
{
  int result = ((NULL != least || NULL != greatest) ? EXIT_SUCCESS : EXIT_FAILURE);
#if defined(ACC_OPENCL_STREAM_PRIORITIES) && defined(CL_QUEUE_PRIORITY_KHR)
  char buffer[ACC_OPENCL_BUFFERSIZE];
  cl_platform_id platform = NULL;
  cl_device_id active_id = NULL;
  assert(0 < c_dbcsr_acc_opencl_config.ndevices);
  if (EXIT_SUCCESS == result) {
#if defined(_OPENMP)
    const int tid = omp_get_thread_num();
#else
    const int tid = 0; /*master*/
#endif
    result = c_dbcsr_acc_opencl_device(tid, &active_id);
  }
  ACC_OPENCL_CHECK(clGetDeviceInfo(active_id, CL_DEVICE_PLATFORM,
    sizeof(cl_platform_id), &platform, NULL),
    "retrieve platform associated with active device", result);
  ACC_OPENCL_CHECK(clGetPlatformInfo(platform, CL_PLATFORM_EXTENSIONS,
    ACC_OPENCL_BUFFERSIZE, buffer, NULL),
    "retrieve platform extensions", result);
  if (EXIT_SUCCESS == result) {
    if (NULL != strstr(buffer, "cl_khr_priority_hints")) {
      if (NULL != least) *least = CL_QUEUE_PRIORITY_LOW_KHR;
      if (NULL != greatest) *greatest = CL_QUEUE_PRIORITY_HIGH_KHR;
    }
    else
#endif
    {
      if (NULL != least) *least = -1;
      if (NULL != greatest) *greatest = -1;
    }
#if defined(ACC_OPENCL_STREAM_PRIORITIES) && defined(CL_QUEUE_PRIORITY_KHR)
  }
#endif
  assert(least != greatest); /* no alias */
  ACC_OPENCL_RETURN(result);
}


int c_dbcsr_acc_stream_sync(void* stream)
{
  int result = EXIT_SUCCESS;
  assert(NULL != stream);
  if (0 == (2 & c_dbcsr_acc_opencl_config.flush)) {
    ACC_OPENCL_CHECK(clFinish(*ACC_OPENCL_STREAM(stream)),
      "synchronize stream", result);
  }
  else {
    ACC_OPENCL_CHECK(clFlush(*ACC_OPENCL_STREAM(stream)),
      "synchronize stream", result);
  }
  ACC_OPENCL_RETURN(result);
}


int c_dbcsr_acc_stream_wait_event(void* stream, void* event)
{ /* wait for an event (device-side) */
  int result = EXIT_SUCCESS;
  assert(NULL != stream && NULL != event);
  ACC_OPENCL_CHECK(ACC_OPENCL_WAIT_EVENT(*ACC_OPENCL_STREAM(stream),
    ACC_OPENCL_EVENT(event)), "wait for an event", result);
  ACC_OPENCL_RETURN(result);
}

#if defined(__cplusplus)
}
#endif

#endif /*__OPENCL*/
