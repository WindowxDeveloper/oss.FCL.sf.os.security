/*
* Copyright (c) 2004-2009 Nokia Corporation and/or its subsidiary(-ies).
* All rights reserved.
* This component and the accompanying materials are made available
* under the terms of the License "Eclipse Public License v1.0"
* which accompanies this distribution, and is available
* at the URL "http://www.eclipse.org/legal/epl-v10.html".
*
* Initial Contributors:
* Nokia Corporation - initial contribution.
*
* Contributors:
*
* Description: 
*
*/


#include "cfskeystoreserver.h"
#include "CKeyDataManager.h"
#include "CKeyStoreConduit.h"
#include "CKeyStoreSession.h"
#include "fstokencliserv.h"
#include "CCreateKey.h"
#include "OpenedKeys.h"
#include "keystorepassphrase.h"
#include "fsdatatypes.h"
#include "keystreamutils.h"
#include <fstokenutil.h>
#include <asnpkcs.h>
#include <pbedata.h>
#include <securityerr.h>
#include <pbe.h>
#include <asnpkcs.h>
#include <asn1enc.h>
#include <x509keys.h>
#include <keyidentifierutil.h>
#include <mctauthobject.h>
#include <utf.h>

// We don't currently allow any keys larger than 2048 bits.  It may be necessary to
// increase this limit in the future. 
const TUint KTheMinKeySize = 512;
const TUint KTheMaxKeySize = 2048;

// Security policies
_LIT_SECURITY_POLICY_C1(KSetTimeoutSecurityPolicy, ECapabilityWriteDeviceData);
_LIT_SECURITY_POLICY_C1(KCreateSecurityPolicy, ECapabilityWriteUserData);
_LIT_SECURITY_POLICY_C1(KListSecurityPolicy, ECapabilityReadUserData);

CFSKeyStoreServer* CFSKeyStoreServer::NewL()
	{
	CFSKeyStoreServer* me = new (ELeave) CFSKeyStoreServer();
	CleanupStack::PushL(me);
	me->ConstructL();
	CleanupStack::Pop(me);
	return (me);
	}

CFSKeyStoreServer::CFSKeyStoreServer() :
	CActive(EPriorityStandard),
	iAction(EIdle),
	iExportBuf(NULL, 0)
	{
	}

void CFSKeyStoreServer::ConstructL()
	{
	iKeyDataManager = CFileKeyDataManager::NewL();
	iConduit = CKeyStoreConduit::NewL(*this);
	CActiveScheduler::Add(this);
	}

CFSKeyStoreServer::~CFSKeyStoreServer()
	{
	Cancel();

	delete iKeyDataManager;
	delete iConduit;
	delete iKeyCreator;
	iSessions.Close();
	}

CKeyStoreSession* CFSKeyStoreServer::CreateSessionL()
	{
	CPassphraseManager* passMan = iKeyDataManager->CreatePassphraseManagerLC();
	CKeyStoreSession* session = CKeyStoreSession::NewL(*this, passMan);
	CleanupStack::Pop(passMan);
	CleanupStack::PushL(session);
	User::LeaveIfError(iSessions.Append(session));
	CleanupStack::Pop(session);
	return session;
	}

void CFSKeyStoreServer::RemoveSession(CKeyStoreSession& aSession)
	{
	if (iSession == &aSession) // just in case
		{
		iSession = NULL; 
		}
	
	for (TInt index = 0 ; index < iSessions.Count() ; ++index)
		{
		if (iSessions[index] == &aSession)
			{
			iSessions.Remove(index);
			return;
			}
		}
	User::Invariant();
	}

void CFSKeyStoreServer::ServiceRequestL(const RMessage2& aMessage, CKeyStoreSession& aSession)
	{
	iMessage = &aMessage;
	iSession = &aSession;
	iConduit->ServiceRequestL(aMessage, aSession);
	}

//	*********************************************************************************
//	From MCTKeyStore
//	*********************************************************************************
void CFSKeyStoreServer::ListL(const TCTKeyAttributeFilter& aFilter,
							  RPointerArray<CKeyInfo>& aKeys)
	{
	ASSERT(iMessage);
	
	// Check the calling process has ReadUserData capability
	if (!KListSecurityPolicy.CheckPolicy(*iMessage))
		{
		User::Leave(KErrPermissionDenied);
		}
	
	TInt count = iKeyDataManager->Count();

	for (TInt i = 0; i < count; ++i)
		{
		const CFileKeyData* data = (*iKeyDataManager)[i];
		CKeyInfo* info = iKeyDataManager->ReadKeyInfoLC(*data);
		if (KeyMatchesFilterL(*info, aFilter))
			{
			User::LeaveIfError(aKeys.Append(info));
			CleanupStack::Pop(info);
			}
		else
			{
			CleanupStack::PopAndDestroy(info);
			}
		}
	}

TBool CFSKeyStoreServer::KeyMatchesFilterL(const CKeyInfo& aInfo,
										   const TCTKeyAttributeFilter& aFilter)
	{
	ASSERT(iMessage);
	
	if (aFilter.iKeyId.Length() && aFilter.iKeyId != aInfo.ID())
		{
		return EFalse;
		}

	if (aFilter.iUsage != EPKCS15UsageAll)
		{
		if ((aInfo.Usage() & aFilter.iUsage) == 0)
			return EFalse;
		}

	if (aFilter.iKeyAlgorithm != CCTKeyInfo::EInvalidAlgorithm && 
		aFilter.iKeyAlgorithm != aInfo.Algorithm())
		{
		return EFalse;
		}

	switch (aFilter.iPolicyFilter)
		{
		case TCTKeyAttributeFilter::EAllKeys:
			// All keys pass
			break;
			   
		case TCTKeyAttributeFilter::EUsableKeys:
			if (!aInfo.UsePolicy().CheckPolicy(*iMessage))
				{
				return EFalse;
				}
			break;
			
		case TCTKeyAttributeFilter::EManageableKeys:
			if (!aInfo.ManagementPolicy().CheckPolicy(*iMessage))
				{
				return EFalse;
				}
			break;

		case TCTKeyAttributeFilter::EUsableOrManageableKeys:
			if (!aInfo.UsePolicy().CheckPolicy(*iMessage) &&
				!aInfo.ManagementPolicy().CheckPolicy(*iMessage))
				{
				return EFalse;
				}
			break;
						
		default:
			User::Leave(KErrArgument);
		}

	return ETrue;
	}

void CFSKeyStoreServer::GetKeyInfoL(TInt aObjectId, CKeyInfo*& aInfo)
	{
	const CFileKeyData* keyData = iKeyDataManager->Lookup(aObjectId);
	if (!keyData)
		{
		User::Leave(KErrNotFound);
		}

	CKeyInfo* result = iKeyDataManager->ReadKeyInfoLC(*keyData);
	if (!result->UsePolicy().CheckPolicy(*iMessage))
		{
		User::Leave(KErrPermissionDenied);
		}

	aInfo = result;
	CleanupStack::Pop(aInfo);
	}

TInt CFSKeyStoreServer::GetKeyLengthL(TInt aObjectId)
	{
	const CFileKeyData* keyData = iKeyDataManager->Lookup(aObjectId);
	if (!keyData)
		{
		User::Leave(KErrNotFound);
		}

	// this could be cached in memory (would break file format though)
	CKeyInfo* keyInfo = iKeyDataManager->ReadKeyInfoLC(*keyData);
	TInt result = keyInfo->Size();
	CleanupStack::PopAndDestroy(keyInfo);

	return result;
	}

COpenedKey* CFSKeyStoreServer::OpenKeyL(TInt aHandle, TUid aOpenedKeyType)
	{
	ASSERT(iMessage);
	
	const CFileKeyData *keyData = iKeyDataManager->Lookup(aHandle);
	if (!keyData)
		{
		User::Leave(KErrNotFound);
		}

	return COpenedKey::NewL(*keyData, aOpenedKeyType, *iMessage,
							*iKeyDataManager, iSession->PassphraseManager());
	}

void CFSKeyStoreServer::ExportPublicL(TInt aObjectId,
									  TDes8& aOut)
	{
	const CFileKeyData* keyData = iKeyDataManager->Lookup(aObjectId);
	if (!keyData)
		{
		User::Leave(KErrNotFound);
		}

	CKeyInfo* keyInfo = iKeyDataManager->ReadKeyInfoLC(*keyData);

	RStoreReadStream stream;
	iKeyDataManager->OpenPublicDataStreamLC(*keyData, stream);

	CKeyInfo::EKeyAlgorithm keyAlgorithm = keyInfo->Algorithm();
	
	switch(keyAlgorithm)
		{
		case (CKeyInfo::ERSA):
			{
			CRSAPublicKey* publicKey = NULL;

			CreateL(stream, publicKey);
			ASSERT(publicKey);
			CleanupStack::PushL(publicKey);

			TX509RSAKeyEncoder encoder(*publicKey, ESHA1);
			CASN1EncBase* encoded = encoder.EncodeKeyLC();

			if (encoded->LengthDER() > static_cast<TUint>(aOut.MaxLength()))
				{
				User::Leave(KErrOverflow);
				}
			
			//	Get the Public key DER encoding
			TUint pos=0;
			encoded->WriteDERL(aOut, pos);
			
			// WriteDERL does not set the length of the buffer, we do it ourselves			
			aOut.SetLength(encoded->LengthDER());			
			
			CleanupStack::PopAndDestroy(2, publicKey);
			}
		break;

		case (CKeyInfo::EDSA):
			{
			CDSAPublicKey* publicKey = NULL;

			CreateL(stream, publicKey);
			ASSERT(publicKey);
			CleanupStack::PushL(publicKey);

			TX509DSAKeyEncoder encoder(*publicKey, ESHA1);
			CASN1EncBase* encoded = encoder.EncodeKeyLC();

			if (encoded->LengthDER() > static_cast<TUint>(aOut.MaxLength()))
				{
				User::Leave(KErrOverflow);
				}

			//	Get the Public key DER encoding
			TUint pos=0;
			encoded->WriteDERL(aOut, pos);
			
			// WriteDERL does not set the length of the buffer, we do it ourselves			
			aOut.SetLength(encoded->LengthDER());						
			
			CleanupStack::PopAndDestroy(2, publicKey);
			}
		break;

		case (CKeyInfo::EDH):
		default:
			User::Leave(KErrKeyAlgorithm);
			break;
	}

	CleanupStack::PopAndDestroy(2, keyInfo); //stream, keyinfo
	}

//	*********************************************************************************
//	From MCTKeyStoreManager
//	*********************************************************************************

TInt CFSKeyStoreServer::CheckKeyAttributes(CKeyInfo& aKey, TNewKeyOperation aOp)
	{
	ASSERT(iMessage);
	
	// Sort out the access rights
	TInt access = aKey.AccessType(); 
	
	// Only allow sensitive and extractable to be sepcified
	if (access & ~(CKeyInfo::ESensitive | CKeyInfo::EExtractable))
		{
		return KErrKeyAccess;
		}	
		
	// If it's sensitive and either created internally 
	// or imported from an encrypted source then it's always been sensitive
	if ((access & CKeyInfo::ESensitive) &&
		(aOp == ENewKeyCreate || aOp == ENewKeyImportEncrypted))
		{
		access |= CKeyInfo::EAlwaysSensitive;		
		}
		
	// If it's not extractable and it's created internally
	// then it's never been extractable
	if ((!(access & CKeyInfo::EExtractable)) && aOp == ENewKeyCreate)
		{
		access |= CKeyInfo::ENeverExtractable;		
		}
		
	aKey.SetAccessType(access);
	
	// check management policy allows the calling process to manage the key
	if (!aKey.ManagementPolicy().CheckPolicy(*iMessage))
		{
		return KErrArgument;
		}

	// check end date is not in the past
	TTime timeNow;
	timeNow.UniversalTime();
	if (aKey.EndDate().Int64() != 0 && aKey.EndDate() <= timeNow)
		{
		return KErrKeyValidity;
		}

	// We don't support non-repudiation, however we currently allow keys
	// to be created with this usage

	return KErrNone;
	}

TInt CFSKeyStoreServer::CheckKeyAlgorithmAndSize(CKeyInfo& aKey)
	{
	CKeyInfo::EKeyAlgorithm keyAlgorithm = aKey.Algorithm();
	if ( ((keyAlgorithm!=CKeyInfo::ERSA) && (keyAlgorithm!=CKeyInfo::EDSA) && (keyAlgorithm!=CKeyInfo::EDH) ))
		{
		return KErrKeyAlgorithm;
		}
	
	if (aKey.Size() < KTheMinKeySize || aKey.Size() > KTheMaxKeySize)
		{
		return KErrKeySize;
		}

	return KErrNone;
	}

void CFSKeyStoreServer::CreateKey(CKeyInfo& aReturnedKey, TRequestStatus& aStatus)
	{
	ASSERT(iSession);
	ASSERT(iMessage);

	TInt err = KErrNone;
	
	// Check the calling process has WriteUserData capability
	if (!KCreateSecurityPolicy.CheckPolicy(*iMessage))
		{
		err = KErrPermissionDenied;
		}
	
	// Check the incoming information has been initialised correctly
	if (err == KErrNone)
		{
		err = CheckKeyAttributes(aReturnedKey, ENewKeyCreate);
		}
	
	if (err == KErrNone)
		{
		err = CheckKeyAlgorithmAndSize(aReturnedKey);
		}
	
	if (err != KErrNone)
		{
		TRequestStatus* status = &aStatus;
		User::RequestComplete(status, err);
		return;
		}

	// DEF042306: Make it local only if it's created in the keystore
	aReturnedKey.SetAccessType(aReturnedKey.AccessType() | CKeyInfo::ELocal);

	if (iKeyDataManager->IsKeyAlreadyInStore(aReturnedKey.Label()))
		{
		TRequestStatus* status = &aStatus;
		User::RequestComplete(status, KErrAlreadyExists);
		return;
		}
	
	iCallerRequest = &aStatus;
	iKeyInfo = &aReturnedKey;

	GetKeystorePassphrase(ECreateKeyCreate);	
	}

void CFSKeyStoreServer::CancelCreateKey()
	{
	if (iAction == ECreateKeyCreate ||
		iAction == ECreateKeyFinal)
		{
		Cancel();
		}
	}

void CFSKeyStoreServer::DoCreateKeyL()
{
	__ASSERT_DEBUG(iAction==ECreateKeyCreate, PanicServer(EPanicECreateKeyNotReady));
	__ASSERT_DEBUG(iKeyInfo, PanicServer(EPanicNoClientData));
	
	if (iKeyCreator)
	{
		delete iKeyCreator;
		iKeyCreator = NULL;
	}

	iKeyCreator = new (ELeave) CKeyCreator();
	iStatus = KRequestPending;
	iAction = EKeyCreated;
	iKeyCreator->DoCreateKeyAsyncL(iKeyInfo->Algorithm(), iKeyInfo->Size(), iStatus);
	SetActive(); 
}
												  
/**
 * Get the default passphrase for the store, or create one if it hasn't been set
 * yet.  This is used for key creation, import and export.
 *
 * Calls SetActive(), and sets the next state.
 */
void CFSKeyStoreServer::GetKeystorePassphrase(ECurrentAction aNextState)
	{
	ASSERT(iSession);
	TStreamId passphraseId = iKeyDataManager->DefaultPassphraseId();
	TInt timeout = iKeyDataManager->GetPassphraseTimeout();
	if (passphraseId == KNullStreamId)
		{
		iSession->PassphraseManager().CreatePassphrase(timeout, iPassphrase, iStatus);
		}
	else
		{
		iSession->PassphraseManager().GetPassphrase(passphraseId, timeout, iPassphrase, iStatus);
		}
	iAction = aNextState;
	SetActive();
	}

/**
 * Store a key.
 *
 * 
 *
 * This method is not transaction-safe, as if a leave occurs after key data has
 * been written to the store, it is not cleaned up.  Also, it shares most of its
 * code with PKCS8ToKeyL (and the same problem applied).  This should be
 * corrected by creating an interface for generic key data, including write
 * private key / write public key / get key identifier operations, which can be
 * passed to a single method in the key data manager to do the job of these two
 * methods.
 *
 * I'm not fixing this now because it's not a big problem for real-world use,
 * and I don't have the time to spend on it.
 *
 * - jc
 */
void CFSKeyStoreServer::DoStoreKeyL()
	{
	__ASSERT_DEBUG(iAction==ECreateKeyFinal, PanicServer(EPanicECreateKeyNotReady));
	__ASSERT_DEBUG(iKeyInfo, PanicServer(EPanicNoClientData));
	__ASSERT_DEBUG(iKeyCreator, PanicServer(ENoCreatedKeyData));
	ASSERT(iPassphrase);

	const CFileKeyData* keyData = iKeyDataManager->CreateKeyDataLC(iKeyInfo->Label(), iPassphrase->StreamId());	   

	RStoreWriteStream privateStream;
	iKeyDataManager->OpenPrivateDataStreamLC(*keyData, *iPassphrase, privateStream);
	
	CKeyInfo::EKeyAlgorithm keyAlgorithm = iKeyInfo->Algorithm();

// 	Get key identifier and externalize private key 
	TKeyIdentifier theKeyId;
	switch (keyAlgorithm)
		{
		case (CKeyInfo::ERSA):
			{
			CRSAKeyPair* newKey = iKeyCreator->GetCreatedRSAKey();
			KeyIdentifierUtil::RSAKeyIdentifierL(newKey->PublicKey(), theKeyId);			
			privateStream << newKey->PrivateKey();
			break;
			}
			
		case (CKeyInfo::EDSA):
			{
			CDSAKeyPair* newKey = iKeyCreator->GetCreatedDSAKey();			
			KeyIdentifierUtil::DSAKeyIdentifierL(newKey->PublicKey(), theKeyId);
			privateStream << newKey->PrivateKey();
			break;
			}

		case (CKeyInfo::EDH):
			{
			RInteger newKey; 
			iKeyCreator->GetCreatedDHKey(newKey);			
			KeyIdentifierUtil::DHKeyIdentifierL(newKey, theKeyId);

			if (newKey.IsZero())
				User::Leave(KErrArgument);

			privateStream << newKey;
			privateStream.CommitL();
			break;
			}
		
		default:
			__ASSERT_DEBUG(EFalse, PanicServer(EPanicInvalidKeyCreateReq));
			break;
		}

	privateStream.CommitL();
	CleanupStack::PopAndDestroy(); // privateStream
	
//	Fill in the CCTKeyInfo data currently missing (TKeyIdentifier and handle)
	iKeyInfo->SetHandle(keyData->Handle());
	iKeyInfo->SetIdentifier(theKeyId);	

// 	Externalize public key

	RStoreWriteStream publicStream;
	iKeyDataManager->OpenPublicDataStreamLC(*keyData, publicStream);

	switch (keyAlgorithm)
		{
		case (CKeyInfo::ERSA):
			publicStream << iKeyCreator->GetCreatedRSAKey()->PublicKey();			
			break;
			
		case (CKeyInfo::EDSA):
			publicStream << iKeyCreator->GetCreatedDSAKey()->PublicKey();
			break;

		case (CKeyInfo::EDH):
			// Nothing to do for DH
			break;
		
		default:
			__ASSERT_DEBUG(EFalse, PanicServer(EPanicInvalidKeyCreateReq));
			break;
		}
	
	publicStream.CommitL();
	CleanupStack::PopAndDestroy(); // publicStream

//	Finished with the key creator
	if (iKeyCreator)
	{
		delete iKeyCreator;
		iKeyCreator = NULL;
	}

//	Externalize the CKeyInfo data associated with the key,
	iKeyDataManager->WriteKeyInfoL(*keyData, *iKeyInfo);

//	Now add the new key to the data manager (which adds it to the store)
	iKeyDataManager->AddL(keyData);
	CleanupStack::Pop(); // keydata
}

void CFSKeyStoreServer::ImportKey(const TDesC8& aKey, CKeyInfo& aReturnedKey, TBool aIsEncrypted, TRequestStatus& aStatus)
	{
	ASSERT(iMessage);
 
	TInt err = KErrNone;

	// Check the calling process has WriteUserData capability
	if (!KCreateSecurityPolicy.CheckPolicy(*iMessage))
		{
		err = KErrPermissionDenied;
		}

	if (err == KErrNone)
		{
		err = CheckKeyAttributes(aReturnedKey,
								 aIsEncrypted ? ENewKeyImportEncrypted : ENewKeyImportPlaintext);
		}

	if (err == KErrNone && iKeyDataManager->IsKeyAlreadyInStore(aReturnedKey.Label()))
		{
		err = KErrAlreadyExists;
		}

	if (err != KErrNone)
		{
		TRequestStatus* status = &aStatus;
		User::RequestComplete(status, err);
		return;
		}

	iPKCS8Data.Set(aKey);
	
	iImportingEncryptedKey = aIsEncrypted;
	iCallerRequest = &aStatus;
	iKeyInfo = &aReturnedKey;

	iAction = EImportOpenPrivateStream;
	SetActive();

	if (aIsEncrypted)
		{
		TPasswordManager::ImportPassword(iPassword, iStatus);
		}
	else
		{
		TRequestStatus* status = &iStatus;
		User::RequestComplete(status, KErrNone);
		}
	}

void CFSKeyStoreServer::CancelImportKey()
	{
	if (iAction == EImportOpenPrivateStream ||
		iAction == EImportKey)
		{
		Cancel();
		}
	}

void CFSKeyStoreServer::DoImportKeyL()
	{
	// Generate a decode PKCS8 data object from the incoming descriptor of PKCS8
	// data Creation of this will parse the DER stream and generate the
	// appropriate key representation based on the algorithm

	ASSERT(iPKCS8Data.Ptr());

	CDecPKCS8Data* pkcs8Data = NULL;

	if (iImportingEncryptedKey)
		{
		// Convert import passphrase to 8 bit representation
		TBuf8<32> password;
		
		CnvUtfConverter::ConvertFromUnicodeToUtf8(password, iPassword);
		pkcs8Data = TASN1DecPKCS8::DecodeEncryptedDERL(iPKCS8Data, password);
		}
	else
		{
		pkcs8Data = TASN1DecPKCS8::DecodeDERL(iPKCS8Data);
		}
	
	CleanupStack::PushL(pkcs8Data);	
	PKCS8ToKeyL(pkcs8Data);
	CleanupStack::PopAndDestroy(pkcs8Data);
}

void CFSKeyStoreServer::PKCS8ToKeyL(CDecPKCS8Data* aPKCS8Data)
{ 
	ASSERT(iPassphrase);
	ASSERT(aPKCS8Data);
	
	MPKCS8DecodedKeyPairData* keyPairData = aPKCS8Data->KeyPairData();

	// Set algorithm and size from pkcs8 data, and sanity check them
	if (aPKCS8Data->Algorithm() != ERSA && aPKCS8Data->Algorithm() != EDSA)
		{
		User::Leave(KErrKeyAlgorithm);
		}
	iKeyInfo->SetAlgorithm((aPKCS8Data->Algorithm() == ERSA) ? CKeyInfoBase::ERSA : CKeyInfoBase::EDSA);
	iKeyInfo->SetSize(keyPairData->KeySize());
	User::LeaveIfError(CheckKeyAlgorithmAndSize(*iKeyInfo));

	// Retrieve and store any PKCS8 attributes (in DER encoded descriptor)
	// These will form part of CKeyInfo & available for callers to decode	
	TPtrC8 theAttributes(aPKCS8Data->PKCS8Attributes());
	if (theAttributes != KNullDesC8)
		{
		iKeyInfo->SetPKCS8AttributeSet(theAttributes.AllocL());
		}
		
	const CFileKeyData* keyData = iKeyDataManager->CreateKeyDataLC(iKeyInfo->Label(), iPassphrase->StreamId());
	RStoreWriteStream privateStream;
	iKeyDataManager->OpenPrivateDataStreamLC(*keyData, *iPassphrase, privateStream);

	// Generate the key identifier
	TKeyIdentifier theKeyId;
	keyPairData->GetKeyIdentifierL(theKeyId);
	
	// Fill in the CKeyInfo data currently missing (TKeyIdentifier and handle)
	iKeyInfo->SetHandle(keyData->Handle());
	iKeyInfo->SetIdentifier(theKeyId);	

	CKeyInfo::EKeyAlgorithm keyAlgorithm = iKeyInfo->Algorithm();

	// Externalize private key data
	switch (keyAlgorithm)
		{
		case (CKeyInfo::ERSA):
			privateStream << static_cast<CPKCS8KeyPairRSA*>(keyPairData)->PrivateKey();
			break;
			
		case (CKeyInfo::EDSA):
			privateStream << static_cast<CPKCS8KeyPairDSA*>(keyPairData)->PrivateKey();
			break;
			
		default:
			__ASSERT_DEBUG(EFalse, PanicServer(EPanicInvalidKeyCreateReq));
			break;
		}

	privateStream.CommitL();
	CleanupStack::PopAndDestroy(&privateStream);

	// Externalize public key data
	RStoreWriteStream publicStream;
	iKeyDataManager->OpenPublicDataStreamLC(*keyData, publicStream);

	switch (keyAlgorithm)
		{
		case (CKeyInfo::ERSA):
			publicStream << static_cast<CPKCS8KeyPairRSA*>(keyPairData)->PublicKey();
			break;
			
		case (CKeyInfo::EDSA):
			publicStream << static_cast<CPKCS8KeyPairDSA*>(keyPairData)->PublicKey();
			break;
			
		default:
			__ASSERT_DEBUG(EFalse, PanicServer(EPanicInvalidKeyCreateReq));
			break;
		}

	publicStream.CommitL();
	CleanupStack::PopAndDestroy(&publicStream);

	// Externalize the CKeyInfo data associated with the key,
	iKeyDataManager->WriteKeyInfoL(*keyData, *iKeyInfo);

	// Now add the new key to the data manager (which adds it to the store)
	iKeyDataManager->AddL(keyData);
	CleanupStack::Pop(const_cast<CFileKeyData*>(keyData));
}

void CFSKeyStoreServer::ExportKey(TInt aObjectId, const TPtr8& aKey, TRequestStatus& aStatus)
	{
	iExportingKeyEncrypted = EFalse;				
	TRAPD(err, DoExportKeyL(aObjectId, aKey, aStatus));
	if (err != KErrNone)
		{
		TRequestStatus* status = &aStatus;
		User::RequestComplete(status, err);
		}
	}

void CFSKeyStoreServer::CancelExportKey()
	{

	if (iAction == EExportKey ||
		iAction == EExportKeyGetPassphrase)
		{
		Cancel();
		}
	}	

void CFSKeyStoreServer::ExportEncryptedKey(TInt aObjectId, const TPtr8& aKey, CPBEncryptParms& aParams, TRequestStatus& aStatus)
	{
	iExportingKeyEncrypted	= ETrue;	
	iPbeParams = &aParams;		
	TRAPD(err, DoExportKeyL(aObjectId, aKey, aStatus));
	if (err != KErrNone)
		{
		TRequestStatus* status = &aStatus;
		User::RequestComplete(status, err);
		}
	}


void CFSKeyStoreServer::CancelExportEncryptedKey()
	{
	if (iAction == EExportKeyGetPassphrase ||
		iAction == EExportKey)
		{
		Cancel();
		}
	}	

void CFSKeyStoreServer::DoExportKeyL(TInt aObjectId, const TPtr8& aKey, TRequestStatus& aStatus)
	{
	ASSERT(iMessage);
	ASSERT(!iKeyData);
	ASSERT(!iKeyInfo);
	
	const CFileKeyData* keyData = iKeyDataManager->Lookup(aObjectId);
	if (!keyData)
		{
		User::Leave(KErrNotFound);
		}

	CKeyInfo* keyInfo = iKeyDataManager->ReadKeyInfoLC(*keyData);

	// Check access flags allow key to be exported
	if (!(keyInfo->AccessType() & CCTKeyInfo::EExtractable) ||
		((keyInfo->AccessType() & CCTKeyInfo::ESensitive) && !iExportingKeyEncrypted))
		{
		User::Leave(KErrKeyAccess);
		}

	// Check this isn't a DH key
	if (keyInfo->Algorithm() != CKeyInfo::ERSA &&
		keyInfo->Algorithm() != CKeyInfo::EDSA)
		{
		User::Leave(KErrNotSupported);
		}

	// Check the caller is allowed by the management policy
	if (!keyInfo->ManagementPolicy().CheckPolicy(*iMessage))
		{
		User::Leave(KErrPermissionDenied);
		}

	iKeyData = keyData;
	iKeyInfo = keyInfo;
	CleanupStack::Pop(keyInfo);
	iExportBuf.Set(aKey);		
	iCallerRequest = &aStatus;
		
	if (iExportingKeyEncrypted)
		{
		iAction = EExportEncryptedKeyGetPassphrase;			
		}
	else
		{
		iAction = EExportKeyGetPassphrase;
		}
	SetActive();				
	TRequestStatus* status = &iStatus;
	User::RequestComplete(status, KErrNone);		
	}
	
void CFSKeyStoreServer::CompleteKeyExportL(TBool encrypted /*=EFalse*/)
	{
	ASSERT(iPassphrase);
	ASSERT(iKeyData);
	ASSERT(iExportBuf.Ptr());
	
	CKeyInfo::EKeyAlgorithm keyAlgorithm = iKeyInfo->Algorithm();
	RStoreReadStream privStream;		
	iKeyDataManager->OpenPrivateDataStreamLC(*iKeyData, *iPassphrase, privStream);

	CASN1EncSequence* encoded = NULL;
			
	switch(keyAlgorithm)
		{
		case (CKeyInfo::ERSA):
			{
			RStoreReadStream pubStream;
			iKeyDataManager->OpenPublicDataStreamLC(*iKeyData, pubStream);
			CRSAPublicKey* publicKey = NULL;
			CreateL(pubStream, publicKey);
			ASSERT(publicKey);
			CleanupStack::PushL(publicKey);
					
			CRSAPrivateKey* privateKey = NULL;
			CreateL(privStream, privateKey);
			ASSERT(privateKey);
			CleanupStack::PushL(privateKey);			

			if (encrypted)
				{
				TBuf8<32> password;
				CnvUtfConverter::ConvertFromUnicodeToUtf8(password, iPassword);

				CPBEncryptElement* encryptElement = CPBEncryptElement::NewLC(password, *iPbeParams);
				CPBEncryptor* encryptor = encryptElement->NewEncryptLC();
				encoded = TASN1EncPKCS8::EncodeEncryptedL(*(static_cast<CRSAPrivateKeyCRT*>(privateKey)), *publicKey, *encryptor, *iPbeParams, iKeyInfo->PKCS8AttributeSet());
				CleanupStack::PopAndDestroy(2, encryptElement); 	// encryptor, encryptElement 
				}
			else
				{
				encoded = TASN1EncPKCS8::EncodeL(*(static_cast<CRSAPrivateKeyCRT*>(privateKey)), *publicKey, iKeyInfo->PKCS8AttributeSet());					
				}
			CleanupStack::PopAndDestroy(3, &pubStream);          // privateKey,  publicKey, pubStream
			}
			break;

		case (CKeyInfo::EDSA):
			{
			CDSAPrivateKey* privateKey = NULL;

			CreateL(privStream, privateKey);
			ASSERT(privateKey);
			CleanupStack::PushL(privateKey);

			if (encrypted)
				{			
				TBuf8<32> password;
				CnvUtfConverter::ConvertFromUnicodeToUtf8(password, iPassword);
										
				CPBEncryptElement* encryptElement = CPBEncryptElement::NewLC(password, *iPbeParams);         				
				CPBEncryptor* encryptor = encryptElement->NewEncryptLC(); 								
				encoded = TASN1EncPKCS8::EncodeEncryptedL(*privateKey, *encryptor, *iPbeParams, iKeyInfo->PKCS8AttributeSet());					
				CleanupStack::PopAndDestroy(2, encryptElement);	// encryptor, encryptElement	
				}
			else
				{
				encoded = TASN1EncPKCS8::EncodeL(*privateKey, iKeyInfo->PKCS8AttributeSet());					
				}					
			CleanupStack::PopAndDestroy(privateKey);
			}
			break;

		case (CKeyInfo::EInvalidAlgorithm):
		default:
			User::Leave(KErrKeyAlgorithm);		
			break;
		}
			
	// common to all algorithms			
	ASSERT(encoded);
	CleanupStack::PushL(encoded);
	if (encoded->LengthDER() > static_cast<TUint>(iExportBuf.MaxLength()))
		{
		User::Leave(KErrOverflow);
		}
	TUint pos=0;
	encoded->WriteDERL(iExportBuf, pos);

	// WriteDERL does not set the length of the buffer, we do it ourselves			
	iExportBuf.SetLength(encoded->LengthDER());
			
	CleanupStack::PopAndDestroy(encoded); 
	CleanupStack::PopAndDestroy(&privStream); 
	RunError(KErrNone);
	}

void CFSKeyStoreServer::DeleteKeyL(TInt aObjectId)
	{
	ASSERT(iMessage);
	
	const CFileKeyData* keyData = iKeyDataManager->Lookup(aObjectId);
	if (!keyData)
		{
		User::Leave(KErrNotFound);
		}

	CKeyInfo* keyInfo = iKeyDataManager->ReadKeyInfoLC(*keyData);

	// Check the caller is allowed by the management policy
	if (!keyInfo->ManagementPolicy().CheckPolicy(*iMessage))
		{
		User::Leave(KErrPermissionDenied);
		}

	CleanupStack::PopAndDestroy(keyInfo);

	// Check if any session has this key open
	for (TInt i = 0 ; i < iSessions.Count() ; ++i)
		{
		CKeyStoreSession& session = *iSessions[i];
		if (session.HasOpenKey(aObjectId))
			{
			User::Leave(KErrInUse);
			}	
		}

	iKeyDataManager->RemoveL(aObjectId);
}

void CFSKeyStoreServer::SetUsePolicyL(TInt aObjectId, const TSecurityPolicy& aPolicy)
	{
	ASSERT(iMessage);
	
	const CFileKeyData* keyData = iKeyDataManager->Lookup(aObjectId);
	if (!keyData)
		{
		User::Leave(KErrNotFound);
		}

	CKeyInfo* keyInfo = iKeyDataManager->ReadKeyInfoLC(*keyData);

	// Check the caller is allowed by the management policy
	if (!keyInfo->ManagementPolicy().CheckPolicy(*iMessage))
		{
		User::Leave(KErrPermissionDenied);
		}

	// should revert change if write fails
	keyInfo->SetUsePolicy(aPolicy); 
	iKeyDataManager->SafeWriteKeyInfoL(*keyData, *keyInfo);	

	CleanupStack::PopAndDestroy(keyInfo);
	}
	
void CFSKeyStoreServer::SetManagementPolicyL(TInt aObjectId, const TSecurityPolicy& aPolicy)
	{
	ASSERT(iMessage);
	
	const CFileKeyData* keyData = iKeyDataManager->Lookup(aObjectId);
	if (!keyData)
		{
		User::Leave(KErrNotFound);
		}

	CKeyInfo* keyInfo = iKeyDataManager->ReadKeyInfoLC(*keyData);

	// Check the caller is allowed by current management policy
	if (!keyInfo->ManagementPolicy().CheckPolicy(*iMessage))
		{
		User::Leave(KErrPermissionDenied);
		}

	// Check the caller is allowed by new management policy
	if (!aPolicy.CheckPolicy(*iMessage))
		{
		User::Leave(KErrArgument);
		}

	// should revert change if write fails
	keyInfo->SetManagementPolicy(aPolicy);
	iKeyDataManager->SafeWriteKeyInfoL(*keyData, *keyInfo);	

	CleanupStack::PopAndDestroy(keyInfo);
	}

// For MCTAuthenticationObject

void CFSKeyStoreServer::ChangePassphrase(TRequestStatus& aStatus)
	{
	if (iKeyDataManager->DefaultPassphraseId() == KNullStreamId)
		{
		// No passphrase set, can't change it
		TRequestStatus* status = &aStatus;
		User::RequestComplete(status, KErrNotFound);		
		}
	
	iCallerRequest = &aStatus;
	iAction = EChangePassphrase;
	SetActive();
	TRequestStatus* status = &iStatus;
	User::RequestComplete(status, KErrNone);
	}

void CFSKeyStoreServer::DoChangePassphrase()
	{
	ASSERT(iSession);
	ASSERT(iKeyDataManager->DefaultPassphraseId() != KNullStreamId);
	iSession->PassphraseManager().ChangePassphrase(iKeyDataManager->DefaultPassphraseId(), iStatus);
	iAction = EChangePassphraseClearCached;
	SetActive();
	}

void CFSKeyStoreServer::CancelChangePassphrase()
	{
	if (iAction == EChangePassphrase ||
		iAction == EChangePassphraseClearCached)
		{
		Cancel();
		}
	}

void CFSKeyStoreServer::AuthOpen(TRequestStatus& aStatus)
	{
	iCallerRequest = &aStatus;
	GetKeystorePassphrase(EAuthOpen);
	}

void CFSKeyStoreServer::CancelAuthOpen()
	{
	if (iAction == EAuthOpen)
		{
		Cancel();
		}
	}

void CFSKeyStoreServer::AuthClose()
	{
	ASSERT(iSession);
	iSession->PassphraseManager().RemoveCachedPassphrases(iKeyDataManager->DefaultPassphraseId());
	}

TInt CFSKeyStoreServer::GetTimeRemainingL()
	{
	ASSERT(iSession);
	TStreamId passStreamId = iKeyDataManager->DefaultPassphraseId();
	return iSession->PassphraseManager().TimeRemainingL(passStreamId);
	}

void CFSKeyStoreServer::SetTimeoutL(TInt aTimeout)
	{
	ASSERT(iMessage);

	if (!KSetTimeoutSecurityPolicy.CheckPolicy(*iMessage))
		{
		User::Leave(KErrPermissionDenied);
		}
	
	if (aTimeout < KTimeoutNever)
		{
		User::Leave(KErrArgument);
		}

	iKeyDataManager->SetPassphraseTimeoutL(aTimeout);
	RemoveCachedPassphrases(KNullStreamId);
	}

TInt CFSKeyStoreServer::GetTimeoutL()
	{
	return iKeyDataManager->GetPassphraseTimeout();
	}

void CFSKeyStoreServer::Relock()
	{
	RemoveCachedPassphrases(KNullStreamId);
	}

void CFSKeyStoreServer::RemoveCachedPassphrases(TStreamId aStreamId)
	{
	for (TInt index = 0 ; index < iSessions.Count() ; ++index)
		{
		CKeyStoreSession& session = *iSessions[index];					
		session.PassphraseManager().RemoveCachedPassphrases(aStreamId);
		}			
	}

//	*********************************************************************************
//	From CActive
//	*********************************************************************************
TInt CFSKeyStoreServer::RunError(TInt aError)
	{ 
	// Delete anything we might have created
	delete iKeyCreator; iKeyCreator = NULL;

	if (iAction == EExportKeyGetPassphrase ||
		iAction == EExportEncryptedKeyGetPassphrase ||
		iAction == EExportKey ||
		iAction == EExportEncryptedKey)
		{
		// we only own iKeyInfo for export operations
		delete iKeyInfo;
		iKeyInfo = NULL;
		}
	
	// Zero pointers to things we don't own
	iPassphrase = NULL;
	iKeyInfo = NULL;
	iKeyData = NULL;
	iExportBuf.Set(NULL, 0, 0);
	iPKCS8Data.Set(NULL, 0);
	iSession = NULL;
	iMessage = NULL;

	if (iCallerRequest)
		User::RequestComplete(iCallerRequest, aError);

	iAction = EIdle;		//	Reset action
	return (KErrNone);		//	Handled
	}

void CFSKeyStoreServer::DoCancel()
	{
	switch (iAction)
		{
		case EImportKey:
		case ECreateKeyCreate:
		case EExportKey:
		case EExportEncryptedKey:
		case EChangePassphraseClearCached:
			ASSERT(iSession);
			iSession->PassphraseManager().Cancel();
			break;

		case ECreateKeyFinal:
			ASSERT(iKeyCreator);
			iKeyCreator->Cancel();
			break;

		default:
			// Nothing to do
			break;
		}
	
	RunError(KErrCancel);
	}

void CFSKeyStoreServer::RunL()
	{
	User::LeaveIfError(iStatus.Int()); 

	switch (iAction)
		{
		case EImportOpenPrivateStream:
			ASSERT(iKeyInfo);
			GetKeystorePassphrase(EImportKey);
			break;
		case ECreateKeyCreate:
			DoCreateKeyL();	
			iAction = ECreateKeyFinal;
			break;
		case ECreateKeyFinal:
			DoStoreKeyL();			 
			//	Check iKeyInfo was initialised for the caller
			ASSERT(iKeyInfo->HandleID() != 0);						
			RunError(KErrNone);
			break;
		case EImportKey:
			{
			TRAPD(err, DoImportKeyL());
			if (err == KErrTooBig)
				{
				// Returned by ASN library if data is unexpected probably as a result of
				// bad import data
				err = KErrArgument;
				}
			User::LeaveIfError(err);
			RunError(KErrNone);
			break;
			}
		case EExportKeyGetPassphrase:
			GetKeystorePassphrase(EExportKey);
			break;
		case EExportEncryptedKeyGetPassphrase:
			GetKeystorePassphrase(EExportEncryptedKey);
			break;
		case EExportEncryptedKey:
			{
			TPasswordManager::ExportPassword(iPassword, iStatus); 
			iAction = EExportKey;
			SetActive();
			}	
			break;
		case EExportKey:
			{
			CompleteKeyExportL(iExportingKeyEncrypted);
			}
			break;				
		case EChangePassphrase:
			DoChangePassphrase();
			break;
		case EChangePassphraseClearCached:
			// Make sure the passphrase we just changed is not cached anywhere
			RemoveCachedPassphrases(iKeyDataManager->DefaultPassphraseId());
			RunError(KErrNone);
			break;
		case EAuthOpen:
			RunError(KErrNone);
			break;
		default:
			ASSERT(EFalse);
		}
	}