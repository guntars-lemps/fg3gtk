/*********************************************************************************************//**
 *   @file       PortableSerialLib.c
 *
 *   @author     Copyright (c) 2012 René Staffen r.staffen@gmx.de
 *   @version    $Id: PortableSerialLib.c 12 2012-03-09 20:58:40Z Vlad $
 *
 *   @copyright  This program is free software; you can redistribute it
 *               and/or modify it under the terms of the GNU General Public
 *               License as published by the Free Software Foundation;
 *               either version 2 of the License, or (at your option) any
 *               later version.
 *               For a full text of the license look at the enclosed gpl2.txt
 *               or at http://www.gnu.org/licenses/gpl-2.0.html
 *
 *//*********************************************************************************************/


#include "serial_lib.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>

#include <termios.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <limits.h>
#include <errno.h>


static const char *PSerLib_getPlatformSpecificErrorMessage(PSL_ErrorCodes_e i_errorCode);
static const char c_errMsgUnknown[] = "Unknown error code.";

struct serial_struct {
    int type;
    int line;
    unsigned int port;
    int irq;
    int flags;
    int xmit_fifo_size;
    int custom_divisor;
    int baud_base;
    unsigned short close_delay;
    char io_type;
    char reserved_char[1];
    int hub6;
    unsigned short closing_wait; /* time to wait before closing */
    unsigned short closing_wait2; /* no longer used... */
    unsigned char *iomem_base;
    unsigned short iomem_reg_shift;
    unsigned int port_high;
    unsigned long iomap_base; /* cookie passed into ioremap */
};

const char *PSerLib_getErrorMessage(PSL_ErrorCodes_e i_errorCode)
{
    static const char *c_errMsg[] = {
        /* [PSL_ERROR_none               ] */ "No error occurred - everything is fine",
        /* [PSL_ERROR_couldNotOpen       ] */ "Specified port could not be opened.",
        /* [PSL_ERROR_invalidHandle      ] */ "Got invalid Handle as input.",
        /* [PSL_ERROR_unsupportedBitrate ] */ "Selected bit rate is not supported.",
        /* [PSL_ERROR_unsupportedConfig  ] */ "Selected configuration parameters are not supported.",
        /* [PSL_ERROR_configurePort      ] */ "Could not configure port.",
        /* [PSL_ERROR_notYetImplemented  } */ "The desired feature is not implemented yet.",
        /* [PSL_ERROR_bufferToSmall      } */ "The output buffer is to small to receive complete available data.",
    };
    if ((i_errorCode < 0) || (i_errorCode >= sizeof(c_errMsg) / sizeof(*c_errMsg))) {
        return PSerLib_getPlatformSpecificErrorMessage(i_errorCode);
    }
    return c_errMsg[i_errorCode];
}


/* ------------------------------------------------------------------------- */
/* - writing functions                                                     - */
/* ------------------------------------------------------------------------- */

PSL_ErrorCodes_e PSerLib_putc(PSerLibHandle_t io_port, char c)
{
    return PSerLib_writeBinaryData(io_port, (uint8_t*)&c, sizeof(c), NULL);
}


PSL_ErrorCodes_e PSerLib_puts(PSerLibHandle_t io_port, const char *i_str)
{
    const int size = strlen(i_str);
    return PSerLib_writeBinaryData(io_port, (uint8_t*)i_str, size, NULL);
}


/* ------------------------------------------------------------------------- */
/* - reading functions                                                     - */
/* ------------------------------------------------------------------------- */

PSL_ErrorCodes_e PSerLib_readLine(PSerLibHandle_t io_port,
                                  char *o_string,
                                  int i_bufferLength,
                                  const char *i_endLineChars)
{
    char c;
    int endLineCharsCount = strlen(i_endLineChars);
    int endlineFound = 0;
    PSL_ErrorCodes_e ret;
    int i = 0;
    do {
        ret = PSerLib_readBinaryData(io_port, (uint8_t*)(&c), 1, NULL);
        if (ret != PSL_ERROR_none) {
            return ret;
        }
        o_string[i] = c;
        endlineFound = (memchr(i_endLineChars, c, endLineCharsCount) != NULL);
        ++i;
    } while (!endlineFound && (i < (i_bufferLength - 1)));
    o_string[i] = '\0';
    if (!endlineFound) {
        return PSL_ERROR_bufferToSmall;
    } else {
        return PSL_ERROR_none;
    }
}


/* ========================================================================= */
/* = internal port structure                                               = */
/* ========================================================================= */
struct PSerLibHandleInternal_t
{
    int port;
    struct termios oldSettings;
};


/* ========================================================================= */
/* = some look up tables for mapping params to platform dependent constants= */
/* ========================================================================= */
static const int c_baudrates[] = {
    B50, B75, B110, B134, B150, B200, B300, B600, B1200, B1800, B2400, B4800, B9600,
    -1, B19200, -1, B38400, -1, B57600, B115200, -1, B230400, -1, B460800, B500000,
    B576000, B921600, B1000000, B1152000, B1500000, B2000000, B2500000, B3000000,
    B3500000, B4000000};

static const int c_baudConsts2baudrates[] = {
    50, 75, 110, 134, 150, 200, 300, 600, 1200, 1800, 2400, 4800, 9600, 14400,
    19200, 28800, 38400, 56000, 57600, 115200, 128000, 230400, 256000, 460800,
    500000, 576000, 921600, 1000000, 1152000, 1500000, 2000000, 2500000, 3000000,
    3500000, 4000000};

static const int c_parity[] = {
    /* none */ 0,
    /* odd  */ PARENB | PARODD,
    /* even */ PARENB,
    /* mark */ -1,
    /* space*/ -1
};

static const int c_stopbits[] ={
    /* 1   */  0,
    /* 1.5 */ -1,
    /* 2   */  CSTOPB
};

static const int c_databits[] = {CS5, CS6, CS7, CS8};

static const int c_flowcontrol[] = {
    /* PSL_FC_none              */ 0,
    /* PSL_FC_rts_cts           */ CRTSCTS,
    /* PSL_FC_xon_xoff          */ -1,
    /* PSL_FC_rts_cts__xon_xoff */ -1
};


/* ========================================================================= */
/* = helper functions                                                      = */
/* ========================================================================= */

static const char *PSerLib_getPlatformSpecificErrorMessage(PSL_ErrorCodes_e i_errorCode)
{
    static const char *c_errMsg[] = {
        /* [PSL_ERROR_couldNotReadProcTtyFile] = */ "Error opening/reading /proc/tty/drivers.",
        /* [PSL_ERROR_couldParseProcTtyFile  ] = */ "Error parsing /proc/tty/drivers - format unknown.",
        /* [PSL_ERROR_couldNotReadSysClassTty] = */ "Error reading directory /sys/class/tty.",
    };
    int ec = i_errorCode - PSL_ERROR_START_LINUX_SPECIFIC;
    if ((ec < 0) || (ec >= (int)(sizeof(c_errMsg) / sizeof(*c_errMsg)))) {
        return c_errMsgUnknown;
    }
    return c_errMsg[ec];
}


/* ========================================================================= */
/* = interface implementation                                              = */
/* ========================================================================= */

/* ------------------------------------------------------------------------- */
/* - port handling                                                         - */
/* ------------------------------------------------------------------------- */


PSL_ErrorCodes_e PSerLib_open(const char *i_portName, PSerLibHandle_t *o_handle)
{
    struct PSerLibHandleInternal_t pi;
    struct termios settings;
    char nameBuffer[50];
    if (NULL == strchr(i_portName, '/')) {  /* seems not to be a devicepath (some kind like /dev/ttyS0 */
        snprintf(nameBuffer, sizeof(nameBuffer), "/dev/%s", i_portName);
    } else {
        strncpy(nameBuffer, i_portName, sizeof(nameBuffer));
    }

    // configuration options
    // O_RDWR - we need read and write access
    // O_CTTY - prevent other input (like keyboard) from affecting what we read
    // O_NDELAY - We don’t care if the other side is connected (some devices don’t explicitly connect)
    pi.port = open(nameBuffer, O_RDWR | O_NOCTTY | O_NONBLOCK);

    if (pi.port == -1) {
        return PSL_ERROR_couldNotOpen;
    }

    if (tcgetattr(pi.port, &(pi.oldSettings)) == -1) {
        close(pi.port);
        return PSL_ERROR_configurePort;
    }

    //
    // cfo - force no Hangup on close (DTR Low), even if that was the old default
    //
    pi.oldSettings.c_cflag &= ~HUPCL;  // clear the HUPCL bit

    settings = pi.oldSettings;
    settings.c_cc[VMIN]  = 1; // block untill n bytes are received
    settings.c_cc[VTIME] = 0; // no timeout
    if (tcsetattr(pi.port, TCSANOW, &settings) == -1) {
        close(pi.port);
        return PSL_ERROR_configurePort;
    }

    *o_handle = malloc(sizeof(**o_handle));
    *(*o_handle) = pi;

    return PSL_ERROR_none;
}


PSL_ErrorCodes_e PSerLib_setParams(PSerLibHandle_t io_port,
                                   PSL_Baudrates_e i_baudrate,
                                   PSL_NumDataBits_e i_bits,
                                   PSL_Parity_e i_parity,
                                   PSL_StopBits_e i_stopbits,
                                   PSL_FlowControl_e i_flowControl)
{
    struct termios newSettings;
    int speed;
    int bits;
    int stopbits;
    int parity;
    int j;
    for (j = 0; j < (int)(sizeof(c_baudConsts2baudrates) / sizeof(*c_baudConsts2baudrates)); ++j) {
        if (c_baudConsts2baudrates[j] == (int)i_baudrate) {
            i_baudrate = (PSL_Baudrates_e)j;
           break;
        }
    };


    if (io_port == PSL_NOPORT_HANDLE) {
        return PSL_ERROR_invalidHandle;
    }
    if ((i_baudrate >= PSL_BR_CONSTANTS_COUNT) || (c_baudrates[i_baudrate] < 0)) {
        return PSL_ERROR_unsupportedBitrate;
    }

    if ((i_bits >= PSL_DB_CONSTANTS_COUNT) || (i_stopbits >= PSL_SB_CONSTANTS_COUNT) ||
        (i_parity >= PSL_P_CONSTANTS_COUNT) || (i_flowControl >= PSL_FC_CONSTANTS_COUNT) ||
        (c_databits[i_bits] < 0) || (c_stopbits[i_stopbits] < 0) ||
        (c_parity[i_parity] < 0) || (c_flowcontrol[i_flowControl] < 0)) {
        return PSL_ERROR_unsupportedConfig;
    }

    speed = c_baudrates[i_baudrate];
    bits = c_databits[i_bits];
    stopbits = c_databits[i_stopbits];
    parity = c_parity[i_parity];

    memset(&newSettings, 0, sizeof(newSettings));  // clear the new struct

    newSettings.c_cflag = bits | parity | stopbits | CLOCAL | CREAD;
    newSettings.c_iflag = IGNPAR;
    newSettings.c_oflag = 0;
    newSettings.c_lflag = 0;
    newSettings.c_cc[VMIN]  = 1; // block untill n bytes are received
    newSettings.c_cc[VTIME] = 0; // no timeout

    if ((cfsetispeed(&newSettings, speed) < 0) || (cfsetospeed(&newSettings, speed) < 0) ||
        (tcsetattr(io_port->port, TCSANOW, &newSettings) < 0)) {
        return PSL_ERROR_configurePort;
    }

    return PSL_ERROR_none;
}


PSL_ErrorCodes_e PSerLib_close(PSerLibHandle_t *io_portH)
{
    if (io_portH && (*io_portH)) {
        tcsetattr((*io_portH)->port, TCSANOW, &((*io_portH)->oldSettings));
        close((*io_portH)->port);
        free(*io_portH);
        (*io_portH) = PSL_NOPORT_HANDLE;
    }
    return PSL_ERROR_none;
}


// Defining comparator function as per the requirement
static int myCompare(const void* a, const void* b)
{
    // setting up rules for comparison
    if (strlen(*(const char**)a) != strlen(*(const char**)b)) {
        return strlen(*(const char**)a) - strlen(*(const char**)b);
    }
    return strcmp(*(const char**)a, *(const char**)b);
}


// Function to sort the array
void sort(char *arr[], int n)
{
    // calling qsort function to sort the array
    // with the help of Comparator
    qsort(arr, n, sizeof(char*), myCompare);
}


int PSerLib_check_device_real(char *device)
{
    struct serial_struct serinfo;
    int fd;

    if (strncmp(device, "/dev/ttyS", strlen("/dev/ttyS"))) {
        return 1;
    }

    if ((fd = open(device, O_RDWR | O_NONBLOCK)) < 0) {
        fprintf(stderr, "error opening device: %s %s\n", device, strerror(errno));
        return 0;
    }
    serinfo.reserved_char[0] = 0;
    if (ioctl(fd, TIOCGSERIAL, &serinfo) < 0) {
        close(fd);
        return 0;
    }
    close(fd);
    if (serinfo.irq || serinfo.port) {
        return 1;
    }
    return 0;
}


PSL_ErrorCodes_e PSerLib_getAvailablePorts(char **(*o_names), int *o_numPortsFound)
{
    int wasEof;
    FILE *procfile;
    DIR *dir;
    PSL_ErrorCodes_e result = PSL_ERROR_none;

    *o_names = NULL;

    *o_numPortsFound = 0;

    procfile = fopen("/proc/tty/drivers", "r");
    if (procfile == NULL) {
        return PSL_ERROR_couldNotReadProcTtyFile;
    }
    dir = opendir("/sys/class/tty");
    if (dir == NULL) {
        fclose(procfile);
        return PSL_ERROR_couldNotReadSysClassTty;
    }

    char lineBuffer[200];

    while (fgets(lineBuffer, sizeof(lineBuffer), procfile)) {
        if (strstr(lineBuffer, " serial\n") != NULL) { // seems to be a serial device
            char *sp;
            char *deviceName = strstr(lineBuffer, " /dev/"); // search for space to be sure it is the second column
            if (deviceName != NULL) {
                deviceName += 6; // to skip the ' /dev/' at beginning
                sp = strchr(deviceName, ' ');
                if (NULL != sp) {
                    struct dirent *entry;
                    *sp = '\0'; // now we hava a device base name in deviceName
                    rewinddir(dir);
                    while ((entry = readdir(dir)) != NULL) {
                        char *tmpbuf = malloc(sizeof(char) * (strlen(entry->d_name) + 6));
                        sprintf(tmpbuf, "/dev/%s", entry->d_name);
                        if (strstr(entry->d_name, deviceName)) {
                            if (PSerLib_check_device_real(tmpbuf)) {
                                *o_names = realloc(*o_names, sizeof(char*) * ((*o_numPortsFound) + 1));
                                (*o_names)[(*o_numPortsFound)] = malloc(sizeof(char) * (strlen(entry->d_name) + 6));
                                sprintf((*o_names)[(*o_numPortsFound)], "/dev/%s", entry->d_name);
                                (*o_numPortsFound)++;
                            }
                        }
                        free(tmpbuf);
                        tmpbuf = NULL;
                    }
                }
            }
        }
    }
    wasEof = feof(procfile);
    closedir(dir);
    fclose(procfile);

    sort(*o_names, *o_numPortsFound);

    if (!wasEof) { // --> error occurred
        return PSL_ERROR_couldNotReadProcTtyFile;
    }
    return result;
}


/* ------------------------------------------------------------------------- */
/* - writing functions                                                     - */
/* ------------------------------------------------------------------------- */

PSL_ErrorCodes_e PSerLib_writeBinaryData(PSerLibHandle_t io_port,
                                         const uint8_t *i_data,
                                         int i_dataLen,
                                         int *o_bytesWritten)
{
    int wordsWritten;
    if (io_port == PSL_NOPORT_HANDLE){
        return PSL_ERROR_invalidHandle;
    }
    if (o_bytesWritten) {
        *o_bytesWritten = 0;
    }
    wordsWritten = write(io_port->port, i_data, i_dataLen);
    if (wordsWritten > 0) {
        if (o_bytesWritten) {
            *o_bytesWritten = wordsWritten;
        }
        return PSL_ERROR_none;
    } else {
        return PSL_ERROR_writeDataFailed;
    }
}


/* ------------------------------------------------------------------------- */
/* - reading functions                                                     - */
/* ------------------------------------------------------------------------- */

PSL_ErrorCodes_e PSerLib_readBinaryData(PSerLibHandle_t io_port,
                                        uint8_t *o_data,
                                        int i_dataToRead,
                                        int *o_bytesRead)
{
    int dataToRead = i_dataToRead;
    if (io_port == PSL_NOPORT_HANDLE) {
        return PSL_ERROR_invalidHandle;
    }
    if (o_bytesRead) {
        *o_bytesRead = 0;
    }
    for (; dataToRead; --dataToRead) {
        int count = read(io_port->port, o_data, 1);
        if ((count < 0) && (errno == EAGAIN)) {
            // there is no data to be read
            return PSL_ERROR_none;
        }
        if (o_bytesRead) {
            ++(*o_bytesRead);
        }
        ++o_data;
    }
    return PSL_ERROR_none;
}


