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

#pragma once

#include <openpeer/core/IContact.h>
#include <openpeer/core/internal/types.h>

namespace openpeer
{
  namespace core
  {
    namespace internal
    {
      interaction IAccountForContact;

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IContactForAccount
      #pragma mark

      interaction IContactForAccount
      {
        ZS_DECLARE_TYPEDEF_PTR(IContactForAccount, ForAccount)

        static ElementPtr toDebug(ForAccountPtr contact);

        static ForAccountPtr createFromPeer(
                                            AccountPtr account,
                                            IPeerPtr peer
                                            );

        virtual String getPeerURI() const = 0;
        virtual IPeerPtr getPeer() const = 0;

        virtual IPeerFilePublicPtr getPeerFilePublic() const = 0;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IContactForConversationThread
      #pragma mark

      interaction IContactForConversationThread
      {
        ZS_DECLARE_TYPEDEF_PTR(IContactForConversationThread, ForConversationThread)

        static ElementPtr toDebug(ForConversationThreadPtr contact);

        static ForConversationThreadPtr createFromPeerURI(
                                                          AccountPtr account,
                                                          const char *peerURI
                                                          );

        virtual String getPeerURI() const = 0;
        virtual IPeerPtr getPeer() const = 0;

        virtual bool isSelf() const = 0;

        virtual IPeerFilePublicPtr getPeerFilePublic() const = 0;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IContactForCall
      #pragma mark

      interaction IContactForCall
      {
        ZS_DECLARE_TYPEDEF_PTR(IContactForCall, ForCall)

        static ElementPtr toDebug(ForCallPtr contact);

        virtual bool isSelf() const = 0;

        virtual String getPeerURI() const = 0;
        virtual IPeerPtr getPeer() const = 0;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Contact
      #pragma mark

      class Contact : public Noop,
                      public IContact,
                      public IContactForAccount,
                      public IContactForConversationThread,
                      public IContactForCall
      {
      public:
        friend interaction IContactFactory;
        friend interaction IContact;
        friend interaction IContactForAccount;
        friend interaction IContactForConversationThread;
        friend interaction IContactForCall;

        ZS_DECLARE_TYPEDEF_PTR(IAccountForContact, UseAccount)

      protected:
        Contact();
        
        Contact(Noop) : Noop(true) {};

        void init();

      public:
        ~Contact();

        static ContactPtr convert(IContactPtr contact);
        static ContactPtr convert(ForAccountPtr contact);
        static ContactPtr convert(ForConversationThreadPtr contact);
        static ContactPtr convert(ForCallPtr contact);

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Contact => IContact
        #pragma mark

        static ElementPtr toDebug(IContactPtr contact);

        static ContactPtr createFromPeerFilePublic(
                                                   AccountPtr account,
                                                   IPeerFilePublicPtr peerFilePublic
                                                   );

        static ContactPtr getForSelf(IAccountPtr account);

        virtual PUID getID() const {return  mID;}

        virtual bool isSelf() const;

        virtual String getPeerURI() const;
        virtual IPeerFilePublicPtr getPeerFilePublic() const;

        virtual IAccountPtr getAssociatedAccount() const;

        virtual void hintAboutLocation(const char *contactsLocationID);

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Contact => IContactForAccount
        #pragma mark

        static ContactPtr createFromPeer(
                                         AccountPtr account,
                                         IPeerPtr peer
                                         );

        // (duplicate) virtual String getPeerURI() const;
        virtual IPeerPtr getPeer() const;

        // (duplicate) virtual IPeerFilePublicPtr getPeerFilePublic() const;

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Contact => IContactForConversationThread
        #pragma mark

        // (duplicate) static ContactPtr createFromPeerFilePublic(
        //                                                        IAccountPtr account,
        //                                                        IPeerFilePublicPtr peerFilePublic
        //                                                        );

        // (duplicate) virtual String getPeerURI() const;
        // (duplicate) virtual IPeerPtr getPeer() const;

        // (duplicate) virtual bool isSelf() const;

        // (duplicate) virtual IPeerFilePublicPtr getPeerFilePublic() const;

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Contact => IContactForCall
        #pragma mark

        // (duplicate) virtual bool isSelf() const;

        // (duplicate) virtual String getPeerURI() const;
        // (duplicate) virtual IPeerPtr getPeer() const;

      private:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Contact => (internal)
        #pragma mark

        Log::Params log(const char *message) const;
        Log::Params debug(const char *message) const;

        virtual ElementPtr toDebug() const;


      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Contact => (data)
        #pragma mark

        AutoPUID mID;
        ContactWeakPtr mThisWeak;

        UseAccountWeakPtr mAccount;
        IPeerPtr mPeer;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IContactFactory
      #pragma mark

      interaction IContactFactory
      {
        static IContactFactory &singleton();

        virtual ContactPtr createFromPeer(
                                          AccountPtr account,
                                          IPeerPtr peer
                                          );

        virtual ContactPtr createFromPeerFilePublic(
                                                    AccountPtr account,
                                                    IPeerFilePublicPtr publicPeerFile
                                                    );

        virtual ContactPtr getForSelf(IAccountPtr account);
      };

    }
  }
}
