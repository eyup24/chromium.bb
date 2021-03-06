// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/page/scrolling/ScrollCustomizationCallbacks.h"

#include "core/page/scrolling/ScrollStateCallback.h"
#include "wtf/Deque.h"

namespace blink {

void ScrollCustomizationCallbacks::setDistributeScroll(Element* element, ScrollStateCallback* scrollStateCallback)
{
    m_distributeScrollCallbacks.set(element, scrollStateCallback);
}

ScrollStateCallback* ScrollCustomizationCallbacks::getDistributeScroll(Element* element)
{
    auto it = m_distributeScrollCallbacks.find(element);
    if (it == m_distributeScrollCallbacks.end())
        return nullptr;
    return it->value.get();
}

void ScrollCustomizationCallbacks::setApplyScroll(Element* element, ScrollStateCallback* scrollStateCallback)
{
    m_applyScrollCallbacks.set(element, scrollStateCallback);
}

ScrollStateCallback* ScrollCustomizationCallbacks::getApplyScroll(Element* element)
{
    auto it = m_applyScrollCallbacks.find(element);
    if (it == m_applyScrollCallbacks.end())
        return nullptr;
    return it->value.get();
}

#if !ENABLE(OILPAN)
void ScrollCustomizationCallbacks::removeCallbacksForElement(Element* element)
{
    m_applyScrollCallbacks.remove(element);
    m_distributeScrollCallbacks.remove(element);
}
#endif

} // namespace blink
