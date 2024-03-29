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


#include <openpeer/core/internal/core_thread.h>
#include <openpeer/core/internal/core_ConversationThread.h>
#include <openpeer/core/internal/core_Contact.h>
#include <openpeer/core/internal/core_Account.h>
#include <openpeer/core/internal/core_Helper.h>

#include <openpeer/stack/message/IMessageHelper.h>
#include <openpeer/stack/IPublication.h>
#include <openpeer/stack/IPeerFiles.h>
#include <openpeer/stack/IPeerFilePrivate.h>
#include <openpeer/stack/IPeerFilePublic.h>
#include <openpeer/stack/IHelper.h>
#include <openpeer/stack/IDiff.h>

#include <openpeer/services/IHelper.h>

#include <zsLib/XML.h>
//#include <zsLib/Log.h>
#include <zsLib/Stringize.h>
#include <zsLib/helpers.h>
#include <zsLib/Numeric.h>
#include <zsLib/IPAddress.h>


namespace openpeer { namespace core { ZS_DECLARE_SUBSYSTEM(openpeer_core) } }

namespace openpeer
{
  namespace core
  {
    namespace internal
    {
      namespace thread
      {
        using services::IHelper;

        using zsLib::Numeric;
        using zsLib::IPAddress;

        typedef zsLib::XML::Exceptions::CheckFailed CheckFailed;

        using stack::IDiff;
        typedef services::IHelper::SplitMap SplitMap;

        typedef IPublication::PublishToRelationshipsMap PublishToRelationshipsMap;
        typedef IPublication::RelationshipList RelationshipList;

        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark (helpers)
        #pragma mark

        //---------------------------------------------------------------------
        static ElementPtr createElement(const char *elementName, const char *id)
        {
          return IMessageHelper::createElementWithID(elementName ? String(elementName) : String(), id ? String(id) : String());
        }

        //---------------------------------------------------------------------
        static ElementPtr createElementWithText(const char *elementName, const char *text)
        {
          return IMessageHelper::createElementWithText(elementName ? String(elementName) : String(), text ? String(text) : String());
        }

        //---------------------------------------------------------------------
        static ElementPtr createElementWithNumber(const char *elementName, const char *number)
        {
          return IMessageHelper::createElementWithNumber(elementName ? String(elementName) : String(), number ? String(number) : String());
        }

        //---------------------------------------------------------------------
        static ElementPtr createElementWithText(const char *inElementName, const char *inID, const char *inText)
        {
          ElementPtr el = createElementWithText(inElementName, inText);

          String id(inID ? String(inID) : String());
          if (id.isEmpty()) return el;

          el->setAttribute("id", id);
          return el;
        }

        //---------------------------------------------------------------------
        static ElementPtr createElementWithNumber(const char *inElementName, const char *inID, const char *inNumber)
        {
          ElementPtr el = createElementWithNumber(inElementName, inNumber);

          String id(inID ? String(inID) : String());
          if (id.isEmpty()) return el;

          el->setAttribute("id", id);
          return el;
        }

        //---------------------------------------------------------------------
        static ElementPtr createElementWithTextAndJSONEncode(const char *inElementName, const char *inText)
        {
          return IMessageHelper::createElementWithTextAndJSONEncode(inElementName ? String(inElementName) : String(), inText ? String(inText) : String());
        }

        //---------------------------------------------------------------------
        static UINT getVersion(ElementPtr el) throw (Numeric<UINT>::ValueOutOfRange)
        {
          if (!el) return 0;
          AttributePtr versionAt = el->findAttribute("version");
          if (!versionAt) return 0;
          return Numeric<UINT>(versionAt->getValue());
        }

        //---------------------------------------------------------------------
        static void convert(const ContactURIList &input, ThreadContactMap &output)
        {
          for (ContactURIList::const_iterator iter = input.begin(); iter != input.end(); ++iter)
          {
            const ContactURI &id = (*iter);
            output[id] = ThreadContactPtr();
          }
        }

        //---------------------------------------------------------------------
        static void convert(const ThreadContactMap &input, ContactURIList &output)
        {
          for (ThreadContactMap::const_iterator iter = input.begin(); iter != input.end(); ++iter)
          {
            const ContactURI &id = (*iter).first;
            output.push_back(id);
          }
        }

        //---------------------------------------------------------------------
        static void convert(const ThreadContactMap &input, ThreadContactList &output)
        {
          for (ThreadContactMap::const_iterator iter = input.begin(); iter != input.end(); ++iter)
          {
            const ThreadContactPtr &contact = (*iter).second;
            output.push_back(contact);
          }
        }

        //---------------------------------------------------------------------
        static void convert(const DialogMap &input, DialogList &output)
        {
          for (DialogMap::const_iterator iter = input.begin(); iter != input.end(); ++iter)
          {
            const DialogPtr &dialog = (*iter).second;
            output.push_back(dialog);
          }
        }

        //---------------------------------------------------------------------
        static void convert(const DialogMap &input, DialogIDList &output)
        {
          for (DialogMap::const_iterator iter = input.begin(); iter != input.end(); ++iter)
          {
            const DialogPtr &dialog = (*iter).second;
            output.push_back(dialog->dialogID());
          }
        }

        //---------------------------------------------------------------------
        static void convert(const DialogList &input, DialogMap &output)
        {
          for (DialogList::const_iterator iter = input.begin(); iter != input.end(); ++iter)
          {
            const DialogPtr &dialog = (*iter);
            output[dialog->dialogID()] = dialog;
          }
        }

        //---------------------------------------------------------------------
        static void convert(const DialogList &input, DialogIDList &output)
        {
          for (DialogList::const_iterator iter = input.begin(); iter != input.end(); ++iter)
          {
            const DialogPtr &dialog = (*iter);
            output.push_back(dialog->dialogID());
          }
        }

        //---------------------------------------------------------------------
        static bool isMessageOrMessageBundle(ElementPtr element)
        {
          if ("messageBundle" == element->getValue()) {
            return true;
          }
          if ("message" == element->getValue()) {
            return true;
          }
          return false;
        }

        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Message
        #pragma mark

        // <thread>
        //  ...
        //  <messages>
        //   ...
        //   <messageBundle xmlns="http://www.openpeer.org/openpeer/1.0/message">
        //    <message id=”e041038922edbc0638cebbded884896” />
        //     <from id=”920bd1d88e4cc3ba0f95e24ea9168e272ff03b3b” />
        //     <sent>2002-Jan-01 10:00:01.123456789</sent>
        //     <mimeType>text/plain</mimeType>
        //     <body>This is a test message.</body>
        //    </message>
        //    <Signature xmlns="http://www.w3.org/2000/09/xmldsig#">
        //     <SignedInfo>
        //      <SignatureMethod Algorithm="http://www.w3.org/2000/09/xmldsig#rsa-sha1" />
        //      <Reference URI="#e041038922edbc0638cebbded884896">
        //       <DigestMethod Algorithm="http://www.w3.org/2000/09/xmldsig#sha1"/>
        //       <DigestValue>YUZSaWJFcFhXVzEwUzJOR2JITmFSazVYVm0xU2VsZHJWVFZpUmxwMVVXeHdW</DigestValue>
        //      </Reference>
        //     </SignedInfo>
        //     <SignatureValue>Y0ZoWFZ6RXdVekpPUjJKSVRtRlNhelZZVm0weFUyVnNa</SignatureValue>
        //    </Signature>
        //   </messageBundle>
        //   ...
        //  </messages>
        //  ...
        // </thread>

        //---------------------------------------------------------------------
        ElementPtr Message::toDebug(MessagePtr message)
        {
          if (!message) return ElementPtr();
          return message->toDebug();
        }

        //---------------------------------------------------------------------
        MessagePtr Message::create(
                                   const char *messageID,
                                   const char *fromPeerURI,
                                   const char *mimeType,
                                   const char *body,
                                   Time sent,
                                   IPeerFilesPtr signer
                                   )
        {
          MessagePtr pThis = MessagePtr(new Message);
          pThis->mThisWeak = pThis;
          pThis->mMessageID = string(messageID);
          pThis->mFromPeerURI = string(fromPeerURI);
          pThis->mMimeType = string(mimeType);
          pThis->mBody = string(body);
          pThis->mSent = sent;

          if (signer) {
            pThis->mBundleEl = pThis->constructBundleElement(signer);
          }

          return pThis;
        }

        //---------------------------------------------------------------------
        MessagePtr Message::create(
                                   UseAccountPtr account,
                                   ElementPtr messageBundleEl
                                   )
        {
          if (!messageBundleEl) return MessagePtr();

          MessagePtr pThis = MessagePtr(new Message);
          pThis->mThisWeak = pThis;

          try {
            ElementPtr messageEl = ("message" == messageBundleEl->getValue() ? messageBundleEl : messageBundleEl->findFirstChildElementChecked("message"));
            ElementPtr fromEl = messageEl->findFirstChildElementChecked("from");
            ElementPtr sentEl = messageEl->findFirstChildElementChecked("sent");
            ElementPtr mimeTypeEl = messageEl->findFirstChildElementChecked("mimeType");
            ElementPtr bodyEl = messageEl->findFirstChildElementChecked("body");

            pThis->mMessageID = messageEl->getAttributeValue("id");
            pThis->mFromPeerURI = fromEl->getAttributeValue("id");
            pThis->mMimeType = mimeTypeEl->getText();
            pThis->mBody = bodyEl->getTextDecoded();
            pThis->mSent = services::IHelper::stringToTime(sentEl->getText());
            if ("message" != messageBundleEl->getValue()) {
              pThis->mBundleEl = messageBundleEl;
            }
          } catch (CheckFailed &) {
            ZS_LOG_ERROR(Detail, pThis->log("message bundle parse element check failure"))
            return MessagePtr();
          }

          if (Time() == pThis->mSent) {
            ZS_LOG_ERROR(Detail, pThis->log("message bundle value out of range parse error"))
            return MessagePtr();
          }
          if (pThis->mMessageID.size() < 1) {
            ZS_LOG_ERROR(Detail, pThis->log("message id missing"))
            return MessagePtr();
          }
          if (pThis->mFromPeerURI.size() < 1) {
            ZS_LOG_ERROR(Detail, pThis->log("missing peer URI"))
            return MessagePtr();
          }

          return pThis;
        }

        //---------------------------------------------------------------------
        ElementPtr Message::messageBundleElement() const
        {
          if (mBundleEl) return mBundleEl->clone()->toElement();
          return constructBundleElement(IPeerFilesPtr());
        }

        //---------------------------------------------------------------------
        ElementPtr Message::toDebug() const
        {
          ElementPtr resultEl = Element::create("thread::Message");

          IHelper::debugAppend(resultEl, "id", mID);
          IHelper::debugAppend(resultEl, "message id (s)", mMessageID);
          IHelper::debugAppend(resultEl, "from peer URI", mFromPeerURI);
          IHelper::debugAppend(resultEl, "mime type", mMimeType);
          IHelper::debugAppend(resultEl, "body", mBody);
          IHelper::debugAppend(resultEl, "sent", mSent);

          return resultEl;
        }

        //---------------------------------------------------------------------
        ElementPtr Message::constructBundleElement(IPeerFilesPtr signer) const
        {
          // now its time to generate the XML
          ElementPtr messageBundleEl = Element::create("messageBundle");
          ElementPtr messageEl = createElement("message", mMessageID);
          ElementPtr fromEl = createElement("from", mFromPeerURI);
          ElementPtr sentEl = createElementWithNumber("sent", services::IHelper::timeToString(mSent));
          ElementPtr mimeTypeEl = createElementWithText("mimeType", mMimeType);
          ElementPtr bodyEl = createElementWithTextAndJSONEncode("body", mBody);

          if (signer) {
            messageBundleEl->adoptAsLastChild(messageEl);
          }

          messageEl->adoptAsLastChild(fromEl);
          messageEl->adoptAsLastChild(sentEl);
          messageEl->adoptAsLastChild(mimeTypeEl);
          messageEl->adoptAsLastChild(bodyEl);

          if (signer) {
            IPeerFilePrivatePtr privatePeer = signer->getPeerFilePrivate();
            ZS_THROW_INVALID_ARGUMENT_IF(!privatePeer)

            privatePeer->signElement(messageEl);

            return messageBundleEl;
          }

          return messageEl;
        }

        //---------------------------------------------------------------------
        Log::Params Message::log(const char *message) const
        {
          ElementPtr objectEl = Element::create("core::thread::Message");
          IHelper::debugAppend(objectEl, "id", mID);
          return Log::Params(message, objectEl);
        }

        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark MessageReceipts
        #pragma mark

        // <thread>
        //  ...
        //  <receipts version="1">
        //   <receipt id="e041038922edbc0638cebbded884896">2002-Jan-01 10:00:01.123456789</receipt>
        //   <receipt id="920bd1d88e4cc3ba0f95e24ea9168e2">2002-Jan-01 10:00:01.123456789</receipt>
        //  </receipts>
        //  ...
        // </thread>

        //---------------------------------------------------------------------
        ElementPtr MessageReceipts::toDebug(MessageReceiptsPtr receipts)
        {
          if (!receipts) return ElementPtr();
          return receipts->toDebug();
        }

        //---------------------------------------------------------------------
        MessageReceiptsPtr MessageReceipts::create(UINT version)
        {
          MessageReceiptMap receipts;
          return create(version, receipts);
        }

        //---------------------------------------------------------------------
        MessageReceiptsPtr MessageReceipts::create(UINT version, const String &messageID)
        {
          MessageReceiptMap receipts;
          receipts[messageID] = zsLib::now();
          return create(version, receipts);
        }

        //---------------------------------------------------------------------
        MessageReceiptsPtr MessageReceipts::create(UINT version, const MessageIDList &messageIDs)
        {
          MessageReceiptMap receipts;
          for (MessageIDList::const_iterator iter = messageIDs.begin(); iter != messageIDs.end(); ++iter)
          {
            const String &messageID = (*iter);
            receipts[messageID] = zsLib::now();
          }
          return create(version, receipts);
        }

        //---------------------------------------------------------------------
        MessageReceiptsPtr MessageReceipts::create(UINT version, const MessageReceiptMap &messageReceipts)
        {
          MessageReceiptsPtr pThis(new MessageReceipts);
          pThis->mThisWeak = pThis;
          pThis->mVersion = version;
          pThis->mReceipts = messageReceipts;

          return pThis;
        }

        //---------------------------------------------------------------------
        MessageReceiptsPtr MessageReceipts::create(ElementPtr messageReceiptsEl)
        {
          if (!messageReceiptsEl) return MessageReceiptsPtr();

          MessageReceiptsPtr pThis(new MessageReceipts);
          pThis->mThisWeak = pThis;

          try {
            pThis->mVersion = Numeric<UINT>(messageReceiptsEl->getAttributeValue("version"));
            ElementPtr receiptEl = messageReceiptsEl->findFirstChildElement("receipt");
            while (receiptEl)
            {
              String id = receiptEl->getAttributeValue("id");
              String timeStr = receiptEl->getText();
              ZS_LOG_TRACE(pThis->log("Parsing receipt") + ZS_PARAM("receipt ID", id) + ZS_PARAM("acknowledged at", timeStr))
              Time time = services::IHelper::stringToTime(timeStr);

              if (Time() == time) {
                ZS_LOG_ERROR(Detail, pThis->log("message receipt parse time invalid"))
                return MessageReceiptsPtr();
              }

              pThis->mReceipts[id] = time;
              ZS_LOG_TRACE(pThis->log("Found receipt") + ZS_PARAM("receipt ID", id) + ZS_PARAM("acknowledged at", time))

              receiptEl = receiptEl->findNextSiblingElement("receipt");
            }
          } catch(CheckFailed &) {
            return MessageReceiptsPtr();
          } catch (Numeric<UINT>::ValueOutOfRange &) {
            ZS_LOG_ERROR(Detail, pThis->log("message receipt parse value out of range"))
            return MessageReceiptsPtr();
          }
          return pThis;
        }

        //---------------------------------------------------------------------
        ElementPtr MessageReceipts::toDebug() const
        {
          ElementPtr resultEl = Element::create("thread::MessageReceipts");

          IHelper::debugAppend(resultEl, "id", mID);
          IHelper::debugAppend(resultEl, "version", mVersion);
          IHelper::debugAppend(resultEl, "receipts", mReceipts.size());

          return resultEl;
        }

        //---------------------------------------------------------------------
        Log::Params MessageReceipts::log(const char *message) const
        {
          ElementPtr objectEl = Element::create("core::thread::MessageReceipts");
          IHelper::debugAppend(objectEl, "id", mID);
          return Log::Params(message, objectEl);
        }

        //---------------------------------------------------------------------
        ElementPtr MessageReceipts::constructReceiptsElement() const
        {
          ElementPtr receiptsEl = Element::create("receipts");
          receiptsEl->setAttribute("version", string(mVersion));

          for (MessageReceiptMap::const_iterator iter = mReceipts.begin(); iter != mReceipts.end(); ++iter)
          {
            const String &messageID = (*iter).first;
            const Time &time = (*iter).second;
            ElementPtr receiptEl = createElementWithNumber("receipt", messageID, services::IHelper::timeToString(time));
            receiptsEl->adoptAsLastChild(receiptEl);
          }

          return receiptsEl;
        }

        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ThreadContact
        #pragma mark

        //---------------------------------------------------------------------
        ElementPtr ThreadContact::toDebug(ThreadContactPtr contact)
        {
          if (!contact) return ElementPtr();
          return contact->toDebug();
        }

        //---------------------------------------------------------------------
        ThreadContactPtr ThreadContact::create(
                                               UseContactPtr contact,
                                               const IdentityContactList &identityContacts
                                               )
        {
          ZS_THROW_INVALID_ARGUMENT_IF(!contact)

          ThreadContactPtr pThis(new ThreadContact);
          pThis->mContact = contact;

          IdentityContactList filtered;

          for (IdentityContactList::const_iterator iter = identityContacts.begin(); iter != identityContacts.end(); ++iter)
          {
            const IdentityContact &sourceIdentityContact = (*iter);

            IdentityContact identityContact;

            identityContact.mIdentityURI = sourceIdentityContact.mIdentityURI;
            identityContact.mIdentityProvider = sourceIdentityContact.mIdentityProvider;

            identityContact.mName = sourceIdentityContact.mName;
            identityContact.mProfileURL = sourceIdentityContact.mProfileURL;
            identityContact.mVProfileURL = sourceIdentityContact.mVProfileURL;

            identityContact.mAvatars = sourceIdentityContact.mAvatars;

            identityContact.mStableID = sourceIdentityContact.mStableID;

            identityContact.mPriority = sourceIdentityContact.mPriority;
            identityContact.mWeight = sourceIdentityContact.mWeight;

            identityContact.mLastUpdated = sourceIdentityContact.mLastUpdated;
            identityContact.mExpires = sourceIdentityContact.mExpires;

            filtered.push_back(identityContact);
          }

          pThis->mIdentityContacts = filtered;
          return pThis;
        }

        //---------------------------------------------------------------------
        ThreadContactPtr ThreadContact::create(
                                               UseAccountPtr account,
                                               ElementPtr contactEl
                                               )
        {
          String id = contactEl->getAttributeValue("id");

          UseContactPtr contact = account->findContact(id);
          if (!contact) {
            contact = IContactForConversationThread::createFromPeerURI(Account::convert(account), id);
            if (!contact) return ThreadContactPtr();
          }

          IdentityContactList identityContacts;

          ElementPtr identitiesEl = contactEl->findFirstChildElement("identities");
          if (identitiesEl) {
            ElementPtr identityEl = identitiesEl->findFirstChildElement("identity");
            while (identityEl) {

              IdentityContact identityContact;
              IdentityInfo identityInfo = IMessageHelper::createIdentity(identityEl);

              Helper::convert(identityInfo, identityContact);
              if (identityContact.hasData()) {
                identityContacts.push_back(identityContact);
              }

              identityEl = identityEl->findNextSiblingElement("identity");
            }
          }

          return ThreadContact::create(contact, identityContacts);
        }

        //---------------------------------------------------------------------
        ElementPtr ThreadContact::toDebug() const
        {
          ElementPtr resultEl = Element::create("core::thread::ThreadContact");

          IHelper::debugAppend(resultEl, "id", mID);
          IHelper::debugAppend(resultEl, UseContact::toDebug(mContact));
          IHelper::debugAppend(resultEl, "identity contacts", mIdentityContacts.size());

          return resultEl;
        }

        //---------------------------------------------------------------------
        ElementPtr ThreadContact::constructContactElement() const
        {
          ElementPtr contactEl = createElement("contact", mContact->getPeerURI());

          if (mIdentityContacts.size() > 0) {
            ElementPtr identitiesEl = Element::create("identities");

            for (IdentityContactList::const_iterator iter = mIdentityContacts.begin(); iter != mIdentityContacts.end(); ++iter)
            {
              const IdentityContact &identityContact = (*iter);

              IdentityInfo identityInfo;

              Helper::convert(identityContact, identityInfo);

              if (identityInfo.hasData()) {
                ElementPtr identityEl = IMessageHelper::createElement(identityInfo);
                identitiesEl->adoptAsLastChild(identityEl);
              }
            }

            if (identitiesEl->hasChildren()) {
              contactEl->adoptAsLastChild(identitiesEl);
            }
          }

          return contactEl;
        }

        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Contacts
        #pragma mark

        // <thread>
        //  ...
        //  <contacts version="1">
        //   <contact ID="peer://domain.com/920bd1d88e4cc3ba0f95e24ea9168e272ff03b3b" disposition="add"><peer version=”1”><sectionBundle xmlns="http://www.openpeer.org/openpeer/1.0/message"><section id=”A”> ... contents ...</section> ... </sectionBundle></peer></contact>
        //   <contact ID="peer://domain.com/f95e24ea9168e242ff03b3920bd1d88a4cc3ba04" disposition="add"><profileBundle><profile></profile></profileBundle><peer version=”1”><sectionBundle xmlns="http://www.openpeer.org/openpeer/1.0/message"><section id=”A”> ... contents ...</section> ... </sectionBundle></peer></contact>
        //   <contact ID="...">...</contact>
        //   <contact ID="..." disposition="remove"></contact>
        //  <contacts>
        //  ...
        // </thread>

        //---------------------------------------------------------------------
        ElementPtr ThreadContacts::toDebug(ThreadContactsPtr contacts)
        {
          if (!contacts) return ElementPtr();
          return contacts->toDebug();
        }

        //---------------------------------------------------------------------
        ThreadContactsPtr ThreadContacts::create(
                                                 UINT version,
                                                 const ThreadContactList &contacts,
                                                 const ThreadContactList &addContacts,
                                                 const ContactURIList &removeContacts
                                                 )
        {
          ThreadContactsPtr pThis(new ThreadContacts());
          pThis->mThisWeak = pThis;
          pThis->mVersion = version;

          for (ThreadContactList::const_iterator iter = contacts.begin(); iter != contacts.end(); ++iter)
          {
            const ThreadContactPtr &contact = (*iter);
            pThis->mContacts[contact->contact()->getPeerURI()] = contact;
          }

          for (ThreadContactList::const_iterator iter = addContacts.begin(); iter != addContacts.end(); ++iter)
          {
            const ThreadContactPtr &contact = (*iter);
            pThis->mAddContacts[contact->contact()->getPeerURI()] = contact;
          }

          pThis->mRemoveContacts = removeContacts;

          ElementPtr contactsEl = Element::create("contacts");
          if (0 != version) {
            contactsEl->setAttribute("version", string(version));
          }

          for (ThreadContactMap::const_iterator iter = pThis->mContacts.begin(); iter != pThis->mContacts.end(); ++iter)
          {
            const ThreadContactPtr &contact = (*iter).second;

            ElementPtr contactEl = contact->contactElement();
            contactsEl->adoptAsLastChild(contactEl);
          }

          for (ThreadContactMap::const_iterator iter = pThis->mAddContacts.begin(); iter != pThis->mAddContacts.end(); ++iter)
          {
            const ThreadContactPtr &contact = (*iter).second;

            ElementPtr contactEl = contact->contactElement();
            contactsEl->adoptAsLastChild(contactEl);
          }

          for (ContactURIList::const_iterator iter = pThis->mRemoveContacts.begin(); iter != pThis->mRemoveContacts.end(); ++iter)
          {
            const String &contactURI = (*iter);

            ElementPtr contactEl = createElement("contact", contactURI);
            contactEl->setAttribute("disposition", "remove");
            contactsEl->adoptAsLastChild(contactEl);
          }

          pThis->mContactsEl = contactsEl;
          return pThis;
        }

        //---------------------------------------------------------------------
        ThreadContactsPtr ThreadContacts::create(
                                                 UseAccountPtr account,
                                                 ElementPtr contactsEl
                                                 )
        {
          ZS_THROW_INVALID_ARGUMENT_IF(!account)

          if (!contactsEl) return ThreadContactsPtr();

          ThreadContactsPtr pThis(new ThreadContacts());
          pThis->mThisWeak = pThis;

          ThreadContactMap previousRemovals;

          try {
            pThis->mVersion = getVersion(contactsEl);

            ElementPtr contactEl = contactsEl->findFirstChildElement("contact");
            while (contactEl)
            {
              // scope: parse the contact information
              {
                String id = contactEl->getAttributeValue("id");
                String disposition = contactEl->getAttributeValue("disposition");

                if (disposition == "remove") {
                  if (previousRemovals.find(id) != previousRemovals.end()) goto next;

                  previousRemovals[id] = ThreadContactPtr();
                  pThis->mRemoveContacts.push_back(id);

                  goto next;
                }

                ThreadContactPtr threadContact = ThreadContact::create(account, contactEl);
                if (!threadContact) {
                  ZS_LOG_WARNING(Detail, pThis->log("thread contact was not legal"))
                  goto next;
                }

                UseContactPtr contact = threadContact->contact();

                if (disposition == "add") {
                  pThis->mAddContacts[contact->getPeerURI()] = threadContact;
                  goto next;
                }
                pThis->mContacts[contact->getPeerURI()] = threadContact;
              }

            next:
              contactEl = contactEl->findNextSiblingElement("contact");
            }
          } catch(CheckFailed &) {
            ZS_LOG_ERROR(Detail, pThis->log("contact element check parser failure"))
            return ThreadContactsPtr();
          } catch (Numeric<UINT>::ValueOutOfRange &) {
            ZS_LOG_ERROR(Detail, pThis->log("contact parser value out of range"))
            return ThreadContactsPtr();
          }
          return pThis;
        }

        //---------------------------------------------------------------------
        ElementPtr ThreadContacts::toDebug() const
        {
          ElementPtr resultEl = Element::create("core::thread::ThreadContacts");

          IHelper::debugAppend(resultEl, "id", mID);
          IHelper::debugAppend(resultEl, "version", mVersion);
          IHelper::debugAppend(resultEl, "contacts", mContacts.size());
          IHelper::debugAppend(resultEl, "add contacts", mAddContacts.size());
          IHelper::debugAppend(resultEl, "remove contacts", mRemoveContacts.size());

          return resultEl;
        }

        //---------------------------------------------------------------------
        Log::Params ThreadContacts::log(const char *message) const
        {
          ElementPtr objectEl = Element::create("core::thread::ThreadContacts");
          IHelper::debugAppend(objectEl, "id", mID);
          return Log::Params(message, objectEl);
        }

        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Dialog
        #pragma mark

        // <thread>
        //  ...
        //  <dialogs version="1">
        //   ...
        //   <dialogBundle>
        //    <dialog id=”5dc367c8392dfba2d08a27d8a2139ef7232b1df1” version="1">
        //     <state>open</state>
        //     <caller id=”920bd1d88e4cc3ba0f95e24ea9168e272ff03b3b” />
        //     <callerLocation id=”ed4c0d9a26d962ab179ee88f4c2e8695ccac3d7c” />
        //     <callee id=”83208ba54e6edb8cd2173b3d7fe83acca2ac5f3f” />
        //     <calleeLocation id=”225456088d6cc20f0797bbfcadfad84dba6fc292” />
        //     <replaces id=”d0790f20bf8de96f09e8d96674a7f30” />
        //     <descriptions>
        //      <description id=”3d6bef7dfb0b79072733fc03391e9” type=”audio” version="1">
        //       <ssrc id=”0eb60347” />
        //       <security>
        //        <secret>T0dVek5qTmtOREJrTW1KbU56bGpabVJqWXpGaE1USmhORGN3T0RJMFlqZ3laR0ps</secret>
        //        <algorithm>aes256</algorithm>
        //       </security>
        //       <codecs>
        //        <codec id=”0”>
        //         <name>ulaw</name>
        //         <ptime>20</ptime>
        //         <rate>8000</rate>
        //         <channels>1</channels>
        //        </codec>
        //        <codec id=”101”>
        //         <name>telephone-event</name>
        //         <ptime>20</ptime>
        //         <rate>8000</rate>
        //         <channels>1</channels>
        //        </codec>
        //       </codecs>
        //       <candidates>
        //        <candidate>
        //         <ip format=”ipv4”>100.200.10.20</ip>
        //         <ports>
        //          <port>9549</port>
        //          <port>49939</port>
        //         </ports>
        //         <username>7475bd88ec76c0f791fde51e56770f0d</username>
        //         <password>ZDMyNGU3MDcxZDNlMGZiMT..VjNjgwMTBlMjVh</password>
        //         <transport>udp</transport>
        //         <protocol>rtp/savp</protocol>
        //        </candidate>
        //        <candidate>
        //         <ip format=”ipv4”>192.168.1.2</ip>
        //         <ports>
        //          <port>15861</port>
        //          <port>43043</port>
        //         </ports>
        //         <username>118b63d5945c91919a4af5de24ebf9d4</username>
        //         <password>MTE4YjYzZDU5NDVjOTE5MTlhNGFmNWRlMjRlYmY5ZDQ=</password>
        //         <transport>udp</transport>
        //         <protocol>rtp/savp</protocol>
        //        </candidate>
        //       </candidates>
        //      </description>
        //      <description id=”938f6910ceb45c2269c6e7a710334bc” type=”video”>
        //       ...
        //      </description>
        //     </descriptions>
        //    </dialog>
        //    <Signature xmlns="http://www.w3.org/2000/09/xmldsig#">
        //     <SignedInfo>
        //      <SignatureMethod Algorithm="http://www.w3.org/2000/09/xmldsig#rsa-sha1" />
        //      <Reference URI="#5dc367c8392dfba2d08a27d8a2139ef7232b1df1">
        //       <DigestMethod Algorithm="http://www.w3.org/2000/09/xmldsig#sha1"/>
        //       <DigestValue>YUZSaWJFcFhXVzEwUzJOR2JITmFSazVYVm0xU2VsZHJWVFZpUmxwMVVXeHdW</DigestValue>
        //      </Reference>
        //     </SignedInfo>
        //     <SignatureValue>Y0ZoWFZ6RXdVekpPUjJKSVRtRlNhelZZVm0weFUyVnNa</SignatureValue>
        //    </Signature>
        //   </dialogBundle>
        //   ...
        //  <dialogs>
        //  ...
        // </thread>

        //---------------------------------------------------------------------
        Dialog::Dialog() :
          mVersion(0),
          mClosedReason(DialogClosedReason_None)
        {
        }

        //---------------------------------------------------------------------
        const char *Dialog::toString(DialogStates state)
        {
          switch (state)
          {
            case DialogState_None:      return "none";
            case DialogState_Preparing: return "preparing";
            case DialogState_Incoming:  return "incoming";
            case DialogState_Placed:    return "placed";
            case DialogState_Early:     return "early";
            case DialogState_Ringing:   return "ringing";
            case DialogState_Ringback:  return "ringback";
            case DialogState_Open:      return "open";
            case DialogState_Active:    return "active";
            case DialogState_Inactive:  return "inactive";
            case DialogState_Hold:      return "hold";
            case DialogState_Closing:   return "closing";
            case DialogState_Closed:    return "closed";
          }
          return "UNDEFINED";
        }

        //---------------------------------------------------------------------
        Dialog::DialogStates Dialog::toDialogStates(const char *state)
        {
          for (int loop = (int)DialogState_First; loop <= DialogState_Last; ++loop)
          {
            const char *name = toString((DialogStates)loop);
            if (string(name) == state) return (DialogStates)loop;
          }
          return DialogState_None;
        }

        //---------------------------------------------------------------------
        const char *Dialog::toString(DialogClosedReasons reason)
        {
          switch (reason)
          {
            case DialogClosedReason_None:                   return "";
            case DialogClosedReason_User:                   return "OK";
            case DialogClosedReason_RequestTimeout:         return "Request Timeout";
            case DialogClosedReason_TemporarilyUnavailable: return "Temporarily Unavailable";
            case DialogClosedReason_Busy:                   return "Busy Here";
            case DialogClosedReason_RequestTerminated:      return "Request Terminated";
            case DialogClosedReason_NotAcceptableHere:      return "Not Acceptable Here";
            case DialogClosedReason_ServerInternalError:    return "Server Internal Error";
            case DialogClosedReason_Decline:                return "Decline";
          }
          return "Unknown";
        }

        //---------------------------------------------------------------------
        ElementPtr Dialog::toDebug(DialogPtr dialog)
        {
          if (!dialog) return ElementPtr();
          return dialog->toDebug();
        }

        //---------------------------------------------------------------------
        DescriptionPtr Dialog::Description::create()
        {
          DescriptionPtr pThis(new Description);
          return pThis;
        }

        //---------------------------------------------------------------------
        DialogPtr Dialog::create(
                                 UINT version,
                                 const char *inDialogID,
                                 DialogStates state,
                                 DialogClosedReasons closedReason,
                                 const char *callerContactURI,
                                 const char *callerLocationID,
                                 const char *calleeContactURI,
                                 const char *calleeLocationID,
                                 const char *replacesDialogID,
                                 const DescriptionList &descriptions,
                                 IPeerFilesPtr signer
                                 )
        {
          ZS_THROW_INVALID_ARGUMENT_IF(!signer)
          ZS_THROW_INVALID_ARGUMENT_IF(!inDialogID)

          String dialogID(inDialogID);
          ZS_THROW_INVALID_ARGUMENT_IF(dialogID.isEmpty())

          DialogPtr pThis(new Dialog());
          pThis->mThisWeak = pThis;

          pThis->mVersion = version;
          pThis->mDialogID = dialogID;
          pThis->mState = state;
          pThis->mClosedReason = closedReason;
          pThis->mClosedReasonMessage = toString(closedReason);
          pThis->mCallerContactURI = string(callerContactURI);
          pThis->mCallerLocationID = string(callerLocationID);
          pThis->mCalleeContactURI = string(calleeContactURI);
          pThis->mCalleeLocationID = string(calleeLocationID);
          pThis->mReplacesDialogID = string(replacesDialogID);
          pThis->mDescriptions = descriptions;

          ElementPtr dialogBundleEl = createElement("dialogBundle", ("bundle_" + dialogID).c_str());
          ElementPtr dialogEl = createElement("dialog", dialogID);
          if (0 != version) {
            dialogEl->setAttribute("version", string(version));
          }
          dialogBundleEl->adoptAsLastChild(dialogEl);

          ElementPtr stateEl = createElementWithText("state", toString(state));
          ElementPtr closedEl;
          if (DialogClosedReason_None != closedReason) {
            closedEl = createElementWithText("closed", string(closedReason), pThis->mClosedReasonMessage);
          }
          ElementPtr fromContactIDEl = createElement("caller", callerContactURI);
          ElementPtr fromLocationIDEl = createElement("callerLocation", callerLocationID);
          ElementPtr toContactIDEl = createElement("callee", calleeContactURI);
          ElementPtr toLocationIDEl = createElement("calleeLocation", calleeLocationID);
          ElementPtr replacesEl;
          if (!(pThis->mReplacesDialogID.isEmpty())) {
            replacesEl = createElement("replaces", replacesDialogID);
          }

          ElementPtr descriptionsEl = Element::create("descriptions");

          dialogEl->adoptAsLastChild(stateEl);
          if (closedEl) {
            dialogEl->adoptAsLastChild(closedEl);
          }
          dialogEl->adoptAsLastChild(fromContactIDEl);
          dialogEl->adoptAsLastChild(fromLocationIDEl);
          dialogEl->adoptAsLastChild(toContactIDEl);
          dialogEl->adoptAsLastChild(toLocationIDEl);
          if (replacesEl) {
            dialogEl->adoptAsLastChild(replacesEl);
          }

          dialogEl->adoptAsLastChild(descriptionsEl);

          for (DescriptionList::const_iterator descIter = descriptions.begin(); descIter != descriptions.end(); ++descIter)
          {
            const DescriptionPtr &description = (*descIter);

            ElementPtr descriptionEl = createElement("description", description->mDescriptionID);
            descriptionEl->setAttribute("type", description->mType);
            if (0 != description->mVersion) {
              descriptionEl->setAttribute("version", string(description->mVersion));
            }

            ElementPtr ssrcEl = createElementWithText("ssrc", string(description->mSSRC));
            ElementPtr securityEl = Element::create("security");
            securityEl->setAttribute("cipher", description->mSecurityCipher);
            ElementPtr secretEl = createElementWithText("secret", description->mSecuritySecret);
            ElementPtr saltEl = createElementWithText("salt", description->mSecuritySalt);

            ElementPtr codecsEl = Element::create("codecs");

            ElementPtr iceUsernameFragEl = IMessageHelper::createElementWithTextAndJSONEncode("iceUsernameFrag", description->mICEUsernameFrag);
            ElementPtr icePasswordEl = IMessageHelper::createElementWithTextAndJSONEncode("icePassword", description->mICEPassword);

            for (CodecList::const_iterator codecIter = description->mCodecs.begin(); codecIter != description->mCodecs.end(); ++codecIter)
            {
              const Codec &codec = (*codecIter);

              ElementPtr codecEl = createElement("codec", string(codec.mCodecID));

              ElementPtr nameEl = createElementWithText("name", codec.mName);
              ElementPtr pTimeEl = createElementWithNumber("ptime", string(codec.mPTime));
              ElementPtr rateEl = createElementWithNumber("rate", string(codec.mRate));
              ElementPtr channelsEl = createElementWithNumber("channels", string(codec.mChannels));

              codecEl->adoptAsLastChild(nameEl);
              codecEl->adoptAsLastChild(pTimeEl);
              codecEl->adoptAsLastChild(rateEl);
              codecEl->adoptAsLastChild(channelsEl);

              codecsEl->adoptAsLastChild(codecEl);
            }

            ElementPtr candidatesEl = Element::create("candidates");

            for (CandidateList::iterator finalIter = description->mCandidates.begin(); finalIter != description->mCandidates.end(); ++finalIter)
            {
              Candidate &finalCandidate = (*finalIter);

              ElementPtr candidateEl = IMessageHelper::createElement(finalCandidate);
              if (candidateEl) {
                candidatesEl->adoptAsLastChild(candidateEl);
              }
            }

            securityEl->adoptAsLastChild(saltEl);
            securityEl->adoptAsLastChild(secretEl);

            descriptionEl->adoptAsLastChild(ssrcEl);
            descriptionEl->adoptAsLastChild(securityEl);
            descriptionEl->adoptAsLastChild(codecsEl);
            descriptionEl->adoptAsLastChild(iceUsernameFragEl);
            descriptionEl->adoptAsLastChild(icePasswordEl);
            if (description->mFinal) {
              ElementPtr finalEl = createElementWithNumber("iceFinal", "true");
              descriptionEl->adoptAsLastChild(finalEl);
            }

            descriptionEl->adoptAsLastChild(candidatesEl);

            descriptionsEl->adoptAsLastChild(descriptionEl);
          }

          IPeerFilePrivatePtr privatePeer = signer->getPeerFilePrivate();
          ZS_THROW_INVALID_ARGUMENT_IF(!privatePeer)

          privatePeer->signElement(dialogEl);

          pThis->mDialogBundleEl = dialogBundleEl;

          return pThis;
        }

        //---------------------------------------------------------------------
        DialogPtr Dialog::create(ElementPtr dialogBundleEl)
        {
          if (!dialogBundleEl) return DialogPtr();

          DialogPtr pThis(new Dialog());
          pThis->mThisWeak = pThis;
          pThis->mDialogBundleEl = dialogBundleEl;

          try {
            ElementPtr dialogEl = dialogBundleEl->findFirstChildElementChecked("dialog");
            pThis->mDialogID = dialogEl->getAttributeValue("id");
            pThis->mVersion = getVersion(dialogEl);

            ElementPtr stateEl = dialogEl->findFirstChildElementChecked("state");
            pThis->mState = toDialogStates(stateEl->getText());

            ElementPtr closedEl = dialogEl->findFirstChildElement("closed");
            if (closedEl) {
              try {
                WORD value = Numeric<WORD>(closedEl->getAttributeValue("id"));
                pThis->mClosedReason = (DialogClosedReasons)value;
              } catch(Numeric<WORD>::ValueOutOfRange &) {
                ZS_LOG_DEBUG(pThis->log("illegal value for closed reason"))
                return DialogPtr();
              }
              pThis->mClosedReasonMessage = closedEl->getTextDecoded();
            }

            ElementPtr callerContactIDEl = dialogEl->findFirstChildElementChecked("caller");
            pThis->mCallerContactURI = callerContactIDEl->getAttributeValue("id");

            ElementPtr callerLocationIDEl = dialogEl->findFirstChildElementChecked("callerLocation");
            pThis->mCallerLocationID = callerLocationIDEl->getAttributeValue("id");

            ElementPtr calleeContactIDEl = dialogEl->findFirstChildElementChecked("callee");
            pThis->mCalleeContactURI = calleeContactIDEl->getAttributeValue("id");

            ElementPtr calleeLocationIDEl = dialogEl->findFirstChildElementChecked("calleeLocation");
            pThis->mCalleeLocationID = calleeLocationIDEl->getAttributeValue("id");

            ElementPtr replacesEl = dialogEl->findFirstChildElement("replaces");
            if (replacesEl) {
              pThis->mReplacesDialogID = replacesEl->getAttributeValue("id");
            }

            ElementPtr descriptionsEl = dialogEl->findFirstChildElement("descriptions");
            ElementPtr descriptionEl = (descriptionsEl ? descriptionsEl->findFirstChildElement("description") : ElementPtr());

            while (descriptionEl)
            {
              DescriptionPtr description = Description::create();

              description->mDescriptionID = descriptionEl->getAttributeValue("id");
              description->mType = descriptionEl->getAttributeValue("type");
              description->mVersion = getVersion(descriptionEl);

              ElementPtr ssrcEl = descriptionEl->findFirstChildElementChecked("ssrc");
              description->mSSRC = Numeric<decltype(description->mSSRC)>(ssrcEl->getText());

              ElementPtr securityEl = descriptionEl->findFirstChildElementChecked("security");
              ElementPtr secretEl = securityEl->findFirstChildElementChecked("secret");
              ElementPtr saltEl = securityEl->findFirstChildElementChecked("salt");

              description->mSecurityCipher = securityEl->getAttributeValue("cipher");
              description->mSecuritySecret = secretEl->getText();
              description->mSecuritySalt = saltEl->getText();

              ElementPtr codecsEl = descriptionEl->findFirstChildElement("codecs");
              ElementPtr codecEl = (codecsEl ? codecsEl->findFirstChildElement("codec") : ElementPtr());

              while (codecEl)
              {
                Codec codec;
                codec.mCodecID = Numeric<decltype(codec.mCodecID)>(codecEl->getAttributeValue("id"));
                codec.mName = codecEl->findFirstChildElementChecked("name")->getText();
                codec.mPTime = Numeric<decltype(codec.mPTime)>(codecEl->findFirstChildElementChecked("ptime")->getText());
                codec.mRate = Numeric<decltype(codec.mRate)>(codecEl->findFirstChildElementChecked("rate")->getText());
                codec.mChannels = Numeric<decltype(codec.mChannels)>(codecEl->findFirstChildElementChecked("channels")->getText());
                description->mCodecs.push_back(codec);
              }

              description->mICEUsernameFrag = IMessageHelper::getElementTextAndDecode(descriptionEl->findFirstChildElementChecked("iceUsernameFrag"));
              description->mICEPassword = IMessageHelper::getElementTextAndDecode(descriptionEl->findFirstChildElementChecked("icePassword"));
              ElementPtr finalEl = descriptionEl->findFirstChildElement("iceFinal");

              description->mFinal = false;
              if (finalEl) {
                try {
                  description->mFinal = Numeric<bool>(finalEl->getText());
                } catch (Numeric<bool>::ValueOutOfRange &) {
                  ZS_LOG_WARNING(Debug, pThis->log("final value could not be interpreted"))
                }
              }

              ElementPtr candidatesEl = descriptionEl->findFirstChildElement("candidates");
              ElementPtr candidateEl = candidatesEl->findFirstChildElement("candidate");

              while (candidateEl)
              {
                Candidate candidate = IMessageHelper::createCandidate(candidateEl);
                if (candidate.hasData()) {
                  description->mCandidates.push_back(candidate);
                }

                candidateEl = candidateEl->findNextSiblingElement("candidate");
              }

              pThis->mDescriptions.push_back(description);

              descriptionEl = descriptionEl->findNextSiblingElement("description");
            }
          } catch(CheckFailed &) {
            ZS_LOG_ERROR(Detail, pThis->log("dialog element parse check failed"))
            return DialogPtr();
          } catch (Numeric<BYTE>::ValueOutOfRange &) {
            ZS_LOG_ERROR(Detail, pThis->log("dialog parse value out of range"))
            return DialogPtr();
          } catch (Numeric<WORD>::ValueOutOfRange &) {
            ZS_LOG_ERROR(Detail, pThis->log("dialog parse value out of range"))
            return DialogPtr();
          } catch (Numeric<DWORD>::ValueOutOfRange &) {
            ZS_LOG_ERROR(Detail, pThis->log("dialog parse value out of range"))
            return DialogPtr();
          } catch (IPAddress::Exceptions::ParseError &) {
            ZS_LOG_ERROR(Detail, pThis->log("dialog parse IP address parse error"))
            return DialogPtr();
          }
          return pThis;
        }

        //---------------------------------------------------------------------
        static String appendValue(const char *name, const String &value)
        {
          if (value.isEmpty()) return String();
          return ", " + String(name) + "=" + value;
        }

        //---------------------------------------------------------------------
        ElementPtr Dialog::toDebug() const
        {
          ElementPtr resultEl = Element::create("core::thread::Dialog");

          IHelper::debugAppend(resultEl, "id", mID);
          IHelper::debugAppend(resultEl, "version", mVersion);
          IHelper::debugAppend(resultEl, "dialog ID", mDialogID);
          IHelper::debugAppend(resultEl, "state", toString(mState));
          IHelper::debugAppend(resultEl, "closed reason", toString(mClosedReason));
          IHelper::debugAppend(resultEl, "closed reason message", mClosedReasonMessage);
          IHelper::debugAppend(resultEl, "caller contact URI", mCallerContactURI);
          IHelper::debugAppend(resultEl, "callee contact URI", mCalleeContactURI);
          IHelper::debugAppend(resultEl, "replaces", mReplacesDialogID);

          return resultEl;
        }

        //---------------------------------------------------------------------
        Log::Params Dialog::log(const char *message) const
        {
          ElementPtr objectEl = Element::create("core::thread::Dialog");
          IHelper::debugAppend(objectEl, "id", mID);
          return Log::Params(message, objectEl);
        }

        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Dialog::Codec
        #pragma mark

        //---------------------------------------------------------------------
        ElementPtr Dialog::Codec::toDebug() const
        {
          ElementPtr resultEl = Element::create("core::thread::Dialog::Codec");

          IHelper::debugAppend(resultEl, "id", string(mCodecID));
          IHelper::debugAppend(resultEl, "name", mName);
          IHelper::debugAppend(resultEl, "ptime", mPTime);
          IHelper::debugAppend(resultEl, "rate", mRate);
          IHelper::debugAppend(resultEl, "channels", mChannels);

          return resultEl;
        }

        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Dialog::Description
        #pragma mark

        //---------------------------------------------------------------------
        ElementPtr Dialog::Description::toDebug() const
        {
          ElementPtr resultEl = Element::create("core::thread::Dialog::Description");

          IHelper::debugAppend(resultEl, "id", mDescriptionID);
          IHelper::debugAppend(resultEl, "version", mVersion);
          IHelper::debugAppend(resultEl, "type", mType);
          IHelper::debugAppend(resultEl, "ssrc", mSSRC);
          IHelper::debugAppend(resultEl, "cipher", mSecurityCipher);
          IHelper::debugAppend(resultEl, "secret", mSecuritySecret);
          IHelper::debugAppend(resultEl, "salt", mSecuritySalt);
          IHelper::debugAppend(resultEl, "codecs", mCodecs.size());
          IHelper::debugAppend(resultEl, "candidates", mCandidates.size());
          IHelper::debugAppend(resultEl, "final", mFinal);

          return resultEl;
        }

        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Details
        #pragma mark

        // <thread>
        //  ...
        //  <details version="1">
        //    <threadBase id="..." />
        //    <threadHost id="..." />
        //    <replaces id="..." />
        //    <state>open</state>
        //    <created></created>
        //  </details>
        //  ...
        // </thread>

        //---------------------------------------------------------------------
        const char *Details::toString(ConversationThreadStates state)
        {
          switch (state) {
            case ConversationThreadState_None:    return "";
            case ConversationThreadState_Open:    return "open";
            case ConversationThreadState_Closed:  return "closed";
          }
          return "UNDEFINED";
        }

        //---------------------------------------------------------------------
        ConversationThreadStates Details::toConversationThreadState(const char *state)
        {
          String compare(state ? state : "");
          for (int loop = (int)ConversationThreadState_First; loop <= ConversationThreadState_Last; ++loop)
          {
            const char *name = toString((ConversationThreadStates)loop);
            if (name == compare) return (ConversationThreadStates)loop;
          }
          return ConversationThreadState_Closed;
        }

        //---------------------------------------------------------------------
        ElementPtr Details::toDebug(DetailsPtr details)
        {
          if (!details) return ElementPtr();
          return details->toDebug();
        }

        //---------------------------------------------------------------------
        DetailsPtr Details::create(
                                   UINT version,
                                   const char *baseThreadID,
                                   const char *hostThreadID,
                                   const char *topic,
                                   const char *replaces,
                                   ConversationThreadStates state
                                   )
        {
          DetailsPtr pThis(new Details());
          pThis->mThisWeak = pThis;
          pThis->mVersion = version;
          pThis->mBaseThreadID = string(baseThreadID);
          pThis->mHostThreadID = string(hostThreadID);
          pThis->mReplacesThreadID = string(replaces);
          pThis->mState = state;
          pThis->mTopic = string(topic);
          pThis->mCreated = zsLib::now();

          ElementPtr detailsEl = Element::create("details");
          if (0 != version) {
            detailsEl->setAttribute("version", string(version));
          }

          ElementPtr threadBaseEl = createElement("threadBase", baseThreadID);
          ElementPtr threadHostEl = createElement("threadHost", hostThreadID);
          ElementPtr replacesEl = createElement("replaces", replaces);
          ElementPtr stateEl = createElementWithText("state", toString(state));
          ElementPtr topicEl = createElementWithTextAndJSONEncode("topic", pThis->mTopic);
          ElementPtr createdEl = createElementWithNumber("created", services::IHelper::timeToString(pThis->mCreated));

          detailsEl->adoptAsLastChild(threadBaseEl);
          detailsEl->adoptAsLastChild(threadHostEl);
          detailsEl->adoptAsLastChild(replacesEl);
          detailsEl->adoptAsLastChild(stateEl);
          detailsEl->adoptAsLastChild(topicEl);
          detailsEl->adoptAsLastChild(createdEl);

          pThis->mDetailsEl = detailsEl;
          return pThis;
        }

        //---------------------------------------------------------------------
        DetailsPtr Details::create(ElementPtr detailsEl)
        {
          if (!detailsEl) return DetailsPtr();

          DetailsPtr pThis(new Details());
          pThis->mThisWeak = pThis;
          pThis->mDetailsEl = detailsEl;

          try {
            pThis->mVersion = 0;
            AttributePtr versionAt = detailsEl->findAttribute("version");
            if (versionAt) {
              pThis->mVersion = Numeric<UINT>(detailsEl->getAttributeValue("version"));
            }
            pThis->mBaseThreadID = detailsEl->findFirstChildElementChecked("threadBase")->getAttributeValue("id");
            pThis->mHostThreadID = detailsEl->findFirstChildElementChecked("threadHost")->getAttributeValue("id");
            pThis->mReplacesThreadID = detailsEl->findFirstChildElementChecked("replaces")->getAttributeValue("id");
            String state = detailsEl->findFirstChildElementChecked("state")->getText();
            state.trim();
            pThis->mState = toConversationThreadState(state);
            pThis->mTopic = detailsEl->findFirstChildElementChecked("topic")->getTextDecoded();
            pThis->mCreated = services::IHelper::stringToTime(detailsEl->findFirstChildElementChecked("created")->getText());
            if (Time() == pThis->mCreated) {
              ZS_LOG_ERROR(Detail, pThis->log("details parse time value not valid"))
              return DetailsPtr();
            }
          } catch(CheckFailed &) {
            ZS_LOG_ERROR(Detail, pThis->log("details element parse check failed"))
            return DetailsPtr();
          } catch (Numeric<UINT>::ValueOutOfRange &) {
            ZS_LOG_ERROR(Detail, pThis->log("details parse value out of range"))
            return DetailsPtr();
          }
          return pThis;
        }

        //---------------------------------------------------------------------
        ElementPtr Details::toDebug() const
        {
          ElementPtr resultEl = Element::create("core::thread::Details");

          IHelper::debugAppend(resultEl, "id", mID);
          IHelper::debugAppend(resultEl, "version", mVersion);
          IHelper::debugAppend(resultEl, "base thread id", mBaseThreadID);
          IHelper::debugAppend(resultEl, "host thread id", mHostThreadID);
          IHelper::debugAppend(resultEl, "replaces thread id", mReplacesThreadID);
          IHelper::debugAppend(resultEl, "state", toString(mState));
          IHelper::debugAppend(resultEl, "topic", mTopic);
          IHelper::debugAppend(resultEl, "created", mCreated);

          return resultEl;
        }

        //---------------------------------------------------------------------
        Log::Params Details::log(const char *message) const
        {
          ElementPtr objectEl = Element::create("core::thread::Details");
          IHelper::debugAppend(objectEl, "id", mID);
          return Log::Params(message, objectEl);
        }

        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Thread
        #pragma mark

        // <thread>
        //  <details version="1">
        //   ...
        //  </details>
        //  <contacts version="1">
        //   ...
        //  </contacts>
        //  <messages version="1">
        //   ...
        //  </messages>
        //  <receipts version="1">
        //   ...
        //  </receipts>
        //  <dialogs version="1">
        //   ...
        //  </dialogs>
        // </thread>

        //---------------------------------------------------------------------
        const char *Thread::toString(ThreadTypes type)
        {
          switch (type) {
            case ThreadType_Host:   return "host";
            case ThreadType_Slave:  return "slave";
          }
          return "UNDEFINED";
        }

        //---------------------------------------------------------------------
        ElementPtr Thread::toDebug(ThreadPtr thread)
        {
          if (!thread) return ElementPtr();
          return thread->toDebug();
        }

        //---------------------------------------------------------------------
        Thread::ThreadTypes Thread::toThreadTypes(const char *inType) throw (InvalidArgument)
        {
          String type = string(inType);
          if (type == "host") return ThreadType_Host;
          if (type == "slave") return ThreadType_Slave;
          return ThreadType_Host;
        }

        //---------------------------------------------------------------------
        Thread::Thread() :
          mType(ThreadType_Host),
          mCanModify(false),
          mModifying(false),
          mMessagesVersion(0),
          mDialogsVersion(0),
          mDetailsChanged(false)
        {
          ZS_LOG_DEBUG(log("created"))
        }

        //---------------------------------------------------------------------
        Thread::~Thread()
        {
          ZS_LOG_DEBUG(log("destroyed"))
        }

        //---------------------------------------------------------------------
        ThreadPtr Thread::create(
                                 UseAccountPtr account,
                                 IPublicationPtr publication
                                 )
        {
          ZS_THROW_INVALID_ARGUMENT_IF(!account)
          ZS_THROW_INVALID_ARGUMENT_IF(!publication)

          ThreadPtr pThis(new Thread);
          pThis->mThisWeak = pThis;
          pThis->mPublication = publication;

          SplitMap result;
          services::IHelper::split(publication->getName(), result, '/');
          String type = services::IHelper::get(result, OPENPEER_CONVERSATION_THREAD_TYPE_INDEX);

          try {
            pThis->mType = toThreadTypes(type);
          } catch (InvalidArgument &) {
            ZS_LOG_ERROR(Detail, pThis->log("thread type is invalid") + ZS_PARAM("type specified", type))
            return ThreadPtr();
          }

          IPublicationLockerPtr lock;
          DocumentPtr doc = publication->getJSON(lock);
          if (!doc) {
            ZS_LOG_ERROR(Detail, pThis->log("thread was unable to get from publication"))
            return ThreadPtr();
          }

          try {
            ElementPtr threadEl = doc->findFirstChildElementChecked("thread");

            // parse the details...
            pThis->mDetails = Details::create(threadEl->findFirstChildElementChecked("details"));
            if (!pThis->mDetails) {
              ZS_LOG_ERROR(Detail, pThis->log("Unable to load thread details from publication"))
              return ThreadPtr();
            }
            pThis->mDetailsChanged = true;

            pThis->mContacts = ThreadContacts::create(account, threadEl->findFirstChildElementChecked("contacts"));
            if (!pThis->mContacts) {
              ZS_LOG_ERROR(Detail, pThis->log("unable to load thread contacts from publication"))
              return ThreadPtr();
            }

            // every contact is to be treated as a change...
            pThis->mContactsChanged = pThis->mContacts->contacts();
            pThis->mContactsToAddChanged = pThis->mContacts->addContacts();
            pThis->mContactsToRemoveChanged = pThis->mContacts->removeContacts();

            ElementPtr messagesEl = threadEl->findFirstChildElementChecked("messages");
            pThis->mMessagesVersion = getVersion(messagesEl);

            ElementPtr messageBundleEl = messagesEl->getFirstChildElement();
            while (messageBundleEl) {
              if (!isMessageOrMessageBundle(messageBundleEl)) goto next_message;

              {
                MessagePtr message = Message::create(account, messageBundleEl);
                if (!message) {
                  ZS_LOG_ERROR(Detail, pThis->log("failed to parse message from thread document"))
                  return ThreadPtr();
                }
                pThis->mMessageMap[message->messageID()] = message;
                pThis->mMessageList.push_back(message);
                pThis->mMessagesChangedTime = zsLib::now();
              }

            next_message:
              messageBundleEl = messageBundleEl->getNextSiblingElement();
            }

            // every message found is a changed message...
            pThis->mMessagesChanged = pThis->mMessageList;

            pThis->mMessageReceipts = MessageReceipts::create(threadEl->findFirstChildElementChecked("receipts"));
            if (!pThis->mMessageReceipts) {
              ZS_LOG_ERROR(Detail, pThis->log("unable to load receipts in this publication"))
              return ThreadPtr();
            }
            // every message receipt is a changed message receipt
            pThis->mMessageReceiptsChanged = pThis->mMessageReceipts->receipts();

            ElementPtr dialogsEl = threadEl->findFirstChildElementChecked("dialogs");
            pThis->mDialogsVersion = getVersion(dialogsEl);

            ElementPtr dialogBundleEl = dialogsEl->findFirstChildElement("dialogBundle");
            while (dialogBundleEl) {
              DialogPtr dialog = Dialog::create(dialogBundleEl);
              if (!dialog) {
                ZS_LOG_ERROR(Detail, pThis->log("failed to parse dialog from thread document"))
                return ThreadPtr();
              }

              // every dialog found is a dialog changed...
              pThis->mDialogsChanged[dialog->dialogID()] = dialog;
              pThis->mDialogs[dialog->dialogID()] = dialog;

              // every description found in a dialog is a description changed...
              for (DescriptionList::const_iterator iter = dialog->descriptions().begin(); iter != dialog->descriptions().end(); ++iter)
              {
                const DescriptionPtr &description = (*iter);
                pThis->mDescriptionsChanged[description->mDescriptionID] = ChangedDescription(dialog->dialogID(), description);
              }

              dialogBundleEl = dialogBundleEl->findNextSiblingElement("dialogBundle");
            }

          } catch (zsLib::XML::Exceptions::CheckFailed &) {
            ZS_LOG_ERROR(Detail, pThis->log("failed to parse document"))
            return ThreadPtr();
          } catch (Numeric<UINT>::ValueOutOfRange &) {
            ZS_LOG_ERROR(Detail, pThis->log("failed to parse document as value was out of range"))
            return ThreadPtr();
          }

          return pThis;
        }

        //---------------------------------------------------------------------
        bool Thread::updateFrom(
                                UseAccountPtr account,
                                IPublicationPtr publication
                                )
        {
          ZS_THROW_INVALID_ARGUMENT_IF(!account)
          ZS_THROW_INVALID_ARGUMENT_IF(!publication)

          ZS_THROW_INVALID_ARGUMENT_IF(publication->getName() != mPublication->getName())

          IPublicationLockerPtr lock;
          DocumentPtr doc = publication->getJSON(lock);
          if (!doc) {
            ZS_LOG_ERROR(Detail, log("publication document was NULL"))
            return false;
          }

          // reset all "changed"
          resetChanged();

          DetailsPtr details;
          ThreadContactsPtr contacts;
          UINT messagesVersion = 0;
          MessageList messages;
          MessageReceiptsPtr receipts;
          UINT dialogsVersion = 0;
          DialogMap dialogs;
          DialogMap dialogsChanged;

          try {
            ElementPtr threadEl = doc->findFirstChildElementChecked("thread");

            // parse the details...
            ElementPtr detailsEl = threadEl->findFirstChildElementChecked("details");
            UINT version = getVersion(detailsEl);

            if (version > mDetails->version()) {
              // parse the details
              details = Details::create(detailsEl);
              if (!details) return false;
            }

            ElementPtr contactsEl = threadEl->findFirstChildElementChecked("contacts");
            version = getVersion(contactsEl);
            if (version > mContacts->version()) {
              contacts = ThreadContacts::create(account, contactsEl);
              if (!contacts) return false;
            }

            ElementPtr messagesEl = threadEl->findFirstChildElementChecked("messages");
            messagesVersion = getVersion(messagesEl);

            if (messagesVersion > mMessagesVersion) {
              ElementPtr messageBundleEl = messagesEl->getLastChildElement();
              ElementPtr firstValidBundleEl;
              while (messageBundleEl) {
                if (!isMessageOrMessageBundle(messageBundleEl)) goto previous_message;

                {
                  ElementPtr messageEl = ("message" == messageBundleEl->getValue() ? messageBundleEl : messageBundleEl->findFirstChildElement("message"));
                  if (!messageEl) {
                    ZS_LOG_ERROR(Detail, log("missing <message ...> element inside messageBundle (which is illegal)"))
                    return false;
                  }
                  String id = messageEl->getAttributeValue("id");
                  if (id.size() < 1) {
                    ZS_LOG_ERROR(Detail, log("ID attribute is missing from <message> (which is illegal)"))
                    return false;
                  }

                  MessageMap::iterator found = mMessageMap.find(id);
                  if (found != mMessageMap.end()) break;

                  firstValidBundleEl = messageBundleEl;
                }

              previous_message:
                messageBundleEl = messageBundleEl->getPreviousSiblingElement();
              }

              messageBundleEl = firstValidBundleEl;
              while (messageBundleEl) {
                if (!isMessageOrMessageBundle(messageBundleEl)) goto next_message;

                {
                  MessagePtr message = Message::create(account, messageBundleEl);
                  if (!message) {
                    ZS_LOG_ERROR(Detail, log("unable to parse message bundle"))
                    return false;
                  }

                  messages.push_back(message);
                }

              next_message:
                messageBundleEl = messageBundleEl->getNextSiblingElement();
              }
            }

            ElementPtr receiptsEl = threadEl->findFirstChildElementChecked("receipts");
            version = getVersion(receiptsEl);
            if (version > mMessageReceipts->version()) {
              receipts = MessageReceipts::create(receiptsEl);
            }

            ElementPtr dialogsEl = threadEl->findFirstChildElementChecked("dialogs");
            dialogsVersion = getVersion(dialogsEl);

            if (dialogsVersion > mDialogsVersion) {
              ElementPtr dialogBundleEl = dialogsEl->findFirstChildElement("dialogBundle");
              while (dialogBundleEl) {
                ElementPtr dialogEl = dialogBundleEl->findFirstChildElement("dialog");
                String id = dialogEl->getAttributeValue("id");

                bool update = false;
                DialogMap::iterator found = mDialogs.find(id);
                if (found != mDialogs.end()) {
                  version = getVersion(dialogEl);
                  DialogPtr &dialog = (*found).second;
                  if (version > dialog->version()) {
                    ZS_LOG_TRACE(log("dialog change detected") + ZS_PARAM("dialog", dialog->toDebug()))
                    update = true;
                  } else {
                    dialogs[id] = dialog; // using existing dialog
                    ZS_LOG_TRACE(log("using existing dialog") + ZS_PARAM("dialog", dialog->toDebug()))
                  }
                } else {
                  update = true;
                  ZS_LOG_TRACE(log("new dialog detected") + ZS_PARAM("dialog ID", id))
                }

                if (update) {
                  DialogPtr dialog = Dialog::create(dialogBundleEl);
                  if (!dialog) return false;
                  dialogs[id] = dialog;
                  dialogsChanged[id] = dialog;
                }

                dialogBundleEl = dialogBundleEl->findNextSiblingElement("dialogBundle");
              }
            } else {
              ZS_LOG_TRACE(log("dialogs did not change"))
              dialogs = mDialogs;
            }
          } catch (zsLib::XML::Exceptions::CheckFailed &) {
            ZS_LOG_ERROR(Detail, log("failed to update document as parsing failed"))
            return false;
          } catch (Numeric<UINT>::ValueOutOfRange &) {
            ZS_LOG_ERROR(Detail, log("failed to update document as value out of range"))
            return false;
          }

          if (details) {
            mDetails = details;
            mDetailsChanged = true;
          }

          if (contacts) {
            // see which contacts have changed...
            const ThreadContactMap &newContacts = contacts->contacts();
            const ThreadContactMap &newAddContacts = contacts->addContacts();
            const ContactURIList &newRemoveContacts = contacts->removeContacts();

            ThreadContactMap newRemoveContactsMap;
            convert(newRemoveContacts, newRemoveContactsMap);

            const ThreadContactMap &oldContacts = mContacts->contacts();
            const ThreadContactMap &oldtAddContacts = mContacts->addContacts();
            const ContactURIList &oldRemoveContacts = mContacts->removeContacts();

            ThreadContactMap oldRemoveContactsMap;
            convert(oldRemoveContacts, oldRemoveContactsMap);

            // figure out which contacts are new...
            for (ThreadContactMap::const_iterator iter = newContacts.begin(); iter != newContacts.end(); ++iter)
            {
              const ContactURI &id = (*iter).first;
              const ThreadContactPtr &contact = (*iter).second;
              ThreadContactMap::const_iterator found = oldContacts.find(id);
              if (found == oldContacts.end()) {
                // didn't find in the old list so it must be 'new'
                mContactsChanged[id] = contact;
              }
            }

            // figure out which "add" contacts are new...
            for (ThreadContactMap::const_iterator iter = newAddContacts.begin(); iter != newAddContacts.end(); ++iter)
            {
              const ContactURI &id = (*iter).first;
              const ThreadContactPtr &contact = (*iter).second;
              ThreadContactMap::const_iterator found = oldtAddContacts.find(id);
              if (found == oldtAddContacts.end()) {
                // didn't find in the old list so it must be 'new'
                mContactsToAddChanged[id] = contact;
              }
            }

            // figure out which "remove" contacts are new...
            for (ThreadContactMap::const_iterator iter = newRemoveContactsMap.begin(); iter != newRemoveContactsMap.end(); ++iter)
            {
              const ContactURI &id = (*iter).first;
              ThreadContactMap::const_iterator found = oldRemoveContactsMap.find(id);
              if (found == oldRemoveContactsMap.end()) {
                // didn't find in the old list so it must be 'new' removal
                mContactsToRemoveChanged.push_back(id);
              }
            }

            // figure out which contacts have been removed...
            for (ThreadContactMap::const_iterator iter = oldContacts.begin(); iter != oldContacts.end(); ++iter)
            {
              const ContactURI &id = (*iter).first;
              ThreadContactMap::const_iterator found = newContacts.find(id);
              if (found == newContacts.end()) {
                // didn't find in the old list so it must be 'new'
                mContactsRemoved.push_back(id);
              }
            }

            // figure out which "add" contacts have been removed...
            for (ThreadContactMap::const_iterator iter = oldtAddContacts.begin(); iter != oldtAddContacts.end(); ++iter)
            {
              const ContactURI &id = (*iter).first;
              ThreadContactMap::const_iterator found = newAddContacts.find(id);
              if (found == newAddContacts.end()) {
                // didn't find in the old list so it must be 'new'
                mContactsToAddRemoved.push_back(id);
              }
            }

            // figure out which "remove" contacts have been removed...
            for (ThreadContactMap::const_iterator iter = oldRemoveContactsMap.begin(); iter != oldRemoveContactsMap.end(); ++iter)
            {
              const ContactURI &id = (*iter).first;
              ThreadContactMap::const_iterator found = newRemoveContactsMap.find(id);
              if (found == newRemoveContactsMap.end()) {
                // didn't find in the old list so it must be 'new'
                mContactsToAddRemoved.push_back(id);
              }
            }

            mContacts = contacts;
          }

          if (messagesVersion > mMessagesVersion) {
            for (MessageList::iterator iter = messages.begin(); iter != messages.end(); ++iter) {
              const MessagePtr &message = (*iter);
              mMessageList.push_back(message);
              mMessageMap[message->messageID()] = message;
              mMessagesChanged.push_back(message);
            }

            if (messages.size() > 0) {
              mMessagesChangedTime = zsLib::now();
            }
          }

          if (receipts) {
            // figure out which message receipts have changed
            const MessageReceiptMap &oldReceipts = mMessageReceipts->receipts();
            const MessageReceiptMap &newReceipts = receipts->receipts();

            for (MessageReceiptMap::const_iterator iter = newReceipts.begin(); iter != newReceipts.end(); ++iter)
            {
              const MessageID &id = (*iter).first;
              const Time &time = (*iter).second;
              MessageReceiptMap::const_iterator found = oldReceipts.find(id);
              if (found == oldReceipts.end()) {
                // did not have this receipt last time so it's new...
                mMessageReceiptsChanged[id] = time;
              }
            }
          }

          if (dialogsVersion > mDialogsVersion) {
            ChangedDescriptionMap oldDescriptions;
            ChangedDescriptionMap newDescriptions;

            // build list of old descriptions...
            for (DialogMap::iterator iter = mDialogs.begin(); iter != mDialogs.end(); ++iter)
            {
              DialogPtr &dialog = (*iter).second;
              for (DescriptionList::const_iterator descIter = dialog->descriptions().begin(); descIter != dialog->descriptions().end(); ++descIter)
              {
                const DescriptionPtr &description = (*descIter);

                ZS_LOG_TRACE(log("found old description") + Dialog::toDebug(dialog) + description->toDebug())
                oldDescriptions[description->mDescriptionID] = ChangedDescription(dialog->dialogID(), description);
              }
            }

            // build a list of new descriptions...
            for (DialogMap::iterator iter = dialogs.begin(); iter != dialogs.end(); ++iter)
            {
              DialogPtr &dialog = (*iter).second;
              for (DescriptionList::const_iterator descIter = dialog->descriptions().begin(); descIter != dialog->descriptions().end(); ++descIter)
              {
                const DescriptionPtr &description = (*descIter);

                ZS_LOG_TRACE(log("found new description") + Dialog::toDebug(dialog) + description->toDebug())
                newDescriptions[description->mDescriptionID] = ChangedDescription(dialog->dialogID(), description);
              }
            }

            // figure out which descriptions are new...
            for (ChangedDescriptionMap::iterator iter = newDescriptions.begin(); iter != newDescriptions.end(); ++iter)
            {
              const DescriptionID &descriptionID = (*iter).first;
              DescriptionPtr &description = (*iter).second.second;
              DialogID &dialogID = (*iter).second.first;
              ChangedDescription &changed = (*iter).second;

              ChangedDescriptionMap::iterator found = oldDescriptions.find(descriptionID);
              if (found != oldDescriptions.end()) {
                DescriptionPtr &oldDescription = (*found).second.second;
                if (description->mVersion > oldDescription->mVersion) {
                  // this description has changed
                  mDescriptionsChanged[descriptionID] = changed;
                  ZS_LOG_TRACE(log("description change detected") + ZS_PARAM("dialog ID", dialogID) + description->toDebug())
                }
              } else {
                // this is a new description entirely
                ZS_LOG_TRACE(log("new description detected") + ZS_PARAM("dialog ID", dialogID) + description->toDebug())
                mDescriptionsChanged[descriptionID] = changed;
              }
            }

            // figure out which descriptions are removed...
            for (ChangedDescriptionMap::iterator iter = oldDescriptions.begin(); iter != oldDescriptions.end(); ++iter)
            {
              const DescriptionID &descriptionID = (*iter).first;
              DialogID &dialogID = (*iter).second.first;

              ChangedDescriptionMap::iterator found = newDescriptions.find(descriptionID);
              if (found == newDescriptions.end()) {
                // this description has now been removed entirely...
                mDescriptionsRemoved.push_back(descriptionID);
                ZS_LOG_TRACE(log("description removal detected") + ZS_PARAM("dialog ID", dialogID) + ZS_PARAM("description ID", descriptionID))
              }
            }

            // figure out which dialogs are now gone
            for (DialogMap::iterator dialogIter = mDialogs.begin(); dialogIter != mDialogs.end(); )
            {
              DialogMap::iterator current = dialogIter;
              ++dialogIter;

              const DialogID &id = (*current).first;
              DialogMap::iterator found = dialogs.find(id);
              if (found == dialogs.end()) {
                // this is dialog is completely gone...
                ZS_LOG_TRACE(log("dialog detected removed") + ZS_PARAM("dialog ID", id))
                mDialogsRemoved.push_back(id);
              }
            }

            mDialogs = dialogs;
            mDialogsChanged = dialogsChanged;

            // mDialogs have now been updated with all the changed dialogs and the
            // removed dialogs are all remembered and then erased from mDialogs...

            // remember the new dialog version...
            mDialogsVersion = dialogsVersion;
          }

          return true;
        }

        //---------------------------------------------------------------------
        ThreadPtr Thread::create(
                                 UseAccountPtr account,
                                 ThreadTypes threadType,         // needed for document name
                                 ILocationPtr creatorLocation,
                                 const char *baseThreadID,
                                 const char *hostThreadID,
                                 const char *topic,
                                 const char *replaces,
                                 ConversationThreadStates state,
                                 ILocationPtr peerHostLocation
                                 )
        {
          ZS_THROW_INVALID_ARGUMENT_IF(!account)
          ZS_THROW_INVALID_ARGUMENT_IF(!creatorLocation)
          ZS_THROW_INVALID_ARGUMENT_IF(!baseThreadID)
          ZS_THROW_INVALID_ARGUMENT_IF(!hostThreadID)

          ThreadPtr pThis = ThreadPtr(new Thread);
          pThis->mThisWeak = pThis;
          pThis->mType = threadType;
          pThis->mCanModify = true;

          stack::IAccountPtr stackAccount = account->getStackAccount();
          if (!stackAccount) {
            ZS_LOG_ERROR(Basic, pThis->log("stack account is null"))
            return ThreadPtr();
          }

          pThis->mDetails = Details::create(1, baseThreadID, hostThreadID, topic, replaces, state);
          if (!pThis->mDetails) return ThreadPtr();

          ThreadContactList empty;
          ContactURIList emptyIDs;
          pThis->mContacts = ThreadContacts::create(1, empty, empty, emptyIDs);

          pThis->mMessagesVersion = 1;
          pThis->mMessageReceipts = MessageReceipts::create(1);
          pThis->mDialogsVersion = 1;

          DocumentPtr doc = Document::create();

          ElementPtr threadEl = Element::create("thread");
          ElementPtr messagesEl = Element::create("messages");
          if (0 != pThis->mMessagesVersion) {
            messagesEl->setAttribute("version", string(pThis->mMessagesVersion));
          }
          ElementPtr dialogsEl = Element::create("dialogs");
          if (0 != pThis->mDialogsVersion) {
            dialogsEl->setAttribute("version", string(pThis->mDialogsVersion));
          }

          doc->adoptAsLastChild(threadEl);
          threadEl->adoptAsLastChild(pThis->mDetails->detailsElement()->clone());
          threadEl->adoptAsLastChild(pThis->mContacts->contactsElement()->clone());
          threadEl->adoptAsLastChild(messagesEl);
          threadEl->adoptAsLastChild(pThis->mMessageReceipts->receiptsElement());
          threadEl->adoptAsLastChild(dialogsEl);

          String baseName = String("/threads/1.0/") + toString(threadType) + "/" + baseThreadID + "/" + hostThreadID + "/";
          String name = baseName + "state";
          String permissionName = baseName + "permissions";

          PublishToRelationshipsMap publishRelationships;
          PublishToRelationshipsMap publishEmptyRelationships;
          RelationshipList relationships;

          if (ThreadType_Slave == threadType) {
            // when acting as a slave the only contact that has permission to read the slave document is the host peer
            ZS_THROW_INVALID_ARGUMENT_IF(!peerHostLocation)
            ZS_THROW_INVALID_ARGUMENT_IF(!peerHostLocation->getPeer())

            String hostContactURI = peerHostLocation->getPeer()->getPeerURI();
            ZS_LOG_TRACE(pThis->log("slave thread") + ZS_PARAM("using host URI", hostContactURI) + ZS_PARAM("base thread ID", baseThreadID) + ZS_PARAM("host thread ID", hostThreadID))

            relationships.push_back(hostContactURI);
          }

          // scope: add "all" permissions
          {
            typedef IPublication::PeerURIList PeerURIList;
            typedef IPublication::PermissionAndPeerURIListPair PermissionAndPeerURIListPair;

            PeerURIList empty;
            publishRelationships[permissionName] = PermissionAndPeerURIListPair(IPublication::Permission_All, empty);
          }

          pThis->mPublication = IPublication::create(creatorLocation, name, "text/x-json-openpeer", doc, publishRelationships, ILocation::getForLocal(stackAccount));
          pThis->mPermissionPublication = IPublication::create(creatorLocation, permissionName, "text/x-json-openpeer-permissions", relationships, publishEmptyRelationships, ILocation::getForLocal(stackAccount));

          doc.reset();  // been adopted

          return pThis;
        }

        //---------------------------------------------------------------------
        void Thread::updateBegin()
        {
          ZS_THROW_INVALID_USAGE_IF(!mCanModify)
          ZS_THROW_INVALID_USAGE_IF(mModifying)
          ZS_THROW_BAD_STATE_IF(mChangesDoc)

          mModifying = true;

          resetChanged();
          mChangesDoc.reset();
        }

        //---------------------------------------------------------------------
        bool Thread::updateEnd()
        {
          ZS_THROW_INVALID_USAGE_IF(!mCanModify)
          ZS_THROW_INVALID_USAGE_IF(!mModifying)

          mModifying = false;

          // scope: figure out difference document (needs to be scoped because
          //        of publication lock returned from getXML(...) could cause
          //        unintended deadlock
          {
            IPublicationLockerPtr lock;
            DocumentPtr doc = mPublication->getJSON(lock);
            ElementPtr threadEl = doc->findFirstChildElementChecked("thread");

            // have the details changed since last time?
            if (mDetailsChanged) {
              ElementPtr detailsEl = threadEl->findFirstChildElementChecked("details");
              IDiff::createDiffs(IDiff::DiffAction_Replace, mChangesDoc, detailsEl, false, mDetails->detailsElement()->clone());
            }

            // have any contacts changed...
            if ((mContactsChanged.size() > 0) ||
                (mContactsRemoved.size() > 0) ||
                (mContactsToAddChanged.size() > 0) ||
                (mContactsToAddRemoved.size() > 0) ||
                (mContactsToRemoveChanged.size() > 0) ||
                (mContactsToRemoveRemoved.size() > 0))
            {
              // something has changed with the contacts, calculate all the differences...
              ThreadContactMap contacts;
              ThreadContactMap addContacts;
              ContactURIList removeContacts;
              ThreadContactMap removeContactsMap;

              // start with the contacts we already have...
              contacts = mContacts->contacts();
              addContacts = mContacts->addContacts();
              const ContactURIList &oldRemoveContacts = mContacts->removeContacts();
              convert(oldRemoveContacts, removeContactsMap);

              // go through and apply the updates...
              for (ThreadContactMap::iterator iter = mContactsChanged.begin(); iter != mContactsChanged.end(); ++iter)
              {
                const ContactURI &id = (*iter).first;
                ThreadContactPtr &contact = (*iter).second;

                // apply the change now...
                contacts[id] = contact;
              }

              // remove all contacts that do not belong anymore...
              for (ContactURIList::iterator iter = mContactsRemoved.begin(); iter != mContactsRemoved.end(); ++iter)
              {
                const ContactURI &id = (*iter);
                ThreadContactMap::iterator found = contacts.find(id);
                if (found == contacts.end()) continue;
                contacts.erase(found);
              }

              // update the "add" contacts next
              for (ThreadContactMap::iterator iter = mContactsToAddChanged.begin(); iter != mContactsToAddChanged.end(); ++iter)
              {
                const ContactURI &id = (*iter).first;
                ThreadContactPtr &contact = (*iter).second;

                // apply the change now...
                addContacts[id] = contact;
              }

              // remove all contacts that do not belong anymore...
              for (ContactURIList::iterator iter = mContactsToAddRemoved.begin(); iter != mContactsToAddRemoved.end(); ++iter)
              {
                const ContactURI &id = (*iter);
                ThreadContactMap::iterator found = addContacts.find(id);
                if (found == addContacts.end()) continue;
                addContacts.erase(found);
              }

              // update the "remove" contacts next
              convert(mContactsToRemoveChanged, removeContactsMap);

              // remove all contacts that do not belong anymore...
              for (ContactURIList::iterator iter = mContactsToRemoveRemoved.begin(); iter != mContactsToRemoveRemoved.end(); ++iter)
              {
                const ContactURI &id = (*iter);
                ThreadContactMap::iterator found = removeContactsMap.find(id);
                if (found == removeContactsMap.end()) continue;
                removeContactsMap.erase(found);
              }

              // move back into the actual "remove" contact list
              convert(removeContactsMap, removeContacts);

              // create a replacement contacts object
              ThreadContactList contactsAsList;
              ThreadContactList addContactsAsList;
              convert(contacts, contactsAsList);
              convert(addContacts, addContactsAsList);

              // this is the replacement "contacts" object
              mContacts = ThreadContacts::create(mContacts->version() + 1, contactsAsList, addContactsAsList, removeContacts);
              ZS_THROW_BAD_STATE_IF(!mContacts)

              ElementPtr contactsEl = threadEl->findFirstChildElementChecked("contacts");

              // this is the replacement for the contacts...
              IDiff::createDiffs(IDiff::DiffAction_Replace, mChangesDoc, contactsEl, false, mContacts->contactsElement()->clone());
            }

            if (mMessagesChanged.size() > 0) {
              ElementPtr messagesEl = threadEl->findFirstChildElementChecked("messages");

              ++mMessagesVersion;

              ElementPtr setEl = Element::create();
              setEl->setAttribute("version", string(mMessagesVersion));

              // put the corrected version on the messages element...
              IDiff::createDiffsForAttributes(mChangesDoc, messagesEl, false, setEl);

              // add the messages to the messages element
              for (MessageList::iterator iter = mMessagesChanged.begin(); iter != mMessagesChanged.end(); ++iter)
              {
                MessagePtr &message = (*iter);
                IDiff::createDiffs(IDiff::DiffAction_AdoptAsLastChild, mChangesDoc, messagesEl, false, message->messageBundleElement());

                // remember these messages in the thread document...
                mMessageList.push_back(message);
                mMessageMap[message->messageID()] = message;
              }
            }

            if (mMessageReceiptsChanged.size() > 0) {
              ElementPtr receiptsEl = threadEl->findFirstChildElementChecked("receipts");

              mMessageReceipts = MessageReceipts::create(mMessageReceipts->version()+1, mMessageReceiptsChanged);

              IDiff::createDiffs(IDiff::DiffAction_Replace, mChangesDoc, receiptsEl, false, mMessageReceipts->receiptsElement());
            }

            if ((mDialogsChanged.size() > 0) ||
                (mDialogsRemoved.size() > 0)) {

              // some dialogs have changed or been removed
              ElementPtr dialogsEl = threadEl->findFirstChildElementChecked("dialogs");
              ++mDialogsVersion;

              ElementPtr setEl = Element::create();
              setEl->setAttribute("version", string(mDialogsVersion));

              IDiff::createDiffsForAttributes(mChangesDoc, dialogsEl, false, setEl);

              for (DialogMap::iterator iter = mDialogsChanged.begin(); iter != mDialogsChanged.end(); ++iter)
              {
                const DialogID &id = (*iter).first;
                DialogPtr &dialog = (*iter).second;

                // remember this dialog in the thread
                mDialogs[id] = dialog;

                bool found = false;

                // dialog is now changing...
                ElementPtr dialogBundleEl = dialogsEl->findFirstChildElement("dialogBundle");
                while (dialogBundleEl) {
                  ElementPtr dialogEl = dialogBundleEl->findFirstChildElement("dialog");
                  ElementPtr signatureEl = dialogBundleEl->findFirstChildElement("Signature");
                  if (dialogEl->getAttributeValue("id") == id) {
                    // found the element to "replace"... so create a diff...
                    IDiff::createDiffs(IDiff::DiffAction_Replace, mChangesDoc, dialogBundleEl, false, dialog->dialogBundleElement()->clone());
                    found = true;
                    break;
                  }
                  dialogBundleEl = dialogBundleEl->findNextSiblingElement("dialogBundle");
                }

                if (!found) {
                  // this dialog needs to be added isntead
                  IDiff::createDiffs(IDiff::DiffAction_AdoptAsLastChild, mChangesDoc, dialogsEl, false, dialog->dialogBundleElement()->clone());
                }
              }

              for (DialogIDList::iterator iter = mDialogsRemoved.begin(); iter != mDialogsRemoved.end(); ++iter)
              {
                const DialogID &id = (*iter);

                DialogMap::iterator found = mDialogs.find(id);
                if (found != mDialogs.end()) {
                  ZS_LOG_DEBUG(log("removing dialog from dialog map") + ZS_PARAM("dialog ID", id))
                  mDialogs.erase(found);
                }

                // dialog is now changing...
                ElementPtr dialogBundleEl = dialogsEl->findFirstChildElement("dialogBundle");
                while (dialogBundleEl) {
                  ElementPtr dialogEl = dialogBundleEl->findFirstChildElement("dialog");
                  if (dialogEl->getAttributeValue("id") == id) {
                    // found the element to "replace"... so create a diff...
                    IDiff::createDiffs(IDiff::DiffAction_Remove, mChangesDoc, dialogBundleEl, false);
                    break;
                  }
                  dialogBundleEl = dialogBundleEl->findNextSiblingElement("dialogBundle");
                }
              }
            }

            if (!mChangesDoc) return false;  // nothing to do
          }

          // the changes need to be adopted/processed by the document
          mPublication->update(mChangesDoc);
          mChangesDoc.reset();

          // check if the permissions document needs updating too...
          if ((mContactsChanged.size() > 0) ||
              (mContactsRemoved.size() > 0)) {

            if (ThreadType_Host == mType) {
              // Only need to update this document when acting as a host and
              // the contacts that are allowed to read this document are all the
              // slave contacts.

              RelationshipList relationships;
              PublishToRelationshipsMap publishEmptyRelationships;

              // add every contact that is part of this thread to the permission document list
              const ThreadContactMap &contacts = mContacts->contacts();
              for (ThreadContactMap::const_iterator iter = contacts.begin(); iter != contacts.end(); ++iter)
              {
                const ContactURI &id = (*iter).first;
                relationships.push_back(id);
              }

              if (mPermissionPublication) {
                mPermissionPublication->update(relationships);
              } else {
                mPermissionPublication = IPublication::create(mPermissionPublication->getCreatorLocation(), mPermissionPublication->getName(), "text/x-json-openpeer-permissions", relationships, publishEmptyRelationships, mPermissionPublication->getPublishedLocation());
              }
            }
          }

          resetChanged();

          // scope: double check if any contacts can be published at this time
          {
            const ThreadContactMap &contacts = mContacts->contacts();
            for (ThreadContactMap::const_iterator iter = contacts.begin(); iter != contacts.end(); ++iter)
            {
              const ThreadContactPtr &contact = (*iter).second;
              publishContact(contact->contact());
            }
          }

          return true;
        }

        //---------------------------------------------------------------------
        void Thread::setState(Details::ConversationThreadStates state)
        {
          ZS_THROW_INVALID_USAGE_IF((!mCanModify) || (!mModifying))

          if (state == mDetails->state()) return; // nothing changed

          mDetailsChanged = true;
          mDetails = Details::create(mDetails->version()+1, mDetails->baseThreadID(), mDetails->hostThreadID(), mDetails->topic(), mDetails->replacesThreadID(), state);
        }

        //---------------------------------------------------------------------
        void Thread::setContacts(const ThreadContactMap &contacts)
        {
          ZS_THROW_INVALID_USAGE_IF((!mCanModify) || (!mModifying))

          const ThreadContactMap &oldContacts = mContacts->contacts();

          // clear current list of changes since a reset it happening on the changes
          mContactsChanged.clear();
          mContactsRemoved.clear();

          // figure out which contacts are new...
          for (ThreadContactMap::const_iterator iter = contacts.begin(); iter != contacts.end(); ++iter)
          {
            const ContactURI &id = (*iter).first;
            const ThreadContactPtr &contact = (*iter).second;

            // first check if it's already on the list...
            ThreadContactMap::const_iterator found = oldContacts.find(id);
            if (found != oldContacts.end()) continue;       // already have this contact on the old list

            mContactsChanged[id] = contact;
          }

          // figure out which contacts need to be removed...
          for (ThreadContactMap::const_iterator iter = oldContacts.begin(); iter != oldContacts.end(); ++iter)
          {
            const ContactURI &id = (*iter).first;
            ThreadContactMap::const_iterator found = contacts.find(id);
            if (found == contacts.end()) {
              // this contact is now gone and it must be removed...
              mContactsRemoved.push_back(id);
            }
          }
        }

        //---------------------------------------------------------------------
        void Thread::setContactsToAdd(const ThreadContactMap &contacts)
        {
          ZS_THROW_INVALID_USAGE_IF((!mCanModify) || (!mModifying))

          const ThreadContactMap &oldContactsToAdd = mContacts->addContacts();

          // clear current list of changes since a reset it happening on the changes
          mContactsToAddChanged.clear();
          mContactsToAddRemoved.clear();

          // figure out which contacts are new...
          for (ThreadContactMap::const_iterator iter = contacts.begin(); iter != contacts.end(); ++iter)
          {
            const ContactURI &id = (*iter).first;
            const ThreadContactPtr &contact = (*iter).second;

            // first check if it's already on the list...
            ThreadContactMap::const_iterator found = oldContactsToAdd.find(id);
            if (found != oldContactsToAdd.end()) continue;       // already have this contact on the old list

            mContactsToAddChanged[id] = contact;
          }

          // figure out which contacts need to be removed...
          for (ThreadContactMap::const_iterator iter = oldContactsToAdd.begin(); iter != oldContactsToAdd.end(); ++iter)
          {
            const ContactURI &id = (*iter).first;
            ThreadContactMap::const_iterator found = contacts.find(id);
            if (found == contacts.end()) {
              // this contact is now gone and it must be removed...
              mContactsToAddRemoved.push_back(id);
            }
          }
        }

        //---------------------------------------------------------------------
        void Thread::setContactsToRemove(const ContactURIList &contacts)
        {
          ZS_THROW_INVALID_USAGE_IF((!mCanModify) || (!mModifying))

          const ContactURIList &oldContactsToRemoveList = mContacts->removeContacts();
          ThreadContactMap oldContactsToRemove;

          // create old contacts into a map instead
          for (ContactURIList::const_iterator iter = oldContactsToRemoveList.begin(); iter != oldContactsToRemoveList.end(); ++iter)
          {
            const ContactURI &id = (*iter);
            oldContactsToRemove[id] = ThreadContactPtr();
          }

          // clear current list of changes since a reset it happening on the changes
          mContactsToRemoveChanged.clear();
          mContactsToRemoveRemoved.clear();

          // figure out which contacts are new...
          for (ContactURIList::const_iterator iter = contacts.begin(); iter != contacts.end(); ++iter)
          {
            const ContactURI &id = (*iter);

            // first check if it's already on the list...
            ThreadContactMap::const_iterator found = oldContactsToRemove.find(id);
            if (found != oldContactsToRemove.end()) continue;       // already have this contact on the old list

            mContactsToRemoveChanged.push_back(id);
          }

          ThreadContactMap tempContacts;
          convert(contacts, tempContacts);

          // figure out which contacts need to be removed...
          for (ThreadContactMap::const_iterator iter = oldContactsToRemove.begin(); iter != oldContactsToRemove.end(); ++iter)
          {
            const ContactURI &id = (*iter).first;
            ThreadContactMap::const_iterator found = tempContacts.find(id);
            if (found == tempContacts.end()) {
              // this contact is now gone and it must be removed...
              mContactsToRemoveRemoved.push_back(id);
            }
          }
        }

        //---------------------------------------------------------------------
        void Thread::addMessage(MessagePtr message)
        {
          ZS_THROW_INVALID_USAGE_IF((!mCanModify) || (!mModifying))

          MessageList list;
          list.push_back(message);
          addMessages(list);
        }

        //---------------------------------------------------------------------
        void Thread::addMessages(const MessageList &messages)
        {
          ZS_THROW_INVALID_USAGE_IF((!mCanModify) || (!mModifying))

          for (MessageList::const_iterator iter = messages.begin(); iter != messages.end(); ++iter)
          {
            const MessagePtr &message = (*iter);
            MessageMap::iterator found = mMessageMap.find(message->messageID());
            if (found != mMessageMap.end()) continue;   // this message is already part of the message list

            // this is a new message to be added to the list...
            mMessagesChanged.push_back(message);
            mMessagesChangedTime = zsLib::now();
          }
        }

        //---------------------------------------------------------------------
        void Thread::setReceived(MessagePtr message)
        {
          ZS_THROW_INVALID_USAGE_IF((!mCanModify) || (!mModifying))

          mMessageReceiptsChanged.clear();

          mMessageReceiptsChanged[message->messageID()] = zsLib::now();
        }

        //---------------------------------------------------------------------
        void Thread::setReceived(const MessageReceiptMap &messages)
        {
          ZS_THROW_INVALID_USAGE_IF((!mCanModify) || (!mModifying))

          if (mMessageReceiptsChanged.size() > 0) {
            // since there is already changes acknowledged just update the changes now...
            mMessageReceiptsChanged = messages;
            return;
          }

          bool changed = false;

          // check if anything is added...
          for (MessageReceiptMap::const_iterator iter = messages.begin(); iter != messages.end(); ++iter)
          {
            const MessageID &id = (*iter).first;
            const Time &time = (*iter).second;
            MessageReceiptMap::const_iterator found = mMessageReceipts->receipts().find(id);
            if (found == mMessageReceipts->receipts().end()) {
              changed = true;
              break;
            }
            const Time &oldTime = (*found).second;
            if (time != oldTime) {
              changed = true;
              break;
            }
          }

          if (!changed) {
            // check if anything is removed...
            for (MessageReceiptMap::const_iterator iter = mMessageReceipts->receipts().begin(); iter != mMessageReceipts->receipts().end(); ++iter)
            {
              const MessageID &id = (*iter).first;
              const Time &time = (*iter).second;
              MessageReceiptMap::const_iterator found = messages.find(id);
              if (found == messages.end()) {
                changed = true;
                break;
              }
              const Time &oldTime = (*found).second;
              if (time != oldTime) {
                changed = true;
                break;
              }
            }
          }

          if (!changed) {
            ZS_LOG_TRACE(log("no message receipts changed detected from previous received map (thus ignoring request to set received)"))
            // nothing changed thus do not cause an update
            return;
          }

          mMessageReceiptsChanged = messages;
        }

        //---------------------------------------------------------------------
        void Thread::addDialogs(const DialogList &dialogs)
        {
          ZS_THROW_INVALID_USAGE_IF((!mCanModify) || (!mModifying))

          for (DialogList::const_iterator iter = dialogs.begin(); iter != dialogs.end(); ++iter)
          {
            const DialogPtr &dialog = (*iter);
            mDialogsChanged[dialog->dialogID()] = dialog;
          }
        }

        //---------------------------------------------------------------------
        void Thread::updateDialogs(const DialogList &dialogs)
        {
          // same logic...
          addDialogs(dialogs);
        }

        //---------------------------------------------------------------------
        void Thread::removeDialogs(const DialogIDList &dialogs)
        {
          ZS_THROW_INVALID_USAGE_IF((!mCanModify) || (!mModifying))

          for (DialogIDList::const_iterator iter = dialogs.begin(); iter != dialogs.end(); ++iter)
          {
            const String &dialogID = (*iter);
            mDialogsRemoved.push_back(dialogID);
          }
        }

        //---------------------------------------------------------------------
        void Thread::getContactPublicationsToPublish(ContactPublicationMap &outContactPublications)
        {
          for (ContactPublicationMap::iterator iter = mContactPublications.begin(); iter != mContactPublications.end(); ++iter)
          {
            const String &contactURI = (*iter).first;
            IPublicationPtr &publication = (*iter).second;
            if (!publication) {
              ZS_LOG_TRACE(log("contact already published"))
              continue;
            }

            // transfer ownership of publication to caller
            outContactPublications[contactURI] = publication;

            // make sure it will never be published again
            publication.reset();
          }
        }

        //---------------------------------------------------------------------
        ElementPtr Thread::toDebug() const
        {
          ElementPtr resultEl = Element::create("core::thread::Thread");

          IHelper::debugAppend(resultEl, "id", mID);
          IHelper::debugAppend(resultEl, "type", toString(mType));
          IHelper::debugAppend(resultEl, "can modify", mCanModify);
          IHelper::debugAppend(resultEl, "modifying", mModifying);
          IHelper::debugAppend(resultEl, IPublication::toDebug(mPublication));
          IHelper::debugAppend(resultEl, IPublication::toDebug(mPermissionPublication));
          IHelper::debugAppend(resultEl, "contact publications", mContactPublications.size());
          IHelper::debugAppend(resultEl, Details::toDebug(mDetails));
          IHelper::debugAppend(resultEl, ThreadContacts::toDebug(mContacts));
          IHelper::debugAppend(resultEl, "message version", mMessagesVersion);
          IHelper::debugAppend(resultEl, "message list", mMessageList.size());
          IHelper::debugAppend(resultEl, "message map", mMessageMap.size());
          IHelper::debugAppend(resultEl, MessageReceipts::toDebug(mMessageReceipts));
          IHelper::debugAppend(resultEl, "dialog version", mDialogsVersion);
          IHelper::debugAppend(resultEl, "dialogs", mDialogs.size());
          IHelper::debugAppend(resultEl, "changes", (bool)mChangesDoc);
          IHelper::debugAppend(resultEl, "details changed", mDetailsChanged);
          IHelper::debugAppend(resultEl, "contacts changed", mContactsChanged.size());
          IHelper::debugAppend(resultEl, "contacts removed", mContactsRemoved.size());
          IHelper::debugAppend(resultEl, "contacts to add changed", mContactsToAddChanged.size());
          IHelper::debugAppend(resultEl, "contacts to add removed", mContactsToAddRemoved.size());
          IHelper::debugAppend(resultEl, "contacts to remove changed", mContactsToRemoveChanged.size());
          IHelper::debugAppend(resultEl, "contacts to remove removed", mContactsToRemoveRemoved.size());
          IHelper::debugAppend(resultEl, "messages changed", mMessagesChanged.size());
          IHelper::debugAppend(resultEl, "messages changed time", mMessagesChangedTime);
          IHelper::debugAppend(resultEl, "messages receipts changed", mMessageReceiptsChanged.size());
          IHelper::debugAppend(resultEl, "dialogs changed", mDialogsChanged.size());
          IHelper::debugAppend(resultEl, "dialogs removed", mDialogsRemoved.size());
          IHelper::debugAppend(resultEl, "descriptions changed", mDescriptionsChanged.size());
          IHelper::debugAppend(resultEl, "descriptions removed", mDescriptionsRemoved.size());
          
          return resultEl;
        }
        
        //---------------------------------------------------------------------
        Log::Params Thread::log(const char *message) const
        {
          ElementPtr objectEl = Element::create("core::thread::Thread");
          IHelper::debugAppend(objectEl, "id", mID);
          return Log::Params(message, objectEl);
        }

        //---------------------------------------------------------------------
        void Thread::resetChanged()
        {
          mDetailsChanged = false;
          mContactsChanged.clear();
          mContactsRemoved.clear();
          mContactsToAddChanged.clear();
          mContactsToAddRemoved.clear();
          mContactsToRemoveChanged.clear();
          mContactsToRemoveRemoved.clear();
          mMessagesChanged.clear();
          mMessageReceiptsChanged.clear();
          mDialogsChanged.clear();
          mDialogsRemoved.clear();
          mDescriptionsChanged.clear();
          mDescriptionsRemoved.clear();
        }

        //---------------------------------------------------------------------
        String Thread::getContactDocumentName(UseContactPtr contact) const
        {
          ZS_THROW_INVALID_ARGUMENT_IF(!contact)

          if (!mDetails) {
            ZS_LOG_WARNING(Detail, log("cannot get document name without document details") + UseContact::toDebug(contact))
            return String();
          }

          String domain;
          String contactID;

          if (!IPeer::splitURI(contact->getPeerURI(), domain, contactID)) {
            ZS_LOG_WARNING(Detail, log("unable to split peer URI") + UseContact::toDebug(contact))
            return String();
          }
          
          String contactBaseName = String("/contacts/1.0/") + toString(mType) + "/" + mDetails->baseThreadID() + "/" + mDetails->hostThreadID() + "/";
          return contactBaseName + contactID;
        }

        //---------------------------------------------------------------------
        void Thread::publishContact(UseContactPtr contact)
        {
          if (!contact) {
            ZS_LOG_WARNING(Detail, log("contact to publish is NULL"))
            return;
          }

          IPeerFilePublicPtr peerFilePublic = contact->getPeerFilePublic();
          if (!peerFilePublic) {
            ZS_LOG_WARNING(Debug, log("contact does not contain peer file thus cannot publish contact") + UseContact::toDebug(contact))
            return;
          }

          if (mContactPublications.end() != mContactPublications.find(contact->getPeerURI())) {
            ZS_LOG_TRACE(log("contact already published") + UseContact::toDebug(contact))
            return;
          }

          if ((!mPermissionPublication) &&
              (!mPublication)) {
            ZS_LOG_WARNING(Debug, log("will not publish contact without a permission document or thread publication document") + UseContact::toDebug(contact))
            return;
          }

          if (!mDetails) {
            ZS_LOG_WARNING(Detail, log("will not publish contact that does not contact thread details") + UseContact::toDebug(contact))
            return;
          }

          ElementPtr publicPeerEl = peerFilePublic->saveToElement();

          if (!publicPeerEl) {
            ZS_LOG_WARNING(Detail, log("unable to export public peer file") + UseContact::toDebug(contact))
            return;
          }

          String name = getContactDocumentName(contact);
          if (name.isEmpty()) {
            ZS_LOG_WARNING(Detail, log("unable to create document name for contact") + UseContact::toDebug(contact))
            return;
          }

          DocumentPtr doc = Document::create();
          doc->adoptAsLastChild(publicPeerEl);

          PublishToRelationshipsMap publishRelationships;

          // scope: add "all" permissions
          {
            typedef IPublication::PeerURIList PeerURIList;
            typedef IPublication::PermissionAndPeerURIListPair PermissionAndPeerURIListPair;

            PeerURIList empty;
            publishRelationships[mPermissionPublication->getName()] = PermissionAndPeerURIListPair(IPublication::Permission_All, empty);
          }

          IPublicationPtr contactPublication = IPublication::create(mPublication->getCreatorLocation(), name, "text/x-json-openpeer", doc, publishRelationships, mPublication->getPublishedLocation());
          doc.reset();  // been adopted

          ZS_LOG_DEBUG(log("publishing contact") + UseContact::toDebug(contact) + IPublication::toDebug(contactPublication))

          mContactPublications[contact->getPeerURI()] = contactPublication;
        }

      }
      
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
    }
    
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
  }
}
