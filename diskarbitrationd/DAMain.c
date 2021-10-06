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

#include "DAMain.h"

#include "DABase.h"
#include "DADialog.h"
#include "DADisk.h"
#include "DAFileSystem.h"
#include "DAInternal.h"
#include "DALog.h"
#include "DAServer.h"
#include "DASession.h"
#include "DAStage.h"
#include "DASupport.h"
#include "DAThread.h"

#include <assert.h>
#include <dirent.h>
#include <libgen.h>
#include <notify.h>
#include <signal.h>
#include <sysexits.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/storage/IOMedia.h>
#include <SystemConfiguration/SystemConfiguration.h>

static const char * __kDAMainDaemonCookie = "___daemon()";

static SCDynamicStoreRef     __gDAConfigurationPort = NULL;
static Boolean               __gDAMainRendezvous    = FALSE;
static CFMachPortRef         __gDANotifyPort        = NULL;
static Boolean               __gDAOptionDebug       = FALSE;
///w:start
Boolean _gDAAuthorize = TRUE;
///w:stop

const char * kDAMainMountPointFolder           = "/Volumes";
const char * kDAMainMountPointFolderCookieFile = ".autodiskmounted";

CFStringRef            gDAConsoleUser                  = NULL;
gid_t                  gDAConsoleUserGID               = 0;
uid_t                  gDAConsoleUserUID               = 0;
CFMutableArrayRef      gDADiskList                     = NULL;
CFMutableArrayRef      gDAFileSystemList               = NULL;
CFMutableArrayRef      gDAFileSystemProbeList          = NULL;
Boolean                gDAIdle                         = TRUE;
io_iterator_t          gDAMediaAppearedNotification    = NULL;
io_iterator_t          gDAMediaDisappearedNotification = NULL;
IONotificationPortRef  gDAMediaPort                    = NULL;
CFMutableArrayRef      gDAMountMapList1                = NULL;
CFMutableArrayRef      gDAMountMapList2                = NULL;
CFMutableDictionaryRef gDAPreferenceList               = NULL;
pid_t                  gDAProcessID                    = NULL;
char *                 gDAProcessName                  = NULL;
char *                 gDAProcessNameID                = NULL;
CFMutableArrayRef      gDARequestList                  = NULL;
CFMutableArrayRef      gDAResponseList                 = NULL;
CFMutableArrayRef      gDASessionList                  = NULL;
CFMutableDictionaryRef gDAUnitList                     = NULL;

static void __rendezvous( int signal )
{
    /*
     * fprintf( stderr, "%s: started.\n", gDAProcessName );
     */

    _exit( EX_OK );
}

static void __usage( void )
{
    /*
     * Print usage.
     */

    fprintf( stderr, "%s: [-d]\n", gDAProcessName );
    fprintf( stderr, "options:\n" );
    fprintf( stderr, "\t-d\tenable debugging\n" );

    exit( EX_USAGE );
}

static Boolean __DAMainCreateMountPointFolder( void )
{
    /*
     * Create the mount point folder in which our mounts will be made.
     */

    struct stat status;
    Boolean     success;

    /*
     * Determine whether the mount point folder exists.
     */

    if ( stat( kDAMainMountPointFolder, &status ) )
    {
        /*
         * Create the mount point folder.
         */

        success = mkdir( kDAMainMountPointFolder, 01777 ) ? FALSE : TRUE;

        if ( success )
        {
            /*
             * Correct the mount point folder's mode.
             */

            chmod( kDAMainMountPointFolder, 01777 );

            /*
             * Correct the mount point folder's ownership.
             */

            chown( kDAMainMountPointFolder, ___UID_ROOT, ___GID_ADMIN );
        }
    }
    else
    {
        /*
         * Determine whether the mount point folder is a folder.
         */

        success = S_ISDIR( status.st_mode ) ? TRUE : FALSE;

        if ( success )
        {
            DIR * folder;

            /*
             * Correct the mount point folder's mode.
             */

            if ( ( status.st_mode & 01777 ) != 01777 )
            {
                chmod( kDAMainMountPointFolder, 01777 );
            }

            /*
             * Correct the mount point folder's ownership.
             */

            if ( status.st_uid != ___UID_ROOT )
            {
                chown( kDAMainMountPointFolder, ___UID_ROOT, -1 );
            }

            if ( status.st_gid != ___GID_ADMIN )
            {
                chown( kDAMainMountPointFolder, -1, ___GID_ADMIN );
            }

            /*
             * Correct the mount point folder's contents.
             */

            folder = opendir( kDAMainMountPointFolder );

            if ( folder )
            {
                struct dirent * item;

                while ( ( item = readdir( folder ) ) )
                {
                    char path[MAXPATHLEN];

                    if ( item->d_type == DT_DIR )
                    {
                        /*
                         * Determine whether the mount point cookie file exists.
                         */

                        strcpy( path, kDAMainMountPointFolder );
                        strcat( path, "/" );
                        strcat( path, item->d_name );
                        strcat( path, "/" );
                        strcat( path, kDAMainMountPointFolderCookieFile );

                        if ( stat( path, &status ) == 0 )
                        {
                            /*
                             * Remove the mount point cookie file.
                             */

                            unlink( path );
                        }

                        /*
                         * Remove the mount point.
                         */

                        rmdir( dirname( path ) );
                    }
                    else if ( item->d_type == DT_LNK )
                    {
                        /*
                         * Remove the link.
                         */

                        strcpy( path, kDAMainMountPointFolder );
                        strcat( path, "/" );
                        strcat( path, item->d_name );

                        unlink( path );
                    }
                }

                closedir( folder );
            }
        }
    }

    return success;
}

static void __DAMain( void )
{
    FILE *             file;
    CFStringRef        key;
    CFMutableArrayRef  keys;
    char               path[MAXPATHLEN];
    mach_port_t        port;
    CFRunLoopSourceRef source;
    int                token;

    /*
     * Initialize classes.
     */

    DADiskInitialize( );

    DAFileSystemInitialize( );

    DASessionInitialize( );

    /*
     * Initialize components.
     */

    DADialogInitialize( );

    /*
     * Initialize console user.
     */

    gDAConsoleUser = SCDynamicStoreCopyConsoleUser( NULL, &gDAConsoleUserUID, &gDAConsoleUserGID );

    /*
     * Initialize log.
     */

    DALogOpen( gDAProcessName, __gDAOptionDebug, TRUE, TRUE );

    /*
     * Initialize process ID.
     */

    gDAProcessID = getpid( );

    /*
     * Initialize process ID tag.
     */

    asprintf( &gDAProcessNameID, "%s [%d]", gDAProcessName, gDAProcessID );

    assert( gDAProcessNameID );

    /*
     * Create the disk list.
     */

    gDADiskList = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );

    assert( gDADiskList );

    /*
     * Create the file system list.
     */

    gDAFileSystemList = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );

    assert( gDAFileSystemList );

    /*
     * Create the file system probe list.
     */

    gDAFileSystemProbeList = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );

    assert( gDAFileSystemProbeList );

    /*
     * Create the mount map list.
     */

    gDAMountMapList1 = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );

    assert( gDAMountMapList1 );

    /*
     * Create the mount map list.
     */

    gDAMountMapList2 = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );

    assert( gDAMountMapList2 );

    /*
     * Create the preference list.
     */

    gDAPreferenceList = CFDictionaryCreateMutable( kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );

    assert( gDAPreferenceList );

    /*
     * Create the request list.
     */

    gDARequestList = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );

    assert( gDARequestList );

    /*
     * Create the response list.
     */

    gDAResponseList = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );

    assert( gDAResponseList );

    /*
     * Create the session list.
     */

    gDASessionList = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );

    assert( gDASessionList );

    /*
     * Create the unit list.
     */

    gDAUnitList = CFDictionaryCreateMutable( kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );

    assert( gDAUnitList );

    /*
     * Create the Disk Arbitration master run loop source.
     */

    source = DAServerCreateRunLoopSource( kCFAllocatorDefault, 0 );

    if ( source == NULL )
    {
        DALogError( "could not create Disk Arbitration master port." );
        exit( EX_SOFTWARE );
    }

    CFRunLoopAddSource( CFRunLoopGetCurrent( ), source, kCFRunLoopDefaultMode );

    CFRelease( source );

    /*
     * Create the BSD notification run loop source.
     */

    __gDANotifyPort = CFMachPortCreate( kCFAllocatorDefault, _DANotifyCallback, NULL, NULL );

    if ( __gDANotifyPort == NULL )
    {
        DALogError( "could not create BSD notification port." );
        exit( EX_SOFTWARE );
    }

    source = CFMachPortCreateRunLoopSource( kCFAllocatorDefault, __gDANotifyPort, 0 );

    if ( source == NULL )
    {
        DALogError( "could not create BSD notification run loop source." );
        exit( EX_SOFTWARE );
    }

    CFRunLoopAddSource( CFRunLoopGetCurrent( ), source, kCFRunLoopDefaultMode );

    CFRelease( source );

    /*
     * Create the I/O Kit notification run loop source.
     */

    gDAMediaPort = IONotificationPortCreate( kIOMasterPortDefault );

    if ( gDAMediaPort == NULL )
    {
        DALogError( "could not create I/O Kit notification port." );
        exit( EX_SOFTWARE );
    }

    source = IONotificationPortGetRunLoopSource( gDAMediaPort ),

    CFRunLoopAddSource( CFRunLoopGetCurrent( ), source, kCFRunLoopDefaultMode );

    /*
     * Create the System Configuration notification run loop source.
     */

    __gDAConfigurationPort = SCDynamicStoreCreate( kCFAllocatorDefault, CFSTR( _kDAServiceName ), _DAConfigurationCallback, NULL );

    if ( __gDAConfigurationPort == NULL )
    {
        DALogError( "could not create System Configuration notification port." );
        exit( EX_SOFTWARE );
    }

    source = SCDynamicStoreCreateRunLoopSource( kCFAllocatorDefault, __gDAConfigurationPort, 0 );

    if ( source == NULL )
    {
        DALogError( "could not create System Configuration notification run loop source." );
        exit( EX_SOFTWARE );
    }

    CFRunLoopAddSource( CFRunLoopGetCurrent( ), source, kCFRunLoopDefaultMode );

    CFRelease( source );

    /*
     * Create the file system run loop source.
     */

    source = DAFileSystemCreateRunLoopSource( kCFAllocatorDefault, 0 );

    if ( source == NULL )
    {
        DALogError( "could not create file system run loop source." );
        exit( EX_SOFTWARE );
    }

    CFRunLoopAddSource( CFRunLoopGetCurrent( ), source, kCFRunLoopDefaultMode );

    CFRelease( source );

    /*
     * Create the stage run loop source.
     */

    source = DAStageCreateRunLoopSource( kCFAllocatorDefault, 0 );

    if ( source == NULL )
    {
        DALogError( "could not create stage run loop source." );
        exit( EX_SOFTWARE );
    }

    CFRunLoopAddSource( CFRunLoopGetCurrent( ), source, kCFRunLoopDefaultMode );

    CFRelease( source );

    /*
     * Create the thread run loop source.
     */

    source = DAThreadCreateRunLoopSource( kCFAllocatorDefault, 0 );

    if ( source == NULL )
    {
        DALogError( "could not create thread run loop source." );
        exit( EX_SOFTWARE );
    }

    CFRunLoopAddSource( CFRunLoopGetCurrent( ), source, kCFRunLoopDefaultMode );

    CFRelease( source );

    /*
     * Create the "media disappeared" notification.
     */

    IOServiceAddMatchingNotification( gDAMediaPort,
                                      kIOTerminatedNotification,
                                      IOServiceMatching( kIOMediaClass ),
                                      _DAMediaDisappearedCallback,
                                      NULL,
                                      &gDAMediaDisappearedNotification );

    if ( gDAMediaDisappearedNotification == NULL )
    {
        DALogError( "could not create \"media disappeared\" notification." );
        exit( EX_SOFTWARE );
    }

    /*
     * Create the "media appeared" notification.
     */

    IOServiceAddMatchingNotification( gDAMediaPort,
                                      kIOMatchedNotification,
                                      IOServiceMatching( kIOMediaClass ),
                                      _DAMediaAppearedCallback,
                                      NULL,
                                      &gDAMediaAppearedNotification );

    if ( gDAMediaAppearedNotification == NULL )
    {
        DALogError( "could not create \"media appeared\" notification." );
        exit( EX_SOFTWARE );
    }

    /*
     * Create the "configuration changed" notification.
     */

    key  = SCDynamicStoreKeyCreateConsoleUser( kCFAllocatorDefault );
    keys = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );

    assert( key  );
    assert( keys );

    CFArrayAppendValue( keys, key );

    if ( SCDynamicStoreSetNotificationKeys( __gDAConfigurationPort, keys, NULL ) == FALSE )
    {
        DALogError( "could not create \"configuration changed\" notification." );
        exit( EX_SOFTWARE );
    }

    CFRelease( key  );
    CFRelease( keys );

    /*
     * Create the "file system unmounted" notification.
     */

    port = CFMachPortGetPort( __gDANotifyPort );

    if ( notify_register_mach_port( "com.apple.system.kernel.unmount", &port, NOTIFY_REUSE, &token ) )
    {
        DALogError( "could not create \"file system unmounted\" notification." );
        exit( EX_SOFTWARE );
    }

    /*
     * Create the mount point folder.
     */

    if ( __DAMainCreateMountPointFolder( ) == FALSE )
    {
        DALogError( "could not create mount point folder." );
        exit( EX_SOFTWARE );
    }

    /*
     * Create the process ID file.
     */

    sprintf( path, "/var/run/%s.pid", gDAProcessName );

    file = fopen( path, "w" );

    if ( file )
    {
        fprintf( file, "%d\n", gDAProcessID );
        fclose( file );
    }

///w:start
{
    struct stat status;

    if ( stat( "/etc/rc.cdrom", &status ) == 0 )
    {
        if ( stat( "/System/Installation", &status ) == 0 )
        {
            _gDAAuthorize = FALSE;
        }
    }
}
///w:stop
    /*
     * Announce our arrival in the debug log.
     */

    DALogDebug( "" );
    DALogDebug( "server has been started." );

    if ( gDAConsoleUser )
    {
        DALogDebug( "  console user = %@ [%d].", gDAConsoleUser, gDAConsoleUserUID );
    }
    else
    {
        DALogDebug( "  console user = none." );
    }

    /*
     * Freshen the file system list.
     */

    DAFileSystemListRefresh( );

    /*
     * Freshen the mount map list.
     */

    DAMountMapListRefresh1( );

    /*
     * Freshen the mount map list.
     */

    DAMountMapListRefresh2( );

    /*
     * Freshen the preference list.
     */

    DAPreferenceListRefresh( );

    /*
     * Process the initial set of media objects in I/O Kit.
     */

    _DAMediaDisappearedCallback( NULL, gDAMediaDisappearedNotification );

    _DAMediaAppearedCallback( NULL, gDAMediaAppearedNotification );

    /*
     * Start the server.
     */

    CFRunLoopRun( );
}

int main( int argc, char * argv[], char * envp[] )
{
    /*
     * Start.
     */

    Boolean        daemonize;
    char           option;
    DAServerStatus status;

    /*
     * Initialize.
     */

    gDAProcessName = basename( argv[0] );

    /*
     * Check credentials.
     */

    if ( getuid( ) )
    {
        fprintf( stderr, "%s: permission denied.\n", gDAProcessName );

        exit( EX_NOPERM );
	}

    /*
     * Process arguments.
     */

    daemonize = TRUE;

    while ( ( option = getopt( argc, argv, "d" ) ) != -1 )
    {
        switch ( option )
        {
            case 'd':
            {
                __gDAOptionDebug = TRUE;

                daemonize = FALSE;

                break;
            }
            default:
            {
                __usage( );

                break;
            }
        }
    }

    /*
     * Determine whether Disk Arbitration is active.
     */

    status = DAServerInitialize( );

    switch ( status )
    {
        case kDAServerStatusActive:
        {
            fprintf( stderr, "%s: server is already active.\n", gDAProcessName );

            exit( EX_UNAVAILABLE );
        }
        case kDAServerStatusInitialize:
        {
            daemonize = FALSE;

            break;
        }
    }

    /*
     * Daemonize.  Wait for the daemonized process to send us a signal before we exit.  We
     * re-execute ourselves to ensure our frameworks are re-initialized, as some resources
     * do not survive the fork, without their knowledge.
     */

    if ( daemonize )
    {
        __gDAMainRendezvous = TRUE;

        if ( getenv( __kDAMainDaemonCookie ) == NULL )
        {
            pid_t daemonPID;

            signal( SIGTERM, __rendezvous );

            daemonPID = ___daemon( 1, 0 );

            if ( daemonPID )
            {
                /*
                 * Parent.
                 */

                if ( daemonPID > 0 )
                {
                    int status;

                    /*
                     * Wait for child.
                     */

                    waitpid( daemonPID, &status, 0 );

                    fprintf( stderr, "%s: could not start up.\n", gDAProcessName );

                    exit( WIFEXITED( status ) ? ( ( char ) WEXITSTATUS( status ) ) : status );
                }
                else
                {
                    fprintf( stderr, "%s: could not daemonize.\n", gDAProcessName );

                    exit( EX_OSERR );
                }
            }
            else
            {
                /*
                 * Child.
                 */

                setenv( __kDAMainDaemonCookie, __kDAMainDaemonCookie, 1 );

                signal( SIGTERM, SIG_DFL );

                execvp( argv[0], argv );

                exit( EX_OSERR );
            }
        }
    }

    /*
     * Continue to start up.
     */

    __DAMain( );

    exit( EX_OK );
}

void DAMainRendezvous( void )
{
    /*
     * Sends parent a signal to let it know to proceed with exit to its controlling terminal.
     */

    if ( __gDAMainRendezvous )
    {
        kill( getppid( ), SIGTERM );

        __gDAMainRendezvous = FALSE;
    }
}
