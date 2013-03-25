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

#include "Chunker.h"

#include <stdexcept>

#include "Constants.h"

using std::fabs;
using std::sin;
using std::cos;
using std::acos;
using std::atan2;
using std::sqrt;
using std::max;
using std::pair;
using std::runtime_error;
using std::swap;
using std::vector;

using boost::scoped_array;

namespace po = boost::program_options;


namespace lsst { namespace qserv { namespace admin { namespace dupr {

int segments(double decMin, double decMax, double width) {
    double dec = max(fabs(decMin), fabs(decMax));
    if (dec > 90.0 - 1/3600.0) {
        return 1;
    }
    if (width >= 180.0) {
        return 1;
    } else if (width < 1/3600.0) {
        width = 1/3600;
    }
    dec *= RAD_PER_DEG;
    double cw = cos(width * RAD_PER_DEG);
    double sd = sin(dec);
    double cd = cos(dec);
    double x = cw - sd * sd;
    double u = cd * cd;
    double y = sqrt(fabs(u * u - x * x));
    return static_cast<int>(
        floor(360.0 / fabs(DEG_PER_RAD * atan2(y, x))));
}

double segmentWidth(double decMin, double decMax, int numSegments) {
    double dec = max(fabs(decMin), fabs(decMax)) * RAD_PER_DEG;
    double cw = cos(RAD_PER_DEG * (360.0 / numSegments));
    double sd = sin(dec);
    double cd = cos(dec);
    return acos(cw * cd * cd + sd * sd) * DEG_PER_RAD;
}


Chunker::Chunker(double overlap,
                 int32_t numStripes,
                 int32_t numSubStripesPerStripe)
{
    _initialize(overlap, numStripes, numSubStripesPerStripe);
}

Chunker::Chunker(po::variables_map const & vm) {
    _initialize(vm["part.overlap"].as<double>(),
                vm["part.num-stripes"].as<int32_t>(),
                vm["part.num-sub-stripes"].as<int32_t>());
}

Chunker::~Chunker() { }

SphericalBox const Chunker::getChunkBounds(int32_t chunkId) const {
    int32_t stripe = _getStripe(chunkId);
    int32_t chunk = _getChunk(chunkId, stripe);
    double width = 360.0 / _numChunksPerStripe[stripe];
    double raMin = chunk*width;
    double raMax = clampRa((chunk + 1)*width);
    double decMin = clampDec(
        stripe*_numSubStripesPerStripe*_subStripeHeight - 90.0);
    double decMax = clampDec(
        (stripe + 1)*_numSubStripesPerStripe*_subStripeHeight - 90.0);
    return SphericalBox(raMin, raMax, decMin, decMax);
}

SphericalBox const Chunker::getSubChunkBounds(int32_t chunkId,
                                              int32_t subChunkId) const
{
    int32_t stripe = _getStripe(chunkId);
    int32_t chunk = _getChunk(chunkId, stripe);
    int32_t subStripe = _getSubStripe(subChunkId, stripe);
    int32_t subChunk = _getSubChunk(subChunkId, stripe, subStripe, chunk);
    double raMin = subChunk*_subChunkWidth[subStripe];
    double raMax = clampRa((subChunk + 1)*_subChunkWidth[subStripe]);
    double decMin = clampDec(subStripe*_subStripeHeight - 90.0);
    double decMax = clampDec((subStripe + 1)*_subStripeHeight - 90.0);
    return SphericalBox(raMin, raMax, decMin, decMax);
}

ChunkLocation const Chunker::locate(
    std::pair<double, double> const & position) const
{
    ChunkLocation loc;
    double const ra = position.first;
    double const dec = position.second;
    int32_t subStripe = static_cast<int32_t>(
        floor((dec + 90.0) / _subStripeHeight));
    int32_t const numSubStripes = _numSubStripesPerStripe*_numStripes;
    if (subStripe >= numSubStripes) {
        subStripe = numSubStripes - 1;
    }
    int32_t const stripe = subStripe/_numSubStripesPerStripe;
    int32_t subChunk = static_cast<int32_t>(
        floor(ra / _subChunkWidth[subStripe]));
    int32_t const numChunks = _numChunksPerStripe[stripe];
    int32_t const numSubChunksPerChunk = _numSubChunksPerChunk[subStripe];
    int32_t const numSubChunks = numChunks*numSubChunksPerChunk;
    if (subChunk >= numSubChunks) {
        subChunk = numSubChunks - 1;
    }
    int32_t const chunk = subChunk / numSubChunksPerChunk;
    loc.chunkId = _getChunkId(stripe, chunk);
    loc.subChunkId = _getSubChunkId(stripe, subStripe, chunk, subChunk);
    loc.kind = ChunkLocation::NON_OVERLAP;
    return loc;
}

void Chunker::locate(pair<double, double> const & position,
                     int32_t chunkId,
                     vector<ChunkLocation> & locations) const
{
    // Find non-overlap location of position.
    double const ra = position.first;
    double const dec = position.second;
    int32_t subStripe = static_cast<int32_t>(
        floor((dec + 90.0) / _subStripeHeight));
    int32_t const numSubStripes = _numSubStripesPerStripe*_numStripes;
    if (subStripe >= numSubStripes) {
        subStripe = numSubStripes - 1;
    }
    int32_t const stripe = subStripe/_numSubStripesPerStripe;
    int32_t subChunk = static_cast<int32_t>(
        floor(ra / _subChunkWidth[subStripe]));
    int32_t const numChunks = _numChunksPerStripe[stripe];
    int32_t const numSubChunksPerChunk = _numSubChunksPerChunk[subStripe];
    int32_t const numSubChunks = numChunks*numSubChunksPerChunk;
    if (subChunk >= numSubChunks) {
        subChunk = numSubChunks - 1;
    }
    int32_t const chunk = subChunk / numSubChunksPerChunk;
    if (chunkId < 0 || _getChunkId(stripe, chunk) == chunkId) {
        // The non-overlap location is in the requested chunk.
        ChunkLocation loc;
        loc.chunkId = _getChunkId(stripe, chunk);
        loc.subChunkId = _getSubChunkId(stripe, subStripe, chunk, subChunk);
        loc.kind = ChunkLocation::NON_OVERLAP;
        locations.push_back(loc);
    }
    if (_overlap == 0.0) {
        return;
    }
    // Get sub-chunk bounds.
    double const raMin = subChunk*_subChunkWidth[subStripe];
    double const raMax = clampRa((subChunk + 1)*_subChunkWidth[subStripe]);
    double const decMin = clampDec(subStripe*_subStripeHeight - 90.0);
    double const decMax = clampDec((subStripe + 1)*_subStripeHeight - 90.0);
    // Check whether the position is in the overlap regions of sub-chunks in
    // the sub-stripe above and below.
    if (subStripe > 0 && dec < decMin + _overlap) {
        // The position is in the full-overlap region of sub-chunks
        // 1 sub-stripe down.
        _upDownOverlap(ra, chunkId, ChunkLocation::FULL_OVERLAP,
                       (subStripe - 1) / _numSubStripesPerStripe,
                       subStripe - 1, locations);
    }
    if (subStripe < numSubStripes - 1 && dec >= decMax - _overlap) {
        // The position is in the full and self-overlap regions of sub-chunks
        // 1 sub-stripe up.
        _upDownOverlap(ra, chunkId, ChunkLocation::SELF_OVERLAP,
                       (subStripe + 1) / _numSubStripesPerStripe,
                       subStripe + 1, locations);
    }
    // Check whether the position is in the overlap regions of the sub-chunks
    // to the left and right.
    if (numSubChunks == 1) {
        return;
    }
    double const alpha = _alpha[subStripe];
    if (ra < raMin + alpha) {
        // The position is in the full and self-overlap region of the sub-chunk
        // to the left.
        int32_t overlapChunk, overlapSubChunk;
        if (subChunk == 0) {
            overlapChunk = numChunks - 1;
            overlapSubChunk = numSubChunks - 1;
        } else {
            overlapChunk = (subChunk - 1) / numSubChunksPerChunk;
            overlapSubChunk = subChunk - 1;
        }
        if (chunkId < 0 || _getChunkId(stripe, overlapChunk) == chunkId) {
            ChunkLocation loc;
            loc.chunkId = _getChunkId(stripe, overlapChunk);
            loc.subChunkId = _getSubChunkId(
                stripe, subStripe, overlapChunk, overlapSubChunk);
            loc.kind = ChunkLocation::SELF_OVERLAP;
            locations.push_back(loc);
        }
    }
    if (ra > raMax - alpha) {
        // The position is in the full-overlap region of the sub-chunk
        // to the right.
        int32_t overlapChunk, overlapSubChunk;
        if (subChunk == numSubChunks - 1) {
            overlapChunk = 0;
            overlapSubChunk = 0;
        } else {
            overlapChunk = (subChunk + 1) / numSubChunksPerChunk;
            overlapSubChunk = subChunk + 1;
        }
        if (chunkId < 0 || _getChunkId(stripe, overlapChunk) == chunkId) {
            ChunkLocation loc;
            loc.chunkId = _getChunkId(stripe, overlapChunk);
            loc.subChunkId = _getSubChunkId(
                stripe, subStripe, overlapChunk, overlapSubChunk);
            loc.kind = ChunkLocation::FULL_OVERLAP;
            locations.push_back(loc);
        }
    }
}

vector<int32_t> const Chunker::getChunksFor(SphericalBox const & region,
                                            uint32_t node,
                                            uint32_t numNodes,
                                            bool hashChunks) const
{
    if (numNodes == 0) {
        throw runtime_error("There must be at least one node to assign chunks to");
    }
    if (node >= numNodes) {
        throw runtime_error("Node number must be in range [0, numNodes)");
    }
    vector<int32_t> chunks;
    uint32_t n = 0;
    // The slow and easy route - loop over every chunk, see if it belongs to
    // the given node, and if it also intersects with region, return it.
    for (int32_t stripe = 0; stripe < _numStripes; ++stripe) {
        for (int32_t chunk = 0; chunk < _numChunksPerStripe[stripe]; ++chunk, ++n) {
            int32_t const chunkId = _getChunkId(stripe, chunk);
            if (hashChunks) {
                if (mulveyHash(static_cast<uint32_t>(chunkId)) % numNodes != node) {
                    continue;
                }
            } else {
                if (n % numNodes != node) {
                    continue;
                }
            }
            SphericalBox box = getChunkBounds(_getChunkId(stripe, chunk));
            if (region.intersects(box)) {
                chunks.push_back(chunkId);
            }
        }
    }
    return chunks;
}

void Chunker::defineOptions(po::options_description & opts) {
    opts.add_options()
        ("part.num-stripes", po::value<int32_t>()->default_value(18),
         "The number of declination stripes to divide the sky into.")
        ("part.num-sub-stripes", po::value<int32_t>()->default_value(100),
         "The number of sub-stripes to divide each stripe into.")
        ("part.overlap", po::value<double>()->default_value(0.01),
         "Chunk/sub-chunk overlap radius (deg).");
}

void Chunker::_initialize(double overlap,
                          int32_t numStripes,
                          int32_t numSubStripesPerStripe)
{
    if (numStripes < 1 || numSubStripesPerStripe < 1) {
        throw runtime_error(
            "The number of stripes and sub-stripes per stripe "
            "must be positive.");
    }
    if (overlap < 0.0 || overlap > 10.0) {
        throw runtime_error("The overlap radius must be in range "
                            "[0, 10] deg.");
    }
    int32_t const numSubStripes = numStripes * numSubStripesPerStripe;
    _overlap = overlap;
    _numStripes = numStripes;
    _numSubStripesPerStripe = numSubStripesPerStripe;
    double const stripeHeight = 180.0 / numStripes;
    double const subStripeHeight = 180.0 / numSubStripes;
    if (subStripeHeight < overlap) {
        throw runtime_error("The overlap radius is greater than "
                            "the sub-stripe height.");
    }
    _subStripeHeight = subStripeHeight;
    scoped_array<int32_t> numChunksPerStripe(new int32_t[numStripes]);
    scoped_array<int32_t> numSubChunksPerChunk(new int32_t[numSubStripes]);
    scoped_array<double> subChunkWidth(new double[numSubStripes]);
    scoped_array<double> alpha(new double[numSubStripes]);
    int32_t maxSubChunksPerChunk = 0;
    for (int32_t i = 0; i < numStripes; ++i) {
        int32_t nc = segments(
            i*stripeHeight - 90.0, (i + 1)*stripeHeight - 90.0, stripeHeight);
        numChunksPerStripe[i] = nc;
        for (int32_t j = 0; j < numSubStripesPerStripe; ++j) {
            int32_t ss = i * numSubStripesPerStripe + j;
            double decMin = ss*subStripeHeight - 90.0;
            double decMax = (ss + 1)*subStripeHeight - 90.0;
            int32_t nsc = segments(decMin, decMax, subStripeHeight) / nc;
            maxSubChunksPerChunk = max(maxSubChunksPerChunk, nsc);
            numSubChunksPerChunk[ss] = nsc;
            double scw = 360.0 / (nsc * nc);
            subChunkWidth[ss] = scw;
            double a = maxAlpha(overlap, max(fabs(decMin), fabs(decMax)));
            if (a > scw) {
                throw runtime_error("The overlap radius is greater than "
                                    "the sub-chunk width.");
            }
            alpha[ss] = a;
        }
    }
    _maxSubChunksPerChunk = maxSubChunksPerChunk;
    swap(numChunksPerStripe, _numChunksPerStripe);
    swap(numSubChunksPerChunk, _numSubChunksPerChunk);
    swap(subChunkWidth, _subChunkWidth);
    swap(alpha, _alpha);
}

void Chunker::_upDownOverlap(double ra,
                             int32_t chunkId,
                             ChunkLocation::Kind kind,
                             int32_t stripe,
                             int32_t subStripe,
                             vector<ChunkLocation> & locations) const
{
    int32_t const numChunks = _numChunksPerStripe[stripe];
    int32_t const numSubChunksPerChunk = _numSubChunksPerChunk[subStripe];
    int32_t const numSubChunks = numChunks*numSubChunksPerChunk;
    double const subChunkWidth = _subChunkWidth[subStripe];
    double const alpha = _alpha[subStripe];
    int32_t minSubChunk = static_cast<int32_t>(floor((ra - alpha) / subChunkWidth));
    int32_t maxSubChunk = static_cast<int32_t>(floor((ra + alpha) / subChunkWidth));
    if (minSubChunk < 0) {
        minSubChunk += numSubChunks;
    }
    if (maxSubChunk >= numSubChunks) {
        maxSubChunk -= numSubChunks;
    }
    if (minSubChunk > maxSubChunk) {
        // 0/360 wrap around
        for (int32_t subChunk = minSubChunk; subChunk < numSubChunks; ++subChunk) {
            int32_t chunk = subChunk / numSubChunksPerChunk;
            if (chunkId < 0 || _getChunkId(stripe, chunk) == chunkId) {
                ChunkLocation loc;
                loc.chunkId = _getChunkId(stripe, chunk);
                loc.subChunkId = _getSubChunkId(stripe, subStripe, chunk, subChunk);
                loc.kind = kind;
                locations.push_back(loc);
            }
        }
        minSubChunk = 0;
    }
    for (int32_t subChunk = minSubChunk; subChunk <= maxSubChunk; ++subChunk) {
        int32_t chunk = subChunk / numSubChunksPerChunk;
        if (chunkId < 0 || _getChunkId(stripe, chunk) == chunkId) {
            ChunkLocation loc;
            loc.chunkId = _getChunkId(stripe, chunk);
            loc.subChunkId = _getSubChunkId(stripe, subStripe, chunk, subChunk);
            loc.kind = kind;
            locations.push_back(loc);
        }
    }
}

}}}} // namespace lsst::qserv::admin::dupr
