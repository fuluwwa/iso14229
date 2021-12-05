
/**
 * @brief maximum allowable number of user defined 0x31 RoutineControl routines
 * The number of routines in theory ranges from [0-0xFFFF]
 */
#ifndef ISO14229_USER_DEFINED_MAX_ROUTINES
#define ISO14229_USER_DEFINED_MAX_ROUTINES 10
#endif

/**
 * @brief maximum allowable number of download handlers
 *
 */
#ifndef ISO14229_USER_DEFINED_MAX_DOWNLOAD_HANDLERS
#define ISO14229_USER_DEFINED_MAX_DOWNLOAD_HANDLERS 1
#endif

/*
The iso14229 server must delay sending an outgoing response for up to p2
milliseconds. Outgoing responses go in a buffer of this size until p2 elapses.

This extraneous buffer can be removed by using the one in the ISO-TP layer
but I didn't see an obvious way of doing that.
*/
#define ISO14229_TPORT_SEND_BUFSIZE 255

/*
provide a debug function with -DISO14229USERDEBUG=printf when compiling this
library
*/
#ifndef ISO14229USERDEBUG
#define ISO14229USERDEBUG(fmt, ...) ((void)fmt)
#endif