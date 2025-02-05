/*
 * Copyright (c) 1998-2015 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#ifndef __DISKARBITRATIOND_DAPROBE__
#define __DISKARBITRATIOND_DAPROBE__

#include <CoreFoundation/CoreFoundation.h>

#include "DADisk.h"
#include "DAFileSystem.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef void ( *DAProbeCallback )( int             status,
                                   DAFileSystemRef filesystem,
                                   int             cleanStatus,
                                   CFStringRef     name,
                                   CFStringRef     type,
                                   CFUUIDRef       uuid,
                                   void *          context );

typedef struct __DAProbeCallbackContext __DAProbeCallbackContext;

struct __DAProbeCallbackContext
{
    DAProbeCallback   callback;
    void *            callbackContext;
    CFMutableArrayRef candidates;
    DADiskRef         disk;
    DADiskRef         containerDisk;
    DAFileSystemRef   filesystem;
    uint64_t          startTime;
#ifdef DA_FSKIT
    int               gotFSModules;
#endif
};

extern void DAProbe( DADiskRef       disk,
                     DADiskRef containerDisk,
                     DAProbeCallback callback,
                     void *          callbackContext );

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* !__DISKARBITRATIOND_DAPROBE__ */
