// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SmallCapsIterator_h
#define SmallCapsIterator_h

#include "platform/fonts/FontOrientation.h"
#include "platform/fonts/ScriptRunIterator.h"
#include "platform/fonts/UTF16TextIterator.h"

namespace blink {

class PLATFORM_EXPORT SmallCapsIterator {
public:
    enum SmallCapsBehavior {
        SmallCapsSameCase,
        SmallCapsUppercaseNeeded,
        SmallCapsInvalid
    };

    SmallCapsIterator(const UChar* buffer, unsigned bufferSize);

    bool consume(unsigned* capsLimit, SmallCapsBehavior*);
private:
    OwnPtr<UTF16TextIterator> m_utf16Iterator;
    unsigned m_bufferSize;
    UChar32 m_nextUChar32;
    bool m_atEnd;

    SmallCapsBehavior m_currentSmallCapsBehavior;
    SmallCapsBehavior m_previousSmallCapsBehavior;
};

} // namespace blink

#endif
