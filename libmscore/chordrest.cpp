//=============================================================================
//  MuseScore
//  Music Composition & Notation
//
//  Copyright (C) 2002-2011 Werner Schweer
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License version 2
//  as published by the Free Software Foundation and appearing in
//  the file LICENCE.GPL
//=============================================================================

#include "chordrest.h"
#include "chord.h"
#include "xml.h"
#include "style.h"
#include "system.h"
#include "measure.h"
#include "staff.h"
#include "tuplet.h"
#include "score.h"
#include "sym.h"
#include "slur.h"
#include "beam.h"
#include "breath.h"
#include "barline.h"
#include "articulation.h"
#include "tempo.h"
#include "tempotext.h"
#include "note.h"
#include "arpeggio.h"
#include "dynamic.h"
#include "stafftext.h"
#include "sig.h"
#include "clef.h"
#include "lyrics.h"
#include "segment.h"
#include "stafftype.h"
#include "undo.h"
#include "stem.h"
#include "harmony.h"
#include "figuredbass.h"
#include "icon.h"
#include "utils.h"
#include "keysig.h"

namespace Ms {

//---------------------------------------------------------
//   hasArticulation
//---------------------------------------------------------

Articulation* ChordRest::hasArticulation(const Articulation* aa)
      {
      ArticulationType idx = aa->articulationType();
      foreach(Articulation* a, _articulations) {
            if (idx == a->articulationType())
                  return a;
            }
      return 0;
      }

//---------------------------------------------------------
//   ChordRest
//---------------------------------------------------------

ChordRest::ChordRest(Score* s)
   : DurationElement(s)
      {
      _staffMove   = 0;
      _beam        = 0;
      _tabDur      = 0;
      _beamMode    = Beam::Mode::AUTO;
      _up          = true;
      _small       = false;
      _crossMeasure = CrossMeasure::UNKNOWN;
      }

ChordRest::ChordRest(const ChordRest& cr, bool link)
   : DurationElement(cr)
      {
      _durationType = cr._durationType;
      _staffMove    = cr._staffMove;
      _beam         = 0;
      _tabDur       = 0;  // tab sur. symb. depends upon context: can't be
                          // simply copied from another CR

      for (Articulation* a : cr._articulations) {    // make deep copy
            Articulation* na = new Articulation(*a);
            if (link)
                  na->linkTo(a);
            na->setParent(this);
            na->setTrack(track());
            _articulations.append(na);
            }

      _beamMode     = cr._beamMode;
      _up           = cr._up;
      _small        = cr._small;
      _crossMeasure = cr._crossMeasure;
      _space        = cr._space;

      for (Lyrics* l : cr._lyricsList) {        // make deep copy
            if (l == 0) {
                  _lyricsList.append(0);
                  continue;
                  }
            Lyrics* nl = new Lyrics(*l);
            if (link)
                  nl->linkTo(l);
            nl->setParent(this);
            nl->setTrack(track());
            _lyricsList.append(nl);
            }
      }

//---------------------------------------------------------
//   undoUnlink
//---------------------------------------------------------

void ChordRest::undoUnlink()
      {
      DurationElement::undoUnlink();
      for (Articulation* a : _articulations)
            a->undoUnlink();
      for (Lyrics* l : _lyricsList) {
            if (l)
                  l->undoUnlink();
            }
      }

//---------------------------------------------------------
//   ChordRest
//---------------------------------------------------------

ChordRest::~ChordRest()
      {
      foreach (Articulation* a,  _articulations)
            delete a;
      foreach (Lyrics* l, _lyricsList)
            delete l;
      if (_tabDur)
            delete _tabDur;
      }

//---------------------------------------------------------
//   scanElements
//---------------------------------------------------------

void ChordRest::scanElements(void* data, void (*func)(void*, Element*), bool all)
      {
      if (_beam && (_beam->elements().front() == this)
       && !measure()->slashStyle(staffIdx()))
            _beam->scanElements(data, func, all);
      foreach(Articulation* a, _articulations)
            func(data, a);
      foreach(Lyrics* l, _lyricsList) {
            if (l)
                  l->scanElements(data, func, all);
            }
      DurationElement* de = this;
      while (de->tuplet() && de->tuplet()->elements().front() == de) {
            de->tuplet()->scanElements(data, func, all);
            de = de->tuplet();
            }
      if (_tabDur)
            func(data, _tabDur);
      }

//---------------------------------------------------------
//   writeProperties
//---------------------------------------------------------

void ChordRest::writeProperties(Xml& xml) const
      {
      DurationElement::writeProperties(xml);

      //
      // Beam::Mode default:
      //    REST  - Beam::Mode::NONE
      //    CHORD - Beam::Mode::AUTO
      //
      if ((type() == Element::Type::REST && _beamMode != Beam::Mode::NONE)
         || (type() == Element::Type::CHORD && _beamMode != Beam::Mode::AUTO)) {
            QString s;
            switch(_beamMode) {
                  case Beam::Mode::AUTO:    s = "auto"; break;
                  case Beam::Mode::BEGIN:   s = "begin"; break;
                  case Beam::Mode::MID:     s = "mid"; break;
                  case Beam::Mode::END:     s = "end"; break;
                  case Beam::Mode::NONE:    s = "no"; break;
                  case Beam::Mode::BEGIN32: s = "begin32"; break;
                  case Beam::Mode::BEGIN64: s = "begin64"; break;
                  case Beam::Mode::INVALID: s = "?"; break;
                  }
            xml.tag("BeamMode", s);
            }
      writeProperty(xml, P_ID::SMALL);
      if (actualDurationType().dots())
            xml.tag("dots", actualDurationType().dots());
      writeProperty(xml, P_ID::STAFF_MOVE);

      if (actualDurationType().isValid())
            xml.tag("durationType", actualDurationType().name());

      if (!duration().isZero() && (!actualDurationType().fraction().isValid()
         || (actualDurationType().fraction() != duration())))
            xml.fTag("duration", duration());

      foreach(const Articulation* a, _articulations)
            a->write(xml);
#ifndef NDEBUG
      if (_beam && (MScore::testMode || !_beam->generated()))
            xml.tag("Beam", _beam->id());
#else
      if (_beam && !_beam->generated())
            xml.tag("Beam", _beam->id());
#endif
      foreach(Lyrics* lyrics, _lyricsList) {
            if (lyrics)
                  lyrics->write(xml);
            }
      if (!isGrace()) {
            Fraction t(globalDuration());
            if (staff())
                  t /= staff()->timeStretch(xml.curTick);
            xml.curTick += t.ticks();
            }
      for (auto i : score()->spanner()) {     // TODO: dont search whole list
            Spanner* s = i.second;
            if (s->generated() || s->type() != Element::Type::SLUR || !xml.canWrite(s))
                  continue;

            if (s->startElement() == this) {
                  int id = xml.spannerId(s);
                  xml.tagE(QString("Slur type=\"start\" id=\"%1\"").arg(id));
                  }
            else if (s->endElement() == this) {
                  int id = xml.spannerId(s);
                  xml.tagE(QString("Slur type=\"stop\" id=\"%1\"").arg(id));
                  }
            }
      }

//---------------------------------------------------------
//   readProperties
//---------------------------------------------------------

bool ChordRest::readProperties(XmlReader& e)
      {
      const QStringRef& tag(e.name());

      if (tag == "durationType") {
            setDurationType(e.readElementText());
            if (actualDurationType().type() != TDuration::DurationType::V_MEASURE) {
                  if (score()->mscVersion() < 112 && (type() == Element::Type::REST) &&
                              // for backward compatibility, convert V_WHOLE rests to V_MEASURE
                              // if long enough to fill a measure.
                              // OTOH, freshly created (un-initialized) rests have numerator == 0 (< 4/4)
                              // (see Fraction() constructor in fraction.h; this happens for instance
                              // when pasting selection from clipboard): they should not be converted
                              duration().numerator() != 0 &&
                              // rest durations are initialized to full measure duration when
                              // created upon reading the <Rest> tag (see Measure::read() )
                              // so a V_WHOLE rest in a measure of 4/4 or less => V_MEASURE
                              (actualDurationType()==TDuration::DurationType::V_WHOLE && duration() <= Fraction(4, 4)) ) {
                        // old pre 2.0 scores: convert
                        setDurationType(TDuration::DurationType::V_MEASURE);
                        }
                  else  // not from old score: set duration fraction from duration type
                        setDuration(actualDurationType().fraction());
                  }
            else {
                  if (score()->mscVersion() < 115) {
                        SigEvent event = score()->sigmap()->timesig(e.tick());
                        setDuration(event.timesig());
                        }
                  }
            }
      else if (tag == "BeamMode") {
            QString val(e.readElementText());
            Beam::Mode bm = Beam::Mode::AUTO;
            if (val == "auto")
                  bm = Beam::Mode::AUTO;
            else if (val == "begin")
                  bm = Beam::Mode::BEGIN;
            else if (val == "mid")
                  bm = Beam::Mode::MID;
            else if (val == "end")
                  bm = Beam::Mode::END;
            else if (val == "no")
                  bm = Beam::Mode::NONE;
            else if (val == "begin32")
                  bm = Beam::Mode::BEGIN32;
            else if (val == "begin64")
                  bm = Beam::Mode::BEGIN64;
            else
                  bm = Beam::Mode(val.toInt());
            _beamMode = Beam::Mode(bm);
            }
      else if (tag == "Attribute" || tag == "Articulation") {     // obsolete: "Attribute"
            Articulation* atr = new Articulation(score());
            atr->setTrack(track());
            atr->read(e);
            add(atr);
            }
      else if (tag == "leadingSpace") {
            qDebug("ChordRest: leadingSpace obsolete"); // _extraLeadingSpace = Spatium(val.toDouble());
            e.skipCurrentElement();
            }
      else if (tag == "trailingSpace") {
            qDebug("ChordRest: trailingSpace obsolete"); // _extraTrailingSpace = Spatium(val.toDouble());
            e.skipCurrentElement();
            }
      else if (tag == "Beam") {
            int id = e.readInt();
            Beam* beam = e.findBeam(id);
            if (beam)
                  beam->add(this);        // also calls this->setBeam(beam)
            else
                  qDebug("Beam id %d not found", id);
            }
      else if (tag == "small")
            _small = e.readInt();
      else if (tag == "duration")
            setDuration(e.readFraction());
      else if (tag == "ticklen") {      // obsolete (version < 1.12)
            int mticks = score()->sigmap()->timesig(e.tick()).timesig().ticks();
            int i = e.readInt();
            if (i == 0)
                  i = mticks;
            if ((type() == Element::Type::REST) && (mticks == i)) {
                  setDurationType(TDuration::DurationType::V_MEASURE);
                  setDuration(Fraction::fromTicks(i));
                  }
            else {
                  Fraction f = Fraction::fromTicks(i);
                  setDuration(f);
                  setDurationType(TDuration(f));
                  }
            }
      else if (tag == "dots")
            setDots(e.readInt());
      else if (tag == "move")
            _staffMove = e.readInt();
      else if (tag == "Slur") {
            int id = e.intAttribute("id");
            if (id == 0)
                  id = e.intAttribute("number");                  // obsolete
            Spanner* spanner = e.findSpanner(id);
            QString atype(e.attribute("type"));

            if (!spanner) {
                  if (atype == "stop") {
                        SpannerValues sv;
                        sv.spannerId = id;
                        sv.track2    = track();
                        sv.tick2     = e.tick();
                        e.addSpannerValues(sv);
                        }
                  else if (atype == "start")
                        qDebug("spanner: start without spanner");
                  }
            else {
                  if (atype == "start") {
                        if (spanner->ticks() > 0 && spanner->tick() == -1) // stop has been read first
                              spanner->setTicks(spanner->ticks() - e.tick() - 1);
                        spanner->setTick(e.tick());
                        spanner->setTrack(track());
                        if (spanner->type() == Element::Type::SLUR)
                              spanner->setStartElement(this);
                        if (e.pasteMode()) {
                              for (ScoreElement* e : spanner->linkList()) {
                                    if (e == spanner)
                                          continue;
                                    Spanner* ls = static_cast<Spanner*>(e);
                                    ls->setTick(spanner->tick());
                                    for (ScoreElement* ee : linkList()) {
                                          ChordRest* cr = static_cast<ChordRest*>(ee);
                                          if (cr->score() == ee->score() && cr->staffIdx() == ls->staffIdx()) {
                                                ls->setTrack(cr->track());
                                                if (ls->type() == Element::Type::SLUR)
                                                      ls->setStartElement(cr);
                                                break;
                                                }
                                          }
                                    }
                              }
                        }
                  else if (atype == "stop") {
                        spanner->setTick2(e.tick());
                        spanner->setTrack2(track());
                        if (spanner->type() == Element::Type::SLUR)
                              spanner->setEndElement(this);
                        Chord* start = static_cast<Chord*>(spanner->startElement());
                        if (start)
                              spanner->setTrack(start->track());
                        if (e.pasteMode()) {
                              for (ScoreElement* e : spanner->linkList()) {
                                    if (e == spanner)
                                          continue;
                                    Spanner* ls = static_cast<Spanner*>(e);
                                    ls->setTick2(spanner->tick2());
                                    for (ScoreElement* ee : linkList()) {
                                          ChordRest* cr = static_cast<ChordRest*>(ee);
                                          if (cr->score() == ee->score() && cr->staffIdx() == ls->staffIdx()) {
                                                ls->setTrack2(cr->track());
                                                if (ls->type() == Element::Type::SLUR)
                                                      ls->setEndElement(cr);
                                                break;
                                                }
                                          }
                                    }
                              }
                        }
                  else
                        qDebug("ChordRest::read(): unknown Slur type <%s>", qPrintable(atype));
                  }
            e.readNext();
            }
      else if (tag == "Lyrics" /*|| tag == "FiguredBass"*/) {
            Element* element = Element::name2Element(tag, score());
            element->setTrack(e.track());
            element->read(e);
            add(element);
            }
      else if (tag == "pos") {
            QPointF pt = e.readPoint();
            if (score()->mscVersion() > 114)
                  setUserOff(pt * spatium());
            }
      else if (tag == "offset") {
            if (score()->mscVersion() > 114) // || voice() >= 2) {
                  DurationElement::readProperties(e);
            else if (type() == Element::Type::REST) {
                  DurationElement::readProperties(e);
                  setUserXoffset(0.0); // honor Y offset but not X for rests in older scores
                  }
            else
                  e.skipCurrentElement(); // ignore manual layout otherwise
            }
      else if (DurationElement::readProperties(e))
            return true;
      else
            return false;
      return true;
      }

//---------------------------------------------------------
//   setSmall
//---------------------------------------------------------

void ChordRest::setSmall(bool val)
      {
      _small = val;
      }

//---------------------------------------------------------
//   undoSetSmall
//---------------------------------------------------------

void ChordRest::undoSetSmall(bool val)
      {
      undoChangeProperty(P_ID::SMALL, val);
      }

//---------------------------------------------------------
//   layoutArticulations
//    called from chord()->layout()
//---------------------------------------------------------

void ChordRest::layoutArticulations()
      {
      if (parent() == 0 || _articulations.isEmpty())
            return;
      qreal _spatium  = spatium();
      qreal _spStaff  = _spatium * staff()->lineDistance(); // scaled to staff line distance for vert. pos. within a staff

      if (type() == Element::Type::CHORD) {
            if (_articulations.size() == 1) {
                  static_cast<Chord*>(this)->layoutArticulation(_articulations[0]);
                  return;
                  }
            if (_articulations.size() == 2) {
                  //
                  // staccato | tenuto + marcato
                  //
                  Articulation* a1 = _articulations[0];
                  Articulation* a2 = _articulations[1];
                  ArticulationType st1 = a1->articulationType();
                  ArticulationType st2 = a2->articulationType();

                  if ((st2 == ArticulationType::Tenuto || st2 == ArticulationType::Staccato)
                     && (st1 == ArticulationType::Marcato)) {
                        qSwap(a1, a2);
                        qSwap(st1, st2);
                        }
                  if ((st1 == ArticulationType::Tenuto || st1 == ArticulationType::Staccato)
                     && (st2 == ArticulationType::Marcato)) {
                        QPointF pt = static_cast<Chord*>(this)->layoutArticulation(a1);
                        pt.ry() += a1->up() ? -_spStaff * .5 : _spStaff * .5;
                        a2->layout();
                        a2->setUp(a1->up());
                        a2->setPos(pt);
                        a2->adjustReadPos();
                        return;
                        }
                  //
                  // staccato | tenuto + sforzato
                  //
                  if ((st2 == ArticulationType::Tenuto || st2 == ArticulationType::Staccato)
                     && (st1 == ArticulationType::Sforzatoaccent)) {
                        qSwap(a1, a2);
                        qSwap(st1, st2);
                        }
                  if ((st1 == ArticulationType::Tenuto || st1 == ArticulationType::Staccato)
                     && (st2 == ArticulationType::Sforzatoaccent)) {
                        QPointF pt = static_cast<Chord*>(this)->layoutArticulation(a1);
                        pt.ry() += a1->up() ? -_spStaff * .7 : _spStaff * .7;
                        a2->layout();
                        a2->setUp(a1->up());
                        a2->setPos(pt);
                        a2->adjustReadPos();
                        return;
                        }
                  }
            }

      qreal x         = centerX();
      qreal distance0 = score()->styleS(StyleIdx::propertyDistance).val()     * _spatium;
      qreal distance1 = score()->styleS(StyleIdx::propertyDistanceHead).val() * _spatium;
      qreal distance2 = score()->styleS(StyleIdx::propertyDistanceStem).val() * _spatium;

      qreal chordTopY = upPos();    // note position of highest note
      qreal chordBotY = downPos();  // note position of lowest note

      qreal staffTopY = -distance2;
      qreal staffBotY = staff()->height() + distance2;

      // avoid collisions of staff articulations with chord notes:
      // gap between note and staff articulation is distance0 + 0.5 spatium

      if (type() == Element::Type::CHORD) {
            Chord* chord = static_cast<Chord*>(this);
            Stem* stem   = chord->stem();
            if (stem) {
                  qreal y = stem->pos().y() + pos().y();
                  if (up() && stem->stemLen() < 0.0)
                        y += stem->stemLen();
                  else if (!up() && stem->stemLen() > 0.0)
                        y -= stem->stemLen();

                  if (beam()) {
                        qreal bw = score()->styleS(StyleIdx::beamWidth).val() * _spatium;
                        y += up() ? -bw : bw;
                        }
                  if (up())
                        staffTopY = qMin(staffTopY, qreal(y - 0.5 * _spatium));
                  else
                        staffBotY = qMax(staffBotY, qreal(y + 0.5 * _spatium));
                  }
            }

      staffTopY = qMin(staffTopY, qreal(chordTopY - distance0 - 0.5 * _spatium));
      staffBotY = qMax(staffBotY, qreal(chordBotY + distance0 + 0.5 * _spatium));

      qreal dy = 0.0;

      int n = _articulations.size();
      for (int i = 0; i < n; ++i) {
            Articulation* a = _articulations.at(i);
            //
            // determine MScore::Direction
            //
            if (a->direction() != MScore::Direction::AUTO) {
                  a->setUp(a->direction() == MScore::Direction::UP);
                  }
            else {
                  if (a->anchor() == ArticulationAnchor::CHORD)
                        a->setUp(!up());
                  else
                        a->setUp(a->anchor() == ArticulationAnchor::TOP_STAFF || a->anchor() == ArticulationAnchor::TOP_CHORD);
                  }
            }

      //
      //    pass 1
      //    place tenuto and staccato
      //

      for (int i = 0; i < n; ++i) {
            Articulation* a = _articulations.at(i);
            a->layout();
            ArticulationAnchor aa = a->anchor();

            if ((a->articulationType() != ArticulationType::Tenuto)
               && (a->articulationType() != ArticulationType::Staccato))
                  continue;

            if (aa != ArticulationAnchor::CHORD && aa != ArticulationAnchor::TOP_CHORD && aa != ArticulationAnchor::BOTTOM_CHORD)
                  continue;

            bool bottom;
            if ((aa == ArticulationAnchor::CHORD) && measure()->hasVoices(a->staffIdx()))
                  bottom = !up();
            else
                  bottom = (aa == ArticulationAnchor::BOTTOM_CHORD) || (aa == ArticulationAnchor::CHORD && up());
            bool headSide = bottom == up();

            dy += distance1;
            qreal y;
            Chord* chord = static_cast<Chord*>(this);
            if (bottom) {
                  int line = downLine();
                  y = chordBotY + dy;
                  if (!headSide && type() == Element::Type::CHORD && chord->stem()) {
                        Stem* stem = chord->stem();
                        y          = chordTopY + stem->stemLen();
                        if (chord->beam())
                              y += score()->styleS(StyleIdx::beamWidth).val() * _spatium * .5;
                        // aligning horizontally to stem makes sense only for staccato
                        // and only if no other articulations on this side
                        //x = stem->pos().x();
                        int line   = lrint((y+0.5*_spatium) / _spatium);
                        if (line <= 4)    // align between staff lines
                              y = line * _spatium + _spatium * .5;
                        else
                              y += _spatium;
                        }
                  else {
                        int lines = (staff()->lines() - 1) * 2;
                        if (line < lines)
                              y = (line & ~1) + 3;
                        else
                              y = line + 2;
                        y *= _spatium * .5;
                        }
                  }
            else {
                  int line = upLine();
                  y = chordTopY - dy;
                  if (!headSide && type() == Element::Type::CHORD && chord->stem()) {
                        Stem* stem = chord->stem();
                        y          = chordBotY + stem->stemLen();
                        if (chord->beam())
                              y -= score()->styleS(StyleIdx::beamWidth).val() * _spatium * .5;
                        // aligning horizontally to stem makes sense only for staccato
                        // and only if no other articulations on this side
                        //x = stem->pos().x();
                        int line   = lrint((y-0.5*_spatium) / _spatium);
                        if (line >= 0)    // align between staff lines
                              y = line * _spatium - _spatium * .5;
                        else
                              y -= _spatium;
                        }
                  else {
                        if (line > 0)
                              y = ((line+1) & ~1) - 3;
                        else
                              y = line - 2;
                        y *= _spatium * .5;
                        }
                  }
            dy += _spatium * .5;
            a->setPos(x, y);
            }

      // reserve space for slur
      bool botGap = false;
      bool topGap = false;

#if 0 // TODO-S: optimize
      for (Spanner* sp = _spannerFor; sp; sp = sp->next()) {
            if (sp->type() != SLUR)
                  continue;
            Slur* s = static_cast<Slur*>(sp);
            if (s->up())
                  topGap = true;
            else
                  botGap = true;
            }
      for (Spanner* sp = _spannerBack; sp; sp = sp->next()) {
            if (sp->type() != SLUR)
                  continue;
            Slur* s = static_cast<Slur*>(sp);
            if (s->up())
                  topGap = true;
            else
                  botGap = true;
            }
#endif
      if (botGap)
            chordBotY += _spatium;
      if (topGap)
            chordTopY -= _spatium;

      //
      //    pass 2
      //    place all articulations with anchor at chord/rest
      //
      n = _articulations.size();
      for (int i = 0; i < n; ++i) {
            Articulation* a = _articulations.at(i);
            a->layout();
            ArticulationAnchor aa = a->anchor();
            if ((a->articulationType() == ArticulationType::Tenuto)
               || (a->articulationType() == ArticulationType::Staccato))
                  continue;

            if (aa != ArticulationAnchor::CHORD && aa != ArticulationAnchor::TOP_CHORD && aa != ArticulationAnchor::BOTTOM_CHORD)
                  continue;

            // for tenuto and staccate check for staff line collision
            bool staffLineCT = a->articulationType() == ArticulationType::Tenuto
                               || a->articulationType() == ArticulationType::Staccato;

//            qreal sh = a->bbox().height() * mag();
            bool bottom = (aa == ArticulationAnchor::BOTTOM_CHORD) || (aa == ArticulationAnchor::CHORD && up());

            dy += distance1;
            if (bottom) {
                  qreal y = chordBotY + dy;
                  if (staffLineCT && (y <= staffBotY -.1 - dy)) {
                        qreal l = y / _spatium;
                        qreal delta = fabs(l - round(l));
                        if (delta < 0.4) {
                              y  += _spatium * .5;
                              dy += _spatium * .5;
                              }
                        }
                  a->setPos(x, y); // - a->bbox().y() + a->bbox().height() * .5);
                  }
            else {
                  qreal y = chordTopY - dy;
                  if (staffLineCT && (y >= (staffTopY +.1 + dy))) {
                        qreal l = y / _spatium;
                        qreal delta = fabs(l - round(l));
                        if (delta < 0.4) {
                              y  -= _spatium * .5;
                              dy += _spatium * .5;
                              }
                        }
                  a->setPos(x, y); // + a->bbox().y() - a->bbox().height() * .5);
                  }
            }

      //
      //    pass 3
      //    now place all articulations with staff top or bottom anchor
      //
      qreal dyTop = staffTopY;
      qreal dyBot = staffBotY;

/*      if ((upPos() - _spatium) < dyTop)
            dyTop = upPos() - _spatium;
      if ((downPos() + _spatium) > dyBot)
            dyBot = downPos() + _spatium;
  */
      for (int i = 0; i < n; ++i) {
            Articulation* a = _articulations.at(i);
            ArticulationAnchor aa = a->anchor();
            if (aa == ArticulationAnchor::TOP_STAFF || aa == ArticulationAnchor::BOTTOM_STAFF) {
                  if (a->up()) {
                        a->setPos(x, dyTop);
                        dyTop -= distance0;
                        }
                  else {
                        a->setPos(x, dyBot);
                        dyBot += distance0;
                        }
                  }
            a->adjustReadPos();
            }
      }

//---------------------------------------------------------
//   drop
//---------------------------------------------------------

Element* ChordRest::drop(const DropData& data)
      {
      Element* e       = data.element;
      Measure* m       = measure();
      bool fromPalette = (e->track() == -1);
      switch (e->type()) {
            case Element::Type::BREATH:
                  {
                  Breath* b = static_cast<Breath*>(e);
                  int track = staffIdx() * VOICES;
                  b->setTrack(track);

                  // find start tick of next note in staff
#if 0
                  int bt = tick() + actualTicks();    // this could make sense if we allowed breath marks in voice > 1
#else
                  Segment* next = segment()->nextCR(track);
                  int bt = next ? next->tick() : score()->lastSegment()->tick();
#endif

                  // TODO: insert automatically in all staves?

                  Segment* seg = m->undoGetSegment(Segment::Type::Breath, bt);
                  b->setParent(seg);
                  score()->undoAddElement(b);
                  }
                  return e;

            case Element::Type::BAR_LINE:
                  {
                  BarLine* bl = static_cast<BarLine*>(e);
                  bl->setTrack(staffIdx() * VOICES);
                  bl->setGenerated(false);

                  if (tick() == m->tick())
                        return m->drop(data);

                  BarLine* obl = 0;
                  for (Staff* st  : staff()->staffList()) {
                        Score* score = st->score();
                        Measure* measure = score->tick2measure(m->tick());
                        Segment* seg = measure->undoGetSegment(Segment::Type::BarLine, tick());
                        BarLine* l;
                        if (obl == 0)
                              obl = l = bl->clone();
                        else
                              l = static_cast<BarLine*>(obl->linkedClone());
                        l->setTrack(st->idx() * VOICES);
                        l->setScore(score);
                        l->setParent(seg);
                        score->undoAddElement(l);
                        }
                  }
                  delete e;
                  return 0;

            case Element::Type::CLEF:
                  score()->cmdInsertClef(static_cast<Clef*>(e), this);
                  break;

            case Element::Type::TIMESIG:
                  return measure()->drop(data);

            case Element::Type::TEMPO_TEXT:
                  {
                  TempoText* tt = static_cast<TempoText*>(e);
                  tt->setTrack(0);
                  tt->setParent(segment());
                  TextStyleType st = tt->textStyleType();
                  //tt->setTextStyleType(st);
                  if (st >= TextStyleType::DEFAULT && fromPalette)
                        tt->textStyle().restyle(MScore::baseStyle()->textStyle(st), score()->textStyle(st));
                  score()->undoAddElement(tt);
                  }
                  return e;

            case Element::Type::DYNAMIC:
                  {
                  Dynamic* d = static_cast<Dynamic*>(e);
                  d->setTrack(track());
                  TextStyleType st = d->textStyleType();
                  //d->setTextStyleType(st);
                  if (st >= TextStyleType::DEFAULT && fromPalette)
                        d->textStyle().restyle(MScore::baseStyle()->textStyle(st), score()->textStyle(st));
                  d->setParent(segment());
                  score()->undoAddElement(d);
                  }
                  return e;
            case Element::Type::FRET_DIAGRAM:
            case Element::Type::TREMOLOBAR:
            case Element::Type::SYMBOL:
                  e->setTrack(track());
                  e->setParent(segment());
                  score()->undoAddElement(e);
                  return e;

            case Element::Type::NOTE:
                  {
                  Note* note = static_cast<Note*>(e);
                  NoteVal nval;
                  nval.pitch = note->pitch();
                  nval.headGroup = note->headGroup();
                  nval.fret = note->fret();
                  nval.string = note->string();
                  score()->setNoteRest(segment(), track(), nval, data.duration, MScore::Direction::AUTO);
                  delete e;
                  }
                  break;

            case Element::Type::HARMONY:
                  {
                  // transpose
                  Harmony* harmony = static_cast<Harmony*>(e);
                  Interval interval = staff()->part()->instrument()->transpose();
                  if (!score()->styleB(StyleIdx::concertPitch) && !interval.isZero()) {
                        interval.flip();
                        int rootTpc = transposeTpc(harmony->rootTpc(), interval, true);
                        int baseTpc = transposeTpc(harmony->baseTpc(), interval, true);
                        score()->undoTransposeHarmony(harmony, rootTpc, baseTpc);
                        }
                  // render
                  harmony->render();
                  }
                  // fall through
            case Element::Type::TEXT:
            case Element::Type::STAFF_TEXT:
            case Element::Type::STAFF_STATE:
            case Element::Type::INSTRUMENT_CHANGE:
                  if (e->type() == Element::Type::INSTRUMENT_CHANGE
                     && part()->instruments()->find(tick()) != part()->instruments()->end()) {
                        qDebug()<<"InstrumentChange already exists at tick = "<<tick();
                        delete e;
                        return 0;
                        }
                  // fall through
            case Element::Type::REHEARSAL_MARK:
                  e->setParent(segment());
                  e->setTrack((track() / VOICES) * VOICES);
                  {
                  Text* t = static_cast<Text*>(e);
                  TextStyleType st = t->textStyleType();
                  // for palette items, we want to use current score text style settings
                  // except where the source element had explicitly overridden these via text properties
                  // palette text style will be relative to baseStyle, so rebase this to score
                  //f->setTextStyleType(st);
                  if (st >= TextStyleType::DEFAULT && fromPalette)
                        t->textStyle().restyle(MScore::baseStyle()->textStyle(st), score()->textStyle(st));
                  if (e->type() == Element::Type::REHEARSAL_MARK && fromPalette)
                        t->setXmlText(score()->createRehearsalMarkText(static_cast<RehearsalMark*>(e)));
                  }
                  score()->undoAddElement(e);
                  return e;

            case Element::Type::FIGURED_BASS:
                  {
                  bool bNew;
                  FiguredBass * fb = static_cast<FiguredBass *>(e);
                  fb->setParent( segment() );
                  fb->setTrack( (track() / VOICES) * VOICES );
                  fb->setTicks( duration().ticks() );
                  fb->setOnNote(true);
                  FiguredBass::addFiguredBassToSegment(segment(),
                        fb->track(), fb->ticks(), &bNew);
                  if (bNew)
                        score()->undoAddElement(e);
                  return e;
                  }

            case Element::Type::IMAGE:
                  e->setParent(segment());
                  score()->undoAddElement(e);
                  return e;

            case Element::Type::ICON:
                  {
                  switch(static_cast<Icon*>(e)->iconType()) {
                        case IconType::SBEAM:
                              score()->undoChangeProperty(this, P_ID::BEAM_MODE, int(Beam::Mode::BEGIN));
                              break;
                        case IconType::MBEAM:
                              score()->undoChangeProperty(this, P_ID::BEAM_MODE, int(Beam::Mode::MID));
                              break;
                        case IconType::NBEAM:
                              score()->undoChangeProperty(this, P_ID::BEAM_MODE, int(Beam::Mode::NONE));
                              break;
                        case IconType::BEAM32:
                              score()->undoChangeProperty(this, P_ID::BEAM_MODE, int(Beam::Mode::BEGIN32));
                              break;
                        case IconType::BEAM64:
                              score()->undoChangeProperty(this, P_ID::BEAM_MODE, int(Beam::Mode::BEGIN64));
                              break;
                        case IconType::AUTOBEAM:
                              score()->undoChangeProperty(this, P_ID::BEAM_MODE, int(Beam::Mode::AUTO));
                              break;
                        default:
                              break;
                        }
                  }
                  delete e;
                  break;

            case Element::Type::KEYSIG:
                  {
                  KeySig* ks    = static_cast<KeySig*>(e);
                  KeySigEvent k = ks->keySigEvent();
                  delete ks;

                  // apply only to this stave
                  score()->undoChangeKeySig(staff(), tick(), k);
                  }
                  break;

            default:
                  qDebug("cannot drop %s", e->name());
                  delete e;
                  return 0;
            }
      return 0;
      }

//---------------------------------------------------------
//   setBeam
//---------------------------------------------------------

void ChordRest::setBeam(Beam* b)
      {
      _beam = b;
      }

//---------------------------------------------------------
//   setDurationType
//---------------------------------------------------------

void ChordRest::setDurationType(TDuration::DurationType t)
      {
      _durationType.setType(t);
      _crossMeasure = CrossMeasure::UNKNOWN;
      }

void ChordRest::setDurationType(const QString& s)
      {
      _durationType.setType(s);
      _crossMeasure = CrossMeasure::UNKNOWN;
      }

void ChordRest::setDurationType(int ticks)
      {
      _durationType.setVal(ticks);
      _crossMeasure = CrossMeasure::UNKNOWN;
      }

void ChordRest::setDurationType(TDuration v)
      {
      _durationType = v;
      _crossMeasure = CrossMeasure::UNKNOWN;
      }

//---------------------------------------------------------
//   durationUserName
//---------------------------------------------------------

QString ChordRest::durationUserName()
      {
      QString tupletType = "";
      if (tuplet()) {
              switch (tuplet()->ratio().numerator()) {
                  case 2:
                        tupletType = tr("Duplet");
                        break;
                  case 3:
                        tupletType = tr("Triplet");
                        break;
                  case 4:
                        tupletType = tr("Quadruplet");
                        break;
                  case 5:
                        tupletType = tr("Quintuplet");
                        break;
                  case 6:
                        tupletType = tr("Sextuplet");
                        break;
                  case 7:
                        tupletType = tr("Septuplet");
                        break;
                  case 8:
                        tupletType = tr("Octuplet");
                        break;
                  case 9:
                        tupletType = tr("Nonuplet");
                        break;
                  default:
                        tupletType = tr("Custom Tuplet");
                  }
            }
      QString dotString = "";
      if(!tupletType.isEmpty())
          dotString += " ";

      switch (dots()) {
            case 1:
                  dotString += tr("Dotted %1").arg(durationType().durationTypeUserName()).trimmed();
                  break;
            case 2:
                  dotString += tr("Double dotted %1").arg(durationType().durationTypeUserName()).trimmed();
                  break;
            case 3:
                  dotString += tr("Triple dotted %1").arg(durationType().durationTypeUserName()).trimmed();
                  break;
            default:
                  dotString += durationType().durationTypeUserName();
            }
      return QString("%1%2").arg(tupletType).arg(dotString);
      }

//---------------------------------------------------------
//   setTrack
//---------------------------------------------------------

void ChordRest::setTrack(int val)
      {
      foreach(Articulation* a, _articulations)
            a->setTrack(val);
      Element::setTrack(val);
      if (type() == Element::Type::CHORD) {
            foreach(Note* n, static_cast<Chord*>(this)->notes())
                  n->setTrack(val);
            }
      if (_beam)
            _beam->setTrack(val);
      foreach(Lyrics* l, _lyricsList) {
            if (l)
                  l->setTrack(val);
            }
      if (tuplet())
            tuplet()->setTrack(val);
      }

//---------------------------------------------------------
//   tick
//---------------------------------------------------------

int ChordRest::tick() const
      {
      return segment() ? segment()->tick() : -1;
      }

//---------------------------------------------------------
//   rtick
//---------------------------------------------------------

int ChordRest::rtick() const
      {
      return segment() ? segment()->rtick() : -1;
      }

//---------------------------------------------------------
//   add
//---------------------------------------------------------

void ChordRest::add(Element* e)
      {
      e->setParent(this);
      e->setTrack(track());
      switch(e->type()) {
            case Element::Type::ARTICULATION:
                  {
                  Articulation* a = static_cast<Articulation*>(e);
                  _articulations.push_back(a);
                  if (a->timeStretch() != 1.0)
                        score()->fixTicks();          // update tempo map
                  }
                  break;
            case Element::Type::LYRICS:
                  {
                  Lyrics* l = static_cast<Lyrics*>(e);
                  int size = _lyricsList.size();
                  if (l->no() >= size) {
                        for (int i = size-1; i < l->no(); ++i)
                              _lyricsList.append(0);
                        }
                  _lyricsList[l->no()] = l;
                  }
                  break;
            default:
                  qFatal("ChordRest::add: unknown element %s", e->name());
                  break;
            }
      }

//---------------------------------------------------------
//   remove
//---------------------------------------------------------

void ChordRest::remove(Element* e)
      {
      switch (e->type()) {
            case Element::Type::ARTICULATION:
                  {
                  Articulation* a = static_cast<Articulation*>(e);
                  if (!_articulations.removeOne(a))
                        qDebug("ChordRest::remove(): articulation not found");
                  if (a->timeStretch() != 1.0)
                        score()->fixTicks();           // update tempo map
                  }
                  break;
            case Element::Type::LYRICS:
                  {
                  for (int i = 0; i < _lyricsList.size(); ++i) {
                        if (_lyricsList[i] != e)
                              continue;
                        _lyricsList[i]->removeFromScore();
                        _lyricsList[i] = 0;
                        while (!_lyricsList.isEmpty() && _lyricsList.back() == 0)
                              _lyricsList.takeLast();
                        return;
                        }
                  }
                  qDebug("ChordRest::remove: %s %p not found", e->name(), e);
                  break;
            default:
                  qFatal("ChordRest::remove: unknown element <%s>", e->name());
            }
      }

//---------------------------------------------------------
//   removeDeleteBeam
//      beamed - the chordrest is beamed (will get a (new) beam)
//          remove ChordRest from beam
//          delete beam if empty
//---------------------------------------------------------

void ChordRest::removeDeleteBeam(bool beamed)
      {
      if (_beam) {
            Beam* b = _beam;
            _beam->remove(this);
            if (b->isEmpty())
                  score()->undoRemoveElement(b);
            }
      if (!beamed && type() == Element::Type::CHORD)
            static_cast<Chord*>(this)->layoutHook1();
      }

//---------------------------------------------------------
//   undoSetBeamMode
//---------------------------------------------------------

void ChordRest::undoSetBeamMode(Beam::Mode mode)
      {
      undoChangeProperty(P_ID::BEAM_MODE, int(mode));
      }

//---------------------------------------------------------
//   getProperty
//---------------------------------------------------------

QVariant ChordRest::getProperty(P_ID propertyId) const
      {
      switch(propertyId) {
            case P_ID::SMALL:      return QVariant(small());
            case P_ID::BEAM_MODE:  return int(beamMode());
            case P_ID::STAFF_MOVE: return staffMove();
            case P_ID::DURATION_TYPE: return QVariant::fromValue(actualDurationType());
            default:               return DurationElement::getProperty(propertyId);
            }
      }

//---------------------------------------------------------
//   setProperty
//---------------------------------------------------------

bool ChordRest::setProperty(P_ID propertyId, const QVariant& v)
      {
      switch(propertyId) {
            case P_ID::SMALL:      setSmall(v.toBool()); break;
            case P_ID::BEAM_MODE:  setBeamMode(Beam::Mode(v.toInt())); break;
            case P_ID::STAFF_MOVE: setStaffMove(v.toInt()); break;
            case P_ID::VISIBLE:
                  setVisible(v.toBool());
                  measure()->checkMultiVoices(staffIdx());
                  break;
            case P_ID::DURATION_TYPE:
                  setDurationType(v.value<TDuration>());
                  break;
            default:
                  return DurationElement::setProperty(propertyId, v);
            }
      score()->setLayoutAll(true);
      return true;
      }

//---------------------------------------------------------
//   propertyDefault
//---------------------------------------------------------

QVariant ChordRest::propertyDefault(P_ID propertyId) const
      {
      switch(propertyId) {
            case P_ID::SMALL:      return false;
            case P_ID::BEAM_MODE:  return int(Beam::Mode::AUTO);
            case P_ID::STAFF_MOVE: return 0;
            default:          return DurationElement::propertyDefault(propertyId);
            }
      score()->setLayoutAll(true);
      }

//---------------------------------------------------------
//   isGrace
//---------------------------------------------------------

bool ChordRest::isGrace() const
      {
      return type() == Element::Type::CHORD && ((Chord*)this)->noteType() != NoteType::NORMAL;
      }

//---------------------------------------------------------
//   isGraceBefore
//---------------------------------------------------------

bool ChordRest::isGraceBefore() const
      {
      return (type() == Element::Type::CHORD && (((Chord*)this)->noteType() == NoteType::ACCIACCATURA
                                          || ((Chord*)this)->noteType() == NoteType::APPOGGIATURA
                                          || ((Chord*)this)->noteType() == NoteType::GRACE4
                                          || ((Chord*)this)->noteType() == NoteType::GRACE16
                                          || ((Chord*)this)->noteType() == NoteType::GRACE32));
      }

//---------------------------------------------------------
//   isGraceAfter
//---------------------------------------------------------

bool ChordRest::isGraceAfter() const
      {
      return (type() == Element::Type::CHORD && (((Chord*)this)->noteType() == NoteType::GRACE8_AFTER
                                          || ((Chord*)this)->noteType() == NoteType::GRACE16_AFTER
                                          || ((Chord*)this)->noteType() == NoteType::GRACE32_AFTER));
      }

//---------------------------------------------------------
//   writeBeam
//---------------------------------------------------------

void ChordRest::writeBeam(Xml& xml)
      {
      Beam* b = beam();
#ifndef NDEBUG
      if (b && b->elements().front() == this && (MScore::testMode || !b->generated())) {
            b->setId(xml.beamId++);
            b->write(xml);
            }
#else
      if (b && !b->generated() && b->elements().front() == this) {
            b->setId(xml.beamId++);
            b->write(xml);
            }
#endif
      }

//---------------------------------------------------------
//   nextSegmentAfterCR
//    returns first segment at tick CR->tick + CR->actualTicks
//    of given types
//---------------------------------------------------------

Segment* ChordRest::nextSegmentAfterCR(Segment::Type types) const
      {
      for (Segment* s = segment()->next1MM(types); s; s = s->next1MM(types)) {
            // chordrest ends at tick+actualTicks
            // we return the segment at or after the end of the chordrest
            if (s->tick() >= tick() + actualTicks())
                  return s;
            }
      return 0;
      }

//---------------------------------------------------------
//   nextElement
//---------------------------------------------------------

Element* ChordRest::nextElement()
      {
      return segment()->firstInNextSegments(staffIdx());
      }

//---------------------------------------------------------
//   prevElement
//---------------------------------------------------------

Element* ChordRest::prevElement()
      {
      return segment()->lastInPrevSegments(staffIdx());
      }

QString ChordRest::accessibleExtraInfo()
      {
      QString rez = "";
      foreach (Articulation* a, articulations()) {
            if (!score()->selectionFilter().canSelect(a)) continue;
            rez = QString("%1 %2").arg(rez).arg(a->screenReaderInfo());
            }

      foreach (Element* l, lyricsList()) {
            if (!l)
                  continue;
            if (!score()->selectionFilter().canSelect(l)) continue;
            rez = QString("%1 %2").arg(rez).arg(l->screenReaderInfo());
            }

      if (segment()) {
            foreach (Element* e, segment()->annotations()) {
                  if (!score()->selectionFilter().canSelect(e)) continue;
                  if (e->staffIdx() == staffIdx() )
                        rez = QString("%1 %2").arg(rez).arg(e->screenReaderInfo());
                  }

            SpannerMap& smap = score()->spannerMap();
            auto spanners = smap.findOverlapping(tick(), tick());
            for (auto i = spanners.begin(); i < spanners.end(); i++) {
                  const ::Interval<Spanner*> interval = *i;
                  Spanner* s = interval.value;
                  if (!score()->selectionFilter().canSelect(s)) continue;
                  if (s->type() == Element::Type::VOLTA || //voltas are added for barlines
                      s->type() == Element::Type::TIE    ) //ties are added in notes
                        continue;

                  Segment* seg = 0;
                  if (s->type() == Element::Type::SLUR) {
                        if (s->tick() == tick() && s->track() == track())
                              rez = tr("%1 Start of %2").arg(rez).arg(s->screenReaderInfo());
                        if (s->tick2() == tick() && s->track2() == track())
                              rez = tr("%1 End of %2").arg(rez).arg(s->screenReaderInfo());
                        }
                  else  {
                        if (s->tick() == tick() && s->staffIdx() == staffIdx())
                              rez = tr("%1 Start of %2").arg(rez).arg(s->screenReaderInfo());
                        seg = segment()->next1MM(Segment::Type::ChordRest);
                        if (!seg)
                              continue;
                        if (s->tick2() == seg->tick() && s->staffIdx() == staffIdx())
                              rez = tr("%1 End of %2").arg(rez).arg(s->screenReaderInfo());
                        }
                  }
            }
      return rez;
      }
}

