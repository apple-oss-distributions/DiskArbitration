/*
 * Copyright (c) 1998-2014 Apple Inc. All rights reserved.
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

#include "DADialog.h"

#include "DAAgent.h"

#include <vproc.h>
#include <ApplicationServices/ApplicationServices.h>
#include <CoreFoundation/CFUserNotificationPriv.h>
#include <Foundation/Foundation.h>
#include <Foundation/NSBundle_Private.h>
#include <Foundation/NSUserNotification_Private.h>
#include <os/log.h>
#include <os/transaction_private.h>

static NSString * __kDADialogLocalizedStringBundlePath = @"/System/Library/Frameworks/DiskArbitration.framework";

static NSString * __kDADialogLocalizedStringSingleDeviceRemovalKey      = @"Eject \"%@\" before disconnecting or turning it off.";
static NSString * __kDADialogLocalizedStringMultiDeviceRemovalKey      = @"Eject the disk containing \"%@\" and \"%@\" before disconnecting or turning it off.";
static NSString * __kDADialogLocalizedStringDeviceRemovalTitleKey = @"Disk Not Ejected Properly";

static const CFStringRef __kDADialogTextDeviceUnreadableEject      = CFSTR( "Eject" );
static const CFStringRef __kDADialogTextDeviceUnreadableHeader     = CFSTR( "The disk you attached was not readable by this computer." );
static const CFStringRef __kDADialogTextDeviceUnreadableIgnore     = CFSTR( "Ignore" );
static const CFStringRef __kDADialogTextDeviceUnreadableInitialize = CFSTR( "Initialize..." );

static const CFStringRef __kDADialogTextDeviceUnrepairable             = CFSTR( "You can still open or copy files on the disk, but you can't save changes to files on the disk. Back up the disk and reformat it as soon as you can." );
static const CFStringRef __kDADialogTextDeviceUnrepairableHeaderPrefix = CFSTR( "macOS can't repair the disk \"" );
static const CFStringRef __kDADialogTextDeviceUnrepairableHeaderSuffix = CFSTR( "\"." );

NSString *__DALocalizedStringInBundle(NSString *key, NSBundle *bundle ) {
	NSArray <NSString *> *prefs = [[NSUserDefaults standardUserDefaults] objectForKey:@"AppleLanguages"];
	NSString *localization = [[NSBundle preferredLocalizationsFromArray:[bundle localizations] forPreferences:prefs] objectAtIndex:0];
	return [bundle localizedStringForKey:key value:key table:NULL localization:localization];
}

void DADialogShowDeviceRemoval( CFMutableArrayRef disklist )
{
    NSUserNotificationCenter * center;

    center = [ NSUserNotificationCenter _centerForIdentifier: @_kDAAgentName type: _NSUserNotificationCenterTypeSystem ];

    if ( center )
    {
        NSUserNotification * notification;

        notification = [ [ NSUserNotification alloc ] init ];

        if ( notification )
        {
            NSBundle * bundle;

            bundle = [ NSBundle bundleWithPath: __kDADialogLocalizedStringBundlePath ];

            if ( bundle )
            {
                NSDictionary * description;
                NSMutableString *text1 = [NSMutableString stringWithCapacity:0];
                NSMutableString *text2 = [NSMutableString stringWithCapacity:0];
                notification.title           = __DALocalizedStringInBundle( __kDADialogLocalizedStringDeviceRemovalTitleKey, bundle );
                notification._imageURL       = [ NSURL fileURLWithPath: @"/System/Library/CoreServices/CoreTypes.bundle/Contents/Resources/FinderIcon.icns" ];
                notification._persistent     = FALSE;

                for (CFIndex i = 0; i < CFArrayGetCount( disklist ); i++)
                {

                    DADiskRef disk = ( DADiskRef ) CFArrayGetValueAtIndex( disklist, i );
                    description = ( __bridge_transfer id ) DADiskCopyDescription( disk );

                    if ( description )
                    {
                        NSString * name;

                        name = [ description objectForKey: ( __bridge id ) kDADiskDescriptionVolumeNameKey ];

                        if ( name == NULL )
                        {
                            name = __DALocalizedStringInBundle( @"Untitled", bundle );
                        }

                        notification.hasActionButton = FALSE;
                        NSString *newName = [NSString stringWithFormat:@"%@", name];
                        if ( i != 0 && i !=(  CFArrayGetCount( disklist ) - 1 ))
                        {
                            [text1 appendString:@", "];
                        }
                        if ( i <  CFArrayGetCount( disklist ) - 1 )
                        {
                            [text1 appendString:newName];
                        }
                        else
                        {
                            [text2 appendString:newName];
                        }
                    }
               }
               if ( 1 == CFArrayGetCount( disklist ) )
               {
                   notification.informativeText = [NSString stringWithFormat: __DALocalizedStringInBundle( __kDADialogLocalizedStringSingleDeviceRemovalKey, bundle ), text2 ];
               }
               else
               {
                   notification.informativeText = [NSString stringWithFormat: __DALocalizedStringInBundle( __kDADialogLocalizedStringMultiDeviceRemovalKey, bundle ), text1, text2 ];
               }
               [ center deliverNotification: notification ];
            }
        }
    }
}

void DADialogShowDeviceUnreadable( DADiskRef disk )
{
    CFDictionaryRef description;

    description = DADiskCopyDescription( disk );

    if ( description )
    {
        CFMutableDictionaryRef dictionary;

        dictionary = CFDictionaryCreateMutable( kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );

        if ( dictionary )
        {
            CFURLRef path;

            path = CFURLCreateWithFileSystemPath( kCFAllocatorDefault, ( CFStringRef ) __kDADialogLocalizedStringBundlePath, kCFURLPOSIXPathStyle, TRUE );

            if ( path )
            {
                CFUserNotificationRef notification;

                CFDictionarySetValue( dictionary, kCFUserNotificationAlertHeaderKey,        __kDADialogTextDeviceUnreadableHeader );
                CFDictionarySetValue( dictionary, kCFUserNotificationDefaultButtonTitleKey, __kDADialogTextDeviceUnreadableEject  );
                CFDictionarySetValue( dictionary, kCFUserNotificationLocalizationURLKey,    path                                  );
                CFDictionarySetValue( dictionary, kCFUserNotificationOtherButtonTitleKey,   __kDADialogTextDeviceUnreadableIgnore );

                if ( CFDictionaryGetValue( description, kDADiskDescriptionMediaWritableKey ) == kCFBooleanTrue )
                {
                    CFDictionarySetValue( dictionary, kCFUserNotificationAlternateButtonTitleKey, __kDADialogTextDeviceUnreadableInitialize );
                }

                notification = CFUserNotificationCreate( kCFAllocatorDefault, 0, kCFUserNotificationCautionAlertLevel, NULL, dictionary );

                if ( notification )
                {
                    os_transaction_t transaction;
                    
                    transaction = os_transaction_create( "com.apple.agent.DiskArbitrationAgent" );
                    
                    if ( transaction )
                    {
                        CFRetain( disk );

                        CFRetain( notification );

                        dispatch_async( dispatch_get_global_queue( DISPATCH_QUEUE_PRIORITY_DEFAULT, 0 ), ^
                        {
                            CFOptionFlags response;

                            response = 0;

                            CFUserNotificationReceiveResponse( notification, 0, &response );

                            switch ( ( response & 0x3 ) )
                            {
                                case kCFUserNotificationAlternateResponse:
                                {
                                    CFURLRef path;

                                    path = CFURLCreateWithFileSystemPath( kCFAllocatorDefault, CFSTR( "/System/Applications/Utilities/Disk Utility.app" ), kCFURLPOSIXPathStyle, FALSE );

                                    if ( path )
                                    {
                                        LSOpenCFURLRef( path, NULL );

                                        CFRelease( path );
                                    }

                                    break;
                                }
                                case kCFUserNotificationCancelResponse:
                                case kCFUserNotificationDefaultResponse:
                                {
                                    DADiskEject( disk, kDADiskEjectOptionDefault, NULL, NULL );

                                    break;
                                }
                            }

                            ( void ) transaction;

                            CFRelease( notification );

                            CFRelease( disk );
                        } );
                    }

                    CFRelease( notification );
                }

                CFRelease( path );
            }

            CFRelease( dictionary );
        }

        CFRelease( description );
    }
}

void DADialogShowDeviceUnrepairable( DADiskRef disk )
{
    CFDictionaryRef description;

    description = DADiskCopyDescription( disk );

    if ( description )
    {
        CFMutableArrayRef header;

        header = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );

        if ( header )
        {
            CFMutableDictionaryRef dictionary;
            CFStringRef            name;

            name = CFDictionaryGetValue( description, kDADiskDescriptionVolumeNameKey );

            if ( name == NULL )
            {
                name = CFSTR( "Untitled" );
            }

            CFArrayAppendValue( header, __kDADialogTextDeviceUnrepairableHeaderPrefix );
            CFArrayAppendValue( header, name );
            CFArrayAppendValue( header, __kDADialogTextDeviceUnrepairableHeaderSuffix );

            dictionary = CFDictionaryCreateMutable( kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );

            if ( dictionary )
            {
                CFURLRef path;

                path = CFURLCreateWithFileSystemPath( kCFAllocatorDefault, ( CFStringRef ) __kDADialogLocalizedStringBundlePath, kCFURLPOSIXPathStyle, TRUE );

                if ( path )
                {
                    CFUserNotificationRef notification;

                    CFDictionarySetValue( dictionary, kCFUserNotificationAlertHeaderKey,     header                            );
                    CFDictionarySetValue( dictionary, kCFUserNotificationAlertMessageKey,    __kDADialogTextDeviceUnrepairable );
                    CFDictionarySetValue( dictionary, kCFUserNotificationHelpAnchorKey,      CFSTR( "mh26875" )                );
                    CFDictionarySetValue( dictionary, kCFUserNotificationHelpBookKey,        CFSTR( "com.apple.machelp" )      );
                    CFDictionarySetValue( dictionary, kCFUserNotificationLocalizationURLKey, path                              );

                    notification = CFUserNotificationCreate( kCFAllocatorDefault, 0, kCFUserNotificationStopAlertLevel, NULL, dictionary );

                    if ( notification )
                    {
                        CFRelease( notification );
                    }

                    CFRelease( path );
                }

                CFRelease( dictionary );
            }

            CFRelease( header );
        }

        CFRelease( description );
    }
}
