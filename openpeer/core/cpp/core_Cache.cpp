/*

 Copyright (c) 2013, SMB Phone Inc.
 All rights reserved.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice, this
 list of conditions and the following disclaimer.
 2. Redistributions in binary form must reproduce the above copyright notice,
 this list of conditions and the following disclaimer in the documentation
 and/or other materials provided with the distribution.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

 The views and conclusions contained in the software and documentation are those
 of the authors and should not be interpreted as representing official policies,
 either expressed or implied, of the FreeBSD Project.

 */

#include <openpeer/core/internal/core_Cache.h>

#include <openpeer/services/IHelper.h>

#include <zsLib/helpers.h>
#include <zsLib/XML.h>
#include <zsLib/Stringize.h>

namespace openpeer { namespace core { ZS_DECLARE_SUBSYSTEM(openpeer_core) } }

namespace openpeer
{
  namespace core
  {
    namespace internal
    {
      using services::IHelper;

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Cache
      #pragma mark

      //-----------------------------------------------------------------------
      Cache::Cache() :
        mID(zsLib::createPUID())
      {
        ZS_LOG_DETAIL(log("created"))
      }

      //-----------------------------------------------------------------------
      Cache::~Cache()
      {
        mThisWeak.reset();
        ZS_LOG_DETAIL(log("destroyed"))
      }

      //-----------------------------------------------------------------------
      CachePtr Cache::convert(ICachePtr cache)
      {
        return dynamic_pointer_cast<Cache>(cache);
      }

      //-----------------------------------------------------------------------
      CachePtr Cache::create()
      {
        CachePtr pThis(new Cache());
        pThis->mThisWeak = pThis;
        return pThis;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Cache => ICache
      #pragma mark

      //-----------------------------------------------------------------------
      void Cache::setup(ICacheDelegatePtr delegate)
      {
        AutoRecursiveLock lock(mLock);
        mDelegate = delegate;

        ZS_LOG_DEBUG(log("setup called") + ZS_PARAM("has delegate", (bool)delegate))

        stack::ICache::setup(delegate ? mThisWeak.lock() : stack::ICacheDelegatePtr());
      }

      //-----------------------------------------------------------------------
      CachePtr Cache::singleton()
      {
        static SingletonLazySharedPtr<Cache> singleton(Cache::create());
        CachePtr result = singleton.singleton();
        if (!result) {
          ZS_LOG_WARNING(Detail, slog("singleton gone"))
        }
        return result;
      }

      //-----------------------------------------------------------------------
      String Cache::fetch(const char *cookieNamePath) const
      {
        if (!cookieNamePath) return String();

        ICacheDelegatePtr delegate;

        {
          AutoRecursiveLock lock(mLock);
          delegate = mDelegate;
        }

        if (!delegate) {
          ZS_LOG_WARNING(Debug, log("no cache installed (thus cannot fetch cookie)") + ZS_PARAM("cookie name", cookieNamePath))
          return String();
        }

        String result = delegate->fetch(cookieNamePath);
        if (result.hasData()) {
          ZS_LOG_TRACE(log("fetched from cache") + ZS_PARAM("cookie name", cookieNamePath) + ZS_PARAM("result", result))
        }
        return result;
      }

      //-----------------------------------------------------------------------
      void Cache::store(
                        const char *cookieNamePath,
                        Time expires,
                        const char *str
                        )
      {
        if (!cookieNamePath) return;
        if (!str) clear(cookieNamePath);
        if (!(*str)) clear(cookieNamePath);

        ICacheDelegatePtr delegate;

        {
          AutoRecursiveLock lock(mLock);
          delegate = mDelegate;
        }

        if (!delegate) {
          ZS_LOG_WARNING(Debug, log("no cache installed (thus cannot store cookie)") + ZS_PARAM("cookie name", cookieNamePath) + ZS_PARAM("expires", expires) + ZS_PARAM("value", str))
          return;
        }

        ZS_LOG_TRACE(log("storing in cache") + ZS_PARAM("cookie name", cookieNamePath) + ZS_PARAM("expires", expires) + ZS_PARAM("value", str))
        delegate->store(cookieNamePath, expires, str);
      }

      //-----------------------------------------------------------------------
      void Cache::clear(const char *cookieNamePath)
      {
        if (!cookieNamePath) return;

        ICacheDelegatePtr delegate;

        {
          AutoRecursiveLock lock(mLock);
          delegate = mDelegate;
        }

        if (!delegate) {
          ZS_LOG_WARNING(Debug, log("no cache installed (thus cannot clear cookie)") + ZS_PARAM("cookie name", cookieNamePath))
          return;
        }

        ZS_LOG_TRACE(log("clearing from cache") + ZS_PARAM("cookie name", cookieNamePath))
        delegate->clear(cookieNamePath);
      }
      
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Cache => stack::ICacheDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Cache => (internal)
      #pragma mark

      //-----------------------------------------------------------------------
      Log::Params Cache::log(const char *message) const
      {
        ElementPtr objectEl = Element::create("core::Cache");
        IHelper::debugAppend(objectEl, "id", mID);
        return Log::Params(message, objectEl);
      }

      //-----------------------------------------------------------------------
      Log::Params Cache::slog(const char *message)
      {
        return Log::Params(message, "core::Cache");
      }

    }

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark ICache
    #pragma mark

    //-------------------------------------------------------------------------
    void ICache::setup(ICacheDelegatePtr delegate)
    {
      internal::CachePtr singleton = internal::Cache::singleton();
      if (!singleton) return;
      singleton->setup(delegate);
    }

    //-------------------------------------------------------------------------
    String ICache::fetch(const char *cookieNamePath)
    {
      internal::CachePtr singleton = internal::Cache::singleton();
      if (!singleton) return String();
      return singleton->fetch(cookieNamePath);
    }

    //-------------------------------------------------------------------------
    void ICache::store(
                      const char *cookieNamePath,
                      Time expires,
                      const char *str
                      )
    {
      internal::CachePtr singleton = internal::Cache::singleton();
      if (!singleton) return;
      singleton->store(cookieNamePath, expires, str);
    }

    //-------------------------------------------------------------------------
    void ICache::clear(const char *cookieNamePath)
    {
      internal::CachePtr singleton = internal::Cache::singleton();
      if (!singleton) return;
      singleton->clear(cookieNamePath);
    }

  }
}
