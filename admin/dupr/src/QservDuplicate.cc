/*
 * LSST Data Management System
 * Copyright 2013 LSST Corporation.
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

/// \file
/// \brief The Qserv spatial data duplicator.

#include <cstdio>
#include <algorithm>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "boost/filesystem.hpp"
#include "boost/program_options.hpp"
#include "boost/shared_ptr.hpp"

#include "Chunker.h"
#include "ChunkReducer.h"
#include "CmdLineUtils.h"
#include "Csv.h"
#include "Geometry.h"
#include "HtmIndex.h"

using std::cerr;
using std::cout;
using std::distance;
using std::endl;
using std::exception;
using std::numeric_limits;
using std::pair;
using std::runtime_error;
using std::snprintf;
using std::sort;
using std::string;
using std::vector;

using boost::shared_ptr;

namespace fs = boost::filesystem;
namespace po = boost::program_options;


namespace lsst { namespace qserv { namespace admin { namespace dupr {

class Worker;

// High-level duplicator logic and state. Note that worker threads
// have access to internal state, but must never mutate it.
class Duplicator {
public:
    Duplicator() : _blockSize(0), _level(-1) { }

    shared_ptr<ChunkIndex> const run(po::variables_map const & vm);

private:
    // A list of (HTM triangle, chunk ID) pairs.
    typedef vector<pair<uint32_t, int32_t> > TargetList;
    // A map from source HTM triangles to duplication target triangles/chunks.
    typedef boost::unordered_map<uint32_t, TargetList> TargetMap;

    void _makeTargets(int32_t chunkId);
    InputLines const _makeInput() const;

    TargetMap            _targets;
    shared_ptr<Chunker>  _chunker;
    shared_ptr<HtmIndex> _partIndex;
    shared_ptr<HtmIndex> _index;
    fs::path             _partIndexDir;
    fs::path             _indexDir;
    size_t               _blockSize;
    int                  _level;

    friend class Worker;
};

// Find non-empty source triangles for the HTM triangles overlapping
// the given chunk, and add corresponding source to target triangle
// mappings to the duplication target map.
void Duplicator::_makeTargets(int32_t chunkId) {
    typedef vector<uint32_t>::const_iterator Iter;
    SphericalBox box(_chunker->getChunkBounds(chunkId));
    box.expand(_chunker->getOverlap() + 1/3600.0); // 1 arcsec epsilon
    vector<uint32_t> ids;
    box.htmIds(ids, _level);
    for (Iter i = ids.begin(), e = ids.end(); i != e; ++i) {
        uint32_t sourceHtmId = _partIndex->mapToNonEmpty(*i);
        _targets[sourceHtmId].push_back(pair<uint32_t, int32_t>(*i, chunkId));
    }
}

// Create map-reduce input from source HTM triangle IDs.
InputLines const Duplicator::_makeInput() const {
    typedef TargetMap::const_iterator Iter;
    char file[32];
    vector<fs::path> paths;
    for (Iter i = _targets.begin(), e = _targets.end(); i != e; ++i) {
        snprintf(file, sizeof(file), "htm_%lx.txt",
                 static_cast<unsigned long>(i->first));
        paths.push_back(_indexDir / fs::path(file));
    }
    return InputLines(paths, _blockSize*MiB, false);
}


// The global duplicator object.
Duplicator duplicator;

// Used by workers to access `duplicator`, forcing the compiler to verify
// that workers never mutate Duplicator state.
inline Duplicator const & dup() { return duplicator; }


/// Functor for counting the number of IDs less than a given value.
class LessThanCounter {
public:
    LessThanCounter() : _htmId(0) { }
    ~LessThanCounter() { }

    void setup(HtmIndex const & index,
               fs::path const & directory,
               uint32_t const htmId);

    int64_t operator()(int64_t id) const {
        vector<int64_t>::const_iterator p =
            std::lower_bound(_ids.begin(), _ids.end(), id);
        if (p == _ids.end() || id != *p) {
            throw runtime_error("ID lookup failed.");
        }
        return static_cast<int64_t>(distance(_ids.begin(), p));
    }

private:
    vector<int64_t> _ids;
    uint32_t _htmId;
};

void LessThanCounter::setup(HtmIndex const & index,
                            fs::path const & directory,
                            uint32_t const htmId)
{
    if (_htmId == htmId) {
        return;
    } else if (htmLevel(htmId) != index.getLevel()) {
        throw runtime_error("Invalid HTM ID.");
    }
    char file[32];
    snprintf(file, sizeof(file), "htm_%lx.ids",
             static_cast<unsigned long>(htmId));
    InputFile f(directory / fs::path(file));
    size_t nrec = static_cast<size_t>(index(htmId));
    size_t sz = 8*nrec;
    if (static_cast<size_t>(f.size()) != sz) {
        throw runtime_error("Inconsistent ID file size.");
    }
    boost::scoped_array<uint8_t> data(new uint8_t[sz]);
    _ids.clear();
    _ids.reserve(nrec);
    f.read(data.get(), 0, sz);
    for (size_t i = 0; i < nrec; ++i) {
        _ids.push_back(static_cast<int64_t>(decode<uint64_t>(&data[8*i])));
    }
    sort(_ids.begin(), _ids.end());
}


/// Map-reduce worker class for the Qserv spatial data duplicator.
class Worker : public ChunkReducer {
public:
    Worker(po::variables_map const & vm);

    void map(char const * beg, char const * end, Silo & silo);

    static void defineOptions(po::options_description & opts);

private:
    // Decide whether or not to include a record based on the record ID.
    bool _discard(int64_t id) const {
        return hash(static_cast<uint64_t>(id) ^ _seed) > _maxId;
    }

    void _setup(uint32_t htmId);

    // A target triangle/chunk, along with a transform for mapping
    // positions from a source triangle to the target.
    struct Target {
        uint32_t htmId;
        int32_t chunkId;
        Matrix3d transform;

        Target() : htmId(0), chunkId(-1), transform(Matrix3d::Identity()) { }
    };

    struct Pos {
        Vector3d v; // Cartesian coordinates for (ra, dec).
        int ra;     // Right ascension field index.
        int dec;    // Declination field index.
        bool null;  // Set to true if the ra or dec field value is NULL.

        Pos() : v(0.0, 0.0, 0.0), ra(-1), dec(-1), null(false) { }
        Pos(int r, int d) : v(0.0, 0.0, 0.0), ra(r), dec(d), null(false) { }
    };

    csv::Editor           _editor;
    vector<Target>        _targets;
    Pos                   _partPos;
    vector<Pos>           _pos;
    uint64_t              _seed;
    uint64_t              _maxId;
    uint32_t              _sourceHtmId;
    int                   _level;
    int                   _partIdField;
    int                   _idField;
    int                   _chunkIdField;
    int                   _subChunkIdField;
    vector<ChunkLocation> _locations;
    fs::path              _partIndexDir;
    fs::path              _indexDir;
    shared_ptr<LessThanCounter> _partIdsLessThan;
    shared_ptr<LessThanCounter> _idsLessThan;
};

Worker::Worker(po::variables_map const & vm) :
    ChunkReducer(vm),
    _editor(vm),
    _partPos(),
    _sourceHtmId(0),
    _level(dup()._index->getLevel()),
    _partIdField(-1),
    _idField(-1),
    _chunkIdField(-1),
    _subChunkIdField(-1),
    // defend against GCC PR21334
    _partIndexDir(dup()._partIndexDir.string().c_str()),
    _indexDir(dup()._indexDir.string().c_str())
{
    typedef vector<string>::const_iterator StringIter;

    _seed = vm["sample.seed"].as<uint64_t>();
    double d = vm["sample.fraction"].as<double>();
    if (d <= 0.0 || d > 1.0) {
        throw runtime_error("The --sample.fraction option value "
                            "must be in the range (0, 1].");
    }
    if (d == 1.0) {
        _maxId = numeric_limits<uint64_t>::max();
    } else {
        _maxId = static_cast<uint64_t>(std::ldexp(d, 64));
    }
    // Map field names of interest to field indexes.
    if (vm.count("part.pos") == 0) {
        throw runtime_error("The --part.pos option was not specified.");
    }
    FieldNameResolver fields(_editor);
    string s = vm["part.pos"].as<string>();
    pair<string,string> p = parseFieldNamePair("part.pos", s);
    _partPos.ra = fields.resolve("part.pos", s, p.first);
    _partPos.dec = fields.resolve("part.pos", s, p.second);
    if (vm.count("pos") != 0) {
        vector<string> const & pos = vm["pos"].as<vector<string> >();
        for (StringIter i = pos.begin(), e = pos.end(); i != e; ++i) {
            p = parseFieldNamePair("pos", *i);
            _pos.push_back(Pos(fields.resolve("pos", *i, p.first),
                               fields.resolve("pos", *i, p.second)));
        }
    }
    if (vm.count("id") != 0) {
        s = vm["id"].as<string>();
        _idField = fields.resolve("id", s);
    }
    if (vm.count("part.id") != 0) {
        s = vm["part.id"].as<string>();
        _partIdField = fields.resolve("part.id", s, vm.count("id") == 0);
    }
    if (_partIdField >= 0) {
        _partIdsLessThan.reset(new LessThanCounter());
    }
    if (_idField >= 0 && _idField != _partIdField) {
        _idsLessThan.reset(new LessThanCounter());
    } else {
        _idsLessThan = _partIdsLessThan;
    }
    if (vm.count("part.chunk") != 0) {
        s = vm["part.chunk"].as<string>();
        _chunkIdField = fields.resolve("part.chunk", s);
    }
    s = vm["part.sub-chunk"].as<string>();
    _subChunkIdField = fields.resolve("part.sub-chunk", s);
}

void Worker::map(char const * beg, char const * end, Worker::Silo & silo) {
    typedef vector<ChunkLocation>::const_iterator LocIter;
    typedef vector<Target>::const_iterator TgtIter;
    typedef vector<Pos>::iterator PosIter;

    uint32_t sourceHtmId = 0;
    while (beg < end) {
        beg = _editor.readRecord(beg, end);
        // Extract positions.
        pair<double, double> sc;
        for (PosIter p = _pos.begin(), pe = _pos.end(); p != pe; ++p) {
            p->null = _editor.isNull(p->ra) || _editor.isNull(p->dec);
            if (!p->null) {
                sc.first = _editor.get<double>(p->ra);
                sc.second = _editor.get<double>(p->dec);
                p->v = cartesian(sc);
            } else {
                _editor.setNull(p->ra);
                _editor.setNull(p->dec);
            }
        }
        sc.first = _editor.get<double>(_partPos.ra);
        sc.second = _editor.get<double>(_partPos.dec);
        _partPos.v = cartesian(sc);
        if (sourceHtmId == 0) {
            sourceHtmId = htmId(_partPos.v, _level);
            _setup(sourceHtmId);
        }
        // Remap IDs and discard records to match the sampling rate.
        int64_t partId = 0;
        bool partIdIsNull = _editor.isNull(_partIdField);
        if (!partIdIsNull) {
            partId = (*_partIdsLessThan)(_editor.get<int64_t>(_partIdField));
            if (_discard(partId)) {
                continue;
            }
        }
        int64_t id = 0;
        bool idIsNull = _editor.isNull(_idField);
        if (!idIsNull && _idField != _partIdField) {
            id = (*_idsLessThan)(_editor.get<int64_t>(_idField));
            if (partIdIsNull && _discard(id)) {
                continue;
            }
        }
        // Loop over target HTM triangles/chunks
        for (TgtIter t = _targets.begin(), te = _targets.end(); t != te; ++t) {
            int64_t baseId = static_cast<int64_t>(t->htmId) << 32;
            bool mustTransform = (sourceHtmId != t->htmId);
            pair<double, double> pos =
                mustTransform ? spherical(t->transform * _partPos.v) : sc;
            // Locate partitioning position
            _locations.clear();
            dup()._chunker->locate(pos, t->chunkId, _locations);
            if (_locations.empty()) {
                continue;
            }
            if (mustTransform) {
                _editor.set(_partPos.ra, pos.first);
                _editor.set(_partPos.dec, pos.second);
                // Transform positions.
                for (PosIter p = _pos.begin(), pe = _pos.end(); p != pe; ++p) {
                    if (!p->null) {
                        pos = spherical(t->transform * p->v);
                        _editor.set(p->ra, pos.first);
                        _editor.set(p->dec, pos.second);
                    }
                }
            }
            // Set output IDs.
            if (!partIdIsNull) {
                _editor.set(_partIdField, baseId + partId);
            }
            if (!idIsNull && _idField != _partIdField) {
                _editor.set(_idField, baseId + id);
            }
            // Output a record for each location.
            for (LocIter l = _locations.begin(), le = _locations.end();
                 l != le; ++l) {
                _editor.set(_chunkIdField, l->chunkId);
                _editor.set(_subChunkIdField, l->subChunkId);
                silo.add(*l, _editor);
            }
        }
    }
}

void Worker::_setup(uint32_t htmId) {
    typedef Duplicator::TargetList::const_iterator TargetIter;
    if (htmId == _sourceHtmId) {
        return;
    }
    if (_partIdField >= 0) {
        _partIdsLessThan->setup(*dup()._partIndex, _partIndexDir, htmId);
    }
    if (_idField >= 0 && _idField != _partIdField) {
        _idsLessThan->setup(*dup()._index, _indexDir, htmId);
    }
    Duplicator::TargetList const & list = dup()._targets.at(htmId);
    Matrix3d const m = SphericalTriangle(htmId).getBarycentricTransform();
    _targets.clear();
    for (TargetIter i = list.begin(), e = list.end(); i != e; ++i) {
        Target t;
        t.htmId = i->first;
        t.chunkId = i->second;
        if (i->first != htmId) {
            t.transform = SphericalTriangle(i->first).getCartesianTransform() * m;
        }
        _targets.push_back(t);
    }
    _sourceHtmId = htmId;
}

void Worker::defineOptions(po::options_description & opts) {
    po::options_description dup("\\________________ Duplication", 80);
    dup.add_options()
        ("sample.seed", po::value<uint64_t>()->default_value(0),
         "Seed value for sampling PRNG. The seeds used by cooperating "
         "duplicators (e.g. if processing has been split over many nodes) "
         "must be identical.")
        ("sample.fraction", po::value<double>()->default_value(1.0),
         "The fraction of input positions to include in the output.")
        ("index", po::value<string>(),
         "HTM index file name for the data set to duplicate. May be "
         "omitted, in which case --part.index is used as the HTM index "
         "for both the input data set and for partitioning positions.")
        ("id", po::value<string>(),
         "Optional ID field name associated with input records. Note "
         "that if --index and --part.index are identical, then either "
         "--id and --part.id must match, or one must be omitted.")
        ("pos", po::value<vector<string> >(),
         "Optional right ascension and declination field names, "
         "separated by a comma. May be specified any number of times. "
         "These field name pairs identify positions in addition to the "
         "partitioning position fields (identified via --part.pos).")
        ("ra-min", po::value<double>()->default_value(0.0),
         "Minimum right ascension bound (deg) for the duplication region.")
        ("ra-max", po::value<double>()->default_value(360.0),
         "Maximum right ascension bound (deg) for the duplication region.")
        ("dec-min", po::value<double>()->default_value(-90.0),
         "Minimum declination bound (deg) for the duplication region.")
        ("dec-max", po::value<double>()->default_value(90.0),
         "Maximum declination bound (deg) for the duplication region.")
        ("chunk-id", po::value<vector<int32_t> >(),
         "Optionally limit duplication to one or more chunks. If specified, "
         "data will be duplicated for the given chunk(s) regardless of the "
         "the duplication region and node.")
        ("out.node", po::value<uint32_t>(),
         "Optionally limit duplication to chunks for the given output node. "
         "A chunk is assigned to a node when the hash of the chunk ID modulo "
         "the number of nodes is equal to the node number. If this option is "
         "specified, its value must be less than --out.num-nodes. It is "
         "ignored if --chunk-id is specified.");
    po::options_description part("\\_______________ Partitioning", 80);
    part.add_options()
        ("part.index", po::value<string>(),
         "HTM index of partitioning positions. For example, if duplicating "
         "a source table partitioned on associated object RA and Dec, this "
         "would be the name of the HTM index file for the object table. If "
         "this option is omitted, then --index is used as the HTM index for "
         "both the input and partitioning position data sets.")
        ("part.id", po::value<string>(),
         "Optional ID field name associated with the partitioning position. "
         "Note that if --index and --part.index are identical, then one of "
         "--id and --part.id must be omitted, or both must match.")
        ("part.prefix", po::value<string>()->default_value("chunk"),
         "Chunk file name prefix.")
        ("part.chunk", po::value<string>(),
         "Optional chunk ID output field name. This field name is appended "
         "to the output field name list if it isn't already included.")
        ("part.sub-chunk", po::value<string>()->default_value("subChunkId"),
         "Sub-chunk ID output field name. This field field name is appended "
         "to the output field name list if it isn't already included.")
        ("part.pos", po::value<string>(),
         "The partitioning right ascension and declination field names, "
         "separated by a comma.");
    Chunker::defineOptions(part);
    opts.add(dup).add(part);
    defineOutputOptions(opts);
    csv::Editor::defineOptions(opts);
}

typedef Job<Worker> DuplicateJob;


shared_ptr<ChunkIndex> const Duplicator::run(po::variables_map const & vm) {
    // Initialize state
    shared_ptr<Chunker> chunker(new Chunker(vm));
    vector<int32_t> chunks = chunksToDuplicate(*chunker, vm);
    _chunker.swap(chunker);
    DuplicateJob job(vm);
    shared_ptr<ChunkIndex> chunkIndex;
    if (vm.count("id") == 0 && vm.count("part.id") == 0) {
        throw runtime_error("One or both of the --id and --part.id "
                            "options must be specified.");
    }
    if (vm.count("index") == 0 && vm.count("part.index") == 0) {
        throw runtime_error("One or both of the --index and --part.index "
                            "options must be specified.");
    }
    char const * opt = (vm.count("index") != 0 ? "index" : "part.index");
    fs::path indexPath(vm[opt].as<string>());
    opt = (vm.count("part.index") != 0 ? "part.index" : "index");
    fs::path partIndexPath(vm[opt].as<string>());
    _index.reset(new HtmIndex(indexPath));
    if (partIndexPath != indexPath) {
        _partIndex.reset(new HtmIndex(partIndexPath));
    } else {
        _partIndex = _index;
    }
    if (_index->getLevel() != _partIndex->getLevel()) {
        throw runtime_error("Subdivision levels of input data set index "
                            "(--index) and partitioning position index "
                            "(--part.index) do not match.");
    }
    _level = _index->getLevel();
    _indexDir = indexPath.parent_path();
    _partIndexDir = partIndexPath.parent_path();
    _blockSize = vm["mr.block-size"].as<size_t>();
    if (_blockSize == 0 || _blockSize > 1024) {
        throw runtime_error("--mr.block-size must be between 1 and 1024 MiB.");
    }
    // Generate data for numWorkers chunks at a time.
    uint32_t const numWorkers = vm["mr.num-workers"].as<uint32_t>();
    uint32_t n = numWorkers;
    while (!chunks.empty()) {
        _makeTargets(chunks.back());
        chunks.pop_back();
        --n;
        if (n == 0 || chunks.empty()) {
            shared_ptr<ChunkIndex> c = job.run(_makeInput());
            if (c) {
                if (chunkIndex) {
                    chunkIndex->merge(*c);
                } else {
                    chunkIndex = c;
                }
            }
            n = numWorkers;
            _targets.clear();
        }
    }
    return chunkIndex;
}

}}}} // namespace lsst::qserv::admin::dupr


static char const * help =
    "The Qserv duplicator generates partitioned data from an HTM index of\n"
    "an input data set by copying and rotating input data to \"fill in\"\n"
    "parts of the sky not covered by the input.\n";

int main(int argc, char const * const * argv) {
    namespace dupr = lsst::qserv::admin::dupr;
    try {
        po::options_description options;
        dupr::DuplicateJob::defineOptions(options);
        po::variables_map vm;
        dupr::parseCommandLine(vm, options, argc, argv, help);
        dupr::ensureOutputFieldExists(vm, "part.chunk");
        dupr::ensureOutputFieldExists(vm, "part.sub-chunk");
        dupr::makeOutputDirectory(vm, true);
        shared_ptr<dupr::ChunkIndex> index = dupr::duplicator.run(vm);
        if (!index->empty()) {
            fs::path d(vm["out.dir"].as<string>());
            fs::path f = vm["part.prefix"].as<string>() + "_index.bin";
            index->write(d / f, false);
        }
        if (vm.count("verbose") != 0) {
            index->write(cout, 0);
            cout << endl;
        } else {
            cout << *index << endl;
        }
    } catch (exception const & ex) {
        cerr << ex.what() << endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

