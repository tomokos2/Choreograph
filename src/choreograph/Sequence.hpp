/*
 * Copyright (c) 2014 David Wicks, sansumbrella.com
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or
 * without modification, are permitted provided that the following
 * conditions are met:
 *
 * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "Phrase.hpp"
#include "phrase/Hold.hpp"

namespace choreograph
{

template<typename T>
class SequencePhrase;

template<typename T>
using SequencePhraseRef = std::shared_ptr<SequencePhrase<T>>;

template<typename T>
class Sequence;

template<typename T>
using SequenceRef = std::shared_ptr<Sequence<T>>;

template<typename T>
using SequenceUniqueRef = std::unique_ptr<Sequence<T>>;

///
/// A Sequence of motions.
/// Our essential compositional tool, describing all the transformations to one element.
/// A kind of platonic idea of an animation sequence; this describes a motion without giving it an output.
///
template<typename T>
class Sequence
{
public:
  // Sequences always need to have some valid value.
  Sequence() = delete;

  /// Construct a Sequence with an initial \a value.
  explicit Sequence( const T &value ):
    _initial_value( value )
  {}

  /// Construct a Sequence with and initial \a value.
  explicit Sequence( T &&value ):
    _initial_value( std::forward<T>( value ) )
  {}

  /// Construct a Sequence by duplicating the phrases in an \a other sequence.
  Sequence( const Sequence<T> &other ):
    _initial_value( other._initial_value ),
    _phrases( other._phrases ),
    _duration( calcDuration() )
  {}

  //
  // Sequence manipulation and expansion.
  //

  /// Set the end \a value of Sequence.
  /// If there are no Phrases, this is the initial value.
  /// Otherwise, this is an instantaneous hold at \a value.
  Sequence<T>& set( const T &value );

  /// Add a Phrase to the end of the sequence.
  /// Constructs a new Phrase animating to \a value over \duration from the current end of this Sequence.
  /// Forwards additional arguments to the end of the Phrase constructor.
  /// Example calls look like:
  /// sequence.then<RampTo>( targetValue, duration, EaseInOutQuad() ).then<Hold>( holdValue, duration );
  template<template <typename> class PhraseT, typename... Args>
  Sequence<T>& then( const T &value, Time duration, Args&&... args );

  /// Appends a phrase to the end of the sequence.
  Sequence<T>& then( const std::shared_ptr<Phrase<T>> &phrase_ptr );

  /// Append all Phrases from another Sequence to this Sequence.
  Sequence<T>& then( const Sequence<T> &next );

  /// Append all Phrases from another Sequence to this Sequence.
  /// Specialized to handle shared_ptr's correctly.
  Sequence<T>& then( const std::shared_ptr<Sequence<T>> &next ) { return then( *next ); }

  /// Returns a Phrase that encapsulates this Sequence.
  /// Duplicates the Sequence, so future changes to this do not affect the Phrase.
  PhraseRef<T> asPhrase() const { return std::make_shared<SequencePhrase<T>>( std::unique_ptr<Sequence<T>>( new Sequence<T>( *this ) ) ); }

  //
  // Phrase<T> Equivalents.
  //

  /// Returns the Sequence value at \a atTime.
  T getValue( Time atTime ) const;

  /// Returns the Sequence value at \a atTime, wrapped past the end of .
  T getValueWrapped( Time time, Time inflectionPoint = 0.0f ) const { return getValue( wrapTime( time, getDuration(), inflectionPoint ) ); }

  /// Returns the value at the end of the Sequence.
  T getEndValue() const { return _phrases.empty() ? _initial_value : _phrases.back()->getEndValue(); }

  /// Returns the value at the beginning of the Sequence.
  T getStartValue() const { return _initial_value; }

  /// Returns the Sequence duration.
  Time getDuration() const { return _duration; }

  //
  //
  //

  /// Returns the number of phrases in the Sequence.
  size_t getPhraseCount() const { return _phrases.size(); }

  /// Recalculate Sequence duration. Might never be used...
  Time calcDuration() const;

private:
  // Storing shared_ptr's to Phrases requires their duration to be immutable.
  std::vector<PhraseRef<T>> _phrases;
  T                         _initial_value;
  Time                      _duration = 0;
};

//=================================================
// Sequence Template Implementation.
//=================================================

template<typename T>
Sequence<T>& Sequence<T>::set( const T &value )
{
  if( _phrases.empty() ) {
    _initial_value = value;
  }
  else {
    then<Hold>( value, 0.0f );
  }
  return *this;
}

template<typename T>
template<template <typename> class PhraseT, typename... Args>
Sequence<T>& Sequence<T>::then( const T &value, Time duration, Args&&... args )
{
  _phrases.emplace_back( std::unique_ptr<PhraseT<T>>( new PhraseT<T>( duration, this->getEndValue(), value, std::forward<Args>(args)... ) ) );
  _duration += duration;

  return *this;
}

template<typename T>
Sequence<T>& Sequence<T>::then( const std::shared_ptr<Phrase<T>> &phrase )
{
  _phrases.push_back( phrase );
  _duration += phrase->getDuration();

  return *this;
}

template<typename T>
Sequence<T>& Sequence<T>::then( const Sequence<T> &next )
{
  for( const auto &phrase : next._phrases ) {
    then( phrase );
  }

  return *this;
}

template<typename T>
T Sequence<T>::getValue( Time atTime ) const
{
  if( atTime < 0.0f )
  {
    return _initial_value;
  }
  else if ( atTime >= this->getDuration() )
  {
    return getEndValue();
  }

  for( const auto &phrase : _phrases )
  {
    if( phrase->getDuration() < atTime ) {
      atTime -= phrase->getDuration();
    }
    else {
      return phrase->getValue( atTime );
    }
  }
  // past the end, get the final value
  // this should be unreachable, given that we return early if time >= duration
  return getEndValue();
}

template<typename T>
Time Sequence<T>::calcDuration() const
{
  Time sum = 0;
  for( const auto &phrase : _phrases ) {
    sum += phrase->getDuration();
  }
  return sum;
}

//=================================================
// Convenience functions.
//=================================================

template<typename T>
SequenceRef<T> createSequence( T &&initialValue )
{
  return std::make_shared<Sequence<T>>( std::forward<T>( initialValue ) );
}

template<typename T>
SequenceRef<T> createSequence( const T &initialValue )
{
  return std::make_shared<Sequence<T>>( initialValue );
}

//=================================================
// Sequence Decorator Phrase.
//=================================================

/// A Phrase that wraps up a Sequence.
/// Note that the Sequence becomes immutable, as it is inaccessible outside of the phrase.
template<typename T>
class SequencePhrase: public Phrase<T>
{
public:
  /// Construct a Phrase that wraps a Sequence, allowing it to be passed into meta-Phrases.
  SequencePhrase( SequenceUniqueRef<T> &&sequence ):
    Phrase<T>( sequence->getDuration() ),
    _sequence( std::move( sequence ) )
  {}

  /// Returns the interpolated value at the given time.
  T getValue( Time atTime ) const override { return _sequence->getValue( atTime ); }

  T getStartValue() const override { return _sequence->getStartValue(); }

  T getEndValue() const override { return _sequence->getEndValue(); }
private:
  SequenceUniqueRef<T>  _sequence;
};

} // namespace choreograph
