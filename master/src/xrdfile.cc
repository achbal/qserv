/* 
 * LSST Data Management System
 * Copyright 2008, 2009, 2010 LSST Corporation.
 * 
 * This product includes software developed by the
 * LSST Project (http://www.lsst.org/).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the LSST License Statement and 
 * the GNU General Public License along with this program.  If not, 
 * see <http://www.lsstcorp.org/LegalNotices/>.
 */
// xrdfile.h -- Wrapper for xrootd client API functions
 
//#define FAKE_XRD 1
#include <iostream>
#include <string>
#include <sstream>

// Boost
#include <boost/thread/thread.hpp>

#ifdef FAKE_XRD

#else
#include "XrdPosix/XrdPosixLinkage.hh"
#include "XrdPosix/XrdPosixExtern.hh"
#include "XrdPosix/XrdPosixXrootd.hh"
#include "XrdClient/XrdClientConst.hh"
#include "XrdClient/XrdClientEnv.hh"
#include <limits>
#include <fcntl.h>
#include <assert.h>
#endif

#include "lsst/qserv/master/xrdfile.h"
#include "lsst/qserv/Logger.h"

namespace qMaster = lsst::qserv::master;

// Statics
static bool qMasterXrdInitialized = false; // Library initialized?
extern XrdPosixLinkage Xunix;

namespace dbg {
void recordTrans(char const* path, char const* buf, int len) {
    static char traceFile[] = "/dev/shm/xrdTransaction.trace";
    std::string record;
    {
        std::stringstream ss;
        ss << "####" << path << "####" << std::string(buf, len) << "####\n";
        record = ss.str();
    }
    int fd = open(traceFile, O_CREAT|O_WRONLY|O_APPEND);
    if( fd != -1) {
        int res = write(fd, record.data(), record.length());
        close(fd);
    }
}

} // End namespace dbg


#ifdef FAKE_XRD // Fake placeholder implemenation
void qMaster::xrdInit() {}

int qMaster::xrdOpen(const char *path, int oflag) {
    static int fakeDes=50;
    LOGGER_DBG << "xrd openfile " << path << " returning ("
               << fakeDes << ")" << std::endl;
    return fakeDes;
}

long long qMaster::xrdRead(int fildes, void *buf, unsigned long long nbyte) {
    static char fakeResults[] = "This is totally fake.";
    int len=strlen(fakeResults);
    LOGGER_DBG << "xrd read " << fildes << ": faked" << std::endl;
    if(nbyte > static_cast<unsigned long long>(len)) {
        nbyte = len+1;
    }
    memcpy(buf, fakeResults, nbyte);
    return nbyte;
}

long long qMaster::xrdWrite(int fildes, const void *buf, 
                            unsigned long long nbyte) {
    std::string s;
    s.assign(static_cast<const char*>(buf), nbyte);
    LOGGER_DBG << "xrd write (" <<  fildes << ") \"" 
               << s << std::endl;
    return nbyte;
}

int qMaster::xrdClose(int fildes) {
    LOGGER_DBG << "xrd close (" << fildes << ")" << std::endl;
    return 0; // Always pretend to succeed.
}

long long qMaster::xrdLseekSet(int fildes, off_t offset) {
    return offset; // Always pretend to succeed
}

#else // Not faked: choose the real XrdPosix implementation.

void qMaster::xrdInit() {
    const int xrdOpenFiles = 1024*1024*1024; // 1 billion open files
    static int Init = Xunix.Init(&Init);
    // Use non-OS file descriptors
    static XrdPosixXrootd posixXrd(-xrdOpenFiles);
    qMasterXrdInitialized = true;

    // Set timeouts to effectively disable client timeouts.
    //EnvPutInt(NAME_CONNECTTIMEOUT, 3600*24*10); // Don't set this!
    
    // Don't set these for two-file model?
    //EnvPutInt(NAME_REQUESTTIMEOUT, std::numeric_limits<int>::max());
    //    EnvPutInt(NAME_DATASERVERCONN_TTL, std::numeric_limits<int>::max());

    // TRANSACTIONTIMEOUT needs to get extended since it limits how
    // long the client will wait for an open() callback response.
    // Can't set to max, since it gets added to time(), and max would overflow.
    // Set to 3 years.
    EnvPutInt(NAME_TRANSACTIONTIMEOUT, 60*60*24*365*3); 

    // Disable XrdClient read caching.
    EnvPutInt(NAME_READCACHESIZE, 0);

    // Don't need to lengthen load-balancer timeout.??
    //EnvPutInt(NAME_LBSERVERCONN_TTL, std::numeric_limits<int>::max());
}

#define QSM_TIMESTART(name, path) \
    time_t start;\
    time_t finish;\
    char timebuf[30];\
    time(&start);\
    asctime_r(localtime(&start), timebuf);\
    timebuf[strlen(timebuf)-1] = 0;\
    std::cout << timebuf <<" " << name << " "	\
              << path << " in flight\n";
#define QSM_TIMESTOP(name, path) \
    time(&finish);\
    asctime_r(localtime(&finish), timebuf);\
    timebuf[strlen(timebuf)-1] = 0;\
    std::cout << timebuf  << " (" \
              << finish - start \
              << ") " << name << " " \
              << path << " finished.""\n";
#if 1 // Turn off xrd call timing
#undef QSM_TIMESTART
#undef QSM_TIMESTOP
#define QSM_TIMESTART(name, path)
#define QSM_TIMESTOP(name, path)
#endif

int qMaster::xrdOpen(const char *path, int oflag) {
#ifdef DBG_TEST_OPEN_FAILURE_1
    /*************************************************************
     * TEST FAILURE MODE: Intermittent XRD Open for Read Failure
     * ***********************************************************/
    char *altPath = strdup(path);
    if (oflag == O_RDONLY) {
        int coinToss = rand()%10;
        if (coinToss == 0) {
            LOGGER_WRN << "YOU ARE UNLUCKY, SABOTAGING XRD OPEN!!!!" << std::endl;
            return -1;
        } else {
            LOGGER_WRN << "YOU DODGED A BULLET, NO SABOTAGE THIS TIME!!" << std::endl;
        }
    }
    if(!qMasterXrdInitialized) { xrdInit(); }
    char const* abbrev = altPath;  while((abbrev[0] != '\0') && *abbrev++ != '/');
    QSM_TIMESTART("Open", abbrev);
    int res = XrdPosixXrootd::Open(altPath,oflag);
    QSM_TIMESTOP("Open", abbrev);
    return res;
    /*************************************************/
#elif defined DBG_TEST_OPEN_FAILURE_2
    /*************************************************************
     * TEST FAILURE MODE: Delay before XRD Open for Read
     * (Provides time to manually kill worker process for testing
     * chunk-level failure recovery.)
     * ***********************************************************/
    if (oflag == O_RDONLY) {
        std::cout << "DBG: SLEEPING FOR 10 SECONDS" << std::endl;
        boost::this_thread::sleep(boost::posix_time::seconds(10));
    }
    if(!qMasterXrdInitialized) { xrdInit(); }
    char const* abbrev = path;  while((abbrev[0] != '\0') && *abbrev++ != '/');
    QSM_TIMESTART("Open", abbrev);
    int res = XrdPosixXrootd::Open(path,oflag);
    QSM_TIMESTOP("Open", abbrev);
    return res;
    /*************************************************/
#else
    if(!qMasterXrdInitialized) { xrdInit(); }
    char const* abbrev = path;  while((abbrev[0] != '\0') && *abbrev++ != '/');
    QSM_TIMESTART("Open", abbrev);
    //int res = XrdPosix_Open(path, oflag);
    int res = XrdPosixXrootd::Open(path,oflag);
    QSM_TIMESTOP("Open", abbrev);
    return res;
#endif
}

int qMaster::xrdOpenAsync(const char* path, int oflag, XrdPosixCallBack *cbP) {
    if(!qMasterXrdInitialized) { xrdInit(); }
    char const* abbrev = path;  while((abbrev[0] != '\0') && *abbrev++ != '/');
    while((abbrev[0] != '\0') && *abbrev++ != '/');
    while((abbrev[0] != '\0') && *abbrev++ != '/');
    QSM_TIMESTART("OpenAsy", abbrev);
    int res = XrdPosixXrootd::Open(path, oflag, 0, cbP); 
    // not sure what to do with mode, so set to 0 right now.
    QSM_TIMESTOP("OpenAsy", abbrev);
    assert(res == -1);
    return -errno; // Return something that indicates "in progress"
}

long long qMaster::xrdRead(int fildes, void *buf, unsigned long long nbyte) {
    LOGGER_DBG << "xrd trying to read (" <<  fildes << ") "
               << nbyte << " bytes" << std::endl;
    QSM_TIMESTART("Read", fildes);
    long long readCount;
    readCount = XrdPosixXrootd::Read(fildes, buf, nbyte);
    QSM_TIMESTOP("Read", fildes);

#ifdef DBG_TEST_READ_FAILURE_1
    /*************************************************
     * TEST FAILURE MODE: Reading query result fails.
     * ***********************************************/
    LOGGER_WRN << "SABOTAGING XRD READ!!!!" << std::endl;
    readCount = -1;
    /*************************************************/
#endif

#ifdef DBG_TEST_READ_FAILURE_2
    /*************************************************
     * TEST FAILURE MODE: Fuzz testing - simulate incomplete results.
     * ***********************************************/
    LOGGER_WRN << "SABOTAGING XRD READ!!!!" << std::endl;
    LOGGER_WRN << "XrdPosixXrootd::Read() returned: " << readCount << std::endl;
    readCount = rand()%readCount;
    LOGGER_WRN << "Set readCount = " << readCount << std::endl;
    /*************************************************/
#endif

#ifdef DBG_TEST_READ_FAILURE_3
    /*************************************************
     * TEST FAILURE MODE: Fuzz testing - simulate corrupted byte.
     * ***********************************************/
    LOGGER_WRN << "SABOTAGING XRD READ!!!!" << std::endl;
    LOGGER_WRN << "XrdPosixXrootd::Read() returned: " << readCount << std::endl;
    int position = rand()%readCount;
    char value = (char)(rand()%256);
    *((char *)buf + position) = value;
    /*************************************************/
#endif

#ifdef DBG_TEST_READ_FAILURE_4
    /*************************************************
     * TEST FAILURE MODE: Intermittent Read Failure
     * ***********************************************/
    int coinToss = rand()%10;
    if (coinToss == 0) {
        LOGGER_WRN << "YOU ARE UNLUCKY, SABOTAGING XRD READ!!!!" << std::endl;
        readCount = -1;
    } else {
        LOGGER_WRN << "YOU DODGED A BULLET, NO SABOTAGE THIS TIME!!" << std::endl;
    }
    /*************************************************/
#endif

    if (readCount < 0) {
         if (errno == 0) errno = EREMOTEIO;
         return -1;
    }

    return readCount;
}

long long qMaster::xrdWrite(int fildes, const void *buf,
                            unsigned long long nbyte) {
    std::string s;
    s.assign(static_cast<const char*>(buf), nbyte);
    LOGGER_DBG << "xrd write (" <<  fildes << ") \""
               << s << "\"" << std::endl;
    QSM_TIMESTART("Write", fildes);
    long long res = XrdPosixXrootd::Write(fildes, buf, nbyte);
    QSM_TIMESTOP("Write", fildes);

#ifdef DBG_TEST_WRITE_FAILURE_1
    /*************************************************
     * TEST FAILURE MODE: Writing query result fails.
     * ***********************************************/
    LOGGER_WRN << "SABOTAGING XRD WRITE!!!!" << std::endl;
    res = -1;
    /*************************************************/
#endif

    if (res < 0) {
         if (errno == 0) errno = EREMOTEIO;
         return -1;
    }

    return res;
}

int qMaster::xrdClose(int fildes) {
    QSM_TIMESTART("Close", fildes);    
    int result = XrdPosixXrootd::Close(fildes);
    QSM_TIMESTOP("Close", fildes);    
    return result;
}
 
long long qMaster::xrdLseekSet(int fildes, unsigned long long offset) {
    return XrdPosixXrootd::Lseek(fildes, offset, SEEK_SET);
}

int qMaster::xrdReadStr(int fildes, char *buf, int len) {
    return xrdRead(fildes, static_cast<void*>(buf), 
                   static_cast<unsigned long long>(len));
}

std::string qMaster::xrdGetEndpoint(int fildes) {
    // Re: XrdPosixXrootd::endPoint() 
    // "the max you will ever need is 264 bytes"
    const int maxSize=265;
    char buffer[maxSize]; 
    int port = XrdPosixXrootd::endPoint(fildes, buffer, maxSize);
    if(port > 0) { // valid port?
        return std::string(buffer);
    } else {
        return std::string();
    }
}

/// Return codes for writing and reading are written to *write and *read.
/// Both will be attempted as independently as possible.
/// e.g., if writing fails, the read will drain the file into nothingness.
/// If reading fails, writing can still succeed in writing as much as was read.
///
/// @param fildes -- XrdPosix file descriptor
/// @param fragmentSize -- size to grab from xrootd server 
///        (64K <= size <= 100MB; a few megs are good)
/// @param filename -- filename of file that will receive the result 
/// @param abortFlag -- Flag to check to see if we've been aborted.
/// @return write -- How many bytes were written, or -errno (negative errno).
/// @return read -- How many bytes were read, or -errno (negative errno).
void qMaster::xrdReadToLocalFile(int fildes, int fragmentSize, 
                                 const char* filename, 
                                 bool const *abortFlag,
                                 int* write, int* read) {
    size_t bytesRead = 0;
    size_t bytesWritten = 0;
    int writeRes = 0;
    int readRes = 0;
    const int minFragment = 65536;// Prevent fragments smaller than 64K.
    void* buffer = NULL;

    if(fragmentSize < minFragment) fragmentSize = minFragment; 
    buffer = malloc(fragmentSize);

    if(buffer == NULL) {  // Bail out if we can't allocate.
        *write = -1; 
        *read = -1;
        return;
    }

    int localFileDesc = open(filename, 
                             O_CREAT|O_WRONLY|O_TRUNC,
                             S_IRUSR|S_IWUSR);
    if(localFileDesc == -1) {
        while(errno == -EMFILE) {
            LOGGER_WRN << "EMFILE while trying to write locally." << std::endl;
            sleep(1);
            localFileDesc = open(filename, 
                                 O_CREAT|O_WRONLY|O_TRUNC,
                                 S_IRUSR|S_IWUSR);
        }
        writeRes = -errno;
    }
    while(1) {
        if(abortFlag && (*abortFlag)) break;
        readRes = xrdRead(fildes, buffer, 
                          static_cast<unsigned long long>(fragmentSize));
        if(readRes <= 0) { // Done, or error.
            readRes = -errno;
            break;
        }                                       
        bytesRead += readRes;
        if(writeRes >= 0) { // No error yet?
            while(1) {
                writeRes = pwrite(localFileDesc, buffer, 
                                  readRes, bytesWritten);
                if(writeRes != -1) {
                    bytesWritten += writeRes;
                } else {
                    if(errno == ENOSPC) {
                        sleep(5); // sleep for 5 seconds.
                        continue; // Try again.
                    }
                    writeRes = -errno; // Update write status
                }
                break;
            }
        }
        if(readRes < fragmentSize) {
            break;
        }
    }
    // Close the writing file.
    if(localFileDesc != -1) {
        int res = close(localFileDesc);
        if((res == -1) && (writeRes >= 0)) {
            LOGGER_ERR << "Bad local close for descriptor " << localFileDesc
                       << std::endl;
            writeRes = -errno;
        } else {
            writeRes = bytesWritten; // Update successful result.
            if(readRes >= 0) {
                readRes = bytesRead;
            }
        }
    }
    free(buffer);
    *write = writeRes;
    *read = readRes;
    return;
}

qMaster::XrdTransResult qMaster::xrdOpenWriteReadSaveClose(
    const char *path, 
    const char* buf, int len, // Query
    int fragmentSize,
    const char* outfile) {

    XrdTransResult r; 

    dbg::recordTrans(path, buf, len); // Record the trace file.

    int fh = xrdOpen(path, O_RDWR);
    if(fh == -1) {
        r.open = -errno;
        return r;
    } else {
        r.open = fh;
    }

    int writeCount = xrdWrite(fh, buf, len);
    if(writeCount != len) {
        r.queryWrite = -errno;
    } else {
        r.queryWrite = writeCount;
        xrdLseekSet(fh, 0);
        xrdReadToLocalFile(fh, fragmentSize, outfile, 0,
                           &(r.localWrite), &(r.read));
    }
    xrdClose(fh);
    return r;
}

qMaster::XrdTransResult qMaster::xrdOpenWriteReadSave(
    const char *path, 
    const char* buf, int len, // Query
    int fragmentSize,
    const char* outfile) {

    XrdTransResult r; 

    dbg::recordTrans(path, buf, len); // Record the trace file.

    int fh = xrdOpen(path, O_RDWR);
    if(fh == -1) {
        r.open = -errno;
        return r;
    } else {
        r.open = fh;
    }

    int writeCount = xrdWrite(fh, buf, len);
    if(writeCount != len) {
        r.queryWrite = -errno;
    } else {
        r.queryWrite = writeCount;
        xrdLseekSet(fh, 0);
        xrdReadToLocalFile(fh, fragmentSize, outfile, 0,
                           &(r.localWrite), &(r.read));
    }
    return r;
}

#endif

