/*
 *      Copyright (C) 2005-2013 Team XBMC
 *      http://kodi.tv
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

#pragma once

#include <initializer_list>
#include <memory>
#include <vector>

#include "threads/Condition.h"
#include "threads/SingleLock.h"

// forward declare the CEventGroup
namespace XbmcThreads
{
  class CEventGroup;
}


/**
 * This is an Event class built from a ConditionVariable. The Event adds the state
 * that the condition is gating as well as the mutex/lock.
 *
 * This Event can be 'interruptible' (even though there is only a single place
 * in the code that uses this behavior).
 *
 * This class manages 'spurious returns' from the condition variable.
 */
class CEvent
{
  bool manualReset;
  volatile bool signaled;
  unsigned int numWaits = 0;

  CCriticalSection groupListMutex; // lock for the groups list
  std::unique_ptr<std::vector<XbmcThreads::CEventGroup*>> groups;

  /**
   * To satisfy the TightConditionVariable requirements and allow the
   *  predicate being monitored to include both the signaled and interrupted
   *  states.
   */
  XbmcThreads::ConditionVariable actualCv;
  XbmcThreads::TightConditionVariable<volatile bool&> condVar;
  CCriticalSection mutex;

  friend class XbmcThreads::CEventGroup;

  void addGroup(XbmcThreads::CEventGroup* group);
  void removeGroup(XbmcThreads::CEventGroup* group);

  // helper for the two wait methods
  inline bool prepReturn() { bool ret = signaled; if (!manualReset && numWaits == 0) signaled = false; return ret; }

  CEvent(const CEvent&) = delete;
  CEvent& operator=(const CEvent&) = delete;

public:
  inline CEvent(bool manual = false, bool signaled_ = false) :
    manualReset(manual), signaled(signaled_), condVar(actualCv,signaled) {}

  inline void Reset() { CSingleLock lock(mutex); signaled = false; }
  void Set();

  /** Returns true if Event has been triggered and not reset, false otherwise. */
  inline bool Signaled() { CSingleLock lock(mutex); return signaled; }

  /**
   * This will wait up to 'milliSeconds' milliseconds for the Event
   *  to be triggered. The method will return 'true' if the Event
   *  was triggered. Otherwise it will return false.
   */
  inline bool WaitMSec(unsigned int milliSeconds)
  { CSingleLock lock(mutex); numWaits++; condVar.wait(mutex,milliSeconds); numWaits--; return prepReturn(); }

  /**
   * This will wait for the Event to be triggered. The method will return
   * 'true' if the Event was triggered. If it was either interrupted
   * it will return false. Otherwise it will return false.
   */
  inline bool Wait()
  { CSingleLock lock(mutex); numWaits++; condVar.wait(mutex); numWaits--; return prepReturn(); }

  /**
   * This is mostly for testing. It allows a thread to make sure there are
   *  the right amount of other threads waiting.
   */
  inline int getNumWaits() { CSingleLock lock(mutex); return numWaits; }

};

#ifdef HAS_DS_PLAYER
class CAutoSetEvent
{
public:
  CAutoSetEvent(CEvent *event)
    : m_event(event)
    {
      if (m_event)
      m_event->Reset();
    }
    ~CAutoSetEvent()
	{
      if (m_event)
      m_event->Set();
    }
private:
  CEvent* m_event;
};
#endif

namespace XbmcThreads
{
  /**
   * CEventGroup is a means of grouping CEvents to wait on them together.
   * It is equivalent to WaitOnMultipleObject that returns when "any" Event
   * in the group signaled.
   */
  class CEventGroup
  {
    std::vector<CEvent*> events;
    CEvent* signaled{};
    XbmcThreads::ConditionVariable actualCv;
    XbmcThreads::TightConditionVariable<CEvent*&> condVar{actualCv, signaled};
    CCriticalSection mutex;

    unsigned int numWaits{0};

    // This is ONLY called from CEvent::Set.
    inline void Set(CEvent* child) { CSingleLock l(mutex); signaled = child; condVar.notifyAll(); }

    friend class ::CEvent;

    CEventGroup(const CEventGroup&) = delete;
    CEventGroup& operator=(const CEventGroup&) = delete;

  public:
    /**
     * Create a CEventGroup from a number of CEvents.
     */
    CEventGroup(std::initializer_list<CEvent*> events);

    ~CEventGroup();

    /**
     * This will block until any one of the CEvents in the group are
     * signaled at which point a pointer to that CEvents will be
     * returned.
     */
    CEvent* wait();

    /**
     * This will block until any one of the CEvents in the group are
     * signaled or the timeout is reached. If an event is signaled then
     * it will return a pointer to that CEvent, otherwise it will return
     * NULL.
     */
    CEvent* wait(unsigned int milliseconds);

    /**
     * This is mostly for testing. It allows a thread to make sure there are
     *  the right amount of other threads waiting.
     */
    inline int getNumWaits() { CSingleLock lock(mutex); return numWaits; }

  };
}
