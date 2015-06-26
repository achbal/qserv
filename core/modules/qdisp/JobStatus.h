// -*- LSST-C++ -*-
/*
 * LSST Data Management System
 * Copyright 2015 LSST Corporation.
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
#ifndef LSST_QSERV_QDISP_EXECSTATUS_H
#define LSST_QSERV_QDISP_EXECSTATUS_H

// System headers
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <time.h>

// Qserv headers
#include "global/ResourceUnit.h"

namespace lsst {
namespace qserv {
namespace qdisp {

/** Monitor execution of a chunk query against a xrootd ressource
 *
 *  JobStatus instances receive timestamped reports of execution State. This
 *  allows a manager object to receive updates on status without exposing its
 *  existence to a delegate class.
 *
 *  TODO: The JobStatus class could be extended to
 *  save all received reports to provide a timeline of state changes.
 *
 *  @see qdisp::JobDescription
 */
class JobStatus {
public:
    typedef std::shared_ptr<JobStatus> Ptr;
    JobStatus(ResourceUnit const& r) : _info(r) {}

    // TODO: these shouldn't be exposed, and so shouldn't be user-level error
    // codes, but maybe we can be clever and avoid an ugly remap/translation
    // with msgCode.h. 1201-1289 (inclusive) are free and MSG_FINALIZED==2000
    enum State { UNKNOWN=0,
                 PROVISION=1201,
                 PROVISION_ERROR, PROVISION_NACK,
                 PROVISION_OK, // ???
                 REQUEST, REQUEST_ERROR,
                 RESPONSE_READY, RESPONSE_ERROR,
                 RESPONSE_DATA, RESPONSE_DATA_NACK, RESPONSE_DATA_ERROR,
                 RESPONSE_DATA_ERROR_OK, RESPONSE_DATA_ERROR_CORRUPT,
                 RESPONSE_DONE,
                 RESULT_ERROR,
                 MERGE_OK, // ???
                 MERGE_ERROR,
                 CANCEL, COMPLETE=2000};
    /** Report a state transition.
     *
     *	Useful for logging and error reporting
     *
     * TODO: Save past state history:
     *  - resourceUnit should be extracted from Info (beware of mutex)
     *  - Info should be put in a vector
     */
    void updateInfo(State s, int code=0, std::string const& desc=_empty) {
        std::lock_guard<std::mutex> lock(_mutex);
#if 0
        std::ofstream of("/tmp/deleteme_qs_rpt", std::ofstream::app);
        of << "Reporting " << (void*)this
                << " state " << s << std::endl;
#endif
        _info.stateTime = ::time(NULL);
        _info.state = s;
        _info.stateCode = code;
        _info.stateDesc = desc;
    }

    struct Info {
        Info(ResourceUnit const& resourceUnit_);
        ResourceUnit const resourceUnit; ///< Reference id for status
        // More detailed debugging may store a vector of states, appending
        // with each invocation of report().
        State state; ///< Actual state
        time_t stateTime; ///< Last modified timestamp
        int stateCode; ///< Code associated with state (e.g. xrd or mysql error code)
        std::string stateDesc; ///< Textual description
    };

    Info getInfo() const {
        std::lock_guard<std::mutex> lock(_mutex);
        return _info;
    }

private:
    friend std::ostream& operator<<(std::ostream& os, JobStatus const& es);
    Info _info;

private:
    mutable std::mutex _mutex; ///< Mutex to guard concurrent updates
    static std::string const _empty;
};
std::ostream& operator<<(std::ostream& os, JobStatus const& es);
std::ostream& operator<<(std::ostream& os, JobStatus::Info const& inf);
std::ostream& operator<<(std::ostream& os, JobStatus::State const& state);

}}} // namespace lsst::qserv::qdisp

#endif // LSST_QSERV_QDISP_EXECSTATUS_H
