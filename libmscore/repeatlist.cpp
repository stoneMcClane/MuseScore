//=============================================================================
//  MuseScore
//  Music Composition & Notation
//
//  Copyright (C) 2009-2011 Werner Schweer
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License version 2
//  as published by the Free Software Foundation and appearing in
//  the file LICENCE.GPL
//=============================================================================

#include "repeatlist.h"
#include "score.h"
#include "measure.h"
#include "tempo.h"
#include "volta.h"
#include "segment.h"
#include "marker.h"
#include "jump.h"

namespace Ms {

//---------------------------------------------------------
//   searchVolta
//    return volta at tick
//---------------------------------------------------------

Volta* Score::searchVolta(int tick) const
      {
      for (const std::pair<int,Spanner*>& p : _spanner.map()) {
            Spanner* s = p.second;
            if (s->type() != Element::Type::VOLTA)
                  continue;
            Volta* volta = static_cast<Volta*>(s);
            if (tick >= volta->tick() && tick < volta->tick2())
                  return volta;
            }
      return 0;
      }

//---------------------------------------------------------
//   searchLabel
//---------------------------------------------------------

Measure* Score::searchLabel(const QString& s)
      {
      if (s == "start")
            return firstMeasure();
      else if (s == "end")
            return lastMeasure();
      for (Measure* m = firstMeasure(); m; m = m->nextMeasure()) {
            for (auto e : m->el()) {
                  if (e->type() == Element::Type::MARKER) {
                        const Marker* marker = static_cast<const Marker*>(e);
                        if (marker->label() == s)
                              return m;
                        }
                  }
            }
      return 0;
      }

//---------------------------------------------------------
//   RepeatLoop
//---------------------------------------------------------

struct RepeatLoop {
      enum class LoopType : char { REPEAT, JUMP };

      LoopType type;
      Measure* m;   // start measure of LoopType::REPEAT
      int count;
      QString stop, cont;

      RepeatLoop() {}
      RepeatLoop(Measure* _m)  {
            m     = _m;
            count = 0;
            type  = LoopType::REPEAT;
            }
      RepeatLoop(const QString s, const QString c)
         : stop(s), cont(c)
            {
            m    = 0;
            type = LoopType::JUMP;
            }
      };

//---------------------------------------------------------
//   RepeatSegment
//---------------------------------------------------------

RepeatSegment::RepeatSegment()
      {
      tick       = 0;
      len        = 0;
      utick      = 0;
      utime      = 0.0;
      timeOffset = 0.0;
      }

//---------------------------------------------------------
//   RepeatList
//---------------------------------------------------------

RepeatList::RepeatList(Score* s)
      {
      _score = s;
      idx1  = 0;
      idx2  = 0;
      }

//---------------------------------------------------------
//   ticks
//---------------------------------------------------------

int RepeatList::ticks()
      {
      if (length() > 0) {
            RepeatSegment* s = last();
            return s->utick + s->len;
            }
      return 0;
      }

//---------------------------------------------------------
//   update
//---------------------------------------------------------

void RepeatList::update()
      {
      const TempoMap* tl = _score->tempomap();

      int utick = 0;
      qreal t  = 0;

      for(RepeatSegment* s : *this) {
            s->utick      = utick;
            s->utime      = t;
            qreal ct      = tl->tick2time(s->tick);
            s->timeOffset = t - ct;
            utick        += s->len;
            t            += tl->tick2time(s->tick + s->len) - ct;
            }
      }

//---------------------------------------------------------
//   utick2tick
//---------------------------------------------------------

int RepeatList::utick2tick(int tick) const
      {
      unsigned n = size();
      if (n == 0)
            return tick;
      if (tick < 0)
            return 0;
      unsigned ii = (idx1 < n) && (tick >= at(idx1)->utick) ? idx1 : 0;
      for (unsigned i = ii; i < n; ++i) {
            if ((tick >= at(i)->utick) && ((i + 1 == n) || (tick < at(i+1)->utick))) {
                  idx1 = i;
                  return tick - (at(i)->utick - at(i)->tick);
                  }
            }
      if (MScore::debugMode) {
            qFatal("tick %d not found in RepeatList", tick);
            }
      return 0;
      }

//---------------------------------------------------------
//   tick2utick
//---------------------------------------------------------

int RepeatList::tick2utick(int tick) const
      {
      for (const RepeatSegment* s : *this) {
            if (tick >= s->tick && tick < (s->tick + s->len))
                  return s->utick + (tick - s->tick);
            }
      return last()->utick + (tick - last()->tick);
      }

//---------------------------------------------------------
//   utick2utime
//---------------------------------------------------------

qreal RepeatList::utick2utime(int tick) const
      {
      unsigned n = size();
      unsigned ii = (idx1 < n) && (tick >= at(idx1)->utick) ? idx1 : 0;
      for (unsigned i = ii; i < n; ++i) {
            if ((tick >= at(i)->utick) && ((i + 1 == n) || (tick < at(i+1)->utick))) {
                  int t     = tick - (at(i)->utick - at(i)->tick);
                  qreal tt = _score->tempomap()->tick2time(t) + at(i)->timeOffset;
                  return tt;
                  }
            }
      return 0.0;
      }

//---------------------------------------------------------
//   utime2utick
//---------------------------------------------------------

int RepeatList::utime2utick(qreal t) const
      {
      unsigned n = size();
      unsigned ii = (idx2 < n) && (t >= at(idx2)->utime) ? idx2 : 0;
      for (unsigned i = ii; i < n; ++i) {
            if ((t >= at(i)->utime) && ((i + 1 == n) || (t < at(i+1)->utime))) {
                  idx2 = i;
                  return _score->tempomap()->time2tick(t - at(i)->timeOffset) + (at(i)->utick - at(i)->tick);
                  }
            }
      if (MScore::debugMode) {
            qFatal("time %f not found in RepeatList", t);
            }
      return 0;
      }

//---------------------------------------------------------
//   dump
//---------------------------------------------------------

void RepeatList::dump() const
      {
#if 0
      qDebug("==Dump Repeat List:==");
      foreach(const RepeatSegment* s, *this) {
            qDebug("%p  tick: %3d(%d) %3d(%d) len %d(%d) beats  %f + %f", s,
               s->utick / MScore::division,
               s->utick / MScore::division / 4,
               s->tick / MScore::division,
               s->tick / MScore::division / 4,
               s->len / MScore::division,
               s->len / MScore::division / 4,
               s->utime, s->timeOffset);
            }
#endif
      }

//---------------------------------------------------------
//   unwind
//    implements:
//          - repeats
//          - volta
//          - d.c. al fine
//          - d.s. al fine
//          - d.s. al coda
//---------------------------------------------------------

void RepeatList::unwind()
      {
      qDeleteAll(*this);
      clear();
      Measure* fm = _score->firstMeasure();
      if (!fm)
            return;

// qDebug("unwind===================");
      QList<Jump*> jumps; // take the jumps only once so store them
      rs                  = new RepeatSegment;
      rs->tick            = 0;
      Measure* endRepeat  = 0; // measure where the current repeat should stop
      Measure* continueAt = 0; // measure where the playback should continue after the repeat (To coda)
      int loop            = 0;
      int repeatCount     = 0;
      bool isGoto         = false;

      for (Measure* m = fm; m; m = m->nextMeasure())
            m->setPlaybackCount(0);

      for (Measure* m = fm; m;) {
            m->setPlaybackCount(m->playbackCount() + 1);
            Repeat flags = m->repeatFlags();
            bool doJump = false; // process jump after endrepeat

            // during any DC or DS, will take last time through repeat
            if (isGoto && (flags & Repeat::END))
                  loop = m->repeatCount() - 1;

//            qDebug("m%d(tick %7d) %p: playbackCount %d loop %d repeatCount %d isGoto %d endRepeat %p continueAt %p flags 0x%x",
//                   m->no()+1, m->tick(), m, m->playbackCount(), loop, repeatCount, isGoto, endRepeat, continueAt, int(flags));


            if (endRepeat) {
                  Volta* volta = _score->searchVolta(m->tick());
                  if (volta && !volta->hasEnding(m->playbackCount())) {
                        // skip measure
                        if (rs->tick < m->tick()) {
                              rs->len = m->tick() - rs->tick;
                              append(rs);
                              rs = new RepeatSegment;
                              }
                        rs->tick = m->endTick();
                        }
                  else if (flags & Repeat::JUMP) {
                        doJump = true;
                        isGoto = false;
                        }
                  }
            else if (flags & Repeat::JUMP) { // Jumps are only accepted outside of other repeats
                  doJump = true;
                  }
                  

            if (isGoto && (endRepeat == m)) {
                  if (continueAt == 0) {
// qDebug("  isGoto && endReapeat == %p, continueAt == 0", m);
                        rs->len = m->endTick() - rs->tick;
                        if (rs->len)
                              append(rs);
                        else
                              delete rs;
                        update();
                        dump();
                        return;
                        }
                  rs->len = m->endTick() - rs->tick;
                  append(rs);
                  rs       = new RepeatSegment;
                  rs->tick = continueAt->tick();
                  m        = continueAt;
                  isGoto   = false;
                  endRepeat = 0;
                  continue;
                  }
            else if (flags & Repeat::END) {
                  if (endRepeat == m) {
                        ++loop;
                        if (loop >= repeatCount) {
                              endRepeat = 0;
                              loop = 0;
                              }
                        else {
                              m = jumpToStartRepeat(m);
                              continue;
                              }
                        }
                  else if (endRepeat == 0) {
                        if (m->playbackCount() >= m->repeatCount())
                             break;
                        endRepeat   = m;
                        repeatCount = m->repeatCount();
                        loop        = 1;
                        m = jumpToStartRepeat(m);
                        continue;
                        }
                  }
            if (doJump && !isGoto) {
                  Jump* s = 0;
                  foreach(Element* e, m->el()) {
                        if (e->type() == Element::Type::JUMP) {
                              s = static_cast<Jump*>(e);
                              break;
                              }
                        }
                  // jump only once
                  if (jumps.contains(s)) {
                        m = m->nextMeasure();
                        if (endRepeat == _score->searchLabel(s->playUntil()))
                              endRepeat = 0;
                        continue;
                        }
                  jumps.append(s);
                  if (s) {
                        Measure* nm = _score->searchLabel(s->jumpTo());
                        endRepeat   = _score->searchLabel(s->playUntil());
                        continueAt  = _score->searchLabel(s->continueAt());

                        if (nm && endRepeat) {
                              isGoto      = true;
                              rs->len = m->endTick() - rs->tick;
                              append(rs);
                              rs = new RepeatSegment;
                              rs->tick  = nm->tick();
                              m = nm;
                              continue;
                              }
                        }
                  else
                        qDebug("Jump not found");
                  }
            m = m->nextMeasure();
            }

      if (rs) {
            Measure* lm = _score->lastMeasure();
            rs->len     = lm->endTick() - rs->tick;
            if (rs->len)
                  append(rs);
            else
                  delete rs;
            }
      update();
      dump();
      }

//---------------------------------------------------------
//   jumpToStartRepeat
//---------------------------------------------------------

Measure* RepeatList::jumpToStartRepeat(Measure* m)
      {
      // finalize the previous repeat segment
      rs->len = m->tick() + m->ticks() - rs->tick;
      append(rs);

      // search backwards until find start of repeat
      while (true) {

            if (m->repeatFlags() & Repeat::START)
                  break;

            if (m == _score->firstMeasure())
                  break;

            if (m->prevMeasure()->sectionBreak())
                  break;

            m = m->prevMeasure();
            }

      // initialize the next repeat segment
      rs        = new RepeatSegment;
      rs->tick  = m->tick();
      return m;
      }

}

