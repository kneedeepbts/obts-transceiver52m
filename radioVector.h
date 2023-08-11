/*
 * Written by Thomas Tsou <ttsou@vt.edu>
 * Based on code by Harvind S Samra <hssamra@kestrelsp.com>
 *
 * Copyright 2011 Free Software Foundation, Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * See the COPYING file in the main directory for details.
 */

#ifndef RADIOVECTOR_H
#define RADIOVECTOR_H

#include <vector>

#include "sigProcLib.h"

#include "gsmtime.h"
#include "LinkedLists.h"
#include "Interthread.h"
//#include "GSMCommon.h"

class radioVector : public signalVector {
public:
    radioVector(const signalVector &wVector, GsmTime &wTime);

    [[nodiscard]] GsmTime getTime() const;

    void setTime(const GsmTime &wTime);

    bool operator>(const radioVector &other) const;

private:
    GsmTime mTime;
};

class noiseVector : std::vector<float> {
public:
    explicit noiseVector(size_t len = 0);

    bool insert(float val);

    float avg();

private:
    std::vector<float>::iterator it;
};

class VectorFIFO {
public:
    unsigned size();

    void put(radioVector * ptr);

    radioVector * get();

private:
    PointerFIFO mQ;
};

class VectorQueue : public InterthreadPriorityQueue<radioVector> {
public:
    GsmTime nextTime() const;

    radioVector * getStaleBurst(const GsmTime &targTime);

    radioVector * getCurrentBurst(const GsmTime &targTime);
};

#endif /* RADIOVECTOR_H */
