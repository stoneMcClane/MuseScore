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

#ifndef __CHORD_H__
#define __CHORD_H__

/**
 \file
 Definition of classes Chord, HelpLine and NoteList.
*/

#include <functional>
#include "chordrest.h"

class QPainter;

namespace Ms {

class Note;
class Hook;
class Arpeggio;
class Tremolo;
class Chord;
//class Glissando;
class Stem;
class Chord;
class StemSlash;
class LedgerLine;
class AccidentalState;

enum class TremoloChordType : char { TremoloSingle, TremoloFirstNote, TremoloSecondNote };
enum class PlayEventType : char    {
      Auto,       // Play events for all notes are calculated by MuseScore.
      User,       // Some play events are modified by user. The events must be written into the mscx file.
      InvalidUser // The user modified play events must be replaced by MuseScore generated ones on
                  // next recalculation. The actual play events must be saved on the undo stack.
      };

//---------------------------------------------------------
//   @@ Chord
///    Graphic representation of a chord.
///    Single notes are handled as degenerated chords.
//
//   @P beam        Beam            the beam of the chord if any (read only)
//   @P graceNotes  array[Chord]    the list of grace note chords (read only)
//   @P hook        Hook            the hook of the chord if any (read only)
//   @P lyrics      array[Lyrics]   the list of lyrics (read only)
//   @P notes       array[Note]     the list of notes (read only)
//   @P stem        Stem            the stem of the chord if any (read only)
//   @P stemSlash   StemSlash       the stem slash of the chord (acciaccatura) if any (read only)
//---------------------------------------------------------

class Chord : public ChordRest {
      Q_OBJECT

      struct LedgerLineData {
            int   line;
            qreal minX, maxX;
            bool  visible;
            bool  accidental;
            };

      Q_PROPERTY(Ms::Beam* beam              READ beam)
      Q_PROPERTY(QQmlListProperty<Ms::Chord> graceNotes READ qmlGraceNotes)
      Q_PROPERTY(Ms::Hook* hook              READ hook)
      Q_PROPERTY(QQmlListProperty<Ms::Lyrics> lyrics READ qmlLyrics)
      Q_PROPERTY(QQmlListProperty<Ms::Note> notes READ qmlNotes)
      Q_PROPERTY(Ms::Stem* stem              READ stem)
      Q_PROPERTY(Ms::StemSlash* stemSlash    READ stemSlash)

      QList<Note*>         _notes;       // sorted to decreasing line step
      LedgerLine*          _ledgerLines; // single linked list

      Stem*               _stem;
      Hook*               _hook;
      StemSlash*          _stemSlash;    // for acciacatura

      Arpeggio*           _arpeggio;
      Tremolo*            _tremolo;
      bool                _endsGlissando;///< true if this chord is the ending point of a glissando (nneeded for layout)
      ElementList         _el;           ///< chordline, slur
      QList<Chord*>       _graceNotes;
      int                 _graceIndex;   ///< if this is a grace note, index in parent list

      MScore::Direction  _stemDirection;
      NoteType           _noteType;      ///< mark grace notes: acciaccatura and appoggiatura
      bool               _noStem;
      PlayEventType      _playEventType; ///< play events were modified by user

      virtual qreal upPos()   const;
      virtual qreal downPos() const;
      virtual qreal centerX() const;
      void createLedgerLines(int track, std::vector<LedgerLineData> &vecLines, bool visible);
      void addLedgerLines(int move);
      void processSiblings(std::function<void(Element*)> func);
      void layoutPitched();
      void layoutTablature();

   public:
      Chord(Score* s = 0);
      Chord(const Chord&, bool link = false);
      ~Chord();
      Chord &operator=(const Chord&) = delete;

      virtual Chord* clone() const       { return new Chord(*this, false); }
      virtual Element* linkedClone()     { return new Chord(*this, true); }
      virtual void undoUnlink() override;

      virtual void setScore(Score* s);
      virtual Element::Type type() const         { return Element::Type::CHORD; }
      virtual qreal mag() const;

      virtual void write(Xml& xml) const;
      virtual void read(XmlReader&);
      virtual void setSelected(bool f);
      virtual Element* drop(const DropData&);

      void setStemDirection(MScore::Direction d) { _stemDirection = d; }
      MScore::Direction stemDirection() const    { return _stemDirection; }

      LedgerLine* ledgerLines()                  { return _ledgerLines; }

      qreal defaultStemLength();

      void layoutStem1();
      void layoutHook1();     // create hook if required
      void layoutStem();
      void layoutArpeggio2();

      QQmlListProperty<Ms::Note> qmlNotes()       { return QQmlListProperty<Ms::Note>(this, _notes);        }
      QQmlListProperty<Ms::Lyrics> qmlLyrics()    { return QQmlListProperty<Ms::Lyrics>(this, _lyricsList); }
      QQmlListProperty<Ms::Chord> qmlGraceNotes() { return QQmlListProperty<Ms::Chord>(this, _graceNotes);  }
      QList<Note*>& notes()                       { return _notes; }
      const QList<Note*>& notes() const           { return _notes; }

      // Chord has at least one Note
      Note* upNote() const;
      Note* downNote() const;
      virtual int upString() const;
      virtual int downString() const;

      qreal maxHeadWidth() const;

      Note* findNote(int pitch) const;

      Stem* stem() const                     { return _stem; }
      Arpeggio* arpeggio() const             { return _arpeggio;  }
      Tremolo* tremolo() const               { return _tremolo;   }
      void setTremolo(Tremolo* t)            { _tremolo = t;      }
//      Glissando* glissando() const           { return _glissando; }
      bool endsGlissando() const             { return _endsGlissando; }
      void setEndsGlissando (bool val)       { _endsGlissando = val; }
      StemSlash* stemSlash() const           { return _stemSlash; }
      bool slash();
      void setSlash(bool flag, bool stemless);
      void removeMarkings(bool keepTremolo = false);

      const QList<Chord*>& graceNotes() const { return _graceNotes; }
      QList<Chord*>& graceNotes()             { return _graceNotes; }

      QList<Chord*> graceNotesBefore() const;
      QList<Chord*> graceNotesAfter() const;

      int graceIndex() const                        { return _graceIndex; }
      void setGraceIndex(int val)                   { _graceIndex = val;  }

      virtual int upLine() const;
      virtual int downLine() const;
      virtual QPointF stemPos() const;          ///< page coordinates
      virtual QPointF stemPosBeam() const;      ///< page coordinates
      virtual qreal stemPosX() const;
      bool underBeam() const;
      Hook* hook() const                     { return _hook; }

      //@ add an element to the Chord
      Q_INVOKABLE virtual void add(Ms::Element*);
      //@ remove the element from the Chord
      Q_INVOKABLE virtual void remove(Ms::Element*);

      Note* selectedNote() const;
      virtual void layout();
      void layout2();
      void cmdUpdateNotes(AccidentalState*);

      NoteType noteType() const       { return _noteType; }
      void setNoteType(NoteType t)    { _noteType = t; }
      bool isGrace() const            { return _noteType != NoteType::NORMAL; }
      void toGraceAfter();
      virtual void scanElements(void* data, void (*func)(void*, Element*), bool all=true);

      virtual void setTrack(int val);

      void computeUp();

      qreal dotPosX() const;

      bool noStem() const                           { return _noStem;  }
      void setNoStem(bool val)                      { _noStem = val;   }

      PlayEventType playEventType() const           { return _playEventType; }
      void setPlayEventType(PlayEventType v)        { _playEventType = v;    }

      TremoloChordType tremoloChordType() const;

      ElementList& el()               { return _el; }
      const ElementList& el() const   { return _el; }

      QPointF layoutArticulation(Articulation*);

      virtual void crossMeasureSetup(bool on);

      virtual QVariant getProperty(P_ID propertyId) const override;
      virtual bool setProperty(P_ID propertyId, const QVariant&) override;
      virtual QVariant propertyDefault(P_ID) const override;

      virtual void reset();

      virtual Segment* segment() const;
      virtual Measure* measure() const;

      void sortNotes();

      virtual Element* nextElement() override;
      virtual Element* prevElement() override;
      virtual QString accessibleExtraInfo() override;
      };


}     // namespace Ms
#endif

