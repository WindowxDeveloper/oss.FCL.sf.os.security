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


#include "clientsession.h"
#include <e32std.h>
#include <e32uid.h>
#include "clientutils.h"
#include "fstokenservername.h"


//	\\	//	\\	//	\\	//	\\	//	\\	//	\\	//	\\	//	\\	//	\\	//	\\	
//	Tokentype session class for file based certificate store
//	Connects and passes messages to the file store tokentype server
//	Coded specifically for file store token type
//	\\	//	\\	//	\\	//	\\	//	\\	//	\\	//	\\	//	\\	//	\\	//	\\	

_LIT(KFSTokenServerImg,"fstokenserver");

RFileStoreClientSession::RFileStoreClientSession()
{}

TInt RFileStoreClientSession::SendRequest(TFSTokenMessages aRequest, const TIpcArgs& aArgs) const
{
	return SendReceive(aRequest, aArgs);
}

void RFileStoreClientSession::SendAsyncRequest(TFSTokenMessages aRequest, const TIpcArgs& aArgs, TRequestStatus* aStatus) const
{
	__ASSERT_ALWAYS(aStatus, FSTokenPanic(EBadArgument));

	if (aStatus)
	{
		*aStatus = KRequestPending;
		SendReceive(aRequest, aArgs, *aStatus);
	}
}


//	\\	//	\\	//	\\	//	\\	//	\\	//	\\	//	\\	//	\\	//	\\	//	\\	
//	Client-server startup code
//	\\	//	\\	//	\\	//	\\	//	\\	//	\\	//	\\	//	\\	//	\\	//	\\	

static TInt StartServer();	//	Forward declaration
//
// Connect to the server, attempting to start it if necessary
//
TInt RFileStoreClientSession::Connect(ETokenEnum aToken)
	{
	// The version is made up of three pieces of information:
	// 1. iMajor - The token we want to talk to
	// 2. iMinor - The protocol version number
	// 3. iBuild - unused
	TVersion version(aToken, KFSProtolVersion, 0);
	
	TInt retry=2;
	for (;;)
		{
		TInt r=CreateSession(KFSTokenServerName, version, 1);
		if (r!=KErrNotFound && r!=KErrServerTerminated)
			return r;
		if (--retry==0)
			return r;
		r=StartServer();
		if (r!=KErrNone && r!=KErrAlreadyExists)
			return r;
		}
	}

TInt StartServer()
	{
	// Server startup is different for WINS in EKA1 mode ONLY (lack of process
	// emulation - we load the library in this instance
	const TUidType serverUid(KNullUid, KNullUid, KUidFSTokenServer);

	RProcess server;	
	TInt r = server.Create(KFSTokenServerImg, KNullDesC, serverUid);
	
	if (r != KErrNone)
		{
		return r;
		}

	// Synchronise with the process to make sure it hasn't died straight away
	TRequestStatus stat;
	server.Rendezvous(stat);
	if (stat != KRequestPending)
		{
		// logon failed - server is not yet running, so cannot have terminated
		server.Kill(0);				// Abort startup
		}
	else
		{
		// logon OK - start the server
		server.Resume();
		}

	// Wait to synchronise with server - if it dies in the meantime, it
	// also gets completed
	User::WaitForRequest(stat);	

	// We can't use the 'exit reason' if the server panicked as this
	// is the panic 'reason' and may be '0' which cannot be distinguished
	// from KErrNone
	r = (server.ExitType()==EExitPanic) ? KErrGeneral : stat.Int();
	server.Close();
	return (r);
	}