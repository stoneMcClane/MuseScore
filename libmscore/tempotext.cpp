//=============================================================================
//  MuseScore
//  Music Composition & Notation
//
//  Copyright (C) 2008-2011 Werner Schweer
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License version 2
//  as published by the Free Software Foundation and appearing in
//  the file LICENCE.GPL
//=============================================================================

#include "score.h"
#include "tempotext.h"
#include "tempo.h"
#include "system.h"
#include "measure.h"
#include "xml.h"

namespace Ms {

//---------------------------------------------------------
//   TempoText
//---------------------------------------------------------

TempoText::TempoText(Score* s)
   : Text(s)
      {
      _tempo      = 2.0;      // propertyDefault(P_TEMPO).toDouble();
      _followText = false;
      setPlacement(Element::Placement::ABOVE);
      setTextStyleType(TextStyleType::TEMPO);
      }

//---------------------------------------------------------
//   write
//---------------------------------------------------------

void TempoText::write(Xml& xml) const
      {
      xml.stag("Tempo");
      xml.tag("tempo", _tempo);
      if (_followText)
            xml.tag("followText", _followText);
      Text::writeProperties(xml);
      xml.etag();
      }

//---------------------------------------------------------
//   read
//---------------------------------------------------------

void TempoText::read(XmlReader& e)
      {
      while (e.readNextStartElement()) {
            const QStringRef& tag(e.name());
            if (tag == "tempo")
                  _tempo = e.readDouble();
            else if (tag == "followText")
                  _followText = e.readInt();
            else if (!Text::readProperties(e))
                  e.unknown();
            }
      if (score()->mscVersion() < 119) {
            //
            // Reset text in old version to
            // style.
            //
//TODO            if (textStyle() != TextStyleType::INVALID) {
//                  setStyled(true);
//                  styleChanged();
//                  }
            }
      // check sanity
      if (xmlText().isEmpty()) {
            setXmlText(QString("<sym>metNoteQuarterUp</sym> = %1").arg(lrint(60 * _tempo)));
            setVisible(false);
            }
      }

//---------------------------------------------------------
//   TempoPattern
//---------------------------------------------------------

struct TempoPattern {
      const char* pattern;
      qreal f;
      TDuration d;
      TempoPattern(const char* s, qreal v, TDuration::DurationType val, int dots = 0) : pattern(s), f(v), d(val) { d.setDots(dots); }
      };

// note: findTempoDuration requires the longer patterns to be before the shorter patterns in tp

static const TempoPattern tp[] = {
      TempoPattern("<sym>metNoteWhole</sym><sym>space</sym><sym>metAugmentationDot</sym>",  1.5/15.0, TDuration::DurationType::V_WHOLE, 1),    // dotted whole
      TempoPattern("<sym>metNoteWhole</sym>\\s*<sym>metAugmentationDot</sym>",              1.5/15.0, TDuration::DurationType::V_WHOLE, 1),    // dotted whole
      TempoPattern("<sym>metNoteHalfUp</sym><sym>space</sym><sym>metAugmentationDot</sym><sym>space</sym><sym>metAugmentationDot</sym>", 1.75/30.0, TDuration::DurationType::V_HALF, 2), // double dotted 1/2
      TempoPattern("<sym>metNoteHalfUp</sym>\\s*<sym>metAugmentationDot</sym>\\s*<sym>metAugmentationDot</sym>",                         1.75/30.0, TDuration::DurationType::V_HALF, 2), // double dotted 1/2
      TempoPattern("<sym>metNoteHalfUp</sym><sym>space</sym><sym>metAugmentationDot</sym>", 1.5/30.0,  TDuration::DurationType::V_HALF, 1),    // dotted 1/2
      TempoPattern("<sym>metNoteHalfUp</sym>\\s*<sym>metAugmentationDot</sym>",             1.5/30.0,  TDuration::DurationType::V_HALF, 1),    // dotted 1/2
      TempoPattern("<sym>metNoteQuarterUp</sym><sym>space</sym><sym>metAugmentationDot</sym><sym>space</sym><sym>metAugmentationDot</sym>", 1.75/60.0, TDuration::DurationType::V_QUARTER, 2), // double dotted 1/4
      TempoPattern("<sym>metNoteQuarterUp</sym>\\s*<sym>metAugmentationDot</sym>\\s*<sym>metAugmentationDot</sym>",                         1.75/60.0, TDuration::DurationType::V_QUARTER, 2), // double dotted 1/4
      TempoPattern("<sym>metNoteQuarterUp</sym><sym>space</sym><sym>metAugmentationDot</sym>",         1.5/60.0,  TDuration::DurationType::V_QUARTER, 1), // dotted 1/4
      TempoPattern("<sym>metNoteQuarterUp</sym>\\s*<sym>metAugmentationDot</sym>",          1.5/60.0,  TDuration::DurationType::V_QUARTER, 1), // dotted 1/4
      TempoPattern("<sym>metNote8thUp</sym><sym>metAugmentationDot</sym><sym>space</sym><sym>metAugmentationDot</sym>", 1.75/120.0, TDuration::DurationType::V_EIGHTH, 2), // double dotted 1/8
      TempoPattern("<sym>metNote8thUp</sym>\\s*<sym>metAugmentationDot</sym>\\s*<sym>metAugmentationDot</sym>",         1.75/120.0, TDuration::DurationType::V_EIGHTH, 2), // double dotted 1/8
      TempoPattern("<sym>metNote8thUp</sym><sym>space</sym><sym>metAugmentationDot</sym>",  1.5/120.0, TDuration::DurationType::V_EIGHTH, 1),  // dotted 1/8
      TempoPattern("<sym>metNote8thUp</sym>\\s*<sym>metAugmentationDot</sym>",              1.5/120.0, TDuration::DurationType::V_EIGHTH, 1),  // dotted 1/8
      TempoPattern("<sym>metNote16thUp</sym><sym>space</sym><sym>metAugmentationDot</sym>", 1.5/240.0, TDuration::DurationType::V_16TH, 1),  // dotted 1/16
      TempoPattern("<sym>metNote16thUp</sym>\\s*<sym>metAugmentationDot</sym>",             1.5/240.0, TDuration::DurationType::V_16TH, 1),  // dotted 1/16
      TempoPattern("<sym>metNote32ndUp</sym><sym>space</sym><sym>metAugmentationDot</sym>", 1.5/480.0, TDuration::DurationType::V_32ND, 1),  // dotted 1/32
      TempoPattern("<sym>metNote32ndUp</sym>\\s*<sym>metAugmentationDot</sym>",             1.5/480.0, TDuration::DurationType::V_32ND, 1),  // dotted 1/32
      TempoPattern("<sym>metNoteWhole</sym>",                                               1.0/15.0, TDuration::DurationType::V_WHOLE),    // whole
      TempoPattern("<sym>metNoteHalfUp</sym>",                                              1.0/30.0,  TDuration::DurationType::V_HALF),       // 1/2
      TempoPattern("<sym>metNoteQuarterUp</sym>",                                           1.0/60.0,  TDuration::DurationType::V_QUARTER),    // 1/4
      TempoPattern("<sym>metNote8thUp</sym>",                                               1.0/120.0, TDuration::DurationType::V_EIGHTH),     // 1/8
      TempoPattern("<sym>metNote16thUp</sym>",                                              1.0/240.0, TDuration::DurationType::V_16TH),     // 1/16
      TempoPattern("<sym>metNote32ndUp</sym>",                                              1.0/480.0, TDuration::DurationType::V_32ND),     // 1/32
      TempoPattern("<sym>metNote64thUp</sym>",                                              1.0/960.0, TDuration::DurationType::V_64TH),     // 1/64
      };

//---------------------------------------------------------
//   findTempoDuration
//    find the duration part (note + dot) of a tempo text in string s
//    return the match position or -1 if not found
//    set len to the match length and dur to the duration value
//---------------------------------------------------------

int TempoText::findTempoDuration(const QString& s, int& len, TDuration& dur)
      {
      len = 0;
      dur = TDuration();

      for (const auto& i : tp) {
            QRegExp re(i.pattern);
            int pos = re.indexIn(s);
            if (pos != -1) {
                  len = re.matchedLength();
                  dur = i.d;
                  return pos;
                  }
            }
      return -1;
      }

//---------------------------------------------------------
//   duration2tempoTextString
//    find the tempoText string representation for duration
//---------------------------------------------------------

QString TempoText::duration2tempoTextString(const TDuration dur)
      {
      for (const TempoPattern& pa : tp) {
            if (pa.d == dur) {
                  QString res = pa.pattern;
                  res.remove("\\s*");
                  return res;
                  }
            }
      return "";
      }

//---------------------------------------------------------
//   textChanged
//    text may have changed
//---------------------------------------------------------

void TempoText::textChanged()
      {
      if (!_followText)
            return;
      QString s = plainText();
      s.replace(",", ".");
      for (const TempoPattern& pa : tp) {
            QRegExp re(QString(pa.pattern)+"\\s*=\\s*(\\d+[.]{0,1}\\d*)");
            if (re.indexIn(s) != -1) {
                  QStringList sl = re.capturedTexts();
                  if (sl.size() == 2) {
                        qreal nt = qreal(sl[1].toDouble()) * pa.f;
                        if (nt != _tempo) {
                              _tempo = qreal(sl[1].toDouble()) * pa.f;
                              if(segment())
                                    score()->setTempo(segment(), _tempo);
                              score()->setPlaylistDirty();
                              }
                        break;
                        }
                  }
            }
      }

//---------------------------------------------------------
//   undoSetTempo
//---------------------------------------------------------

void TempoText::undoSetTempo(qreal v)
      {
      score()->undoChangeProperty(this, P_ID::TEMPO, v);
      }

//---------------------------------------------------------
//   undoSetFollowText
//---------------------------------------------------------

void TempoText::undoSetFollowText(bool v)
      {
      score()->undoChangeProperty(this, P_ID::TEMPO_FOLLOW_TEXT, v);
      }

//---------------------------------------------------------
//   getProperty
//---------------------------------------------------------

QVariant TempoText::getProperty(P_ID propertyId) const
      {
      switch(propertyId) {
            case P_ID::TEMPO:             return _tempo;
            case P_ID::TEMPO_FOLLOW_TEXT: return _followText;
            default:
                  return Text::getProperty(propertyId);
            }
      }

//---------------------------------------------------------
//   setProperty
//---------------------------------------------------------

bool TempoText::setProperty(P_ID propertyId, const QVariant& v)
      {
      switch(propertyId) {
            case P_ID::TEMPO:
                  _tempo = v.toDouble();
                  score()->setTempo(segment(), _tempo);
                  break;
            case P_ID::TEMPO_FOLLOW_TEXT:
                  _followText = v.toBool();
                  break;
            default:
                  if (!Text::setProperty(propertyId, v))
                        return false;
                  break;
            }
      score()->setLayoutAll(true);
      return true;
      }

//---------------------------------------------------------
//   propertyDefault
//---------------------------------------------------------

QVariant TempoText::propertyDefault(P_ID id) const
      {
      switch(id) {
            case P_ID::TEMPO:             return 2.0;
            case P_ID::TEMPO_FOLLOW_TEXT: return false;
            case P_ID::PLACEMENT:         return int(Element::Placement::ABOVE);
            default:
                  return Text::propertyDefault(id);
            }
      }

//---------------------------------------------------------
//   layout
//---------------------------------------------------------

void TempoText::layout()
      {
      setPos(textStyle().offset(spatium()));
      Text::layout1();
      Segment* s = segment();
      if (s && !s->rtick()) {
            // tempo text on first chordrest of measure should align over time sig if present
            Segment* p = segment()->prev(Segment::Type::TimeSig);
            if (p) {
                  rxpos() -= s->x() - p->x();
                  Element* e = p->element(staffIdx() * VOICES);
                  if (e)
                        rxpos() += e->x();
                  // correct user offset in older scores
                  if (score()->mscVersion() <= 114 && !userOff().isNull())
                        rUserXoffset() += s->x() - p->x();
                  }
            }
      if (placement() == Element::Placement::BELOW) {
            rypos() = -rypos() + 4 * spatium();
            // rUserYoffset() *= -1;
            // text height ?
            }
      adjustReadPos();
      }

//---------------------------------------------------------
//   accessibleInfo
//---------------------------------------------------------

QString TempoText::accessibleInfo()
      {
      TDuration t;
      int len;
      int x = findTempoDuration(plainText(), len, t);
      if (x != -1) {
            QString dots;

            switch (t.dots()) {
                  case 1: dots = tr("Dotted %1").arg(t.durationTypeUserName());
                        break;
                  case 2: dots = tr("Double dotted %1").arg(t.durationTypeUserName());
                        break;
                  case 3: dots = tr("Triple dotted %1").arg(t.durationTypeUserName());
                        break;
                  default:
                        dots = t.durationTypeUserName();
                        break;
                  }

            QString bpm = plainText().split(" = ").back();

            return QString("%1: %2 %3").arg(Element::accessibleInfo()).arg(dots).arg(tr("note = %1").arg(bpm));
            }
      else
            return Text::accessibleInfo();
      }

}

