/*
* Copyright (c) 2003-2009 Nokia Corporation and/or its subsidiary(-ies).
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
* ** IMPORTANT ** PublishedPartner API's in this file are published to 3rd party developers via the 
* Symbian website. Changes to these API's should be treated as PublishedAll API changes and the Security TA should be consulted.
* Asymmetric crypto implementation
*
*/




/**
 @file 
 @internalAll
*/
 
#ifndef __ASYMMETRIC_H__
#define __ASYMMETRIC_H__

#include <padding.h>
#include <asymmetrickeys.h>
#include <random.h>
#include <hash.h>

// All the classes in this file have their default constructors and
// assignment operators defined private, but not implemented, in order to
// prevent their use.

/** 
* Mixin class defining common operations for public key encryption and
* decryption classes.
* 
* @publishedPartner
* @released 
*/
class MCryptoSystem 
	{
public:
	/**
	 * Gets the maximum size of input accepted by this object.
	 *	
	 * @return	The maximum input length allowed in bytes.
	 */	 
	virtual TInt MaxInputLength(void) const = 0;
	
	/**
	 * Gets the maximum size of output that can be generated by this object.
	 *
	 * @return	The maximum output length in bytes.
	 */	 
	virtual TInt MaxOutputLength(void) const = 0;
protected:
	/**
	 * Constructor
 	 */	 
	IMPORT_C MCryptoSystem(void);
private:
	MCryptoSystem(const MCryptoSystem&);
	MCryptoSystem& operator=(const MCryptoSystem&);
	};

/** 
* Abstract base class for all public key encryptors.
* 
* @publishedPartner
* @released 
*/
class CEncryptor : public CBase, public MCryptoSystem
	{
public:
	/**
	 * Encrypts the specified plaintext into ciphertext.
	 * 
	 * @param aInput	The plaintext
	 * @param aOutput	On return, the ciphertext
	 *
	 * @panic KCryptoPanic	If the input data is too long.
	 *						See ECryptoPanicInputTooLarge
	 * @panic KCryptoPanic	If the supplied output descriptor is not large enough to store the result.
	 *						See ECryptoPanicOutputDescriptorOverflow
	 */	 
	virtual void EncryptL(const TDesC8& aInput, TDes8& aOutput) const = 0;
protected:
	/** Default constructor */	 
	IMPORT_C CEncryptor(void);
private:
	CEncryptor(const CEncryptor&);
	CEncryptor& operator=(const CEncryptor&);
	};

/** 
* Abstract base class for all public key decryptors.
* 
* @publishedPartner
* @released 
*/
class CDecryptor : public CBase, public MCryptoSystem
	{
public:
	/**
	 * Decrypts the specified ciphertext into plaintext
	 *
	 * @param aInput	The ciphertext to be decrypted
	 * @param aOutput	On return, the plaintext
	 *
	 * @panic KCryptoPanic		If the input data is too long.
	 *							See ECryptoPanicInputTooLarge
	 * @panic KCryptoPanic		If the supplied output descriptor is not large enough to store the result.
	 *							See ECryptoPanicOutputDescriptorOverflow
	 */	 
	virtual void DecryptL(const TDesC8& aInput, TDes8& aOutput) const = 0;
protected:
	/** Default constructor */	 
	IMPORT_C CDecryptor(void);
private:
	CDecryptor(const CDecryptor&);
	CDecryptor& operator=(const CDecryptor&);
	};

/**
* Implementation of RSA encryption as described in PKCS#1 v1.5.
* 
* @publishedPartner
* @released 
*/
class CRSAPKCS1v15Encryptor : public CEncryptor
	{
public:
	/**
	 * Creates a new RSA encryptor object using PKCS#1 v1.5 padding.
	 * 
	 * @param aKey	The RSA encryption key
	 * @return		A pointer to a new CRSAPKCS1v15Encryptor object
	 *
	 * @leave KErrKeyNotWeakEnough	If the key size is larger than that allowed by the
	 *								cipher strength restrictions of the crypto library.
	 *								See TCrypto::IsAsymmetricWeakEnoughL()
	 * @leave KErrKeySize			If the key length is too small
	 */
	IMPORT_C static CRSAPKCS1v15Encryptor* NewL(const CRSAPublicKey& aKey);

	/**
	 * Creates a new RSA encryptor object using PKCS#1 v1.5 padding.
	 * 
	 * The returned pointer is put onto the cleanup stack.
	 *
	 * @param aKey	The RSA encryption key
	 * @return		A pointer to a new CRSAPKCS1v15Encryptor object
	 *
	 * @leave KErrKeyNotWeakEnough	If the key size is larger than that allowed by the
	 *								cipher strength restrictions of the crypto library.
	 *								See TCrypto::IsAsymmetricWeakEnoughL()
	 * @leave KErrKeySize			If the key length is too small
	 */
	IMPORT_C static CRSAPKCS1v15Encryptor* NewLC(const CRSAPublicKey& aKey);
	void EncryptL(const TDesC8& aInput, TDes8& aOutput) const;
	TInt MaxInputLength(void) const;
	TInt MaxOutputLength(void) const;
	/** The destructor frees all resources owned by the object, prior to its destruction. */
	virtual ~CRSAPKCS1v15Encryptor(void);
protected:
	/** @internalAll */
	CRSAPKCS1v15Encryptor(const CRSAPublicKey& aKey);
	/** @internalAll */
	void ConstructL(void);
protected:
	/** The RSA public key */	 
	const CRSAPublicKey& iPublicKey;
	/** The PKCS#1 v1.5 encryption padding */	 
	CPaddingPKCS1Encryption* iPadding;
private:
	CRSAPKCS1v15Encryptor(const CRSAPKCS1v15Encryptor&);
	CRSAPKCS1v15Encryptor& operator=(const CRSAPKCS1v15Encryptor&);
	};

/** 
* Implementation of RSA decryption as described in PKCS#1 v1.5.
*
* @publishedPartner
* @released 
*/
class CRSAPKCS1v15Decryptor : public CDecryptor
	{
public:
	/**
	 * Creates a new RSA decryptor object using PKCS#1 v1.5 padding.
	 *
	 * @param aKey	The RSA private key for decryption
	 *
	 * @leave KErrKeyNotWeakEnough	If the key size is larger than that allowed by the
	 *								cipher strength restrictions of the crypto library.
	 * 								See TCrypto::IsAsymmetricWeakEnoughL()
	 * @leave KErrKeySize			If the key length is too small
	 */
	IMPORT_C static CRSAPKCS1v15Decryptor* NewL(const CRSAPrivateKey& aKey);
	
	/**
	 * Creates a new RSA decryptor object using PKCS#1 v1.5 padding
	 *
	 * The returned pointer is put onto the cleanup stack.
	 *
	 * @param aKey	The RSA private key for decryption
	 *
	 * @leave KErrKeyNotWeakEnough	If the key size is larger than that allowed by the
	 *								cipher strength restrictions of the crypto library.
	 * 								See TCrypto::IsAsymmetricWeakEnoughL()
	 * @leave KErrKeySize			If the key length is too small
	 * @leave KErrNotSupported	    If the RSA private key is not a supported TRSAPrivateKeyType
	 */
	IMPORT_C static CRSAPKCS1v15Decryptor* NewLC(const CRSAPrivateKey& aKey);
	void DecryptL(const TDesC8& aInput, TDes8& aOutput) const;
	TInt MaxInputLength(void) const;
	TInt MaxOutputLength(void) const;
	/** The destructor frees all resources owned by the object, prior to its destruction. */
	virtual ~CRSAPKCS1v15Decryptor(void);
protected:
	/** @internalAll */
	CRSAPKCS1v15Decryptor(const CRSAPrivateKey& aKey);
	/** @internalAll */
	void ConstructL(void);
protected:
	/** The RSA private key */	 
	const CRSAPrivateKey& iPrivateKey;
	/** The PKCS#1 v1.5 encryption padding */	 
	CPaddingPKCS1Encryption* iPadding;
private:
	CRSAPKCS1v15Decryptor(const CRSAPKCS1v15Decryptor&);
	CRSAPKCS1v15Decryptor& operator=(const CRSAPKCS1v15Decryptor&);
	};

/** 
* Mixin class defining operations common to all public key signature systems.
*
* @publishedPartner
* @released 
*/
class MSignatureSystem 
	{
public:
	/**
	 * Gets the maximum size of input accepted by this object.
	 *	
	 * @return	The maximum length allowed in bytes
	 */	 
	virtual TInt MaxInputLength(void) const = 0;
protected:
	/** Constructor */
	IMPORT_C MSignatureSystem(void);
private:
	MSignatureSystem(const MSignatureSystem&);
	MSignatureSystem& operator=(const MSignatureSystem&);
	};

/** 
* Abstract base class for all public key signers.
*
* The template parameter, CSignature, should be a class that encapsulates the
* concept of a digital signature.  Derived signature classes must own their
* respective signatures (and hence be CBase derived).  There are no other
* restrictions on the formation of the signature classes.
* 
* @publishedPartner
* @released 
*/
template <class CSignature> class CSigner : public CBase, public MSignatureSystem
	{
public:
	/**
	 * Digitally signs the specified input message
	 *
	 * @param aInput	The raw data to sign, typically a hash of the actual message
	 * @return			A pointer to a new CSignature object
	 *
	 * @panic ECryptoPanicInputTooLarge	If aInput is larger than MaxInputLength(),
	 *									which is likely to happen if the caller
	 *									has passed in something that has not been
	 *									hashed.
	 */
	virtual CSignature* SignL(const TDesC8& aInput) const = 0;
protected:
	/** @internalAll */
	CSigner(void);
private:
	CSigner(const CSigner&);
	CSigner& operator=(const CSigner&);
	};

/** 
* Abstract class for all public key verifiers.
*
* The template parameter, CSignature, should be a class that encapsulates the
* concept of a digital signature.  Derived signature classes must own their
* respective signatures (and hence be CBase derived).  There are no other
* restrictions on the formation of the signature classes.
* 
* @publishedPartner
* @released 
*/
template <class CSignature> class CVerifier : public CBase, public MSignatureSystem
	{
public:
	/**
	 * Verifies the specified digital signature
	 *
	 * @param aInput		The message digest that was originally signed
	 * @param aSignature	The signature to be verified
	 * 
	 * @return				Whether the signature is the result of signing
	 *						aInput with the supplied key
	 */
	virtual TBool VerifyL(const TDesC8& aInput, 
		const CSignature& aSignature) const = 0;
protected:
	/** @internalAll */
	CVerifier(void);
private:
	CVerifier(const CVerifier&);
	CVerifier& operator=(const CVerifier&);
	};

/* Template nastiness for CVerifier and CSigner in asymmetric.inl */

#include <asymmetric.inl>

/** 
* An encapsulation of a RSA signature.
* 
* @publishedPartner
* @released 
*/
class CRSASignature : public CBase
	{
public:
	/**
	 * Creates a new CRSASignature object from the integer value 
	 * output of a previous RSA signing operation.
	 * 
	 * @param aS	The integer value output from a previous RSA signing operation
	 * @return		A pointer to the new CRSASignature object.
	 */
	IMPORT_C static CRSASignature* NewL(RInteger& aS);
	
	/**
	 * Creates a new CRSASignature object from the integer value 
	 * output of a previous RSA signing operation.
	 * 
	 * The returned pointer is put onto the cleanup stack.
	 *
	 * @param aS	The integer value output from a previous RSA signing operation
	 * @return		A pointer to the new CRSASignature object.
	 */
	IMPORT_C static CRSASignature* NewLC(RInteger& aS);
	
	/**
	 * Gets the integer value of the RSA signature
	 * 
	 * @return	The integer value of the RSA signature
	 */
	IMPORT_C const TInteger& S(void) const;
	
	/**
	 * Whether this RSASignature is identical to a specified RSASignature
	 *
	 * @param aSig	The RSASignature for comparison
	 * @return		ETrue, if the two signatures are identical; EFalse, otherwise.
	 */
	IMPORT_C TBool operator== (const CRSASignature& aSig) const;
	
	/** Destructor */
	/** The destructor frees all resources owned by the object, prior to its destruction. */
	IMPORT_C virtual ~CRSASignature(void);
protected:
	/** 
	 * Second phase constructor
	 *
	 * @see CRSASignature::NewL()
	 *
	 * @param aS	The integer value output from a previous RSA signing operation	
	 */
	IMPORT_C CRSASignature(RInteger& aS);

	/** Default constructor */
	IMPORT_C CRSASignature(void);
protected:
	/** An integer value; the output from a previous RSA signing operation. */
	RInteger iS;
private:
	CRSASignature(const CRSASignature&);
	CRSASignature& operator=(const CRSASignature);
	};

/** 
* Abstract base class for all RSA Signers.
* 
* @publishedPartner
* @released
*/
class CRSASigner : public CSigner<CRSASignature>
	{
public:
	/**
	 * Gets the maximum size of output that can be generated by this object.
	 *
	 * @return	The maximum output length in bytes
	 */	 
	virtual TInt MaxOutputLength(void) const = 0;
protected:
	/** Default constructor */
	IMPORT_C CRSASigner(void);
private:
	CRSASigner(const CRSASigner&);
	CRSASigner& operator=(const CRSASigner&);
	};

/**
* Implementation of RSA signing as described in PKCS#1 v1.5.
* 
* This class creates RSA signatures following the RSA PKCS#1 v1.5 standard (with
* the one caveat noted below) and using PKCS#1 v1.5 signature padding.  The only
* exception is that the SignL() function simply performs a 'raw' PKCS#1 v1.5 sign
* operation on whatever it is given.  It does <b>not</b> hash or in any way
* manipulate the input data before signing.  
* 
* @publishedPartner
* @released 
*/
class CRSAPKCS1v15Signer : public CRSASigner
	{
public:
	/**
	 * Creates a new CRSAPKCS1v15Signer object from a specified RSA private key.
	 *  
	 * @param aKey	The RSA private key to be used for signing
	 * @return		A pointer to the new CRSAPKCS1v15Signer object
	 *
	 * @leave KErrKeySize	If the key length is too small
	 */
	IMPORT_C static CRSAPKCS1v15Signer* NewL(const CRSAPrivateKey& aKey);

	/**
	 * Creates a new CRSAPKCS1v15Signer object from a specified RSA private key.
	 *  
	 * The returned pointer is put onto the cleanup stack.
	 *
	 * @param aKey	The RSA private key to be used for signing
	 * @return		A pointer to the new CRSAPKCS1v15Signer object
	 *
	 * @leave KErrKeySize	If the key length is too small
	 */
	IMPORT_C static CRSAPKCS1v15Signer* NewLC(const CRSAPrivateKey& aKey);
	/**
	 * Digitally signs the specified input message
	 *
	 * @param aInput	The raw data to sign, typically a hash of the actual message
	 * @return			A pointer to a new CSignature object
	 *
	 * @leave KErrNotSupported			If the private key is not a supported TRSAPrivateKeyType
	 * @panic ECryptoPanicInputTooLarge	If aInput is larger than MaxInputLength(),
	 *									which is likely to happen if the caller
	 *									has passed in something that has not been hashed.
	 */
	virtual CRSASignature* SignL(const TDesC8& aInput) const;
	virtual TInt MaxInputLength(void) const;
	virtual TInt MaxOutputLength(void) const;
	/** The destructor frees all resources owned by the object, prior to its destruction. 
	 * @internalAll */
	~CRSAPKCS1v15Signer(void);
protected:
	/** @internalAll */
	CRSAPKCS1v15Signer(const CRSAPrivateKey& aKey);
	/** @internalAll */
	void ConstructL(void);
protected:
	/** The RSA private key to be used for signing */
	const CRSAPrivateKey& iPrivateKey;
	/** The PKCS#1 v1.5 signature padding */
	CPaddingPKCS1Signature* iPadding;
private:
	CRSAPKCS1v15Signer(const CRSAPKCS1v15Signer&);
	CRSAPKCS1v15Signer& operator=(const CRSAPKCS1v15Signer&);
	};

/** 
* Abstract base class for all RSA Verifiers.
*
* @publishedPartner
* @released
*/
class CRSAVerifier : public CVerifier<CRSASignature>
	{
public:
	/**
	 * Gets the maximum size of output that can be generated by this object.
	 *
	 * @return	The maximum output length in bytes
	 */	 
	virtual TInt MaxOutputLength(void) const = 0;

	/**
	 * Performs a decryption operation on a signature using the public key.
	 *
	 * This is the inverse of the sign operation, which performs a encryption
	 * operation on its input data using the private key.  Although this can be
	 * used to verify signatures, CRSAVerifier::VerifyL should be used in
	 * preference.  This method is however required by some security protocols.
	 * 
	 * @param aSignature	The signature to be verified
	 * @return				A pointer to a new buffer containing the result of the
	 *						operation. The pointer is left on the cleanup stack.
	 */
	virtual HBufC8* InverseSignLC(const CRSASignature& aSignature) const = 0;

	IMPORT_C virtual TBool VerifyL(const TDesC8& aInput, 
		const CRSASignature& aSignature) const;
protected:
	/** Default constructor */
	IMPORT_C CRSAVerifier(void);
private:
	CRSAVerifier(const CRSAVerifier&);
	CRSAVerifier& operator=(const CRSAVerifier&);
	};

/**
* This class verifies RSA signatures given a message and its supposed
* signature.  It follows the RSA PKCS#1 v1.5 with PKCS#1 v1.5 padding specification
* with the following exception: the VerifyL() function does <b>not</b> hash or
* in any way manipulate the input data before checking.  Thus in order to verify
* RSA signatures in PKCS#1 v1.5 format, the input data needs to follow PKCS#1 v1.5 
* specification, i.e. be ASN.1 encoded and prefixed  by ASN.1 encoded digestId.
* 
* @publishedPartner
* @released 
*/
class CRSAPKCS1v15Verifier : public CRSAVerifier
	{
public:
	/**
	 * Creates a new CRSAPKCS1v15Verifier object from a specified RSA public key.
	 *
	 * @param aKey	The RSA public key to be used for verifying
	 * @return		A pointer to the new CRSAPKCS1v15Verifier object
	 *
	 * @leave KErrKeySize	If the key length is too small
	 */
	IMPORT_C static CRSAPKCS1v15Verifier* NewL(const CRSAPublicKey& aKey);

	/**
	 * Creates a new CRSAPKCS1v15Verifier object from a specified RSA public key.
	 *  
	 * The returned pointer is put onto the cleanup stack.
	 *
	 * @param aKey	The RSA public key to be used for verifying
	 * @return		A pointer to the new CRSAPKCS1v15Verifier object
	 *
	 * @leave KErrKeySize	If the key length is too small
	 */
	IMPORT_C static CRSAPKCS1v15Verifier* NewLC(const CRSAPublicKey& aKey);
	virtual HBufC8* InverseSignLC(const CRSASignature& aSignature) const;
	virtual TInt MaxInputLength(void) const;
	virtual TInt MaxOutputLength(void) const;
	/** The destructor frees all resources owned by the object, prior to its destruction. */
	virtual ~CRSAPKCS1v15Verifier(void);
protected:
	/** @internalAll */
	CRSAPKCS1v15Verifier(const CRSAPublicKey& aKey);
	/** @internalAll */
	void ConstructL(void);
protected:
	/** The RSA public key to be used for verification */
	const CRSAPublicKey& iPublicKey;
	/** The PKCS#1 v1.5 signature padding */
	CPaddingPKCS1Signature* iPadding;
private:
	CRSAPKCS1v15Verifier(const CRSAPKCS1v15Verifier&);
	CRSAPKCS1v15Verifier& operator=(const CRSAPKCS1v15Verifier&);
	};
	
/** 
* An encapsulation of a DSA signature.
* 
* @publishedPartner
* @released 
*/
class CDSASignature : public CBase
	{
public:
	/**
	 * Creates a new CDSASignature object from the specified R and S values.
	 *
	 * @param aR 	The DSA signature's R value
	 * @param aS	The DSA signature's S value
	 * @return		A pointer to the new CDSASignature object
	 */
	IMPORT_C static CDSASignature* NewL(RInteger& aR, RInteger& aS);

	/**
	 * Creates a new CDSASignature object from the specified R and S values.
	 *  
	 * The returned pointer is put onto the cleanup stack.
	 *
	 * @param aR 	The DSA signature's R value
	 * @param aS	The DSA signature's S value
	 * @return		A pointer to the new CDSASignature object
	 */
	IMPORT_C static CDSASignature* NewLC(RInteger& aR, RInteger& aS);
	
	/**
	 * Gets the DSA signature's R value
	 * 
	 * @return	The R value
	 */
	IMPORT_C const TInteger& R(void) const;
	
	/**
	 * Gets the DSA signature's S value
	 * 
	 * @return	The S value
	 */
	IMPORT_C const TInteger& S(void) const;
	
	/**
	 * Whether this DSASignature is identical to a specified DSASignature
	 *
	 * @param aSig	The DSASignature for comparison
	 * @return		ETrue, if the two signatures are identical; EFalse, otherwise.
	 */
	IMPORT_C TBool operator== (const CDSASignature& aSig) const;
	
	/** The destructor frees all resources owned by the object, prior to its destruction. */
	IMPORT_C virtual ~CDSASignature(void);
protected:
	/**
	 * Protected constructor
	 *
	 * @param aR 	The DSA signature's R value
	 * @param aS	The DSA signature's S value
	 */
	IMPORT_C CDSASignature(RInteger& aR, RInteger& aS);
	
	/** Default constructor */
	IMPORT_C CDSASignature(void);
protected:
	/** The DSA signature's R value */
	RInteger iR;
	/** The DSA signature's S value */
	RInteger iS;
private:
	CDSASignature(const CDSASignature&);
	CDSASignature& operator=(const CDSASignature&);
	};

/**
* Implementation of DSA signing as specified in FIPS 186-2 change request 1.
* 
* @publishedPartner
* @released 
*/
class CDSASigner : public CSigner<CDSASignature>
	{
public:
	/**
	 * Creates a new CDSASigner object from a specified DSA private key.
	 *
	 * @param aKey	The DSA private key to be used for signing
	 * @return		A pointer to the new CDSASigner object
	 */
	IMPORT_C static CDSASigner* NewL(const CDSAPrivateKey& aKey);

	/**
	 * Creates a new CDSASigner object from a specified DSA private key.
	 *  
	 * The returned pointer is put onto the cleanup stack.
	 *
	 * @param aKey	The DSA private key to be used for signing
	 * @return		A pointer to the new CDSASigner object
	 */
	IMPORT_C static CDSASigner* NewLC(const CDSAPrivateKey& aKey);
	/**
	 * Digitally signs the specified input message
	 *
	 * Note that in order to be interoperable and compliant with the DSS, aInput
	 * must be the result of a SHA-1 hash.
	 *
	 * @param aInput	A SHA-1 hash of the message to sign
	 * @return			A pointer to a new CSignature object
	 *
	 * @panic ECryptoPanicInputTooLarge	If aInput is larger than MaxInputLength(),
	 *									which is likely to happen if the caller
	 *									has passed in something that has not been hashed.
	 */
	virtual CDSASignature* SignL(const TDesC8& aInput) const;
	virtual TInt MaxInputLength(void) const;
protected:
	/** @internalAll */
	CDSASigner(const CDSAPrivateKey& aKey);
protected:
	/** The DSA private key to be used for signing */
	const CDSAPrivateKey& iPrivateKey;
private:
	CDSASigner(const CDSASigner&);
	CDSASigner& operator=(const CDSASigner&);
	};

/**
* Implementation of DSA signature verification as specified in FIPS 186-2 change
* request 1.
* 
* @publishedPartner
* @released 
*/
class CDSAVerifier : public CVerifier<CDSASignature>
	{
public:
	/**
	 * Creates a new CDSAVerifier object from a specified DSA public key.
	 *
	 * @param aKey	The DSA public key to be used for verifying
	 * @return		A pointer to the new CDSAVerifier object
	 */
	IMPORT_C static CDSAVerifier* NewL(const CDSAPublicKey& aKey);

	/**
	 * Creates a new CDSAVerifier object from a specified DSA public key.
	 *  
	 * The returned pointer is put onto the cleanup stack.
	 *
	 * @param aKey	The DSA public key to be used for verifying
	 * @return		A pointer to the new CDSAVerifier object
	 */
	IMPORT_C static CDSAVerifier* NewLC(const CDSAPublicKey& aKey);
	/**
	 * Verifies the specified digital signature
	 *
	 * Note that in order to be interoperable and compliant with the DSS, aInput
	 * must be the result of a SHA-1 hash.
	 *
	 * @param aInput		A SHA-1 hash of the received message
	 * @param aSignature	The signature to be verified
	 * 
	 * @return				Whether the signature is the result of signing
	 *						aInput with the supplied key
	 */
	virtual TBool VerifyL(const TDesC8& aInput, const CDSASignature& aSignature) const;
	virtual TInt MaxInputLength(void) const;
protected:
	/** @internalAll */
	CDSAVerifier(const CDSAPublicKey& aKey);
protected:
	/** The DSA public key to be used for verification */
	const CDSAPublicKey& iPublicKey;
private:
	CDSAVerifier(const CDSAVerifier&);
	CDSAVerifier& operator=(const CDSAVerifier&);
	};

/**
* Implementation of Diffie-Hellman key agreement as specified in PKCS#3.
* 
* @publishedPartner
* @released 
*/
class CDH : public CBase
	{
public:
	/**
	 * Creates a new CDH object from a specified DH private key.
	 *
	 * @param aPrivateKey	The private key of this party
	 * @return				A pointer to the new CDH object
	 */
	IMPORT_C static CDH* NewL(const CDHPrivateKey& aPrivateKey);

	/**
	 * Creates a new CDH object from a specified DH private key.
	 *  
	 * The returned pointer is put onto the cleanup stack.
	 *
	 * @param aPrivateKey	The private key of this party
	 * @return				A pointer to the new CDH object
	 */
	IMPORT_C static CDH* NewLC(const CDHPrivateKey& aPrivateKey);
	
	/**
	 * Performs the key agreement operation.
	 *
	 * @param aPublicKey	The public key of the other party
	 * @return				The agreed key
	 */
	IMPORT_C HBufC8* AgreeL(const CDHPublicKey& aPublicKey) const;
protected:
	/**
	 * Constructor
	 *
	 * @param aPrivateKey	The DH private key
	 */
	IMPORT_C CDH(const CDHPrivateKey& aPrivateKey);
protected:
	/** The DH private key */
	const CDHPrivateKey& iPrivateKey;
private:
	CDH(const CDH&);
	CDH& operator=(const CDH&);
	};

#endif	//	__ASYMMETRIC_H__