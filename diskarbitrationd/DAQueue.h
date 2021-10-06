/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
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

#ifndef __DISKARBITRATIOND_DAQUEUE__
#define __DISKARBITRATIOND_DAQUEUE__

#include <CoreFoundation/CoreFoundation.h>

#include "DADisk.h"
#include "DADissenter.h"
#include "DARequest.h"
#include "DASession.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef void ( *DAResponseCallback )( CFTypeRef response, void * context );

extern Boolean _DAResponseDispatch( CFTypeRef response, SInt32 responseID );

extern void DADiskAppearedCallback( DADiskRef disk );

extern void DADiskClaimReleaseCallback( DADiskRef disk, DACallbackRef callback, DAResponseCallback response, void * responseContext );

extern void DADiskClassicCallback( DADiskRef disk );

extern void DADiskDescriptionChangedCallback( DADiskRef disk, CFTypeRef key );

extern void DADiskDisappearedCallback( DADiskRef disk );

extern void DADiskEject( DADiskRef disk, DACallbackRef callback );

extern void DADiskEjectApprovalCallback( DADiskRef disk, DAResponseCallback response, void * responseContext );

extern void DADiskMount( DADiskRef disk, CFURLRef mountpoint, CFStringRef arguments, DACallbackRef callback );

extern void DADiskMountApprovalCallback( DADiskRef disk, DAResponseCallback response, void * responseContext );

extern void DADiskPeekCallback( DADiskRef disk, DACallbackRef callback, DAResponseCallback response, void * responseContext );

extern void DADiskRefresh( DADiskRef disk, DACallbackRef callback );

extern void DADiskUnmount( DADiskRef disk, DACallbackRef callback );

extern void DADiskUnmountApprovalCallback( DADiskRef disk, DAResponseCallback response, void * responseContext );

extern void DAIdleCallback( void );

extern void DAQueueCallback( DACallbackRef callback, DADiskRef argument0, CFTypeRef argument1 );

extern void DAQueueCallbacks( DASessionRef session, _DACallbackKind kind, DADiskRef argument0, CFTypeRef argument1 );

extern void DAQueueReleaseDisk( DADiskRef disk );

extern void DAQueueReleaseSession( DASessionRef session );

extern void DAQueueRequest( DARequestRef request );

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* !__DISKARBITRATIOND_DAQUEUE__ */
